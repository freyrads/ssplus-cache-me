#include "ssplus-cache-me/server_config.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/util.h"
#include <sstream>

namespace ssplus_cache_me::server {

void server_config_t::set_cors_enabled_origins(
    const std::string &val) noexcept {
  cors_enabled_origins.clear();

  std::istringstream f(val);
  std::string s;
  while (getline(f, s, ',')) {
    auto v = util::trim(s);
    if (v.empty())
      continue;

    cors_enabled_origins.push_back(v);
  }

  auto &os = log::io() << "NOTICE: CORS Enabled Origins:\n";
  for (const auto &i : cors_enabled_origins) {
    os << "`" << i << "`\n";
  }
  os << "\n";
}

} // namespace ssplus_cache_me::server
