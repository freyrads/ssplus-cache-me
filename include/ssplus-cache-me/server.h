#ifndef SERVER_H
#define SERVER_H

#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/server_config.h"
#include "uWebSockets/src/App.h"
#include <sqlite3.h>
#include <thread>

#define _SF_SOURCE_FILE_ DEFAULT_DEBUG_INCLUDE_DIRNAME __FILE_NAME__

namespace ssplus_cache_me::server {

// server_t

template <bool WITH_SSL> class server_t {
  int id;

  server_config_t conf;

  std::thread *sthread;
  sqlite3 *db_conn;

  uWS::TemplatedApp<WITH_SSL> *sapp;
  uWS::Loop *sloop;
  us_listen_socket_t *slisten_socket;

  void main() {
    if (!valid())
      throw std::runtime_error("Invalid instance");

    if (init_db() != 0) {
      throw std::runtime_error("Failed initializing db");
    }

    init_server();
    run();
  }

  void start_thread() {
    if (sthread != nullptr)
      return;

    sthread = new std::thread([this] { main(); });
  }

  int init_db() noexcept { return 0; }

  int shutdown_db() noexcept { return 0; }

  void init_server() {
    sapp = new uWS::TemplatedApp<WITH_SSL>{};

    sloop = uWS::Loop::get();

    //
  }

  void run() { sapp->run(); }

  void shutdown_server() {
    if (sapp == nullptr)
      return;

    defer([this] { sapp->close(); });
  }

  bool valid() noexcept { return id >= 0; }

public:
  server_t() noexcept : id(-1) { init(); };

  server_t(int _id) noexcept : id(_id) { init(); };

  ~server_t() {
    shutdown_server();

    if (sthread && sthread->joinable()) {
      sthread->join();
      delete sthread;
      sthread = nullptr;
    }

    shutdown_db();

    slisten_socket = nullptr;
    sloop = nullptr;

    if (sapp) {
      delete sapp;
      sapp = nullptr;
    }
  }

  void init(int _id) noexcept {
    if (_id != -1)
      id = _id;

    sthread = nullptr;
    db_conn = nullptr;

    sapp = nullptr;
    sloop = nullptr;
    slisten_socket = nullptr;
  }

  int start() {
    if (!valid())
      return -1;

    start_thread();

    return 0;
  }

  int start(const server_config_t &_conf) {
    conf = _conf;
    return start();
  }

  int defer(std::function<void()> &&task) {
    if (sloop == nullptr)
      return 1;

    sloop->defer(task);
    return 0;
  }
};

////////////////////////////////////////

} // namespace ssplus_cache_me::server

#undef _SF_SOURCE_FILE_

#endif // SERVER_H
