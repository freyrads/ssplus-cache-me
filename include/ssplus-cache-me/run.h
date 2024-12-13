#ifndef RUN_H
#define RUN_H

#include <condition_variable>
#include <shared_mutex>
#include <sqlite3.h>
#include <vector>

namespace ssplus_cache_me {

struct main_t {
  std::atomic<bool> running;

  std::shared_mutex mm;
  std::condition_variable_any mcv;

  // write only conn
  sqlite3 *db;
  std::vector<std::string> write_queries;
};

int run(const int argc, const char *argv[]);

main_t *get_main_state();

} // namespace ssplus_cache_me

#endif // RUN_H
