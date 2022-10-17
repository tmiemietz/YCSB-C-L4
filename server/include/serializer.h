/* Serialization and deserialization for IPC communication between SQLite server
 * and benchmark application.
 *
 * Author: Viktor Reusch
 */

#pragma once

#include <cstring> // For memcpy and size_t.
#include <vector>

#include "db.h"

namespace serializer {

class Serializer {
  // Store original start so we can later report the length.
  char const *const orig_buf;
  char *buf;
  char const *const end;

  // Assert that the internal buffer has at least `n` bytes remaining.
  void assert_remaining(std::size_t n);
  // Serialize a number of bytes.
  void serialize(char const *, std::size_t);

public:
  // Create a new serializer for the buffer `buf` with length `len`.
  Serializer(char *buf, std::size_t len);

  // Serialize an std::size_t.
  Serializer &operator<<(std::size_t);
  // Serialize a string.
  Serializer &operator<<(std::string const &);
  // Serialize a Table.
  Serializer &operator<<(ycsbc::Table const &);
  // Serialize an std::vector if the value type is also serializable.
  template <class T> inline Serializer &operator<<(std::vector<T> const &v) {
    *this << v.size();
    for (auto &e : v)
      *this << e;
    return *this;
  }

  // Report the start of the buffer (as given in the constructor).
  inline char const *start() const { return orig_buf; }
  // Report the current amount of bytes used in the buffer for serialized data.
  inline std::size_t length() const { return buf - orig_buf; }
};

class Deserializer {
  char const *buf;

public:
  // Create a new deserializer from the buffer `buf`.
  // It is assumed that the data is valid and complete. Then, the buffer
  // accesses should never overflow.
  Deserializer(char const *buf);

  // Deserialize an std::size_t.
  Deserializer &operator>>(std::size_t &);
  // Deserialize a string.
  Deserializer &operator>>(std::string &);
  // Deserialize a Table.
  Deserializer &operator>>(ycsbc::Table &);
  // Deserialize an std::vector if the value type is also deserializable.
  template <class T> inline Deserializer &operator>>(std::vector<T> &v) {
    v.clear();

    std::size_t size{};
    *this >> size;
    v.reserve(size);
    for (std::size_t i = 0; i < size; i++) {
      T e{};
      *this >> e;
      v.push_back(e);
    }
    return *this;
  }
};

} // namespace ycsbc
