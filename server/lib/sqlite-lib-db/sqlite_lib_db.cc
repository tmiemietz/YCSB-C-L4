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
#include <memory>                   // For unique_ptr
#include <algorithm>                // For std::sort
#include <unordered_map>            // For std::unordered_map

using std::string;
using std::vector;

namespace ycsbc {

// Throw error on r != SQLITE_OK.
static void check_sqlite(int r) {
    if (r != SQLITE_OK)
        throw std::runtime_error{"sqlite command failed"};
}

struct Ctx {
    // DB that we are working with
    sqlite3 *database = nullptr;
    // Use a map as a statement cache like in the original YCSB.
    std::unordered_map<std::string, sqlite3_stmt *> stmts{};

    Ctx() = default;
    ~Ctx() {
        for (auto stmt : stmts) {
            check_sqlite(sqlite3_clear_bindings(stmt.second));
            check_sqlite(sqlite3_reset(stmt.second));
            check_sqlite(sqlite3_finalize(stmt.second));
        }
        if (database)
            check_sqlite(sqlite3_close(database));
    }
    Ctx(const Ctx&) = delete;
    Ctx(Ctx&&) = delete;

    Ctx& operator=(const Ctx&) = delete;
    Ctx& operator=(Ctx&&) = delete;

    static Ctx &cast(void *ctx) {
        return *reinterpret_cast<Ctx *>(ctx);
    }
};

/* Create an escaped string.
 *
 * The returned pointer will be correctly freed when going out of scope.
 */
static std::unique_ptr<char, decltype(&sqlite3_free)> escape_sql(const char *str) {
    return {sqlite3_mprintf("%Q", str), sqlite3_free};
}

/* Bind a string to an SQLite statement. */
static void bind_string(sqlite3_stmt *stmt, int pos, const std::string &str) {
    int rc = -1;

    rc = sqlite3_bind_text(stmt, pos, str.c_str(), str.size(), SQLITE_STATIC);
    if (rc != SQLITE_OK) {
        std::cerr << "Binding error: "
                  << sqlite3_errmsg(sqlite3_db_handle(stmt))
                  << std::endl;
        throw std::runtime_error("Failed to bind parameter");
    }
}

/* Bind an integer to an SQLite statement. */
static void bind_int(sqlite3_stmt *stmt, int pos, int val) {
    int rc = -1;

    rc = sqlite3_bind_int(stmt, pos, val);
    if (rc != SQLITE_OK) {
        std::cerr << "Binding error: "
                  << sqlite3_errmsg(sqlite3_db_handle(stmt))
                  << std::endl;
        throw std::runtime_error("Failed to bind parameter");
    }
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
    std::unique_ptr<Ctx> ctx{new Ctx{}};
    ctx->database = OpenDB();

    // TODO: Configure journaling (memory vs. off)

    return ctx.release();
}

void SqliteLibDB::Close(void *ctx) {
    delete &Ctx::cast(ctx);
}

int SqliteLibDB::Read(void *ctx_, const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
    int retval = -1;                    // Return code of this function
    int db_rc  = -1;                    // Return code for DB operations
    
    auto &ctx = Ctx::cast(ctx_);

    // Assemble an SQL selection statement. We always select everything from
    // a row. In case only a subset of the columns is requested, we will 
    // do the filtering afterwards, as we have to transform the query result
    // to the KVPair vector anyways.
    string stmt{"SELECT * FROM "};
    stmt += escape_sql(table.c_str()).get();
    stmt += "  WHERE YCSBC_KEY = ?";

    auto it = ctx.stmts.find(stmt);
    sqlite3_stmt *pStmt = nullptr;
    if (it == ctx.stmts.end()) {
        // Statement was not found in cache, prepare a new statement.
        db_rc = sqlite3_prepare_v2(ctx.database, stmt.c_str(), stmt.length(), &pStmt, nullptr);
        if (db_rc != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to prepare read statement");
        }

        // Insert into cache.
        ctx.stmts.insert({std::move(stmt), pStmt});
    } 
    else {
        pStmt = it->second;
    }

    // Bind key value to prepared SQL statement.
    bind_string(pStmt, 1, key);

    // We only step the database once: Either there is no result, or there
    // is exactly one, as we select for the primary key which is unique by
    // definition. Hence, even after receiving SQLITE_ROW from the stepping
    // function, we should be safe to assume that we don't miss any results.
    db_rc = sqlite3_step(pStmt);
    switch (db_rc) {
    case SQLITE_DONE:
        // Nothing was found
        retval = kErrorNoData;
        break;
    case SQLITE_ROW:
        // Fill the result into the result vector, filter out unwanted columns
        
        // We need to create a separate scope to be able to define variables
        // "private" to this case
        {
            // get number of result columns returned by the sqlite query
            int col_cnt = sqlite3_data_count(pStmt);    
       
            for (int i = 0; i < col_cnt; i++) {
                string col_name(reinterpret_cast<const char *>(
                                sqlite3_column_name(pStmt, i)));
                
                if (fields == nullptr || fields->size() == 0 ||
                    std::find(fields->begin(), fields->end(), col_name) !=
                    fields->end()) {
                    string col_content(reinterpret_cast<const char *>(
                                       sqlite3_column_text(pStmt, i)));

                    result.emplace_back(col_name, col_content);
                }
            }
        }

        retval = kOK;
        break;
    default:
        // Error, bail out
        std::cerr << "Stepping error: " << sqlite3_errmsg(ctx.database) << std::endl;
        throw std::runtime_error("Failed to step read statement");
        break;
    }

    check_sqlite(sqlite3_clear_bindings(pStmt));
    check_sqlite(sqlite3_reset(pStmt));

    // TODO: Are we supposed to free the fields vector here?

    return(retval);
}

int SqliteLibDB::Scan(void *ctx_, const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
    int retval = -1;                    // Return code of this function
    int db_rc  = -1;                    // Return code for DB operations
    
    auto &ctx = Ctx::cast(ctx_);

    // Assemble an SQL selection statement. We always select everything from
    // a row. In case only a subset of the columns is requested, we will 
    // do the filtering afterwards, as we have to transform the query result
    // to the KVPair vector anyways.
    string stmt{"SELECT * FROM "};
    stmt += escape_sql(table.c_str()).get();
    stmt += "  WHERE YCSBC_KEY >= ? LIMIT ?;";

    auto it = ctx.stmts.find(stmt);
    sqlite3_stmt *pStmt = nullptr;
    if (it == ctx.stmts.end()) {
        // Statement was not found in cache, prepare a new statement.
        db_rc = sqlite3_prepare_v2(ctx.database, stmt.c_str(), stmt.length(), &pStmt, nullptr);
        if (db_rc != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to prepare scan statement");
        }

        // Insert into cache.
        ctx.stmts.insert({std::move(stmt), pStmt});
    } 
    else {
        pStmt = it->second;
    }

    // Bind key value and limit to prepared SQL statement.
    bind_string(pStmt, 1, key);
    bind_int(pStmt, 2, len);

    // We have to step the database multiple times, since we have requested
    // several rows at once. Bail out of the whole application upon any errors.
    retval = kErrorNoData;
    while ((db_rc = sqlite3_step(pStmt)) != SQLITE_DONE) {
        if (db_rc == SQLITE_ROW) {
            // Fill the result into the result vector, filter out unwanted 
            // columns
        
            // We need to create a separate scope to be able to define variables
            // "private" to this case
            // get number of result columns returned by the sqlite query
            int col_cnt = sqlite3_data_count(pStmt);

            vector<KVPair> row_vec(fields ? fields->size() : col_cnt);

            for (int i = 0; i < col_cnt; i++) {
                string col_name(reinterpret_cast<const char *>(
                                sqlite3_column_name(pStmt, i)));

                if (fields == nullptr || fields->size() == 0 || 
                    std::find(fields->begin(), fields->end(), col_name) !=
                    fields->end()) {
                    string col_content(reinterpret_cast<const char *>(
                                       sqlite3_column_text(pStmt, i)));

                    row_vec.emplace_back(col_name, col_content);
                }
            }

            result.push_back(row_vec);
            retval = kOK;
        }
        else {
            // Error, bail out
            std::cerr << "Stepping error: " 
                      << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to step scan statement");
        }
    }

    check_sqlite(sqlite3_clear_bindings(pStmt));
    check_sqlite(sqlite3_reset(pStmt));

    // TODO: Are we supposed to free the fields vector here?

    return(retval);
}

int SqliteLibDB::Update(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
    int  rc    = -1;                    // Return code for DB operations
    bool first = true;                  // Are we working on the first value?
    
    std::size_t i = 0;                  // Index into values vector

    auto &ctx = Ctx::cast(ctx_);

    // Assemble an SQL insertion statement.
    string stmt{"UPDATE "};
    stmt += escape_sql(table.c_str()).get();
    stmt += " SET ";
    for (auto &value : values) {
        if (first == true)
            first = false;
        else
            stmt += ", ";
        
        stmt += escape_sql(value.first.c_str()).get();
        stmt += " = ?";
    }
    stmt += " WHERE YCSBC_KEY = ?;";

    auto it = ctx.stmts.find(stmt);
    sqlite3_stmt *pStmt = nullptr;
    if (it == ctx.stmts.end()) {
        // Statement was not found in cache, prepare a new statement.
        rc = sqlite3_prepare_v2(ctx.database, stmt.c_str(), stmt.length(), &pStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to prepare update statement");
        }

        // Insert into cache.
        ctx.stmts.insert({std::move(stmt), pStmt});
    } else
        pStmt = it->second;

    // Bind key and field values.
    for (i = 0; i < values.size(); i++) {
        bind_string(pStmt, i + 1, values[i].second);
    }
    // Bind last argument with primary key
    bind_string(pStmt, i + 1, key);

    // We do not expect any result row, hence SQLITE_DONE should be returned.
    do {
        rc = sqlite3_step(pStmt);
        // Retry loop because concurrent write operations lock others out.
    } while (rc == SQLITE_LOCKED);
    if (rc != SQLITE_DONE) {
        std::cerr << "Stepping error: " << sqlite3_errmsg(ctx.database) << std::endl;
        throw std::runtime_error("Failed to step update statement");
    }

    check_sqlite(sqlite3_clear_bindings(pStmt));
    check_sqlite(sqlite3_reset(pStmt));

    return(kOK);
}

// Insert a set of key-value pairs into the table <table>.
int SqliteLibDB::Insert(void *ctx_, const string &table, const string &key,
                        vector<KVPair> &values) {
    int rc = -1;                    // Return code for DB operations
    
    auto &ctx = Ctx::cast(ctx_);

    // Sort fields such that order becomes irrelevant for key of map.
    std::sort(values.begin(), values.end(),
              [](KVPair a, KVPair b) { return a.first < b.first; });

    // Assemble an SQL insertion statement.
    string stmt{"INSERT INTO "};
    stmt += escape_sql(table.c_str()).get();
    stmt += " (YCSBC_KEY";
    for (auto &value : values) {
        stmt += ", ";
        stmt += escape_sql(value.first.c_str()).get();
    }
    stmt += ") VALUES (?";
    for (std::size_t i = 0; i < values.size(); i++) {
        stmt += ", ?";
    }
    stmt += ");";

    auto it = ctx.stmts.find(stmt);
    sqlite3_stmt *pStmt = nullptr;
    if (it == ctx.stmts.end()) {
        // Statement was not found in cache, prepare a new statement.
        rc = sqlite3_prepare_v2(ctx.database, stmt.c_str(), stmt.length(), &pStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to prepare insert statement");
        }

        // Insert into cache.
        ctx.stmts.insert({std::move(stmt), pStmt});
    } else
        pStmt = it->second;

    // Bind key and field values.
    bind_string(pStmt, 1, key);
    for (std::size_t i = 0; i < values.size(); i++) {
        bind_string(pStmt, i + 2, values[i].second);
    }

    // We do not expect any result row, hence SQLITE_DONE should be returned.
    do {
        rc = sqlite3_step(pStmt);
        // Retry loop because concurrent write operations lock others out.
    } while (rc == SQLITE_LOCKED);
    if (rc != SQLITE_DONE) {
        std::cerr << "Stepping error: " << sqlite3_errmsg(ctx.database) << std::endl;
        throw std::runtime_error("Failed to step insert statement");
    }

    check_sqlite(sqlite3_clear_bindings(pStmt));
    check_sqlite(sqlite3_reset(pStmt));

    return kOK;
}

int SqliteLibDB::Delete(void *ctx_, const string &table, const string &key) {
    int rc = -1;                    // Return code for DB operations
    
    auto &ctx = Ctx::cast(ctx_);

    // Assemble an SQL deletion statement.
    string stmt{"DELETE FROM "};
    stmt += escape_sql(table.c_str()).get();
    stmt += " WHERE YCSBC_KEY = ?";

    auto it = ctx.stmts.find(stmt);
    sqlite3_stmt *pStmt = nullptr;
    if (it == ctx.stmts.end()) {
        // Statement was not found in cache, prepare a new statement.
        rc = sqlite3_prepare_v2(ctx.database, stmt.c_str(), stmt.length(), &pStmt, nullptr);
        if (rc != SQLITE_OK) {
            std::cerr << "SQL error: " << sqlite3_errmsg(ctx.database) << std::endl;
            throw std::runtime_error("Failed to prepare delete statement");
        }

        // Insert into cache.
        ctx.stmts.insert({std::move(stmt), pStmt});
    } else
        pStmt = it->second;

    // Bind key value.
    bind_string(pStmt, 1, key);

    // We do not expect any result row, hence SQLITE_DONE should be returned.
    rc = sqlite3_step(pStmt);
    if (rc != SQLITE_DONE) {
        std::cerr << "Stepping error: " << sqlite3_errmsg(ctx.database) << std::endl;
        throw std::runtime_error("Failed to step delete statement");
    }

    check_sqlite(sqlite3_clear_bindings(pStmt));
    check_sqlite(sqlite3_reset(pStmt));

    return(kOK);
}

SqliteLibDB::~SqliteLibDB() {
    check_sqlite(sqlite3_close(schema_database));
}

} // ycsbc
