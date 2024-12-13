#ifndef LOG_H
#define LOG_H

#include <iostream>

namespace ssplus_cache_me::log {

std::ostream &io(bool out = false);
std::ostream &io(std::ostream &os);

FILE *iof(bool out = false);
FILE *iof(FILE *f);

} // namespace ssplus_cache_me::log

#endif // LOG_H
