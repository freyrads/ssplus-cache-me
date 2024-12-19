#ifndef CACHE_H
#define CACHE_H

#include <mutex>
#include <shared_mutex>
#include <string>
#include <unordered_map>

namespace ssplus_cache_me::cache {

struct data_t {
  std::string value;
  // unix timestamp in ms
  uint64_t expires_at;

  data_t();

  bool empty();
  bool expired();
  void clear();
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
