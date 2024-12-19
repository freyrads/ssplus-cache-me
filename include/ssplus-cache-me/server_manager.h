#ifndef SERVER_MANAGER_H
#define SERVER_MANAGER_H

#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/server.h"

#define _SF_SOURCE_FILE_ DEFAULT_DEBUG_INCLUDE_DIRNAME __FILE_NAME__

namespace ssplus_cache_me {

template <bool WITH_SSL> class server_manager_t {
  using instances_t = std::vector<std::unique_ptr<server::server_t<WITH_SSL>>>;
  instances_t instances;

public:
  int run(int _concurrency, const server::server_config_t &_conf) {
    if (_concurrency < 1)
      return -1;

    if (!instances.empty()) {
      log::io() << DEBUG_WHERE << "Server already running\n";
      return -2;
    }

    instances.reserve(_concurrency);

    for (int i = 0; i < _concurrency; i++) {
      instances.emplace_back(std::make_unique<server::server_t<WITH_SSL>>(i))
          ->start(_conf);
    }

    return 0;
  }

  void shutdown() { instances.clear(); }
};

} // namespace ssplus_cache_me

#undef _SF_SOURCE_FILE_

#endif // SERVER_MANAGER_H
