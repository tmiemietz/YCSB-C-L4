/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.h - A database backend using the sqlite library linked into  *
 *                   the YCSB benchmark process.                              *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#ifndef YCSB_C_SQLITE_LIB_H
#define YCSB_C_SQLITE_LIB_H

#include "core/db.h"                // YCSBC interface for databases
#include <sqlite3.h>                // Definitions for Sqlite

#include <string>
#include <vector>

namespace ycsbc {

struct Ctx;

class SqliteLibDB : public DB {
    public:
        /*
         * Constructor that takes the filename for storing the sqlite
         * benchmark database. By default, the in-memory implementation of
         * sqlite is used.
         */
        SqliteLibDB(const std::string &filename = std::string(":memory:"));
        ~SqliteLibDB() override;

        void CreateSchema(DB::Tables tables) override;
        void *Init() override;
        void Close(void *ctx) override;

        int Read(void *ctx, const std::string &table, const std::string &key,
                 const std::vector<std::string> *fields,
                 std::vector<KVPair> &result) override;

        int Scan(void *ctx, const std::string &table, const std::string &key,
                 int len, const std::vector<std::string> *fields,
                 std::vector<std::vector<KVPair>> &result) override;

        int Update(void *ctx, const std::string &table, const std::string &key,
                   std::vector<KVPair> &values) override;

        int Insert(void *ctx, const std::string &table, const std::string &key,
                  std::vector<KVPair> &values) override;

        int Delete(void *ctx, const std::string &table,
                   const std::string &key) override;

    private:
        // Filename of the DB
        const std::string filename;

        // Database connection used for creating the schema.
        // It must be kept alive to keep in-memory databases alive.
        sqlite3 *schema_database = nullptr;

        // Open a new database connection.
        sqlite3* OpenDB() const;
};  

} // ycsbc

#endif /* YCSB_C_SQLITE_LIB_H */
