#include "ssplus-cache-me/run.h"
#include "nlohmann/json.hpp"
#include "ssplus-cache-me/info.h"
#include "ssplus-cache-me/server_manager.h"
#include "ssplus-cache-me/version.h"
#include <condition_variable>
#include <csignal>
#include <mutex>
#include <stdio.h>
#include <thread>

namespace ssplus_cache_me {

const char *exe_name = "";

void print_info() {
  fprintf(stderr, PROGRAM_NAME " - version %d.%d.%d\n", VERSION_MAJOR,
          VERSION_MINOR, VERSION_PATCH);

  fprintf(stderr, "Compiled with: ");

  fprintf(stderr, "uWebSockets@%d.%d.%d, ", UWEBSOCKETS_VERSION_MAJOR,
          UWEBSOCKETS_VERSION_MINOR, UWEBSOCKETS_VERSION_PATCH);

  // fprintf(stderr, "sqlite3@%d.%d.%d, ", , , );

  fprintf(stderr, "nlohmann/json@%d.%d.%d (%s)", NLOHMANN_JSON_VERSION_MAJOR,
          NLOHMANN_JSON_VERSION_MINOR, NLOHMANN_JSON_VERSION_PATCH,
          EXTERNAL_JSON ? "EXTERNAL" : "BUNDLED");

  fprintf(stderr, "\n\n");
}

////////////////////////////////////////

static main_t main_state;

static void sigint_handler(int) {
  main_state.running = false;
  main_state.mcv.notify_all();
}

static int run_query(std::string_view query) { return 0; }

static void run_queued_queries() {
  auto i = main_state.write_queries.begin();
  while (i != main_state.write_queries.end()) {
    if (run_query(*i) != 0) {
      i++;
      continue;
    }

    i = main_state.write_queries.erase(i);
  }
}

static void main_loop() {
  while (main_state.running) {
    std::unique_lock lk(main_state.mm);

    main_state.mcv.wait(lk, [] {
      return !main_state.write_queries.empty() || !main_state.running;
    });

    // spurious wake guard
    if (main_state.write_queries.empty())
      continue;

    run_queued_queries();
  }
}

static void init_db() {
  std::lock_guard lk(main_state.mm);

  // open main conn
}

static void shutdown_db() {
  std::lock_guard lk(main_state.mm);

  run_queued_queries();

  // close conn
}

int run(const int argc, const char *argv[]) {
  if (argc > 0)
    exe_name = argv[0];

  print_info();

  signal(SIGINT, sigint_handler);

  server::server_config_t sconf{};

  init_db();

  server_manager_t<false> smanager;
  server_manager_t<true> ssl_smanager;

  if (sconf.with_ssl())
    ssl_smanager.run(std::thread::hardware_concurrency(), sconf);
  else
    smanager.run(std::thread::hardware_concurrency(), sconf);

  main_state.running = true;
  main_loop();

  smanager.shutdown();
  ssl_smanager.shutdown();

  shutdown_db();

  return 0;
}

main_t *get_main_state() { return &main_state; }

} // namespace ssplus_cache_me
