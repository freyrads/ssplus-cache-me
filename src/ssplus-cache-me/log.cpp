#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/debug.h"
#include <iostream>

namespace ssplus_cache_me::log {

std::ostream &io(bool out) {
  std::ostream &os = out ? std::cout : std::cerr;

  return io(os);
}

std::ostream &io(std::ostream &os) {
  os << '[';
  debug::pdbg_ts(os);
  os << "] ";
  return os;
}

FILE *iof(bool out) {
  FILE *f = out ? stdout : stderr;

  return iof(f);
}

FILE *iof(FILE *f) {
  fprintf(f, "[");
  debug::pdbg_ts(f);
  fprintf(f, "] ");
  return f;
}

} // namespace ssplus_cache_me::log
