/* Interface for the IPC benchmark server.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/factory>
#include <l4/sys/kobject>

namespace sqlite {
namespace ipc {

// IPC interface to a single benchmark thread, which performs the Read(),
// Scan(), etc. operations.
struct BenchI : L4::Kobject_t<BenchI, L4::Kobject, 0x42> {
  typedef L4::Typeid::Rpcs<> Rpcs;
};

// Interface for the database management and the factory for new benchmark
// threads.
struct DbI : L4::Kobject_t<DbI, L4::Kobject, 0x43> {
  // Spawn a new thread with its own database connection.
  // Returns an IPC gate for communication with this thread.
  L4_INLINE_RPC(long, spawn, (L4::Ipc::Out<L4::Cap<BenchI>>));
  typedef L4::Typeid::Rpcs<spawn_t> Rpcs;
};

} // namespace ipc
} // namespace sqlite
