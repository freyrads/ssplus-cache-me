#include "ssplus-cache-me/cache.h"
#include <mutex>
#include <shared_mutex>

namespace ssplus_cache_me::cache {

// data_t //////////////////////////////////////////////////////////////////////

data_t::data_t() : expires_at(0) {}

bool data_t::empty() const { return value.empty() && expires_at == 0; }

bool data_t::expired() const {
  return expires_at <=
         static_cast<uint64_t>(
             std::chrono::duration_cast<std::chrono::milliseconds>(
                 std::chrono::system_clock::now().time_since_epoch())
                 .count());
}

void data_t::clear() {
  value.clear();
  expires_at = 0;
}

void data_t::mark_cached() {
  if (expires_at == 0)
    expires_at = 1;
}

bool data_t::cached() const { return expires_at != 0 || !value.empty(); }

std::string data_t::from_json_str() {}

nlohmann::json data_t::from_json() {}

nlohmann::json data_t::to_json() const {}

std::string data_t::to_json_str() const {}

////////////////////////////////////////////////////////////////////////////////

static cache_map_t mcache;
static std::shared_mutex mcache_m;

std::lock_guard<std::shared_mutex> acquire_lock() {
  return std::lock_guard(mcache_m);
}

std::shared_lock<std::shared_mutex> acquire_shared_lock() {
  return std::shared_lock(mcache_m);
}

data_t get_unlocked(const std::string &key) {
  auto i = mcache.find(key);
  if (i == mcache.end())
    return {};

  return i->second;
}

data_t get(const std::string &key) {
  std::shared_lock lk(mcache_m);

  return get_unlocked(key);
}

set_return_t set_unlocked(const std::string &key, const data_t &value) {
  return mcache.insert_or_assign(key, value);
}

set_return_t set(const std::string &key, const data_t &value) {
  std::lock_guard lk(mcache_m);
  return set_unlocked(key, value);
}

size_t del_unlocked(const std::string &key) { return mcache.erase(key); }

size_t del(const std::string &key) {
  std::lock_guard lk(mcache_m);
  return del_unlocked(key);
}

} // namespace ssplus_cache_me::cache
