#ifndef RUN_H
#define RUN_H

#include "ssplus-cache-me/log.h"
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <limits>
#include <shared_mutex>
#include <sqlite3.h>
#include <stdexcept>
#include <string>
#include <thread>

namespace ssplus_cache_me {

struct query_schedule_t {
  using run_fn =
      std::function<int(sqlite3_stmt **, const query_schedule_t &, sqlite3 *)>;

  std::string id;

  uint64_t ts;
  std::string query;
  // skip running this query on shutdown if not on schedule
  bool must_on_schedule;

  // stepping and finalizing is entirely user controlled
  run_fn run;

  query_schedule_t() { init(); }

  query_schedule_t(const std::string &_id) : id(_id) { init(); }

  query_schedule_t &set_id(const std::string &_id);

  query_schedule_t &set_schedule_ts(uint64_t _ts);

  bool operator==(const query_schedule_t &o) const;

private:
  void init() noexcept {
    ts = 0;
    must_on_schedule = false;
  }
};

class write_query_queue_t : public std::deque<query_schedule_t> {
  auto get_top_iter() {
    ssize_t idx = -1;

    if (empty())
      return end();

    uint64_t min_ts = std::numeric_limits<uint64_t>::max();
    auto beg = begin();

    for (size_t i = 0; i < size(); i++) {
      auto t = (beg + i)->ts;
      if (t < min_ts) {
        idx = i;
        min_ts = t;
      }
    }

    // return first index if all ts is 0
    if (idx == -1)
      idx = 0;

    return beg + idx;
  }

public:
  value_type &top() {
    auto i = get_top_iter();
    if (i == end())
      throw std::logic_error("Empty queue");

    return *i;
  }

  void pop() {
    auto i = get_top_iter();
    if (i == end())
      return;

    erase(i);
  }
};

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

  // program configs
  int concurrency;

  main_t() : db(nullptr), concurrency(0) {}

  void set_concurrency(int _concurrency) noexcept {
    static auto hwcon = std::thread::hardware_concurrency();
    if (static_cast<unsigned int>(_concurrency) > hwcon) {
      log::io() << "NOTICE: Setting server concurrency to a higher value than "
                   "hardware concurrency ("
                << hwcon << "): " << _concurrency << "\n";
    }

    concurrency = _concurrency;
  }
};

int run(int argc, char *argv[]);

main_t *get_main_state() noexcept;

bool remove_query(const query_schedule_t &q) noexcept;

void enqueue_write_query(const query_schedule_t &q);

const char *get_exe_name() noexcept;

} // namespace ssplus_cache_me

#endif // RUN_H
