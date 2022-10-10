/******************************************************************************
 *                                                                            *
 * sqlite_lib_db.cc - A database backend using the sqlite library linked into *
 *                    the YCSB benchmark process.                             *
 *                                                                            *
 * Author: Till Miemietz <till.miemietz@barkhauseninstitut.org>               *
 *                                                                            *
 ******************************************************************************/

#include "sqlite_lib_db.h"          // Class definitions for sqlite_lib_db

#include <iostream>
#include <exception>
#include <utility>                  // For std::get and friends...

using std::string;
using std::vector;

namespace ycsbc {

/* Default constructor for the library version of sqlite                      */
SqliteLibDB::SqliteLibDB(const string &filename) {
    int rc = -1;                    // Return code for DB operations

    // Init auxiliary structures
    table_names = std::set<std::string>();

    // Open a new database
    rc = sqlite3_open(filename.c_str(), &database);
    if (rc != SQLITE_OK) {
        std::cerr << "Cannot open sqlite database " << filename << ": "
                  << sqlite3_errmsg(database) << std::endl;;
        sqlite3_close(database);

        throw std::runtime_error("Failed to open database.");
    }

    // TODO: Turn on write-ahead logging

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

/* Creates a new table, for the schema definition see the header of this class*/
int SqliteLibDB::CreateTable(const string &name) {
    int rc = -1;                    // Return code for DB operations

    char *err_msg = NULL;           // Error message returned from sqlite

    // Assemble an SQL table creation command
    string create_cmd = "CREATE TABLE IF NOT EXISTS " + name + "(" +
                        "YCSBC_KEY VARCHAR(64) PRIMARY KEY, " +
                        "YCSBC_TAG VARCHAR(64)" +
                        "YCSBC_VAL VARCHAR(64)" +
                        ");";
    
    // We only expect one result row to be returned, hence no separate 
    // callback function is needed.
    rc = sqlite3_exec(database, create_cmd.c_str(), NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;

        sqlite3_free(err_msg);
        return(1);
    }

    return(0);
}

int SqliteLibDB::Read(const string &table, const string &key,
                      const vector<std::string> *fields,
                      vector<KVPair> &result) {
    int rc = -1;                    // Return code for DB operations

    char *err_msg = NULL;           // Error message returned from sqlite

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
    rc = sqlite3_exec(database, read_cmd.c_str(), &SqliteVecAddCallback,
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

int SqliteLibDB::Scan(const string &table, const string &key,
                      int len, const vector<std::string> *fields,
                      vector<std::vector<KVPair>> &result) {
    return(0);
}

int SqliteLibDB::Update(const string &table, const string &key,
                        vector<KVPair> &values) {
    return(0);
}

// Insert a set of key-value pairs into the table <table>.
int SqliteLibDB::Insert(const string &table, const string &key,
                        vector<KVPair> &values) {
    int rc = -1;                    // Return code for DB operations

    char *err_msg = NULL;           // Error message returned from sqlite
    
    // Check if table <table> exists; if not so, create it
    // The lock should not be a hgue performance bottleneck here, since sqlite
    // serializes modification operations anway.
    table_lock.lock();
    if (table_names.find(table) == table_names.end()) {
        if (CreateTable(table) != 0) {
            table_lock.unlock();
            throw std::runtime_error("Failed to create new table.");
        }

        table_names.insert(table);
    }
    table_lock.unlock();
    
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
    rc = sqlite3_exec(database, insert_cmd.c_str(), NULL, NULL, &err_msg);
    if (rc != SQLITE_OK) {
        std::cerr << "SQL error: " << err_msg << std::endl;

        sqlite3_free(err_msg);
        return(kErrorConflict);
    }

    return(kOK);
}

int SqliteLibDB::Delete(const string &table, const string &key) {
    return(0);
}

} // ycsbc

