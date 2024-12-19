#ifndef DB_H
#define DB_H

#include <sqlite3.h>

namespace ssplus_cache_me::db {

int setup() noexcept;

int init(const char *path, sqlite3 **out) noexcept;

int close(sqlite3 **conn) noexcept;

} // namespace ssplus_cache_me::db

#endif // DB_H
