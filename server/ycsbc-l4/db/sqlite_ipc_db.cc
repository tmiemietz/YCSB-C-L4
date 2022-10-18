/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.cc - A database backend using the sqlite IPC server.         *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#include "sqlite_ipc_db.h"          // Class definitions for sqlite_ipc_db
#include "serializer.h"

#include <array>
#include <assert.h>
#include <cstddef>
#include <exception>
#include <iostream>
#include <l4/re/error_helper>       // L4Re::Chkcap and friends
#include <l4/re/util/cap_alloc>
#include <l4/re/rm>
#include <stdexcept>
#include <sys/ipc.h>

using serializer::Serializer;
using sqlite::ipc::YCSBC_DS_SIZE;
using sqlite::ipc::BenchI;
using sqlite::ipc::DbI;
using std::string;
using std::vector;

namespace ycsbc {

/* Initialize IPC gate capability. */
SqliteIpcDB::SqliteIpcDB(const string &filename) :
  filename{filename},
  server{L4Re::Env::env()->get_cap<DbI>("ipc")} {
  L4Re::chkcap(server);

  // Setup the main thread's data space used for sending database schema
  // information to the server during SqliteIpcDB::CreateSchema()
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
  if ((l = L4Re::Env::env()->rm()->attach(&db_infopage_addr, YCSBC_DS_SIZE,
                                     L4Re::Rm::F::Search_addr |
                                     L4Re::Rm::F::RW,
                                     L4::Ipc::make_cap_rw(db_infopage))) < 0) {
    std::cerr << "Attach failed: " << l << std::endl;
    throw std::runtime_error{"Failed to attach db_infopage dataspace."};
  }
}

/* Send IPC for creating the schema. */
void SqliteIpcDB::CreateSchema(DB::Tables tables) {
  char *map_ptr;                // Pointer for interating through infopage

  // At first, write size of filename as well as the filename itself to
  // the infopage, followed by a serialized table
  std::size_t fname_size = filename.size();
  map_ptr = db_infopage_addr;
  memcpy(map_ptr, &fname_size, sizeof(std::size_t));
  map_ptr += sizeof(std::size_t);
  memcpy(map_ptr, filename.c_str(), fname_size);
  map_ptr += fname_size;

  // Funnel the schema description into the infopage
  Serializer s{map_ptr, YCSBC_DS_SIZE - fname_size - sizeof(std::size_t)};
  s << tables;

  // Call the server
  L4::Ipc::Cap<L4Re::Dataspace> snd_cap(db_infopage);
  auto rc = server->schema(snd_cap);
  assert(rc == L4_EOK);

  std::cout << "Schema created." << std::endl;
}

/* Create a new session for this thread at the SQLite server. */
void *SqliteIpcDB::Init() {
  // FIXME: Free capability.
  L4::Cap<BenchI> bench = L4Re::Util::cap_alloc.alloc<BenchI>();
  L4Re::chkcap(bench);

  // Send spawn command to server.
  assert(server->spawn(bench) == L4_EOK);

  std::cout << "New thread initialized." << std::endl;
  return nullptr;
}

int SqliteIpcDB::Read(void *ctx_, const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Scan(void *ctx_, const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Update(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Insert(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Delete(void *ctx_, const string &table, const string &key) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

} // namespace ycsbc
