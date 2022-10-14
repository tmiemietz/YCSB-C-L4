/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.cc - A database backend using the sqlite IPC server.         *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#include "sqlite_ipc_db.h" // Class definitions for sqlite_ipc_db
#include "ipc_server.h"

#include <assert.h>
#include <exception>
#include <iostream>
#include <l4/re/env>
#include <l4/re/error_helper>
#include <l4/re/util/cap_alloc>
#include <stdexcept>
#include <sys/ipc.h>

using sqlite::ipc::BenchI;
using sqlite::ipc::DbI;
using std::string;
using std::vector;

namespace ycsbc {

/* Initialize IPC gate capability. */
SqliteIpcDB::SqliteIpcDB() : server{L4Re::Env::env()->get_cap<DbI>("ipc")} {
  L4Re::chkcap(server);
}

/* Send IPC for creating schema. */
void SqliteIpcDB::CreateSchema(DB::Tables tables) {
  // TODO
}

/* Create a new session for this thread at the SQLite server. */
void *SqliteIpcDB::Init() {
  // FIXME: Free capability.
  L4::Cap<BenchI> bench = L4Re::Util::cap_alloc.alloc<BenchI>();
  L4Re::chkcap(bench);

  // Send spawn command to server.
  assert(server->spawn(bench) == L4_EOK);

  std::cout << "New thread initialized." << std::endl;
  return nullptr;
}

int SqliteIpcDB::Read(void *ctx_, const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Scan(void *ctx_, const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Update(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Insert(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

int SqliteIpcDB::Delete(void *ctx_, const string &table, const string &key) {
  // TODO
  throw std::runtime_error{"unimplemented"};
}

} // namespace ycsbc
