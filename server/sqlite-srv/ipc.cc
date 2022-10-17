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

using serializer::Deserializer;
using ycsbc::DB;

namespace sqlite {
namespace ipc {

// Implements a single benchmark thread, which performs the Read(), Scan(), etc.
// operations.
class BenchServer : public L4::Epiface_t<BenchServer, BenchI> {
  // A superpage should be enough to hold the results even for a scan.
  static constexpr unsigned char SIZE_SHIFT = L4_SUPERPAGESHIFT;
  static constexpr std::size_t SIZE = 1 << SIZE_SHIFT;

  // Registry of this thread. Handles the server loop.
  // The default constructor must not be used from a non-main thread.
  Registry registry{L4::Cap<L4::Thread>{pthread_l4_cap(pthread_self())},
                    L4Re::Env::env()->factory()};
  // This dataspace holds the memory used for returning results.
  // FIXME: Dataspace is not freed.
  L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  // Addr of the dataspace.
  l4_addr_t addr;

public:
  BenchServer() {
    if (!ds.is_valid())
      throw std::runtime_error{"failed to allocate dataspace cap"};
    if (L4Re::Env::env()->mem_alloc()->alloc(SIZE, ds))
      throw std::runtime_error{"failed to allocate dataspace"};
    // Attach the dataspace size-aligned. This allows to send it via a single
    // flexpage.
    if (L4Re::Env::env()->rm()->attach(
            &addr, SIZE, L4Re::Rm::F::Search_addr | L4Re::Rm::F::RW, ds, 0,
            SIZE_SHIFT))
      throw std::runtime_error{"failed to attach dataspace"};

    // TODO: remove
    *reinterpret_cast<char *>(addr) = 'X';
    std::cout << "dataspace addr: " << addr
              << " stores: " << *reinterpret_cast<char *>(addr) << std::endl;
  }

  // Create a new benchmark server running its own server loop on this thread.
  static void loop(pthread_barrier_t *barrier, L4::Cap<BenchI> *gate) {
    // FIXME: server is never freed.
    auto server = new BenchServer{};
    // FIXME: Capability is never unregistered.
    L4Re::chkcap(server->registry.registry()->register_obj(server));

    *gate = server->obj_cap();

    // Signal that gate is now set.
    int rc = pthread_barrier_wait(barrier);
    assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

    // Start waiting for communication.
    server->registry.loop();
  }
};

// Implements the interface for the database management and a factory for new
// benchmark threads.
class DbServer : public L4::Epiface_t<DbServer, DbI> {
  // YCSB SQLite backend which we are testing against.
  ycsbc::SqliteLibDB db{};
  pthread_barrier_t barrier;

public:
  DbServer() {
    // Use a barrier to wait for the other thread to return gate.
    // FIXME: Destroy barrier.
    pthread_barrier_t barrier;
    assert(!pthread_barrier_init(&barrier, NULL, 2));
  }

  long op_schema(DbI::Rights, L4::Ipc::Array_in_buf<char> const &data) {
    DB::Tables tables{};
    Deserializer d{data.data};

    d >> tables;
    db.CreateSchema(tables);

    return L4_EOK;
  }

  long op_spawn(DbI::Rights, L4::Ipc::Cap<BenchI> &res) {
    L4::Cap<BenchI> gate;
    // Thread object must not be constructed on the stack.
    // FIXME: Cleanup thread object.
    new std::thread{&BenchServer::loop, &barrier, &gate};

    // Wait for other thread to set gate.
    int rc = pthread_barrier_wait(&barrier);
    assert(rc == 0 || rc == PTHREAD_BARRIER_SERIAL_THREAD);

    // Return the IPC gate to the benchmark server.
    res = L4::Ipc::make_cap_rw(gate);

    return L4_EOK;
  }
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
