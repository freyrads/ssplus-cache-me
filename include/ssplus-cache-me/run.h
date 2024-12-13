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
  std::function<void(int, const query_schedule_t &)> err_cb;
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
  sqlite3 *db;
  write_query_queue_t write_queries;

  main_t() : write_queries(query_schedule_cmp_t) {}
};

int run(const int argc, const char *argv[]);

main_t *get_main_state();

} // namespace ssplus_cache_me

#endif // RUN_H
