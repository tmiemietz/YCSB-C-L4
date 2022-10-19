/* Benchmark server using shared memory for communication.
 *
 * Author: Viktor Reusch
 */

#include <cassert>
#include <iostream>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <l4/sys/err.h>
#include <l4/sys/factory>
#include <l4/sys/ipc_gate>
#include <l4/util/util.h> // l4_sleep()
#include <pthread-l4.h>
#include <stdexcept>
#include <thread>

#include "db.h"
#include "serializer.h"
#include "sqlite_lib_db.h"
#include "sqlite_shm_server.h" // IPC interface for this server
#include "utils.h"

using serializer::Deserializer;
using serializer::Serializer;
using ycsbc::DB;

namespace sqlite {
namespace shm {

// Server object for the main server
Registry main_server;

// Implements a single benchmark thread, which performs the Read(), Scan(), etc.
// operations.
class BenchServer {
  // Dataspaces received from client for input and output respectively
  L4::Cap<L4Re::Dataspace> ds_in;
  char *ds_in_addr = 0;

  L4::Cap<L4Re::Dataspace> ds_out;
  char *ds_out_addr = 0;

  // SqliteLibDB object create in the main thread
  ycsbc::SqliteLibDB *database;

  // Context object returned from SqliteLibDB object
  void *sqlite_ctx;

public:
  BenchServer(L4::Cap<L4Re::Dataspace> in, L4::Cap<L4Re::Dataspace> out,
              ycsbc::SqliteLibDB *db) {
    ds_in = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    L4Re::chkcap(ds_in);

    ds_out = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
    L4Re::chkcap(ds_out);

    // Move input capabilities to local cap slots
    ds_in.move(in);
    ds_out.move(out);

    database = db;

    // Attach memory windows to this AS
    // Map new dataspaces into this AS
    if (L4Re::Env::env()->rm()->attach(&ds_in_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::RW,
                                       L4::Ipc::make_cap_full(ds_in)) < 0) {
      throw std::runtime_error{"Failed to attach db_in dataspace."};
    }
    if (L4Re::Env::env()->rm()->attach(&ds_out_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::RW,
                                       L4::Ipc::make_cap_full(ds_out)) < 0) {
      throw std::runtime_error{"Failed to attach db_out dataspace."};
    }

    sqlite_ctx = database->Init();
  }

  // Wait for incoming messages by busy-waiting on the first bit of the input
  // dataspace to be non-zero.
  // Signal a response in the same way on the output dataspace.
  void loop() {
    std::cout << "Spawned new server thread." << std::endl;

    for (;;) {
      char op;

      // A new message is indicated by a non-zero value in the first byte.
      // The non-zero value actually specifies the operation to perform.
      // I would like to use std::atomic_ref here. But it is only available
      // since C++20.
      // Alignment should be irrelevant here because we only access
      // byte-granular.
      while (!(op = __atomic_load_n(ds_in_addr, __ATOMIC_ACQUIRE))) {
        // Use PAUSE to hint a spin-wait loop. This should use YIELD on ARM.
        __builtin_ia32_pause();
        // The program hangs without this line.
        l4_sleep(1);
      }

      // Create (de)serializer honoring the 1 byte used for synchronization.
      Deserializer de{ds_in_addr + 1};
      Serializer ser{ds_out_addr + 1, YCSBC_DS_SIZE - 1};
      long rc = -1;
      // Parse opcode.
      switch (op) {
      case 0:
        throw std::runtime_error{"unreachable"};
      case 'r':
        rc = read(de, ser);
        break;
      case 's':
        rc = scan(de, ser);
        break;
      case 'i':
        rc = insert(de);
        break;
      case 'u':
        rc = update(de);
        break;
      case 'd':
        rc = del(de);
        break;
      case 'c':
        // Send response before unmapping the necessary dataspace.
        __atomic_store_n(ds_out_addr, 1, __ATOMIC_RELEASE);
        assert(close() == L4_EOK);
        return;
      default:
        throw std::runtime_error{"invalid opcode"};
      }

      assert(rc == L4_EOK);

      // Reset notification byte.
      *ds_in_addr = 0;
      __atomic_store_n(ds_out_addr, 1, __ATOMIC_RELEASE);
    }
  }

private:
  // Read some value from the database
  long read(Deserializer &d, Serializer &s) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<std::string> fields = std::vector<std::string>(0);

    // Output vector, sent back to client after operation
    std::vector<DB::KVPair> result;

    d >> table;
    d >> key;
    d >> fields;

    if (database->Read(sqlite_ctx, table, key, &fields, result) != DB::kOK) {
      return (-L4_EINVAL);
    }

    // Put result into output dataspace
    s << result;

    return (L4_EOK);
  }

  // Scan for some values from the database
  long scan(Deserializer &d, Serializer &s) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    int len = 0;
    std::vector<std::string> fields = std::vector<std::string>(0);

    // Output vector, sent back to client after operation
    std::vector<std::vector<DB::KVPair>> result;

    d >> table;
    d >> key;
    d >> len;
    d >> fields;

    if (database->Scan(sqlite_ctx, table, key, len, &fields, result) !=
        DB::kOK) {
      return (-L4_EINVAL);
    }

    // Put result into output dataspace
    s << result;

    return (L4_EOK);
  }

  // Insert a value into the database
  long insert(Deserializer &d) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<DB::KVPair> values;

    d >> table;
    d >> key;
    d >> values;

    if (database->Insert(sqlite_ctx, table, key, values) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Update a value in the database
  long update(Deserializer &d) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<DB::KVPair> values;

    d >> table;
    d >> key;
    d >> values;

    if (database->Update(sqlite_ctx, table, key, values) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Deletes a value from the database
  long del(Deserializer &d) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;

    d >> table;
    d >> key;

    if (database->Delete(sqlite_ctx, table, key) != DB::kOK) {
      return (-L4_EINVAL);
    }

    return (L4_EOK);
  }

  // Unmaps the client-provided memory windows and terminates the server
  long close() {
    // Detach client mappings
    if (L4Re::Env::env()->rm()->detach(ds_in_addr, &ds_in) < 0) {
      std::cerr << "Failed to detach input dataspace." << std::endl;
      return (-L4_EINVAL);
    }
    if (L4Re::Env::env()->rm()->detach(ds_out_addr, &ds_out) < 0) {
      std::cerr << "Failed to detach output dataspace." << std::endl;
      return (-L4_EINVAL);
    }

    // Free the caps associated with the memory mappings
    L4Re::Util::cap_alloc.free(ds_in);
    L4Re::Util::cap_alloc.free(ds_out);

    // TODO: Actually terminate this bench server thread

    return (L4_EOK);
  }
};

// Implements the interface for the database management and a factory for new
// benchmark threads.
class DbServer : public L4::Epiface_t<DbServer, DbI> {
  // YCSB SQLite backend which we are testing against.
  // TODO: Delete when tearing down this object
  ycsbc::SqliteLibDB *db = nullptr;

public:
  long op_schema(DbI::Rights, L4::Ipc::Snd_fpage buf_cap) {
    // At first, check if we actually received a capability
    if (!buf_cap.cap_received()) {
      std::cerr << "Received fpage was not a capability." << std::endl;
      return -L4_EACCESS;
    }

    // Now, map the buffer capability to our infopage (index 0, because we
    // only expect one capability to be sent)
    infopage = main_server.rcv_cap<L4Re::Dataspace>(0);
    if (L4Re::Env::env()->rm()->attach(&infopage_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                           L4Re::Rm::F::R,
                                       L4::Ipc::make_cap_full(infopage)) < 0) {
      std::cerr << "Failed to map client-provided infopage.";
      return -L4_EINVAL;
    }

    Deserializer d{infopage_addr};

    std::string fname{};
    d >> fname;
    db = new ycsbc::SqliteLibDB(fname);

    DB::Tables tables{};

    d >> tables;
    db->CreateSchema(tables);

    return L4_EOK;
  }

  long op_spawn(DbI::Rights, L4::Ipc::Snd_fpage in_buf,
                L4::Ipc::Snd_fpage out_buf) {
    // Check if we actually received capabilities
    if (!in_buf.cap_received() || !out_buf.cap_received()) {
      std::cerr << "Received fpages were not capabilities." << std::endl;
      return (-L4_EACCESS);
    }

    // Construct the memory buffer caps from the input arguments
    L4::Cap<L4Re::Dataspace> in = main_server.rcv_cap<L4Re::Dataspace>(0);
    L4::Cap<L4Re::Dataspace> out = main_server.rcv_cap<L4Re::Dataspace>(1);

    // FIXME: server is never freed.
    auto server = new BenchServer{in, out, db};

    // Thread object must not be constructed on the stack.
    // FIXME: Cleanup thread object.
    new std::thread{&BenchServer::loop, server};

    return L4_EOK;
  }

private:
  // Dataspace and address for transferring metadata (such as table layout)
  // from the client to the server
  L4::Cap<L4Re::Dataspace> infopage;
  char *infopage_addr;
};

static void registerServer(Registry &registry) {
  static DbServer server;

  // Register server
  if (!registry.registry()->register_obj(&server, "shm").is_valid())
    throw std::runtime_error{
        "Could not register IPC server, is there an 'shm' in the caps table?"};
}

} // namespace shm
} // namespace sqlite

int main() {
  std::cout << "SQLite 3 Version: " << SQLITE_VERSION << std::endl;

  sqlite::shm::registerServer(sqlite::shm::main_server);
  std::cout << "Servers registered. Waiting for requests..." << std::endl;
  sqlite::shm::main_server.loop();

  return (0);
}
