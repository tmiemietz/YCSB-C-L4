//
//  lock_stl_db.h
//  YCSB-C
//
//  Created by Jinglei Ren on 12/25/14.
//  Copyright (c) 2014 Jinglei Ren <jinglei@ren.systems>.
//

#ifndef YCSB_C_LOCK_STL_DB_H_
#define YCSB_C_LOCK_STL_DB_H_

#include "db/hashtable_db.h"

#include <string>
#include <vector>
#include <mutex>
#include "lib/stl_hashtable.h"

namespace ycsbc {

class LockStlDB : public HashtableDB {
 public:
  LockStlDB() : HashtableDB(
      new vmp::StlHashtable<HashtableDB::FieldHashtable *>) { }

  ~LockStlDB() {
    std::vector<KeyHashtable::KVPair> key_pairs = key_table_->Entries();
    for (auto &key_pair : key_pairs) {
      DeleteFieldHashtable(key_pair.second);
    }
    delete key_table_;
  }

  int Read(void *ctx, const std::string &table, const std::string &key,
           const std::vector<std::string> *fields,
           std::vector<KVPair> &result) {
    std::lock_guard<std::mutex> lock(lock_);
    return HashtableDB::Read(ctx, table, key, fields, result);
  }

  int Scan(void *ctx, const std::string &table, const std::string &key,
           int len, const std::vector<std::string> *fields,
           std::vector<std::vector<KVPair>> &result) {
    std::lock_guard<std::mutex> lock(lock_);
    return HashtableDB::Scan(ctx, table, key, len, fields, result);
  }

  int Update(void *ctx, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) {
    std::lock_guard<std::mutex> lock(lock_);
    return HashtableDB::Update(ctx, table, key, values);
  }

  int Insert(void *ctx, const std::string &table, const std::string &key,
             std::vector<KVPair> &values) {
    std::lock_guard<std::mutex> lock(lock_);
    return HashtableDB::Insert(ctx, table, key, values);
  }

  int Delete(void *ctx, const std::string &table,
             const std::string &key) {
    std::lock_guard<std::mutex> lock(lock_);
    return HashtableDB::Delete(ctx, table, key);
  }

 protected:
  HashtableDB::FieldHashtable *NewFieldHashtable() {
    return new vmp::StlHashtable<const char *>;
  }

  void DeleteFieldHashtable(HashtableDB::FieldHashtable *table) {
    std::vector<FieldHashtable::KVPair> pairs = table->Entries();
    for (auto &pair : pairs) {
      DeleteString(pair.second);
    }
    delete table;
  }

  const char *CopyString(const std::string &str) {
    char *value = new char[str.length() + 1];
    strcpy(value, str.c_str());
    return value;
  }

  void DeleteString(const char *str) {
    delete[] str;
  }

 private:
  mutable std::mutex lock_;
};

} // ycsbc

#endif // YCSB_C_LOCK_STL_DB_H_
