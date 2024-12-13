#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
namespace ssplus_cache_me::server {

struct server_config_t {
  int port;
  std::string certfile;
  std::string pemfile;

  server_config_t() : port(3000) {}

  bool with_ssl() { return !certfile.empty() && !pemfile.empty(); }
};

} // namespace ssplus_cache_me::server

#endif // SERVER_CONFIG_H
