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

} // namespace ssplus_cache_me::util
