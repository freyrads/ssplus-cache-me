#include "ssplus-cache-me/debug.h"
#include <chrono>
#include <ctime>

namespace ssplus_cache_me::debug {

void get_ts(char *o) {
  const auto n = std::chrono::system_clock::now();
  const auto curtime = std::chrono::system_clock::to_time_t(n);
  const auto curtime_ms =
      std::chrono::time_point_cast<std::chrono::milliseconds>(n);

  const int milli = curtime_ms.time_since_epoch().count() % 1000;

  char tbuf[64] = "";
  strftime(tbuf, sizeof(tbuf), "%FT%T", std::gmtime(&curtime));

  sprintf(o, "%s.%03dZ", tbuf, milli);
}

void pdbg_ts(FILE *f) {
  char o[256] = "";
  get_ts(o);

  fprintf(f, "%s", o);
}

void pdbg_ts(std::ostream &os) {
  char o[256] = "";
  get_ts(o);

  os << o;
}

} // namespace ssplus_cache_me::debug
