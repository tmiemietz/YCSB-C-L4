/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.h - A database backend using the sqlite library linked into  *
 *                   the YCSB benchmark process.                              *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 *                                                                            *
 ******************************************************************************/

#ifndef YCSB_C_SQLITE_LIB_H
#define YCSB_C_SQLITE_LIB_H

#include "core/db.h"                // YCSBC interface for databases
#include <sqlite3.h>                // Definitions for Sqlite

#include <string>
#include <vector>
#include <set>
#include <mutex>

namespace ycsbc {

class SqliteLibDB : public DB {
    public:
        /*
         * Constructor that takes the filename for storing the sqlite
         * benchmark database. By default, the in-memory implementation of
         * sqlite is used.
         */
        SqliteLibDB(const std::string &filename = std::string(":memory:"));

        int Read(const std::string &table, const std::string &key,
                 const std::vector<std::string> *fields,
                 std::vector<KVPair> &result);

        int Scan(const std::string &table, const std::string &key,
                 int len, const std::vector<std::string> *fields,
                 std::vector<std::vector<KVPair>> &result);

        int Update(const std::string &table, const std::string &key,
                   std::vector<KVPair> &values);

        int Insert(const std::string &table, const std::string &key,
                  std::vector<KVPair> &values);

        int Delete(const std::string &table, const std::string &key);

    private:
        sqlite3 *database;                  // DB that we are working with

        // Vector for quick lookup on which tables have already been created
        std::set<std::string> table_names;
        // Mutex for guarding the table creation process. We should only have
        // to lock operations that concern the table_names set, which should
        // only be the case during insert operations. For all other cases, 
        // sqlite should do the proper concurrency control itself, given that
        // we compiled it in serialized mode. Hence, we only have to make sure
        // not to share transaction specific state between multiple threads.
        // Also, keep in mind that we deliberately accept queries to 
        // non-existing tables during all operations but insertions and return
        // a no-record error in this case.
        std::mutex table_lock;

        /*********************************************************************
         *
         * Creates a new table named <name>.
         *
         * The schema of newly create tables is as follows:
         *      - YCSBC_KEY (VARCHAR(64), primary key of table)
         *      - YCSBC_TAG (VARCHAR(64), key component of KV pairs returned)
         *      - YCSBC_VAL (VARCHAR(64), value component of KV pairs returned)
         */
        int CreateTable(const std::string &name);


        /* Sqlite callback for adding a result to a result vector             */
        static int SqliteVecAddCallback(void *vector, int cnt, char **data, 
                                        char **cols);
};  

} // ycsbc

#endif /* YCSB_C_SQLITE_LIB_H */
