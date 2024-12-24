#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/query_runner.h"
#include "ssplus-cache-me/run.h"
#include <sqlite3.h>

DECLARE_DEBUG_INFO_DEFAULT();

namespace ssplus_cache_me::db {

int setup() noexcept {
  static bool initialized = false;
  if (initialized)
    return -1;

  log::io() << "Setting global db conn configuration\n";

  int status = sqlite3_config(SQLITE_CONFIG_MULTITHREAD);

  if (status == SQLITE_OK) {
    initialized = true;
  } else {
    log::io() << DEBUG_WHERE
              << "Failed setting up global db conn configuration, status: "
              << status << "\n";
  }

  return status;
}

int init(const char *path, sqlite3 **out) noexcept {
  int status = sqlite3_open(path, out);

  if (status != SQLITE_OK) {
    log::io() << DEBUG_WHERE << sqlite3_errmsg(*out) << "\n";

    if (*out) {
      sqlite3_close(*out);
      *out = nullptr;
    }
  }

  return status;
}

int close(sqlite3 **conn) noexcept {
  int status = sqlite3_close(*conn);

  if (status == SQLITE_OK)
    *conn = nullptr;

  return status;
}

int prepare_statement(sqlite3 *conn, const char *query,
                      sqlite3_stmt **stmt) noexcept {

  int status = sqlite3_prepare_v2(conn, query, -1, stmt, NULL);

  if (status != SQLITE_OK) {
    log::io() << DEBUG_WHERE << "Error preparing statement with status("
              << status << "):\n"
              << query << "\n\n";

    if (*stmt != nullptr) {
      sqlite3_finalize(*stmt);
      *stmt = nullptr;
    }
  }

  return status;
}

cache::data_t get_cache(sqlite3 *conn, const std::string &key) noexcept {
  cache::data_t ret;
  if (key.empty())
    return ret;

  std::string query =
      "SELECT \"value\",\"expires_at\" FROM \"cache\" WHERE \"key\" = ?1 ;";

  sqlite3_stmt *statement = nullptr;
  int status = prepare_statement(conn, query.c_str(), &statement);

  if (status != SQLITE_OK)
    return ret;

  int klen = static_cast<int>(key.length());
  status = sqlite3_bind_text(statement, 1, key.c_str(), klen, SQLITE_STATIC);

  if (status != SQLITE_OK) {
    log::io() << DEBUG_WHERE << "Failed binding key(" << key
              << ") to query with status(" << status << "):\n"
              << query << "\n\n";

    goto ret;
  }

  // execute statement
  status = sqlite3_step(statement);
  if (status == SQLITE_ROW) {
    // columns: "value","expires_at"
    ret.value =
        reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));

    ret.expires_at = static_cast<uint64_t>(sqlite3_column_int64(statement, 1));
  }

ret:
  if (statement) {
    sqlite3_finalize(statement);
    statement = nullptr;
  }

  return ret;
}

int set_cache(const std::string &key, const cache::data_t &data) noexcept {
  if (key.empty() || data.empty())
    return 1;

  query_schedule_t q(key);

  q.query = "INSERT INTO \"cache\" "
            "(\"key\", \"value\", \"expires_at\") "

            "VALUES (?1, ?2, ?3) "

            "ON CONFLICT (\"key\") "
            "DO UPDATE SET "

            "\"value\" = ?2, "
            "\"expires_at\" = ?3 ;";

  q.run = [key, data](sqlite3_stmt *statement, const query_schedule_t &q,
                      sqlite3 *conn) -> int {
    auto log_bind_fail = [statement](const std::string &name,
                                     const std::string &v) {
      log::io() << DEBUG_WHERE << "Failed binding " << name << "(" << v
                << ")\n";

      sqlite3_finalize(statement);
    };

    int klen = static_cast<int>(key.length());
    int status =
        sqlite3_bind_text(statement, 1, key.c_str(), klen, SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log_bind_fail("key", key);
      return status;
    }

    int vlen = static_cast<int>(data.value.length());
    status = sqlite3_bind_text(statement, 2, data.value.c_str(), vlen,
                               SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log_bind_fail("value", data.value);
      return status;
    }

    status = sqlite3_bind_int64(statement, 3,
                                static_cast<int64_t>(data.get_expires_at()));

    if (status != SQLITE_OK) {
      log_bind_fail("expires_at", std::to_string(data.get_expires_at()));
      return status;
    }

    return query_runner::run_until_done(statement, q, conn);
  };

  enqueue_write_query(q);

  return 0;
}

int delete_cache(const std::string &key, uint64_t at) noexcept {
  if (key.empty())
    return 1;

  query_schedule_t q(key);

  q.query = "DELETE FROM \"cache\" WHERE \"key\" = ?1 ;";

  q.run = [key](sqlite3_stmt *statement, const query_schedule_t &q,
                sqlite3 *conn) -> int {
    int klen = static_cast<int>(key.length());
    int status =
        sqlite3_bind_text(statement, 1, key.c_str(), klen, SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log::io() << DEBUG_WHERE << "Failed binding key(" << key << ")\n";

      sqlite3_finalize(statement);
      return status;
    }

    return query_runner::run_until_done(statement, q, conn);
  };

  q.ts = at;

  enqueue_write_query(q);

  return 0;
}

} // namespace ssplus_cache_me::db
