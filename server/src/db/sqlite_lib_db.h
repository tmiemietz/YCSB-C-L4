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

namespace ycsbc {

class SqliteLibDB : public DB {
    public:
        /*
         * Constructor that takes the filename for storing the sqlite
         * benchmark database. By default, the in-memory implementation of
         * sqlite is used.
         */
        SqliteLibDB(const std::string &filename = std::string(":memory:"),
                    size_t db_col_cnt = 10);

        void Init() override;

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
        // Filename of the DB
        const std::string &filename;
        // DB that we are working with
        sqlite3 *database;

        // Number of data columns used in the benchmark table, defaults to
        // 10 (set in the constructor)
        size_t ycsbc_num_cols;

        /*********************************************************************
         *
         * Creates a new table named <name>.
         *
         * The schema of newly created tables is as follows:
         *      - one column named YCSBC_KEY (VARCHAR, primary key of table)
         *      - cols times TEXT columns, named from FIELD0 to 
         *        FIELD<cols - 1>
         */
        int CreateTable(const std::string &name, size_t cols);


        /* Sqlite callback for adding a result to a result vector             */
        static int SqliteVecAddCallback(void *vector, int cnt, char **data, 
                                        char **cols);
};  

} // ycsbc

#endif /* YCSB_C_SQLITE_LIB_H */
