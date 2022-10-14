/* SQLite YCSB Benchmark Server for L4Re
 *
 * Author: Viktor Reusch
 */

#include <iostream>
#include <l4/re/env>
#include <l4/re/util/br_manager>
#include <l4/re/util/cap_alloc>
#include <l4/re/util/object_registry>
#include <l4/sys/cxx/ipc_epiface>
#include <sqlite3.h>

#include "ipc.h"
#include "utils.h"

using namespace sqlite;

static Registry server;

int main() {
  std::cout << "SQLite 3 Version: " << SQLITE_VERSION << std::endl;

  ipc::registerServer(server);
  std::cout << "Servers registered. Waiting for requests..." << std::endl;
  server.loop();

  return 0;
}
