/* Interface for the IPC benchmark server.
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
namespace ipc {

// Standard size for datasapces used for exchanging information during the      
// benchmark, currently set to 1 MiB.                                           
static const size_t YCSBC_DS_SIZE = 1 << 20; 

// IPC interface to a single benchmark thread, which performs the Read(),
// Scan(), etc. operations.
struct BenchI : L4::Kobject_t<BenchI, L4::Kobject, 0x42> {
  // Performs a read operation by collecting parameters from the input dataspace
  // handed over previously during the spawn procedure
  L4_INLINE_RPC(long, read, ());

  // Performs a scan operation by collecting parameters from the input dataspace
  // handed over previously during the spawn procedure
  L4_INLINE_RPC(long, scan, ());
  
  // Performs an insert operation. Parameters are collected from the input
  // Dataspace
  L4_INLINE_RPC(long, insert, ());
  
  // Performs an update operation. Parameters are collected from the input
  // Dataspace
  L4_INLINE_RPC(long, update, ());
  
  // Performs a delete operation. Parameters are collected from the input
  // Dataspace
  L4_INLINE_RPC(long, del, ());
  
  typedef L4::Typeid::Rpcs<read_t, scan_t, insert_t, update_t, del_t> Rpcs;
};

// Interface for the database management and the factory for new benchmark
// threads. Make sure to reserve two capability slots in this IF.
struct DbI : L4::Kobject_t<DbI, L4::Kobject, 0x43, L4::Type_info::Demand_t<2>> {
  // Create the database schema.
  // The table information as well as database startup parameters are 
  // serialized in the infopage dataspace, to which the server gains a 
  // client-provided capability.
  L4_INLINE_RPC(long, schema, (L4::Ipc::Cap<L4Re::Dataspace>));
  
  // Spawn a new thread with its own database connection.
  // Returns an IPC gate for communication with this thread.
  L4_INLINE_RPC(long, spawn, (L4::Ipc::Cap<L4Re::Dataspace>, 
                              L4::Ipc::Cap<L4Re::Dataspace>,
                              L4::Ipc::Out<L4::Cap<BenchI>>));

  typedef L4::Typeid::Rpcs<schema_t, spawn_t> Rpcs;
};

} // namespace ipc
} // namespace sqlite
