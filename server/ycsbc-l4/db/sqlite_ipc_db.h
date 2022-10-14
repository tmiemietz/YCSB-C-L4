/******************************************************************************
 *                                                                            *
 * sqlite_ipc_db.h - A database backend using the sqlite IPC server.          *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#pragma once

#include "db.h"         // YCSBC interface for databases
#include "ipc_server.h" // Interfaces for the SQLite server.
#include <sqlite3.h>    // Definitions for Sqlite

#include <string>
#include <vector>

namespace ycsbc {

struct Ctx;

class SqliteIpcDB : public DB {
public:
  SqliteIpcDB();
  // FIXME: Add destructor.

  void CreateSchema(DB::Tables tables) override;
  void *Init() override;
  // FIXME: Implement Close().

  int Read(void *ctx, const std::string &table, const std::string &key,
           const std::vector<std::string> *fields,
           std::vector<KVPair> &result) override;

  int Scan(void *ctx, const std::string &table, const std::string &key, int len,
           const std::vector<std::string> *fields,
           std::vector<std::vector<KVPair>> &result) override;

  int Update(void *ctx, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) override;

  int Insert(void *ctx, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) override;

  int Delete(void *ctx, const std::string &table,
             const std::string &key) override;

private:
  L4::Cap<sqlite::ipc::DbI> server;
};

} // namespace ycsbc
