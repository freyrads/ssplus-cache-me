#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

#include <string>
#include <vector>

namespace ssplus_cache_me::server {

struct server_config_t {
  int port;
  // std::string certfile;
  // std::string pemfile;
  std::vector<std::string> cors_enabled_origins;

  std::string db_path;

  server_config_t() : port(3000) {}

  // bool with_ssl() { return !certfile.empty() && !pemfile.empty(); }
  bool with_ssl() { return false; }

  // parse string with format "origin,origin,origin" to vector, splitting coma
  // and stripping origin
  void set_cors_enabled_origins(const std::string &val) noexcept;
};

} // namespace ssplus_cache_me::server

#endif // SERVER_CONFIG_H
