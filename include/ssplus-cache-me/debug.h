#ifndef DEBUG_H
#define DEBUG_H

#include <cstdio>
#include <iostream>

#define DEFAULT_DEBUG_DIRNAME "src/ssplus-cache-me/"
#define DEFAULT_DEBUG_INCLUDE_DIRNAME "include/ssplus-cache-me/"

#define DECLARE_DEBUG_INFO(DIRNAME)                                            \
  constexpr const char _SF_SOURCE_FILE_[] = DIRNAME __FILE_NAME__

#define DECLARE_DEBUG_INFO_DEFAULT() DECLARE_DEBUG_INFO(DEFAULT_DEBUG_DIRNAME)

#define DEBUG_WHERE                                                            \
  _SF_SOURCE_FILE_ << ':' << __LINE__ << ' ' << __FUNCTION__ << ": "

#define LOGDEBUG_WF(LOGFILE, SEV, FMT, ...)                                    \
  do {                                                                         \
    fprintf(LOGFILE, "[");                                                     \
    ::ssplus_cache_me::debug::pdbg_ts(LOGFILE);                                \
    fprintf(LOGFILE, " " SEV "] %s:%d %s: ", _SF_SOURCE_FILE_, __LINE__,       \
            __FUNCTION__);                                                     \
    fprintf(LOGFILE, FMT "\n", __VA_ARGS__);                                   \
  } while (0)

#define LOGDEBUG(SEV, FMT, ...) LOGDEBUG_WF(stderr, SEV, FMT, __VA_ARGS__)

#define DECLARE_BENCHMARK_WS(SUF)                                              \
  std::chrono::time_point<std::chrono::steady_clock, std::chrono::nanoseconds> \
      _bench_start_##SUF;                                                      \
  long long _bench_res_##SUF

#define START_BENCHMARK_WS(SUF)                                                \
  do {                                                                         \
    _bench_start_##SUF = std::chrono::steady_clock::now();                     \
  } while (0)

#define PRINT_BENCHMARK_WSF(LOGFILE, NAME, SUF)                                \
  fprintf(LOGFILE, "[BENCHMARK] \"%s\": %.9fs\n", NAME,                        \
          (double)_bench_res_##SUF / 1000000000LL)

#define END_BENCHMARK_WSF(LOGFILE, NAME, SUF, PRINT)                           \
  do {                                                                         \
    _bench_res_##SUF =                                                         \
        (std::chrono::steady_clock::now() - _bench_start_##SUF).count();       \
    if constexpr (PRINT)                                                       \
      PRINT_BENCHMARK_WSF(LOGFILE, NAME, SUF);                                 \
  } while (0)

#define PRINT_BENCHMARK_WS(NAME, SUF) PRINT_BENCHMARK_WSF(stderr, NAME, SUF)

#define END_BENCHMARK_WS(NAME, SUF, PRINT)                                     \
  END_BENCHMARK_WSF(stderr, NAME, SUF, PRINT)

#define DECLARE_BENCHMARK() DECLARE_BENCHMARK_WS(a)
#define START_BENCHMARK() START_BENCHMARK_WS(a)
#define END_BENCHMARK(NAME) END_BENCHMARK_WS(NAME, a, true)
#define PRINT_BENCHMARK(NAME) PRINT_BENCHMARK_WS(NAME, a)

namespace ssplus_cache_me::debug {

void pdbg_ts(FILE *f = stderr);
void pdbg_ts(std::ostream &os = std::cerr);

} // namespace ssplus_cache_me::debug

#endif // DEBUG_H
