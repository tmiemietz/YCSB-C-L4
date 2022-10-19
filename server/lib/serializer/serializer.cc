/* Serialization and deserialization for IPC communication between SQLite server
 * and benchmark application.
 *
 * Author: Viktor Reusch
 */

#include <stdexcept> // For std::runtime_error.

#include "db.h"
#include "serializer.h"

using namespace serializer;
using ycsbc::Table;

// Even with overflow for `end`, this program works correctly.
Serializer::Serializer(char *buf, std::size_t len)
    : orig_buf{buf}, buf{buf}, end{buf + len} {}

Serializer &Serializer::operator<<(int i) {
  serialize(reinterpret_cast<char const *>(&i), sizeof(i));
  return *this;
}

Serializer &Serializer::operator<<(std::size_t n) {
  serialize(reinterpret_cast<char const *>(&n), sizeof(n));
  return *this;
}

Serializer &Serializer::operator<<(std::string const &s) {
  *this << s.size();
  serialize(s.data(), s.size());
  return *this;
}

Serializer &Serializer::operator<<(Table const &t) {
  return *this << t.name << t.columns;
}

void Serializer::assert_remaining(std::size_t n) {
  if (reinterpret_cast<std::size_t>(end) - reinterpret_cast<std::size_t>(buf) <
      n)
    throw std::runtime_error{"Serializer overflowed"};
}

void Serializer::serialize(char const *bytes, std::size_t n) {
  assert_remaining(n);
  memcpy(buf, bytes, n);
  buf += n;
}

Deserializer::Deserializer(char const *buf) : buf{buf} {}

Deserializer &Deserializer::operator>>(int &i) {
  std::memcpy(&i, buf, sizeof(i));
  buf += sizeof(i);
  return *this;
}

Deserializer &Deserializer::operator>>(std::size_t &n) {
  std::memcpy(&n, buf, sizeof(n));
  buf += sizeof(n);
  return *this;
}

Deserializer &Deserializer::operator>>(std::string &s) {
  std::size_t size{};
  *this >> size;
  s.clear();
  s.reserve(size);
  s.append(buf, size);
  buf += size;
  return *this;
}

Deserializer &Deserializer::operator>>(Table &t) {
  return *this >> t.name >> t.columns;
}
