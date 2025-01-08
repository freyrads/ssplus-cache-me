#include "ssplus-cache-me/util.h"
#include <chrono>

namespace ssplus_cache_me::util {

uint64_t get_current_ts() {
  return std::chrono::time_point_cast<std::chrono::milliseconds>(
             std::chrono::system_clock::now())
      .time_since_epoch()
      .count();
}

uint64_t ms_to_ns(uint64_t ms) noexcept { return ms * 1000'000; }

static bool trim_pred(char v) { return std::isspace(v); };

std::string trim(const std::string &s) {
  auto start = std::find_if_not(s.begin(), s.end(), trim_pred);
  auto end = std::find_if_not(s.rbegin(), s.rend(), trim_pred);

  if (start != s.end() && end != s.rend()) {
    return std::string{start.base(), end.base().base()};
  }

  return {};
}

} // namespace ssplus_cache_me::util
