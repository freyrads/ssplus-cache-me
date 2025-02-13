#include "ssplus-cache-me/cache.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include <mutex>
#include <shared_mutex>

DECLARE_DEBUG_INFO_DEFAULT();

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

data_t &data_t::clear() {
  value.clear();
  expires_at = 0;
  return *this;
}

data_t &data_t::mark_cached() {
  if (expires_at == 0)
    expires_at = 1;

  return *this;
}

bool data_t::cached() const { return expires_at != 0 || !value.empty(); }

uint64_t data_t::get_expires_at() const noexcept {
  return expires_at == 1 ? 0 : expires_at;
}

int data_t::from_json_str(const std::string &s) noexcept {
  try {
    auto d = nlohmann::json::parse(s);

    return from_json(d);
  } catch (std::exception &e) {
    log::io() << DEBUG_WHERE << e.what() << "\n";

    return 3;
  }
}

int data_t::from_json(const nlohmann::json &d) {
  if (!d.is_object())
    return 1;

  auto iv = d.find("value");
  auto iex = d.find("expires_at");

  auto ie = d.end();
  if (iv == ie || iex == ie || !iv->is_string() || !iex->is_number_unsigned())
    return 2;

  value = iv->get<std::string>();
  expires_at = iex->get<uint64_t>();

  return 0;
}

nlohmann::json data_t::to_json() const {
  return {{
              "value",
              value,
          },
          {"expires_at", get_expires_at()}};
}

std::string data_t::to_json_str(int indent) const {
  return to_json().dump(indent);
}

////////////////////////////////////////////////////////////////////////////////

static cache_map_t mcache;
static std::shared_mutex mcache_m;

static vector_data_t mallcache;
static bool mallcache_loaded = false;
static std::shared_mutex mallcache_m;

static void reset_mallcache_unlocked() {
  mallcache.clear();
  mallcache_loaded = false;
}

static void reset_mallcache() {
  std::lock_guard lk(mallcache_m);
  return reset_mallcache_unlocked();
}

[[nodiscard]] std::lock_guard<std::shared_mutex> acquire_lock() {
  return std::lock_guard(mcache_m);
}

[[nodiscard]] std::shared_lock<std::shared_mutex> acquire_shared_lock() {
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

get_all_return_t get_all_unlocked() { return {mallcache, mallcache_loaded}; }

get_all_return_t get_all() {
  std::shared_lock lk(mallcache_m);
  return get_all_unlocked();
}

set_return_t set_unlocked(const std::string &key, const data_t &value) {
  reset_mallcache();
  return mcache.insert_or_assign(key, value);
}

set_return_t set(const std::string &key, const data_t &value) {
  std::lock_guard lk(mcache_m);
  return set_unlocked(key, value);
}

get_all_return_t set_all_unlocked(const vector_data_t &values,
                                  bool loaded_state) {
  mallcache = values;
  mallcache_loaded = loaded_state;
  return {mallcache, mallcache_loaded};
}

get_all_return_t set_all(const vector_data_t &values, bool loaded_state) {
  std::lock_guard lk(mallcache_m);
  return set_all_unlocked(values, loaded_state);
}

size_t del_unlocked(const std::string &key) {
  reset_mallcache();
  return mcache.erase(key);
}

size_t del(const std::string &key) {
  std::lock_guard lk(mcache_m);
  return del_unlocked(key);
}

} // namespace ssplus_cache_me::cache
