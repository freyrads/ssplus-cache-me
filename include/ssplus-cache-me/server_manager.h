#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/server.h"

DECLARE_DEBUG_INFO_DEFAULT();

namespace ssplus_cache_me {

template <bool WITH_SSL> class server_manager_t {
  std::vector<server::server_t<WITH_SSL>> instances;

public:
  int run(int _concurrency, const server::server_config_t &_conf) {
    if (_concurrency < 1)
      return -1;

    if (!instances.empty()) {
      log::io() << DEBUG_WHERE << "Server already running\n";
      return -2;
    }

    for (int i = 0; i < _concurrency; i++) {
      instances.emplace_back(i).start(_conf);
    }

    return 0;
  }

  void shutdown() { instances.clear(); }
};

} // namespace ssplus_cache_me

#endif // SERVER_MANAGER_H
