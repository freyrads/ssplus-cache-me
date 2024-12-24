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
          {"expires_at", expires_at == 1 ? 0 : expires_at}};
}

std::string data_t::to_json_str() const { return to_json().dump(); }

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
