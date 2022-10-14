/* Benchmark server using only IPC for communication.
 *
 * Author: Viktor Reusch
 */

#include <iostream>
#include <l4/re/dataspace>
#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>

#include "ipc.h"
#include "ipc_server.h"
#include "sqlite_lib_db.h"

namespace sqlite {
namespace ipc {

class SqliteIpcServer : public L4::Epiface_t<SqliteIpcServer, SqliteIpc> {
  // A superpage should be enough to hold the results even for a scan.
  static constexpr unsigned char SIZE_SHIFT = L4_SUPERPAGESHIFT;
  static constexpr std::size_t SIZE = 1 << SIZE_SHIFT;

  // YCSB SQLite backend which we are testing against.
  ycsbc::SqliteLibDB db{};
  // This dataspace holds the memory used for returning results.
  // FIXME: Dataspace is not freed.
  L4::Cap<L4Re::Dataspace> ds = L4Re::Util::cap_alloc.alloc<L4Re::Dataspace>();
  // Addr of the dataspace.
  l4_addr_t addr;

public:
  SqliteIpcServer() {
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
};

void registerServer(Registry &registry) {
  static SqliteIpcServer server;

  // Register server
  if (!registry.registry()->register_obj(&server, "ipc").is_valid())
    throw std::runtime_error{
        "Could not register IPC server, is there an 'ipc' in the caps table?"};
}

} // namespace ipc
} // namespace sqlite
