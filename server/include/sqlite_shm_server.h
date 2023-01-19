/* Interface for the shared memory benchmark server.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>
#include <l4/sys/factory>
#include <l4/sys/kobject>
#include <l4/re/dataspace>

namespace sqlite {
namespace shm {

// Interface for the database management and the factory for new benchmark
// threads. Make sure to reserve two capability slots in this IF.
struct DbI : L4::Kobject_t<DbI, L4::Kobject, 0x43, L4::Type_info::Demand_t<2>> {
  // Create the database schema.
  // The table information as well as database startup parameters are 
  // serialized in the infopage dataspace, to which the server gains a 
  // client-provided capability.
  L4_INLINE_RPC(long, schema, (L4::Ipc::Cap<L4Re::Dataspace>));
  
  // Spawn a new thread on cpu with its own database connection.
  L4_INLINE_RPC(long, spawn, (L4::Ipc::Cap<L4Re::Dataspace>, 
                              L4::Ipc::Cap<L4Re::Dataspace>, l4_umword_t cpu));

  typedef L4::Typeid::Rpcs<schema_t, spawn_t> Rpcs;
};

} // namespace shm
} // namespace sqlite
