/* Interface for the IPC benchmark server.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <l4/sys/capability>
#include <l4/sys/cxx/ipc_iface>

namespace sqlite {
namespace ipc {

struct SqliteIpc : L4::Kobject_t<SqliteIpc, L4::Kobject, 0x42> {
  typedef L4::Typeid::Rpcs<> Rpcs;
};

} // namespace ipc
} // namespace sqlite
