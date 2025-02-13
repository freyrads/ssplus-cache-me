#ifndef DB_H
#define DB_H

#include "ssplus-cache-me/cache.h"
#include <sqlite3.h>
#include <string>

namespace ssplus_cache_me::db {

int setup() noexcept;

int init(const char *path, sqlite3 **out) noexcept;

int close(sqlite3 **conn) noexcept;

int prepare_statement(sqlite3 *conn, const char *query, sqlite3_stmt **stmt,
                      const std::string &stmt_key = std::string()) noexcept;

void reset_statement(sqlite3_stmt **stmt) noexcept;

int finalize_statement(const std::string &stmt_key,
                       sqlite3_stmt **stmt) noexcept;

// only servers are allowed to call this
cache::data_t get_cache(sqlite3 *conn, const std::string &key,
                        int server_id) noexcept;

// only servers are allowed to call this
cache::vector_data_t get_all_cache(sqlite3 *conn, int server_id) noexcept;

int set_cache(const std::string &key, const cache::data_t &data) noexcept;
int delete_cache(const std::string &key, uint64_t at = 0) noexcept;

int cleanup() noexcept;

} // namespace ssplus_cache_me::db

#endif // DB_H
