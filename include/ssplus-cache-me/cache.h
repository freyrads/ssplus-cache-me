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

  uint64_t get_expires_at() const noexcept;

  // build struct from json string.
  // any validation error will leave the struct unmodified
  int from_json_str(const std::string &s) noexcept;

  // build struct from json.
  // any validation error will leave the struct unmodified.
  // may throw nlohmann error (although highly unlikely)
  int from_json(const nlohmann::json &d);

  nlohmann::json to_json() const;
  std::string to_json_str(int indent = -1) const;
};

using cache_map_t = std::unordered_map<std::string, data_t>;
using set_return_t = std::pair<cache_map_t::iterator, bool>;
using vector_data_t = std::vector<data_t>;
using get_all_return_t = std::pair<vector_data_t, bool>;

std::lock_guard<std::shared_mutex> acquire_lock();
std::shared_lock<std::shared_mutex> acquire_shared_lock();

data_t get_unlocked(const std::string &key);
data_t get(const std::string &key);

get_all_return_t get_all_unlocked();
get_all_return_t get_all();

set_return_t set_unlocked(const std::string &key, const data_t &value);
set_return_t set(const std::string &key, const data_t &value);

get_all_return_t set_all_unlocked(const vector_data_t &values,
                                  bool loaded_state);
get_all_return_t set_all(const vector_data_t &values, bool loaded_state);

size_t del_unlocked(const std::string &key);
size_t del(const std::string &key);

} // namespace ssplus_cache_me::cache

#endif // CACHE_H
