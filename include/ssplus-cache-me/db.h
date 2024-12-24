#ifndef DB_H
#define DB_H

#include "ssplus-cache-me/cache.h"
#include <sqlite3.h>
#include <string>

namespace ssplus_cache_me::db {

int setup() noexcept;

int init(const char *path, sqlite3 **out) noexcept;

int close(sqlite3 **conn) noexcept;

cache::data_t get_cache(sqlite3 *conn, const std::string &key) noexcept;
cache::data_t set_cache(const std::string &key) noexcept;
cache::data_t delete_cache(const std::string &key) noexcept;

} // namespace ssplus_cache_me::db

#endif // DB_H
