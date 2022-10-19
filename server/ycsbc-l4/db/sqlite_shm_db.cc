/******************************************************************************
 *                                                                            *
 * sqlite_shm_db.cc - A database backend using the sqlite shared mem server.  *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#include "sqlite_shm_db.h" // Class definitions for sqlite_shm_db
#include "serializer.h"
#include "utils.h"

#include <array>
#include <assert.h>
#include <cstddef>
#include <exception>
#include <iostream>
#include <l4/re/error_helper> // L4Re::Chkcap and friends
#include <l4/re/rm>
#include <l4/re/util/cap_alloc>
#include <l4/util/util.h> // l4_sleep()
#include <memory>         // unique_ptr etc.
#include <stdexcept>
#include <sys/ipc.h>

using serializer::Deserializer;
using serializer::Serializer;
using sqlite::YCSBC_DS_SIZE;
using sqlite::shm::DbI;
using std::string;
using std::vector;

namespace ycsbc {

/*
 * Context structure for clients of the sqlite IPC server.
 */
struct IpcCltCtx {
  // Dataspace for transmitting input parameters of benchmark functions
  L4::Cap<L4Re::Dataspace> ds_in;
  char *ds_in_addr = 0;

  // Dataspace for receiving output of benchmark functions
  L4::Cap<L4Re::Dataspace> ds_out;
  char *ds_out_addr = 0;

  IpcCltCtx() = default;

  ~IpcCltCtx() {
    // Resource deallocation is currently done inside SqliteShmDB::Close().
  }

  IpcCltCtx(const IpcCltCtx &) = delete;
  IpcCltCtx(IpcCltCtx &&) = delete;

  IpcCltCtx &operator=(const IpcCltCtx &) = delete;
  IpcCltCtx &operator=(IpcCltCtx &&) = delete;

  Serializer serializer() const {
    return std::move(Serializer{ds_in_addr + 1, YCSBC_DS_SIZE - 1});
  }

  // Sends the message to the other side and waits for a response.
  Deserializer call(char opcode) const {
    // Notify other side about message.
    __atomic_store_n(ds_in_addr, opcode, __ATOMIC_RELEASE);

    // Wait for incoming message.
    while (!(__atomic_load_n(ds_out_addr, __ATOMIC_ACQUIRE))) {
      // Use PAUSE to hint a spin-wait loop. This should use YIELD on ARM.
      __builtin_ia32_pause();
      // The program hangs without this line.
      l4_sleep(1);
    }

    // Reset notification byte.
    *ds_out_addr = 0;

    return std::move(Deserializer{ds_out_addr + 1});
  }

  static IpcCltCtx &cast(void *ctx) {
    return *reinterpret_cast<IpcCltCtx *>(ctx);
  }
};

/* Initialize IPC gate capability. */
SqliteShmDB::SqliteShmDB(const string &filename)
    : filename{filename}, server{L4Re::Env::env()->get_cap<DbI>("shm")} {
  L4Re::chkcap(server);

  // Setup the main thread's data space used for sending database schema
  // information to the server during SqliteShmDB::CreateSchema()
  db_infopage = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  L4Re::chkcap(db_infopage);

  // Allocate the new dataspace
  if (L4Re::Env::env()->mem_alloc()->alloc(YCSBC_DS_SIZE, db_infopage) < 0) {
    throw std::runtime_error{"Failed to allocate db_infopage dataspace."};
  }

  // Attach to this AS, use 0 as a mapping address (first arg) to allow for
  // mapping the DS anywhere.
  // KEEP IN MIND: Unlike in previous versions of L4, we now have to specify
  // the desired rights of the memory region explicitely, or this operation
  // will fail with ENOENT
  long l = 0;
  if ((l = L4Re::Env::env()->rm()->attach(
           &db_infopage_addr, YCSBC_DS_SIZE,
           L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
           L4::Ipc::make_cap_rw(db_infopage))) < 0) {
    std::cerr << "Attach failed: " << l << std::endl;
    throw std::runtime_error{"Failed to attach db_infopage dataspace."};
  }
}

/* Send IPC for creating the schema. */
void SqliteShmDB::CreateSchema(DB::Tables tables) {
  // Funnel the filename and the schema description into the infopage.
  Serializer s{db_infopage_addr, YCSBC_DS_SIZE};
  s << filename;
  s << tables;

  // Call the server
  L4::Ipc::Cap<L4Re::Dataspace> snd_cap(db_infopage);
  auto rc = server->schema(snd_cap);
  assert(rc == L4_EOK);

  std::cout << "Schema created." << std::endl;
}

/* Create a new session for this thread at the SQLite server. */
void *SqliteShmDB::Init() {
  std::unique_ptr<IpcCltCtx> ctx{new IpcCltCtx{}};

  ctx->ds_in = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  L4Re::chkcap(ctx->ds_in);

  ctx->ds_out = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  L4Re::chkcap(ctx->ds_out);

  // Allocate the new dataspaces for input and output
  if (L4Re::Env::env()->mem_alloc()->alloc(YCSBC_DS_SIZE, ctx->ds_in) < 0) {
    throw std::runtime_error{"Failed to allocate db_in dataspace."};
  }
  if (L4Re::Env::env()->mem_alloc()->alloc(YCSBC_DS_SIZE, ctx->ds_out) < 0) {
    throw std::runtime_error{"Failed to allocate db_out dataspace."};
  }

  // Map new dataspaces into this AS
  if (L4Re::Env::env()->rm()->attach(&ctx->ds_in_addr, YCSBC_DS_SIZE,
                                     L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                                     L4::Ipc::make_cap_rw(ctx->ds_in)) < 0) {
    throw std::runtime_error{"Failed to attach db_in dataspace."};
  }
  if (L4Re::Env::env()->rm()->attach(&ctx->ds_out_addr, YCSBC_DS_SIZE,
                                     L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW,
                                     L4::Ipc::make_cap_rw(ctx->ds_out)) < 0) {
    throw std::runtime_error{"Failed to attach db_out dataspace."};
  }

  // Set the first notification bits of the dataspaces to zero.
  // Thus, the receiving threads will wait initially.
  *ctx->ds_in_addr = 0;

  // Send spawn command to server. Pay attiontion to the fact that we have to
  // explicitely make read-write capabilities in order for the sender to be
  // able to write to the memory that we send him!
  assert(server->spawn(L4::Ipc::make_cap_rw(ctx->ds_in),
                       L4::Ipc::make_cap_rw(ctx->ds_out)) == L4_EOK);

  std::cout << "New thread initialized." << std::endl;
  return (ctx.release());
}

int SqliteShmDB::Read(void *ctx_, const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  // Serialize everything into the input dataspace
  Serializer s = std::move(ctx.serializer());
  s << table;
  s << key;
  // We must transfer anything at all, even if it is just an empty vector
  if (fields != nullptr)
    s << *fields;
  else
    s << std::vector<std::string>(0);

  // Call the server
  Deserializer d = std::move(ctx.call('r'));

  // Deserialize the operation results
  d >> result;

  if (result.size() == 0)
    return (kErrorNoData);
  else
    return (kOK);
}

int SqliteShmDB::Scan(void *ctx_, const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  // Serialize everything into the input dataspace
  Serializer s = std::move(ctx.serializer());
  s << table;
  s << key;
  s << len;
  // We must transfer anything at all, even if it is just an empty vector
  if (fields != nullptr)
    s << *fields;
  else
    s << std::vector<std::string>(0);

  // Call the server
  Deserializer d = std::move(ctx.call('s'));

  // Deserialize the operation results
  d >> result;

  if (result.size() == 0)
    return (kErrorNoData);
  else
    return (kOK);
}

int SqliteShmDB::Update(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  // Serialize everything into the input dataspace
  Serializer s = std::move(ctx.serializer());
  s << table;
  s << key;
  s << values;

  // Call the server
  ctx.call('u');

  return (kOK);
}

int SqliteShmDB::Insert(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  // Serialize everything into the input dataspace
  Serializer s = std::move(ctx.serializer());
  s << table;
  s << key;
  s << values;

  // Call the server
  ctx.call('i');

  return (kOK);
}

int SqliteShmDB::Delete(void *ctx_, const string &table, const string &key) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  // Serialize everything into the input dataspace
  Serializer s = std::move(ctx.serializer());
  s << table;
  s << key;

  // Call the server
  ctx.call('d');

  return (kOK);
}

// Signals the end of the connection to the Sqlite IPC server and destroys the
// context associated with this worker thread. This also involves freeing all
// dataspaces used for communication with the server.
void SqliteShmDB::Close(void *ctx_) {
  auto &ctx = IpcCltCtx::cast(ctx_);

  ctx.call('c');

  // Detach communication mappings from this address space
  if (L4Re::Env::env()->rm()->detach(ctx.ds_in_addr, &ctx.ds_in) < 0) {
    std::cerr << "Failed to detach input dataspace." << std::endl;
    return;
  }
  if (L4Re::Env::env()->rm()->detach(ctx.ds_out_addr, &ctx.ds_out) < 0) {
    std::cerr << "Failed to detach output dataspace." << std::endl;
    return;
  }

  // Return the memory of the dataspaces
  // Note that we could have also directly disabled the derived mappings in
  // the server process by adding L4_FP_ALL_SPACES to the flags. However, I
  // thought that it would be nice to notify the server anyway, so we trust it
  // to do the unmapping itself.
  L4Re::Env::env()->task()->unmap(ctx.ds_in.fpage(), L4_FP_DELETE_OBJ);
  L4Re::Env::env()->task()->unmap(ctx.ds_out.fpage(), L4_FP_DELETE_OBJ);

  // Free the caps associated with the communication mappings
  L4Re::Util::cap_alloc.free(ctx.ds_in);
  L4Re::Util::cap_alloc.free(ctx.ds_out);

  delete &ctx;

  std::cerr << "Benchmark thread terminated." << std::endl;
}

} // namespace ycsbc
