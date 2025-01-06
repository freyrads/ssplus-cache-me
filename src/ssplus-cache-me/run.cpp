#include "ssplus-cache-me/run.h"
#include "nlohmann/json.hpp"
#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/info.h"
#include "ssplus-cache-me/query_runner.h"
#include "ssplus-cache-me/server_manager.h"
#include "ssplus-cache-me/util.h"
#include "ssplus-cache-me/version.h"
#include <chrono>
#include <condition_variable>
#include <csignal>
#include <exception>
#include <mutex>
#include <sqlite3.h>
#include <stdio.h>
#include <thread>
#include <unistd.h>

DECLARE_DEBUG_INFO_DEFAULT();

namespace ssplus_cache_me {

const char *exe_name = "";

// query_schedule_t ////////////////////

query_schedule_t &query_schedule_t::set_id(const std::string &_id) {
  id = _id;
  return *this;
}

query_schedule_t &query_schedule_t::set_schedule_ts(uint64_t _ts) {
  ts = _ts;
  return *this;
}

bool query_schedule_t::operator==(const query_schedule_t &o) const {
  return !id.empty() && id == o.id;
}

////////////////////////////////////////

static void print_info() {
  fprintf(stderr, PROGRAM_NAME " - version %d.%d.%d\n", VERSION_MAJOR,
          VERSION_MINOR, VERSION_PATCH);

  fprintf(stderr, "Compiled with: ");

  fprintf(stderr, "uWebSockets@%d.%d.%d, ", UWEBSOCKETS_VERSION_MAJOR,
          UWEBSOCKETS_VERSION_MINOR, UWEBSOCKETS_VERSION_PATCH);

  fprintf(stderr, "sqlite3@" SQLITE_VERSION ", ");

  fprintf(stderr, "nlohmann/json@%d.%d.%d (%s)", NLOHMANN_JSON_VERSION_MAJOR,
          NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH,
          EXTERNAL_JSON ? "EXTERNAL" : "BUNDLED");

  fprintf(stderr, "\n\n");
}

////////////////////////////////////////

static main_t main_state;

static int sigint_count = 0;

static void sigint_handler(int) {
  main_state.running = false;

  constexpr const char rcvmsg[] = "\nRECEIVED SIGINT\n";
  write(STDERR_FILENO, rcvmsg, sizeof(rcvmsg));
  sigint_count++;

  if (sigint_count >= 3) {
    constexpr const char forceexitmsg[] =
        "\nRECEIVED 3 SIGINT, FORCE TERMINATING...\n";
    write(STDERR_FILENO, forceexitmsg, sizeof(forceexitmsg));
    std::terminate();
  }

  main_state.mcv.notify_all();
}

// write_query_routine /////////////////

static int run_query(const query_schedule_t &q) {
  log::io() << "[" << util::get_current_ts()
            << "] Running scheduled query on ts(" << q.ts << ") `" << q.id
            << "`:\n"
            << q.query << "\n";

  sqlite3_stmt *stmt = nullptr;

  int status = db::prepare_statement(main_state.db, q.query.c_str(), &stmt);

  if (status != SQLITE_OK) {
    log::io() << "^^^ Error preparing statement: "
              << sqlite3_errmsg(main_state.db) << "\n";

    if (stmt) {
      sqlite3_finalize(stmt);
      return status;
    }
  }

  if (stmt == nullptr) {
    // no thought, head empty
    return 0;
  }

  status = q.run(stmt, q, main_state.db);

  return status;
}

static void run_queued_queries(const bool shutdown = false) {
  int status = 0;
  query_schedule_t i;

  do {
    {
      std::lock_guard lk(main_state.mm);

      if (main_state.write_queries.empty())
        return;

      i = main_state.write_queries.top();

      bool not_on_schedule = i.ts > util::get_current_ts();
      // should check all runnable query on shutdown
      bool dont_run_now = !shutdown && not_on_schedule;
      if (dont_run_now)
        return;

      main_state.write_queries.pop();

      if (shutdown && i.must_on_schedule && not_on_schedule)
        return;
    }

    if ((status = run_query(i)) != 0) {
      if (status != SQLITE_DONE) {
        log::io() << "Error running scheduled query with status: " << status
                  << "\n";
      }

      status = 0;
    }
  } while (true);
}

static void write_query_routine() {
  {
    std::unique_lock lk(main_state.mm);

    if (!main_state.write_queries.empty()) {
      auto d = main_state.write_queries.top();

      auto top_sch = d.ts;
      auto cur = util::get_current_ts();

      // log::io() << "cur(" << cur << ") top_sch(" << top_sch << ")\n";

      if (top_sch > cur) {
        main_state.mcv.wait_until(
            lk,
            std::chrono::system_clock::time_point{
                std::chrono::milliseconds(top_sch)},
            [&top_sch] {
              return top_sch <= util::get_current_ts() ||
                     top_sch != main_state.write_queries.top().ts ||
                     !main_state.running;
            });

        // spurious wake guard
        if (main_state.write_queries.empty() ||
            top_sch != main_state.write_queries.top().ts)
          return;
      }
    } else
      main_state.mcv.wait(lk, [] {
        return !main_state.write_queries.empty() || !main_state.running;
      });

    // spurious wake guard
    if (main_state.write_queries.empty() ||
        main_state.write_queries.top().ts > util::get_current_ts())
      return;
  }

  run_queued_queries();
}

////////////////////////////////////////

static void main_loop() {
  while (main_state.running) {
    write_query_routine();
  }
}

static int init_db(const char *path) {
  db::setup();

  log::io() << "Initializing main db conn\n";

  // open main conn
  int status = db::init(path, &main_state.db);

  if (status != SQLITE_OK) {
    auto &os = log::io() << "Error with status(" << status << ")";

    if (main_state.db == nullptr)
      os << ", main db conn closed\n";
    else
      os << " but main db conn is NOT closed\n";
  } else {
    // check for readonly
    if ((status = sqlite3_db_readonly(main_state.db, "main")) != 0) {
      switch (status) {
      case 1:
        // TODO: Support readonly mode?
        log::io() << "Database `" << path << "` is READONLY. Exiting...\n";
        db::close(&main_state.db);
        return status;
      case -1:
        log::io() << "NOTICE: Unknown database name? Is it not main?\n";
        break;
      default:
        log::io()
            << "NOTICE: Unknown status returned by sqlite3_db_readonly(): "
            << status << "\n";
      }
    }

    // init db tables
    query_schedule_t init_q("init_db");

    init_q.query = "CREATE TABLE IF NOT EXISTS \"cache\" (\"key\" VARCHAR "
                   "UNIQUE PRIMARY KEY NOT NULL, \"value\" VARCHAR NOT NULL, "
                   "\"expires_at\" UNSIGNED BIG INT DEFAULT 0);";

    init_q.run = query_runner::run_until_done;

    enqueue_write_query(init_q);
  }

  return status;
}

static int shutdown_db() {
  int status = 0;

  log::io() << "Shutting down main db conn\n";

  // finish all enqueued queries before shutdown
  run_queued_queries(true);

  if (main_state.db) {
    status = db::close(&main_state.db);

    switch (status) {
    case SQLITE_OK:
      log::io() << "Main db conn closed\n";
      break;
    case SQLITE_BUSY:
      log::io() << "Main db conn is busy and left intact\n";
      break;
    default:
      log::io() << "Closing main db conn returned unknown status: " << status
                << "\n";
    }
  } else
    log::io() << "Main db conn was never made\n";

  return status;
}

int run(const int argc, const char *argv[]) {
  if (argc > 0)
    exe_name = argv[0];

  print_info();

  signal(SIGINT, sigint_handler);

  server::server_config_t sconf{};

  sconf.db_path = "cache.sqlite3";

  // TODO: parse args here

  // TODO: make db filename configurable
  if (init_db(sconf.db_path.c_str()) != 0) {
    log::io() << "Failed initializing database\n";
    return 1;
  }

  main_state.running = true;
  main_state.concurrency = std::thread::hardware_concurrency();

  server_manager_t<false> smanager;
  server_manager_t<true> ssl_smanager;

  if (sconf.with_ssl()) {
    log::io() << "NOTICE: SSL Enabled server\n";
    ssl_smanager.run(main_state.concurrency, sconf);
  } else
    smanager.run(main_state.concurrency, sconf);

  main_loop();

  smanager.shutdown();
  ssl_smanager.shutdown();

  shutdown_db();

  return 0;
}

main_t *get_main_state() noexcept { return &main_state; }

void enqueue_write_query(const query_schedule_t &q) {
  std::lock_guard lk(main_state.mm);

  // remove all schedule with the same id
  auto i = std::find(main_state.write_queries.begin(),
                     main_state.write_queries.end(), q);
  if (i != main_state.write_queries.end())
    main_state.write_queries.erase(i);

  main_state.write_queries.push_back(q);
  main_state.mcv.notify_one();
}

} // namespace ssplus_cache_me
