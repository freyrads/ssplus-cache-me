#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/query_runner.h"
#include "ssplus-cache-me/run.h"
#include <sqlite3.h>
#include <unordered_map>

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

static std::unordered_map<std::string, sqlite3_stmt *> stmt_cache;
static std::shared_mutex stmt_cache_m;

// any create failure won't
int prepare_statement(sqlite3 *conn, const char *query, sqlite3_stmt **stmt,
                      const std::string &stmt_key) noexcept {
  int status = 0;

  const std::string stmt_cache_key = stmt_key.empty() ? query : stmt_key;

  {
    // find it in cache first
    std::shared_lock lk(stmt_cache_m);

    auto i = stmt_cache.find(stmt_cache_key);
    if (i != stmt_cache.end()) {
      // found, return early
      *stmt = i->second;

      return status;
    }
  }

  // else create a new
  status = sqlite3_prepare_v2(conn, query, -1, stmt, NULL);

  if (status != SQLITE_OK) {
    log::io() << DEBUG_WHERE << "Error preparing statement with status("
              << status << "):\n"
              << query << "\n\n";

    if (*stmt != nullptr) {
      sqlite3_finalize(*stmt);
      *stmt = nullptr;
    }
  } else {
    // save this statement in cache
    std::lock_guard lk(stmt_cache_m);
    stmt_cache.insert_or_assign(stmt_cache_key, *stmt);

    log::io() << "Statement cached: `" << stmt_cache_key << "`\n";
  }

  return status;
}

void reset_statement(sqlite3_stmt **stmt) noexcept {
  if (*stmt == nullptr)
    return;

  // make sure this statement is ready for the next query
  sqlite3_reset(*stmt);
  sqlite3_clear_bindings(*stmt);
}

static int finalize_statement_unlocked(const std::string &stmt_key,
                                       sqlite3_stmt **stmt) noexcept {
  int status = 0;

  if (*stmt) {
    sqlite3_finalize(*stmt);
    *stmt = nullptr;
  }

  if (!stmt_key.empty()) {
    auto i = stmt_cache.find(stmt_key);
    if (i == stmt_cache.end()) {
      return status;
    }

    stmt_cache.erase(i);
    stmt_cache.rehash(0);

    log::io() << "Statement destroyed: `" << stmt_key << "`\n";
  }

  return status;
}

int finalize_statement(const std::string &stmt_key,
                       sqlite3_stmt **stmt) noexcept {
  std::lock_guard lk(stmt_cache_m);
  return finalize_statement_unlocked(stmt_key, stmt);
}

cache::data_t get_cache(sqlite3 *conn, const std::string &key,
                        int server_id) noexcept {
  cache::data_t ret;
  if (key.empty())
    return ret;

  std::string query =
      "SELECT \"value\",\"expires_at\" FROM \"cache\" WHERE \"key\" = ?1 ;";

  sqlite3_stmt *statement = nullptr;
  std::string stmt_cache_key = std::to_string(server_id) + "s";
  int status =
      prepare_statement(conn, query.c_str(), &statement, stmt_cache_key);

  int klen = static_cast<int>(key.length());
  if (status != SQLITE_OK)
    goto err;

  status = sqlite3_bind_text(statement, 1, key.c_str(), klen, SQLITE_STATIC);

  if (status != SQLITE_OK) {
    log::io() << DEBUG_WHERE << "Failed binding key(" << key
              << ") to query with status(" << status << "):\n"
              << query << "\n\n";

    goto err;
  }

  // execute statement
  status = sqlite3_step(statement);
  if (status == SQLITE_ROW) {
    // columns: "value","expires_at"
    ret.value =
        reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));

    ret.expires_at = static_cast<uint64_t>(sqlite3_column_int64(statement, 1));
  }

  reset_statement(&statement);

  return ret;

err:
  finalize_statement(stmt_cache_key, &statement);

  return ret;
}

// only servers are allowed to call this
cache::vector_data_t get_all_cache(sqlite3 *conn, int server_id) noexcept {
  cache::vector_data_t ret;

  std::string query = "SELECT \"value\",\"expires_at\" FROM \"cache\";";

  sqlite3_stmt *statement = nullptr;
  std::string stmt_cache_key = std::to_string(server_id) + "a";
  int status =
      prepare_statement(conn, query.c_str(), &statement, stmt_cache_key);

  cache::data_t temp;
  if (status != SQLITE_OK)
    goto err;

  // execute statement
  while ((status = sqlite3_step(statement)) == SQLITE_ROW) {
    // columns: "value","expires_at"
    temp.value =
        reinterpret_cast<const char *>(sqlite3_column_text(statement, 0));

    temp.expires_at = static_cast<uint64_t>(sqlite3_column_int64(statement, 1));

    ret.emplace_back(temp);
  }

  reset_statement(&statement);

  return ret;

err:
  finalize_statement(stmt_cache_key, &statement);

  return ret;
}

int set_cache(const std::string &key, const cache::data_t &data) noexcept {
  if (key.empty() || data.empty())
    return 1;

  query_schedule_t q("set/" + key);

  q.query = "INSERT INTO \"cache\" "
            "(\"key\", \"value\", \"expires_at\") "

            "VALUES (?1, ?2, ?3) "

            "ON CONFLICT (\"key\") "
            "DO UPDATE SET "

            "\"value\" = ?2, "
            "\"expires_at\" = ?3 ;";

  q.run = [key, data](sqlite3_stmt **statement, const query_schedule_t &q,
                      sqlite3 *conn) -> int {
    auto log_bind_fail = [&q, &statement](const std::string &name,
                                          const std::string &v) {
      log::io() << DEBUG_WHERE << "Failed binding " << name << "(" << v
                << ")\n";

      finalize_statement(q.query, statement);
    };

    int klen = static_cast<int>(key.length());
    int status =
        sqlite3_bind_text(*statement, 1, key.c_str(), klen, SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log_bind_fail("key", key);
      return status;
    }

    int vlen = static_cast<int>(data.value.length());
    status = sqlite3_bind_text(*statement, 2, data.value.c_str(), vlen,
                               SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log_bind_fail("value", data.value);
      return status;
    }

    status = sqlite3_bind_int64(*statement, 3,
                                static_cast<int64_t>(data.get_expires_at()));

    if (status != SQLITE_OK) {
      log_bind_fail("expires_at", std::to_string(data.get_expires_at()));
      return status;
    }

    return query_runner::run_until_done(*statement, q, conn);
  };

  enqueue_write_query(q);

  // schedule delete if it has expires_at
  auto eat = data.get_expires_at();
  if (eat == 0) {
    // erase delete query which might exist
    remove_query({"del/" + key});
  } else {
    delete_cache(key, eat);
  }

  return 0;
}

int delete_cache(const std::string &key, uint64_t at) noexcept {
  if (key.empty())
    return 1;

  query_schedule_t q("del/" + key);

  q.query = "DELETE FROM \"cache\" WHERE \"key\" = ?1 ;";

  q.run = [key](sqlite3_stmt **statement, const query_schedule_t &q,
                sqlite3 *conn) -> int {
    cache::del(key);

    int klen = static_cast<int>(key.length());
    int status =
        sqlite3_bind_text(*statement, 1, key.c_str(), klen, SQLITE_STATIC);

    if (status != SQLITE_OK) {
      log::io() << DEBUG_WHERE << "Failed binding key(" << key << ")\n";

      finalize_statement(q.query, statement);
      return status;
    }

    return query_runner::run_until_done(*statement, q, conn);
  };

  q.ts = at;
  // do not run delete query on shutdown
  q.must_on_schedule = true;

  enqueue_write_query(q);

  return 0;
}

int cleanup() noexcept {
  std::lock_guard lk(stmt_cache_m);

  auto i = stmt_cache.begin();
  while (i != stmt_cache.end()) {
    sqlite3_finalize(i->second);

    log::io() << "Statement destroyed: `" << i->first << "`\n";

    i = stmt_cache.erase(i);
  }
  stmt_cache.rehash(0);

  return 0;
}

} // namespace ssplus_cache_me::db
