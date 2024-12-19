#ifndef RUN_H
#define RUN_H

#include <condition_variable>
#include <queue>
#include <shared_mutex>
#include <sqlite3.h>
#include <vector>

namespace ssplus_cache_me {

struct query_schedule_t {
  uint64_t ts;
  std::string query;

  // stepping and finalizing is entirely user controlled
  std::function<int(sqlite3_stmt *, const query_schedule_t &)> run;

  // handles prepare statement and run error, nullable
  std::function<void(int, const query_schedule_t &)> err_cb;

  query_schedule_t() : ts(0) {}
};

inline auto query_schedule_cmp_t = [](const query_schedule_t &a,
                                      const query_schedule_t &b) {
  return a.ts < b.ts;
};

using write_query_queue_t =
    std::priority_queue<query_schedule_t, std::vector<query_schedule_t>,
                        decltype(query_schedule_cmp_t)>;

struct main_t {
  std::atomic<bool> running;

  std::shared_mutex mm;
  std::condition_variable_any mcv;

  // write only conn
  // not protected by mutex because who else gonna
  // use this other than the main thread
  sqlite3 *db;

  // should lock mm to modify this
  write_query_queue_t write_queries;

  main_t() : db(nullptr), write_queries(query_schedule_cmp_t) {}
};

int run(const int argc, const char *argv[]);

main_t *get_main_state() noexcept;

} // namespace ssplus_cache_me

#endif // RUN_H
