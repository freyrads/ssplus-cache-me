#ifndef UTIL_H
#define UTIL_H

#include <cstdint>
#include <string>

namespace ssplus_cache_me::util {

uint64_t get_current_ts();

uint64_t ms_to_ns(uint64_t ms) noexcept;

std::string trim(const std::string &s);

} // namespace ssplus_cache_me::util

#endif // UTIL_H
