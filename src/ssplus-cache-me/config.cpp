#include "ssplus-cache-me/config.h"
#include "nlohmann/json.hpp"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include <fstream>
#include <getopt.h>

DECLARE_DEBUG_INFO_DEFAULT();

/**
 * Config can be set from environment variable, a json file or directly as
 * arguments.
 *
 * Environment variables will be loaded first, config provided as
 * argument will take precedence based on the order.
 */
namespace ssplus_cache_me::config {

/**
 * Lets decide our env config here:
 *
 * Program configs:
 * SPLUS_CONCURRENCY : unsigned integer, number of thread which run the
 *                     server
 * SPLUS_CONF        : string, path to JSON config, this will be loaded
 *                     first if exist before other env config
 *
 * Server configs:
 * PORT               : unsigned integer, any valid port
 * SPLUS_CORS_MAX_AGE : unsigned integer, for cors Access-Control-Max-Age header
 * SPLUS_ALLOW_CORS   : string, a list, coma separated origins
 * SPLUS_DB           : string, path to sqlite db
 *
 */
inline constexpr const struct {
  const char *config_path = "SPLUS_CONF";

  const char *concurrency = "SPLUS_CONCURRENCY";
  const char *port = "PORT";
  const char *cors_max_age = "SPLUS_CORS_MAX_AGE";
  const char *allow_cors = "SPLUS_ALLOW_CORS";
  const char *database = "SPLUS_DB";
} env_keys;

/**
 * JSON for config format:
 *
 * Program configs:
 * concurrency : unsigned integer, number of thread which run the
 *               server
 *
 * Server configs:
 * port         : unsigned integer, any valid port
 * cors_max_age : unsigned integer, for cors Access-Control-Max-Age header
 * allow_cors   : string, a list, coma separated origins
 * database     : string, path to sqlite db
 *
 * Example:
 * {
 *    "concurrency": 8,
 *    "port": 3000,
 *    "cors_max_age": 86400,
 *    "allow_cors": "https://www.google.com,https://www.yahoo.com",
 *    "database": "/home/app/cache.sqlite3"
 * }
 */
inline constexpr const struct {
  const char *concurrency = "concurrency";
  const char *port = "port";
  const char *cors_max_age = "cors_max_age";
  const char *allow_cors = "allow_cors";
  const char *database = "database";
} json_keys;

/**
 * Program arguments for configs:
 *
 * Program configs:
 * -t, --concurrency  : unsigned integer, number of thread which run the server
 * -c, --config       : string, path to json config
 *
 * Server configs:
 * -p, --port         : unsigned integer, any valid port
 * -m, --cors-max-age : unsigned integer, for cors Access-Control-Max-Age header
 * -a, --allow-cors   : string, a list, coma separated origins
 * -d, --database     : string, path to sqlite db
 *
 * Non-config arguments:
 * -h, --help        : print help
 *
 */

static void print_usage() {
  fprintf(stderr, "Usage: %s [options...]\n\n", get_exe_name());
  fprintf(stderr, "Options:\n");

  struct {
    const char *opt;
    const char *arg;
    const char *desc;
  } arglist[] = {{"-h, --help", "", "Print this message and exit."},
                 {"-t, --concurrency", "<uint>",
                  "Number of thread which run the server. Default is available "
                  "CPU cores."},
                 {"-c, --config", "</path/to/conf.json>",
                  "Load configuration from a JSON file."},

                 {"-p, --port", "<uint>", "Port to listen on. Default 3000."},
                 {"-m, --cors-max-age", "<uint>",
                  "CORS header Max Age. Default is CORS_VALID_FOR compile "
                  "definition, or 84000"},
                 {"-a, --allow-cors", "<origins...>",
                  "List of origin enabled for CORS, separated by coma (,)."},
                 {"-d, --database", "</path/to/db.sqlite3>",
                  "Cache database to use. Default \"cache.sqlite3\""}};

  for (size_t i = 0; i < sizeof(arglist) / sizeof(*arglist); i++) {
    auto &v = arglist[i];
    fprintf(stderr, "  %-18s %-30s %s\n", v.opt, v.arg, v.desc);
  }
  fprintf(stderr, "\n");
}

inline constexpr const struct {
  const char *invalid_concurrency = "Invalid concurrency, skipping";
  const char *invalid_port = "Invalid port, skipping";
  const char *invalid_cors = "Invalid allow_cors, skipping";
  const char *invalid_database = "Invalid database, skipping";
  const char *invalid_cors_max_age = "Invalid cors_max_age, skipping";
  /*const char *invalid_;*/
} error_messages;

#define PORT_MAX 65535
static bool valid_port(int val) { return val >= 1 && val <= PORT_MAX; }

static void str_set_concurrency(main_t &main_state, char *str_concurrency) {
  int val = atoi(str_concurrency);
  if (val < 1) {
    log::io() << error_messages.invalid_concurrency << "\n";
  } else {
    main_state.set_concurrency(val);
  }
}

static void str_set_port(server::server_config_t &sconf, char *str_port) {
  int val = atoi(str_port);
  if (!valid_port(val)) {
    log::io() << error_messages.invalid_port << "\n";
  } else {
    sconf.port = val;
  }
}

static void str_set_cors_max_age(server::server_config_t &sconf,
                                 char *str_cors_max_age) {
  uint64_t val = strtoull(str_cors_max_age);
  if (val == ULLONG_MAX) {
    log::io() << error_messages.invalid_cors_max_age << "\n";
  } else if (val) {
    sconf.cors_max_age = val;
  }
}

void load_env(main_t &main_state, server::server_config_t &sconf) {
  auto has = [](char *v) -> bool { return v && strlen(v) > 0; };

  log::io() << "Loading environment\n";

  char *conf_path = std::getenv(env_keys.config_path);
  if (has(conf_path)) {
    parse_json_config(main_state, sconf, conf_path);
  }

  char *str_concurrency = std::getenv(env_keys.concurrency);
  if (has(str_concurrency)) {
    str_set_concurrency(main_state, str_concurrency);
  }

  char *str_port = std::getenv(env_keys.port);
  if (has(str_port)) {
    str_set_port(sconf, str_port);
  }

  char *str_cors_max_age = std::getenv(env_keys.cors_max_age);
  if (has(str_cors_max_age)) {
    str_set_cors_max_age(sconf, str_cors_max_age);
  }

  char *str_allow_cors = std::getenv(env_keys.allow_cors);
  if (has(str_allow_cors)) {
    sconf.set_cors_enabled_origins(str_allow_cors);
  }

  char *str_db = std::getenv(env_keys.database);
  if (has(str_db)) {
    sconf.db_path = str_db;
  }
}

void parse_json_config(main_t &main_state, server::server_config_t &sconf,
                       const char *path) {
  log::io() << "Loading JSON Configuration: `" << path << "`\n";

  nlohmann::json data;

  {
    std::ifstream scs(path);
    if (!scs.is_open()) {
      log::io() << DEBUG_WHERE << "Failed opening JSON file: `" << path
                << "`\n";
      return;
    }

    // this may throw nlohmann error on invalid json
    scs >> data;
    scs.close();
  }

  // parse each property
  auto i = data.find(json_keys.concurrency);
  if (i != data.end()) {
    if (!i->is_number_unsigned()) {
      log::io() << error_messages.invalid_concurrency << "\n";
    } else {
      main_state.set_concurrency(i->get<int>());
    }
  }

  i = data.find(json_keys.port);
  if (i != data.end()) {
    int val = 0;
    if (!i->is_number_unsigned() || !valid_port((val = i->get<int>()))) {
      log::io() << error_messages.invalid_port << "\n";
    } else {
      sconf.port = val;
    }
  }

  i = data.find(json_keys.cors_max_age);
  if (i != data.end()) {
    if (!i->is_number_unsigned()) {
      log::io() << error_messages.invalid_cors_max_age << "\n";
    } else {
      sconf.cors_max_age = i->get<uint64_t>();
    }
  }

  i = data.find(json_keys.allow_cors);
  if (i != data.end()) {
    std::string v;
    if (!i->is_string()) {
      log::io() << error_messages.invalid_cors << "\n";
    } else if (!(v = i->get<std::string>()).empty()) {
      // only set if not empty
      sconf.set_cors_enabled_origins(v);
    }
  }

  i = data.find(json_keys.database);
  if (i != data.end()) {
    std::string v;
    if (!i->is_string()) {
      log::io() << error_messages.invalid_database << "\n";
    } else if (!(v = i->get<std::string>()).empty()) {
      // only set if not empty
      sconf.db_path = v;
    }
  }
}

// if returns 1 should exit with status zero
int parse_args(main_t &main_state, server::server_config_t &sconf, int argc,
               char *argv[]) {
  int status = 0;
  int c;

  while (1) {
    int option_index = 0;
    static struct option long_options[] = {
        {"concurrency", required_argument, 0, 't'},
        {"config", required_argument, 0, 'c'},
        {"port", required_argument, 0, 'p'},
        {"cors-max-age", required_argument, 0, 'm'},
        {"allow-cors", required_argument, 0, 'a'},
        {"database", required_argument, 0, 'd'},

        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}};

    c = getopt_long(argc, argv, "t:c:p:a:d:h", long_options, &option_index);
    if (c == -1)
      break;

    switch (c) {
    case 0: {
      auto &os = log::io() << "Unknown option `"
                           << long_options[option_index].name << "`";
      if (optarg)
        os << " with arg `" << optarg << "`";

      os << "\n";
      break;
    }

    case 't':
      str_set_concurrency(main_state, optarg);
      break;
    case 'c':
      parse_json_config(main_state, sconf, optarg);
      break;
    case 'p':
      str_set_port(sconf, optarg);
      break;
    case 'm':
      str_set_cors_max_age(sconf, optarg);
      break;
    case 'a':
      sconf.set_cors_enabled_origins(optarg);
      break;
    case 'd':
      sconf.db_path = optarg;
      break;

    case 'h':
      status = 1;
    case '?': // invalid usage
    default:
      print_usage();

      if (status != 1)
        status = -1;

      return status;
    }
  }

  while (optind < argc)
    log::io() << "Unknown argument: `" << argv[optind++] << "`\n";

  return status;
}

} // namespace ssplus_cache_me::config
