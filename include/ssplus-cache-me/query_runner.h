#ifndef QUERY_RUNNER_H
#define QUERY_RUNNER_H

#include "ssplus-cache-me/run.h"
#include <sqlite3.h>

namespace ssplus_cache_me::query_runner {

int run_until_done(sqlite3_stmt *statement, const query_schedule_t &q,
                   sqlite3 *conn);

} // namespace ssplus_cache_me::query_runner

#endif // QUERY_RUNNER_H
