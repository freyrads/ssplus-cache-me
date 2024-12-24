#ifndef SCHEDULES_H
#define SCHEDULES_H

#include "ssplus-cache-me/run.h"
#include <string>

namespace ssplus_cache_me::schedules {

struct enqueued_schedule_t {
  bool skip;
  query_schedule_t q;

  enqueued_schedule_t() : skip(false) {}
};

bool should_skip(const std::string &id) noexcept;

void mark_for_skip(const std::string &id) noexcept;

void mark_done(const std::string &id) noexcept;

bool is_skipped(const std::string &id) noexcept;

void enqueue(const query_schedule_t &q);

} // namespace ssplus_cache_me::schedules

#endif // SCHEDULES_H
