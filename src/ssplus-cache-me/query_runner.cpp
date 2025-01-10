#include "ssplus-cache-me/query_runner.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/util.h"
#include <sqlite3.h>

DECLARE_DEBUG_INFO_DEFAULT();

namespace ssplus_cache_me::query_runner {

int run_until_done(sqlite3_stmt *statement, const query_schedule_t &q,
                   sqlite3 *conn) {
  int status = 0;

  do {
    status = sqlite3_step(statement);

    if (status == SQLITE_DONE)
      break;

    if (status == SQLITE_BUSY) {
      log::io() << "Database is busy, rescheduling query...\n";

      // lets reschedule this 5 second later
      query_schedule_t newq(q);

      newq.ts = util::get_current_ts() + 5000;

      enqueue_write_query(newq);
      break;
    }

    if ((SQLITE_ERROR & status) == SQLITE_ERROR) {
      log::io() << DEBUG_WHERE << "SQLITE_ERROR: " << sqlite3_errmsg(conn)
                << "\n";

      break;
    }
  } while (status != SQLITE_DONE);

  return status;
}
} // namespace ssplus_cache_me::query_runner
