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

// Standard size for datasapces used for exchanging information during the      
// benchmark, currently set to 1 MiB.                                           
static const size_t YCSBC_DS_SIZE = 1 << 20;

typedef L4Re::Util::Registry_server<L4Re::Util::Br_manager_hooks> Registry;

} // namespace sqlite
