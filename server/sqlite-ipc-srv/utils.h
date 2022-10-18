/* Utils for sqlite-server.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>

namespace sqlite {

typedef L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> Registry;

} // namespace sqlite
