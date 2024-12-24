#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
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

cache::data_t get_cache(sqlite3 *conn, const std::string &key) noexcept {
  // TODO
}

cache::data_t set_cache(const std::string &key) noexcept {}

cache::data_t delete_cache(const std::string &key) noexcept {}

} // namespace ssplus_cache_me::db
