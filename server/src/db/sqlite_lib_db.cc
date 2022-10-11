/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.cc - A database backend using the sqlite library linked into *
 *                    the YCSB benchmark process.                             *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 * Author: Viktor Reusch                                                      *
 *                                                                            *
 ******************************************************************************/

#include "sqlite_lib_db.h"          // Class definitions for sqlite_lib_db

#include <iostream>
#include <exception>
#include <utility>                  // For std::get and friends...
#include <cassert>                  // For assert
#include <memory>                   // For unique_ptr

using std::string;
using std::vector;

namespace ycsbc {

// Assert that r == SQLITE_OK.
void assert_sqlite(int r) {
    assert(r == SQLITE_OK);
}

struct Ctx {
    // DB that we are working with
    sqlite3 *database = nullptr;

    ~Ctx() {
        if (database)
            assert_sqlite(sqlite3_close(database));
    }

    static Ctx &cast(void *ctx) {
        return *reinterpret_cast<Ctx *>(ctx);
    }
};

/* Create an escaped string.
 *
 * The returned pointer will be correctly freed when going out of scope.
 */
std::unique_ptr<char, decltype(&sqlite3_free)> escape_sql(const char *str) {
    return {sqlite3_mprintf("%Q", str), sqlite3_free};
}

/* Default constructor for the library version of sqlite.
 *
 * filename is copied because default arguments do not outlive the constructor
 * expression.
 */
SqliteLibDB::SqliteLibDB(const string &filename): filename{filename} {}

sqlite3* SqliteLibDB::OpenDB() const {
    int rc = -1;                    // Return code for DB operations

    const char *filename_cstr = filename.c_str();
    // We want multi-threaded mode (SQLITE_OPEN_NOMUTEX).
    int flags =
        SQLITE_OPEN_READWRITE | SQLITE_OPEN_CREATE | SQLITE_OPEN_NOMUTEX;
    // We need cache=shared to share an in-memory DB among multiple threads.
    if (filename == ":memory:") {
        filename_cstr = "file::memory:?cache=shared";
        flags |= SQLITE_OPEN_URI;
    }

    sqlite3 *database;
    // Open a new database
    rc = sqlite3_open_v2(filename_cstr, &database, flags, nullptr);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open sqlite database " << filename << ": "
                  << sqlite3_errmsg(database) << std::endl;
        sqlite3_close(database);

        throw std::runtime_error("Failed to open database.");
    }

    return database;
}

/* Create the database schema (create the tables).
 *
 * The schema of newly created tables is as follows:
 *      - one column named YCSBC_KEY (VARCHAR, primary key of table)
 *      - multiple columns with type TEXT
 */
void SqliteLibDB::CreateSchema(DB::Tables tables) {
    schema_database = OpenDB();

    for (auto &table : tables) {
        int rc = -1;                    // Return code for DB operations

        char *err_msg = NULL;           // Error message returned from sqlite

        // Assemble an SQL table creation statement
        string stmt{"CREATE TABLE IF NOT EXISTS "};
        stmt += escape_sql(table.name.c_str()).get();
        stmt += " (YCSBC_KEY VARCHAR PRIMARY KEY";
        for (auto &col : table.columns) {
            stmt += ", ";
            stmt += escape_sql(col.c_str()).get();
            stmt += " TEXT";
        }
        stmt += ");";

        // We only expect one result row to be returned, hence no separate
        // callback function is needed.
        rc = sqlite3_exec(schema_database, stmt.c_str(), NULL, NULL, &err_msg);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << err_msg << std::endl;

            sqlite3_free(err_msg);

            throw std::runtime_error("Failed to create table");
        }
    }
}

/* Initialize the database connection for this thread. */
void *SqliteLibDB::Init() {
    int rc = -1;                    // Return code for DB operations

    std::unique_ptr<Ctx> ctx{new Ctx{}};
    ctx->database = OpenDB();

    // TODO: Configure journaling (memory vs. off)

    return ctx.release();
}

void SqliteLibDB::Close(void *ctx) {
    delete &Ctx::cast(ctx);
}

/* Sqlite callback for adding a result to a result vector                     */
int SqliteLibDB::SqliteVecAddCallback(void *kvvec, int cnt, 
    char **data, char **cols) {
   
    (void) cols;
    vector<KVPair> *vec = static_cast<vector<KVPair> *>(kvvec);

    // check for expected result format
    if (cnt != 2) 
        return(1);

    vec->push_back(std::pair<string, string>(string(data[0]), string(data[1])));

    return(0);
}

int SqliteLibDB::Read(void *ctx_, const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
    int rc = -1;                    // Return code for DB operations

    char *err_msg = NULL;           // Error message returned from sqlite

    auto ctx = Ctx::cast(ctx_);

    // Assemble an SQL read command
    string read_cmd = "SELECT YCSBC_TAG, YCSBC_VAL FROM " + table + "WHERE ";
    // If fields == NULL, read all records
    if (fields == NULL) {
        read_cmd += "YCSBC_KEY = " + key + ";";
    }
    else {
        read_cmd += "YCSBC_KEY = " + key + " AND YCSBC_TAG IN (";
        for (string const& s : *fields) {
            read_cmd += s + ", ";
        }
        read_cmd += ");";
    }

    // We only expect one result row to be returned, hence no separate 
    // callback function is needed.
    rc = sqlite3_exec(ctx.database, read_cmd.c_str(), &SqliteVecAddCallback,
                      &result, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;

        sqlite3_free(err_msg);
        return(kErrorConflict);
    }

    if (result.empty())
        return(kErrorNoData);
    else
        return(kOK);
}

int SqliteLibDB::Scan(void *ctx, const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
    return(0);
}

int SqliteLibDB::Update(void *ctx, const string &table, const string &key,
                        vector<KVPair> &values) {
    return(0);
}

// Insert a set of key-value pairs into the table <table>.
int SqliteLibDB::Insert(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
    int rc = -1;                    // Return code for DB operations

    char *err_msg = NULL;           // Error message returned from sqlite
    
    auto ctx = Ctx::cast(ctx_);
    
    // Assemble an SQL insertion command
    string insert_cmd = "";
    for (auto const& pair : values) {
        insert_cmd += "INSERT INTO " + table + "VALUES(" +
                      "'" + key + "', " +
                      "'" + std::get<0>(pair) + "', " +
                      "'" + std::get<1>(pair) + "');";
    }

    // We only expect one result row to be returned, hence no separate 
    // callback function is needed.
    rc = sqlite3_exec(ctx.database, insert_cmd.c_str(), NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;

        sqlite3_free(err_msg);
        return(kErrorConflict);
    }

    return(kOK);
}

int SqliteLibDB::Delete(void *ctx, const string &table, const string &key) {
    return(0);
}

SqliteLibDB::~SqliteLibDB() {
    assert_sqlite(sqlite3_close(schema_database));
}

} // ycsbc
