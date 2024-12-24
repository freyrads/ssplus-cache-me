#ifndef CACHE_H
#define CACHE_H

#include "nlohmann/json.hpp"
#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ssplus_cache_me::cache {

struct data_t {
  std::string value;

  // unix timestamp in ms.
  // ts of 1 is magic value to mark key is known to not exist in db
  uint64_t expires_at;

  data_t();

  bool empty() const;
  bool expired() const;
  data_t &clear();

  data_t &mark_cached();
  bool cached() const;

  // build struct from json string.
  // any validation error will leave the struct unmodified
  int from_json_str(const std::string &s) noexcept;

  // build struct from json.
  // any validation error will leave the struct unmodified.
  // may throw nlohmann error (although highly unlikely)
  int from_json(const nlohmann::json &d);

  nlohmann::json to_json() const;
  std::string to_json_str() const;
};

using cache_map_t = std::unordered_map<std::string, data_t>;
using set_return_t = std::pair<cache_map_t::iterator, bool>;

std::lock_guard<std::shared_mutex> acquire_lock();
std::shared_lock<std::shared_mutex> acquire_shared_lock();

data_t get_unlocked(const std::string &key);
data_t get(const std::string &key);

set_return_t set_unlocked(const std::string &key, const data_t &value);
set_return_t set(const std::string &key, const data_t &value);

size_t del_unlocked(const std::string &key);
size_t del(const std::string &key);

} // namespace ssplus_cache_me::cache

#endif // CACHE_H
