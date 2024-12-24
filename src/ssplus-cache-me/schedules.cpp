#include "ssplus-cache-me/schedules.h"
#include <mutex>
#include <vector>

namespace ssplus_cache_me::schedules {

static std::vector<enqueued_schedule_t> queued_schedules;
static std::mutex mm;

static auto find_schedule(const std::string &id) noexcept {
  auto i = queued_schedules.begin();
  while (i != queued_schedules.end()) {
    if (i->q.id == id)
      return i;
  }

  return queued_schedules.end();
}

static bool should_skip_unlocked(const std::string &id) noexcept {
  auto i = find_schedule(id);
  return i != queued_schedules.end() && i->skip;
}

static void erase_schedule(const std::string &id) noexcept {
  auto i = find_schedule(id);
  if (i == queued_schedules.end())
    return;

  queued_schedules.erase(i);
}

static void mark_for_skip_unlocked(const std::string &id) noexcept {
  for (auto i = queued_schedules.begin(); i != queued_schedules.end(); i++) {
    if (i->q.id != id)
      continue;

    i->skip = true;
  }
}

bool should_skip(const std::string &id) noexcept {
  std::lock_guard lk(mm);

  return should_skip_unlocked(id);
}

void mark_for_skip(const std::string &id) noexcept {
  std::lock_guard lk(mm);

  mark_for_skip_unlocked(id);
}

void mark_done(const std::string &id) noexcept {
  std::lock_guard lk(mm);

  erase_schedule(id);
}

bool is_skipped(const std::string &id) noexcept {
  std::lock_guard lk(mm);

  if (should_skip_unlocked(id)) {
    erase_schedule(id);
    return true;
  }

  return false;
}

void enqueue(const query_schedule_t &q) {
  std::lock_guard lk(mm);

  mark_for_skip_unlocked(q.id);

  enqueued_schedule_t s;
  s.q = q;

  queued_schedules.emplace_back(s);
}

} // namespace ssplus_cache_me::schedules
