//
//  db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/10/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//  Copyright (c) 2022 Viktor Reusch.
//

#ifndef YCSB_C_DB_H_
#define YCSB_C_DB_H_

#include <string>
#include <vector>

#ifdef NO_L4
typedef std::size_t l4_umword_t;
#endif

namespace ycsbc {

struct Table {
  // Table name.
  std::string name{};

  // List of column names (excluding key).
  std::vector<std::string> columns{};

  Table() = default;
  inline Table(std::string name, std::vector<std::string> columns)
      : name{std::move(name)}, columns{std::move(columns)} {
  }
};

class DB {
public:
  typedef std::pair<std::string, std::string> KVPair;
  typedef std::vector<Table> Tables;
  static const int kOK = 0;
  static const int kErrorNoData = 1;
  static const int kErrorConflict = 2;
  /// Initializes the database schema with all tables.
  ///
  /// @param tables A list of tables to create.
  virtual void CreateSchema(Tables tables) { (void)tables; }
  ///
  /// See void *Init(l4_umword_t).
  ///
  virtual void *Init() { return nullptr; }
  ///
  /// Initializes any state for accessing this DB.
  /// Called once per DB client (thread); there is a single DB instance
  /// globally.
  /// @param cpu The CPU on which the DB thread should run on if a separate
  ///            thread is spawned.
  /// @return A pointer to a per-thread context object (or NULL).
  ///
  virtual void *Init(l4_umword_t cpu) {
    (void)cpu;
    return Init();
  }
  ///
  /// Clears any state for accessing this DB.
  /// Called once per DB client (thread); there is a single DB instance
  /// globally.
  ///
  /// @param ctx Pointer to the per-thread context object.
  ///
  virtual void Close(void *ctx) { (void)ctx; }
  ///
  /// Reads a record from the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param ctx Pointer to the per-thread context object.
  /// @param table The name of the table.
  /// @param key The key of the record to read.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of field/value pairs for the result.
  /// @return Zero on success, or a non-zero error code on error/record-miss.
  ///
  virtual int Read(void *ctx, const std::string &table, const std::string &key,
                   const std::vector<std::string> *fields,
                   std::vector<KVPair> &result) = 0;
  ///
  /// Performs a range scan for a set of records in the database.
  /// Field/value pairs from the result are stored in a vector.
  ///
  /// @param ctx Pointer to the per-thread context object.
  /// @param table The name of the table.
  /// @param key The key of the first record to read.
  /// @param record_count The number of records to read.
  /// @param fields The list of fields to read, or NULL for all of them.
  /// @param result A vector of vector, where each vector contains field/value
  ///        pairs for one record
  /// @return Zero on success, or a non-zero error code on error.
  ///
  virtual int Scan(void *ctx, const std::string &table, const std::string &key,
                   int record_count, const std::vector<std::string> *fields,
                   std::vector<std::vector<KVPair>> &result) = 0;
  ///
  /// Updates a record in the database.
  /// Field/value pairs in the specified vector are written to the record,
  /// overwriting any existing values with the same field names.
  ///
  /// @param ctx Pointer to the per-thread context object.
  /// @param table The name of the table.
  /// @param key The key of the record to write.
  /// @param values A vector of field/value pairs to update in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual int Update(void *ctx, const std::string &table,
                     const std::string &key, std::vector<KVPair> &values) = 0;
  ///
  /// Inserts a record into the database.
  /// Field/value pairs in the specified vector are written into the record.
  ///
  /// @param ctx Pointer to the per-thread context object.
  /// @param table The name of the table.
  /// @param key The key of the record to insert.
  /// @param values A vector of field/value pairs to insert in the record.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual int Insert(void *ctx, const std::string &table,
                     const std::string &key, std::vector<KVPair> &values) = 0;
  ///
  /// Deletes a record from the database.
  ///
  /// @param ctx Pointer to the per-thread context object.
  /// @param table The name of the table.
  /// @param key The key of the record to delete.
  /// @return Zero on success, a non-zero error code on error.
  ///
  virtual int Delete(void *ctx, const std::string &table,
                     const std::string &key) = 0;

  virtual ~DB() {}
};

} // namespace ycsbc

#endif // YCSB_C_DB_H_
