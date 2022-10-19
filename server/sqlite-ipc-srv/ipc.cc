/* Benchmark server using only IPC for communication.
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
#include <pthread-l4.h>
#include <thread>

#include "db.h"
#include "ipc.h"
#include "sqlite_ipc_server.h"              // IPC interface for this server
#include "serializer.h"
#include "sqlite_lib_db.h"
#include "utils.h"

using serializer::Serializer;
using serializer::Deserializer;
using ycsbc::DB;

namespace sqlite {
namespace ipc {

// Server object for the main server (not the worker threads)
Registry main_server;

// Implements a single benchmark thread, which performs the Read(), Scan(), etc.
// operations.
class BenchServer : public L4::Epiface_t<BenchServer, BenchI> {
  // Registry of this thread. Handles the server loop.
  // The default constructor must not be used from a non-main thread.
  Registry registry{L4::Cap<L4::Thread>{pthread_l4_cap(pthread_self())},
                    L4Re::Env::env()->factory()};
  
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

  // Create a new benchmark server running its own server loop on this thread.
  static void loop(pthread_barrier_t *barrier, L4::Cap<L4Re::Dataspace> *in,
            L4::Cap<L4Re::Dataspace> *out, L4::Cap<BenchI> *gate,
            ycsbc::SqliteLibDB *db) {
    // FIXME: server is never freed.
    auto server = new BenchServer{*in, *out, db};
    // FIXME: Capability is never unregistered.
    L4Re::chkcap(server->registry.registry()->register_obj(server));

    *gate = server->obj_cap();

    std::cout << "Spawned new server thread." << std::endl;;

    // Signal that gate is now set.
    int rc = pthread_barrier_wait(barrier);
    assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

    // Start waiting for communication.
    server->registry.loop();
  }

  // Read some value from the database
  long op_read(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<std::string> fields = std::vector<std::string>(0);
    
    // Output vector, sent back to client after operation
    std::vector<DB::KVPair> result;

    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> fields;
   
    if (database->Read(sqlite_ctx, table, key, &fields, result) != DB::kOK) {
      return(-L4_EINVAL);
    }

    // Put result into output dataspace
    memset(ds_out_addr, '\0', YCSBC_DS_SIZE);
    Serializer s{ds_out_addr, YCSBC_DS_SIZE};
    s << result;

    return(L4_EOK);
  }
  
  // Insert a value into the database
  long op_insert(BenchI::Rights) {
    // Placeholder variables, will be filled from input page
    std::string table;
    std::string key;
    std::vector<DB::KVPair> values;
    
    // Deserialize input from input dataspace
    Deserializer d{ds_in_addr};

    d >> table;
    d >> key;
    d >> values;

    if (database->Insert(sqlite_ctx, table, key, values) != DB::kOK) {
      return(-L4_EINVAL);
    }
    
    return(L4_EOK);
  }
};

// Implements the interface for the database management and a factory for new
// benchmark threads.
class DbServer : public L4::Epiface_t<DbServer, DbI> {
  // YCSB SQLite backend which we are testing against.
  // TODO: Delete when tearing down this object
  ycsbc::SqliteLibDB *db = nullptr;
  pthread_barrier_t barrier;

public:
  DbServer() {
    // Use a barrier to wait for the other thread to return gate.
    // FIXME: Destroy barrier.
    pthread_barrier_t barrier;
    assert(!pthread_barrier_init(&barrier, NULL, 2));
  }

  long op_schema(DbI::Rights, L4::Ipc::Snd_fpage buf_cap) {
    char *map_addr;             // Current position inside the infopage mapping

    // At first, check if we actually received a capability
    if (! buf_cap.cap_received()) {
      std::cerr << "Received fpage was not a capability." << std::endl;
      return(-L4_EACCESS);
    }

    // Now, map the buffer capability to our infopage (index 0, because we
    // only expect one capability to be sent)
    infopage = main_server.rcv_cap<L4Re::Dataspace>(0);
    if (L4Re::Env::env()->rm()->attach(&infopage_addr, YCSBC_DS_SIZE,
                                       L4Re::Rm::F::Search_addr |
                                       L4Re::Rm::F::R,
                                       L4::Ipc::make_cap_full(infopage)) < 0) {
      std::cerr << "Failed to map client-provided infopage.";
      return(-L4_EINVAL);
    }

    map_addr = infopage_addr;
    std::size_t fname_size = 0;
    memcpy(&fname_size, map_addr, sizeof(std::size_t));
    map_addr += sizeof(std::size_t);
    
    std::string fname{};
    fname.append(map_addr, fname_size);
    map_addr += fname_size;

    db = new ycsbc::SqliteLibDB(fname);

    DB::Tables tables{};
    Deserializer d{map_addr};

    d >> tables;
    db->CreateSchema(tables);

    return L4_EOK;
  }

  long op_spawn(DbI::Rights, L4::Ipc::Snd_fpage in_buf,
                L4::Ipc::Snd_fpage out_buf, L4::Ipc::Cap<BenchI> &res) {
    L4::Cap<BenchI> gate;
    
    // Check if we actually received capabilities
    if (! in_buf.cap_received() || ! out_buf.cap_received()) {
      std::cerr << "Received fpages were not capabilities." << std::endl;
      return(-L4_EACCESS);
    }

    // Construct the memory buffer caps from the input arguments
    L4::Cap<L4Re::Dataspace> in  = main_server.rcv_cap<L4Re::Dataspace>(0);
    L4::Cap<L4Re::Dataspace> out = main_server.rcv_cap<L4Re::Dataspace>(1);

    // Thread object must not be constructed on the stack.
    // FIXME: Cleanup thread object.
    new std::thread{&BenchServer::loop, &barrier, &in, &out, 
                    &gate, std::ref(db)};

    // Wait for other thread to set gate.
    int rc = pthread_barrier_wait(&barrier);
    assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

    // Return the IPC gate to the benchmark server.
    res = L4::Ipc::make_cap_rw(gate);

    return L4_EOK;
  }

private:
  // Dataspace and address for transferring metadata (such as table layout)
  // from the client to the server
  L4::Cap<L4Re::Dataspace> infopage;
  char *infopage_addr;
};

void registerServer(Registry &registry) {
  static DbServer server;

  // Register server
  if (!registry.registry()->register_obj(&server, "ipc").is_valid())
    throw std::runtime_error{
        "Could not register IPC server, is there an 'ipc' in the caps table?"};
}

} // namespace ipc
} // namespace sqlite

int main() {
  std::cout << "SQLite 3 Version: " << SQLITE_VERSION << std::endl;

  sqlite::ipc::registerServer(sqlite::ipc::main_server);
  std::cout << "Servers registered. Waiting for requests..." << std::endl;
  sqlite::ipc::main_server.loop();

  return(0);
}
