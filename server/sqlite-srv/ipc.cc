/* Benchmark server using only IPC for communication.
 *
 * Author: Viktor Reusch
 */

#include <iostream>
#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>

#include "sqlite_lib_db.h"
#include "ipc.h"
#include "ipc_server.h"

namespace sqlite {
namespace ipc {

class SqliteIpcServer : public L4::Epiface_t<SqliteIpcServer, SqliteIpc> {
  ycsbc::SqliteLibDB db{};

public:
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
