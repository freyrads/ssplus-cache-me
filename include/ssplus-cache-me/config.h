#ifndef CONFIG_H
#define CONFIG_H

#include "ssplus-cache-me/run.h"
#include "ssplus-cache-me/server_config.h"

namespace ssplus_cache_me::config {

void load_env(main_t &main_state, server::server_config_t &sconf);

// may throws json error
void parse_json_config(main_t &main_state, server::server_config_t &sconf,
                       const char *path);

int parse_args(main_t &main_state, server::server_config_t &sconf,
                int argc, char *argv[]);

} // namespace ssplus_cache_me::config

#endif // CONFIG_H
