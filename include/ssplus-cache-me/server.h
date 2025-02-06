#ifndef SERVER_H
#define SERVER_H

#include "nlohmann/json.hpp"
#include "ssplus-cache-me/cache.h"
#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/server_config.h"
#include "ssplus-cache-me/util.h"
#include "uWebSockets/src/App.h"
#include <chrono>
#include <cstdint>
#include <exception>
#include <sqlite3.h>
#include <stdexcept>
#include <thread>

/*#define _DEV*/

#define _SF_SOURCE_FILE_ DEFAULT_DEBUG_INCLUDE_DIRNAME __FILE_NAME__

// maybe these should be configurable

// a day
#ifndef CORS_VALID_FOR
#define CORS_VALID_FOR "86400"
#endif // CORS_VALID_FOR

namespace ssplus_cache_me::server {

inline constexpr const struct {
  const char *OK_200 = "200 OK";
  const char *NO_CONTENT_204 = "204 No Content";
  const char *NOT_MODIFIED_304 = "304 Not Modified";
  const char *BAD_REQUEST_400 = "400 Bad Request";
  const char *UNAUTHORIZED_401 = "401 Unauthorized";
  const char *FORBIDDEN_403 = "403 Forbidden";
  const char *NOT_FOUND_404 = "404 Not Found";
  const char *INTERNAL_SERVER_ERROR_500 = "500 Internal Server Error";
} http_status_t;

inline constexpr const struct {
  const char *content_type = "Content-Type";
} header_key_t;

inline constexpr const struct {
  const char *json = "application/json";
} content_type_t;

using header_v_t = std::vector<std::pair<std::string, std::string>>;

// server_t ////////////////////////////

template <bool WITH_SSL> class server_t {
  using uws_response_t = uWS::HttpResponse<WITH_SSL>;
  using uws_request_t = uWS::HttpRequest;
  // pair of key with data
  using cache_data_t = std::pair<std::string, cache::data_t>;

  // endpoint_bench_t ////////////////////

  struct endpoint_bench_t {
    std::string name;
    std::chrono::steady_clock::time_point start;
    bool cancelled;

    endpoint_bench_t() { begin(); }

    explicit endpoint_bench_t(const std::string &_name) : name(_name) {
      begin();
    }

    endpoint_bench_t &begin() {
      cancelled = false;
      start = std::chrono::steady_clock::now();

      return *this;
    }

    endpoint_bench_t &set_name(const std::string &s) {
      name = s;
      return *this;
    }

    endpoint_bench_t &cancel(bool c = true) {
      cancelled = c;
      return *this;
    }

    ~endpoint_bench_t() {
      if (cancelled || name.empty())
        return;
      log::io() << "[ENDPOINT_BENCH] [" << name
                << "]: " << (std::chrono::steady_clock::now() - start).count()
                << "ns\n";
    }
  };

  ////////////////////////////////////////

  // http_response_t /////////////////////

  struct http_response_t {
    uws_response_t *res;
    const char *status;

    header_v_t headers;
    std::string data;

    http_response_t &reset(uws_response_t *_res = nullptr) {
      if (_res)
        res = _res;
      else
        res = nullptr;
      status = http_status_t.OK_200;

      headers.clear();
      data.clear();
      return *this;
    }

    http_response_t() : res(nullptr) { init(); }

    explicit http_response_t(uws_response_t *_res) : res(_res) { init(); }

    explicit http_response_t(uws_response_t *_res, const header_v_t &_headers)
        : res(_res), headers(_headers) {
      init();
    }

    ~http_response_t() {
      if (!res || !status)
        return;

      res->writeStatus(status);

      if (!headers.empty())
        write_headers(res, headers);

      if (data.empty())
        res->end();
      else
        res->end(data);

      res = nullptr;
    }

    http_response_t &set_res(uws_response_t *_res = nullptr) {
      res = _res;
      return *this;
    }

    http_response_t &set_status(const char *_status) {
      status = _status;
      return *this;
    }

    http_response_t &set_data(const std::string &_data) {
      data = _data;
      return *this;
    }

    http_response_t &set_data(const nlohmann::json &_data) {
      return set_data(_data.dump());
    }

  private:
    void init() noexcept { status = http_status_t.OK_200; }
  };

  class http_error_t : public std::exception {
    std::string msg;

  public:
    explicit http_error_t(const std::string &_m) : msg(_m) {}
    explicit http_error_t(const char *_m) : msg(_m) {}

    const char *what() const noexcept { return msg.c_str(); };
  };

  using post_cache_custom_handler_fn =
      std::function<bool(http_response_t &, cache_data_t &)>;

  ////////////////////////////////////////

  int id;

  server_config_t conf;

  std::thread *sthread;
  sqlite3 *db_conn;

#ifndef _DEV
  uWS::TemplatedApp<WITH_SSL> *sapp;
#else
  uWS::TemplatedApp<false> *sapp;
#endif // _DEV
  uWS::Loop *sloop;
  us_listen_socket_t *slisten_socket;

  static inline const header_v_t cors_default_additional_headers = {
      {"Access-Control-Expose-Headers", "Content-Length,Content-Range"}};

  static inline const std::string cors_default_allow_headers =
      "DNT,User-Agent,X-Requested-With,If-Modified-Since,"
      "Cache-Control,Content-Type,Range";

  void main() {
    if (!valid())
      throw std::runtime_error(get_id_for_log() + "Invalid instance");

    if (init_db() != 0)
      throw std::runtime_error(get_id_for_log() + "Failed initializing db");

    init_server();

    run();
  }

  void start_thread() {
    if (sthread != nullptr)
      return;

    sthread = new std::thread([this] { main(); });
  }

  int init_db() noexcept {
    // open conn
    int status = db::init(conf.db_path.c_str(), &db_conn);

    if (status != SQLITE_OK) {
      if (db_conn == nullptr)
        log::io() << get_id_for_log() << "Db conn closed\n";
      else
        log::io() << get_id_for_log() << "Error with status(" << status
                  << ") but db conn was NOT closed\n";
    }

    return status;
  }

  int shutdown_db() noexcept {
    int status = 0;

    const auto lstr = get_id_for_log();
    log::io() << lstr << "Shutting down db conn\n";

    if (db_conn) {
      status = db::close(&db_conn);

      switch (status) {
      case SQLITE_OK:
        log::io() << lstr << "Db conn closed\n";
        break;
      case SQLITE_BUSY:
        log::io() << lstr << "Db conn is busy and left intact\n";
        break;
      default:
        log::io() << lstr
                  << "Closing db conn returned unknown status: " << status
                  << "\n";
      }
    } else
      log::io() << lstr << "Db conn was never made\n";

    return status;
  }

  void init_server() {
    /* TODO: support ssl options?
    uWS::SocketContextOptions opts;

    const char *key_file_name = nullptr;
    const char *cert_file_name = nullptr;
    const char *passphrase = nullptr;
    const char *dh_params_file_name = nullptr;
    const char *ca_file_name = nullptr;
    const char *ssl_ciphers = nullptr;
    */

    sapp = new uWS::TemplatedApp<WITH_SSL>{};

    register_routes();

    sapp->listen(conf.port, [this](us_listen_socket_t *listen_socket) {
      if (listen_socket) {
        slisten_socket = listen_socket;
        log::io() << get_id_for_log() << "Listening on port " << conf.port
                  << "\n";
      } else {
        log::io() << DEBUG_WHERE << get_id_for_log()
                  << "Listening socket is null, failed to load certs or to "
                     "bind to port\n";

        throw std::runtime_error(get_id_for_log() + "Unable to start server");
      }
    });

    sloop = uWS::Loop::get();
  }

  void run() {
    sapp->run();

    delete sapp;
    sapp = nullptr;
  }

  void shutdown_server() {
    if (sapp)
      defer([this] { sapp->close(); });

    if (sthread == nullptr)
      return;

    if (sthread->joinable())
      sthread->join();

    delete sthread;
    sthread = nullptr;
  }

  // setup routes
  void register_routes() {
    auto any_any = [this](uws_response_t *res, uws_request_t *req) {
      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      res->writeStatus(http_status_t.NOT_FOUND_404);
      write_headers(res, cors_headers);
      res->end();
    };

    auto options_cors = [this](uws_response_t *res, uws_request_t *req) {
      auto cors_headers =
          cors(res, req,
               {
                   {"Access-Control-Max-Age",
                    conf.cors_max_age != 0 ? std::to_string(conf.cors_max_age)
                                           : CORS_VALID_FOR},
               });

      if (cors_headers.empty())
        return;

      res->writeStatus(http_status_t.NO_CONTENT_204);
      write_headers(res, cors_headers);
      res->end();
    };

    auto get_cache = [this](uws_response_t *res, uws_request_t *req) {
      endpoint_bench_t bench("GET /cache/:key");

      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_response_t hres(res, cors_headers);

      auto key = req->getParameter(0);
      if (key.empty()) {
        hres.set_status(http_status_t.BAD_REQUEST_400);
        return;
      }

      std::string str_key(key);

      http_handlers::get_cache(hres, str_key, db_conn);
    };

    auto post_cache = [this](uws_response_t *res, uws_request_t *req) {
      endpoint_bench_t bench("POST /cache");

      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_handlers::post_cache(res, cors_headers, bench);
    };

    auto get_post_cache = [this](uws_response_t *res, uws_request_t *req) {
      endpoint_bench_t bench("POST /cache/get-or-set");

      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_handlers::post_cache(
          res, cors_headers, bench,
          [this](http_response_t &hres, cache_data_t &data) -> bool {
            if (http_handlers::get_cache(hres, data.first, db_conn) == 0)
              return true;

            hres.reset(hres.res);
            return false;
          });
    };

    auto delete_cache = [this](uws_response_t *res, uws_request_t *req) {
      endpoint_bench_t bench("DELETE /cache/:key");

      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_response_t hres(res, cors_headers);
      auto key = req->getParameter(0);
      if (key.empty()) {
        hres.set_status(http_status_t.BAD_REQUEST_400);
        return;
      }

      std::string str_key(key);

      // - Delete cache in mem
      // - Delete cache in db
      // - Mark skip any schedule with
      //   ID key (already handled by db::delete_cache())
      cache::del(str_key);
      db::delete_cache(str_key);

      set_content_type_json(hres);
      hres.set_data(json_response::success({{"message", "OK"}}));
    };

    // stat endpoints
    auto get_checkhealth = [this](uws_response_t *res, uws_request_t *req) {
      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_response_t hres(res, cors_headers);
      // TODO: what to do?
    };

    // log triggers
    auto get_trigger_log_cache = [this](uws_response_t *res,
                                        uws_request_t *req) {
      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_response_t hres(res, cors_headers);
      // TODO: what to do?
    };

    // REGISTER ROUTES /////////////////////
    // 404, cors middleware
    sapp->any("/*", any_any);
    sapp->options("/*", options_cors);
    sapp->head("/*", options_cors);

    // TODO: how do we implement these?
    // stat endpoints
    // sapp->get("/checkhealth", get_checkhealth);

    // log triggers
    // sapp->get("/trigger_log/cache", get_trigger_log_cache);

    // app endpoints
    // The actual original routes are different, these only follows the doc in
    // README.md
    sapp->get("/cache/:key", get_cache);
    sapp->post("/cache", post_cache);
    sapp->post("/cache/get-or-set", get_post_cache);
    sapp->del("/cache/:key", delete_cache);
  }

public:
  void init(int _id = -1) noexcept {
    if (_id != -1)
      id = _id;

    sthread = nullptr;
    db_conn = nullptr;

    sapp = nullptr;
    sloop = nullptr;
    slisten_socket = nullptr;
  }

  server_t() noexcept : id(-1) { init(); };

  server_t(int _id) noexcept : id(_id) { init(); };

  ~server_t() {
    shutdown_server();

    shutdown_db();

    slisten_socket = nullptr;
    sloop = nullptr;
  }

  bool valid() const noexcept { return id >= 0; }

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

  std::string get_id_for_log() const {
    constexpr int bufsz = 16;
    char ret[bufsz];
    snprintf(ret, bufsz, "[server:%3d] ", id);

    return std::string(ret);
  }

  ////////////////////////////////////////

  // util methods ////////////////////////

  static inline header_v_t
  get_cors_headers(std::string_view req_allow_headers) {
    std::string allow_headers = cors_default_allow_headers;

    if (!req_allow_headers.empty()) {
      allow_headers += ',';
      allow_headers += req_allow_headers;
    }

    return {
        {"Access-Control-Allow-Methods", "HEAD, GET, POST, OPTIONS"},
        {"Access-Control-Allow-Headers", allow_headers},
        {"Access-Control-Allow-Credentials", "true"},
        // other security headers
        {"X-Content-Type-Options", "nosniff"},
        {"X-XSS-Protection", "1; mode=block"},
    };
  }

  // somewhat of a middleware ////////////
  header_v_t
  cors(uws_response_t *res, uws_request_t *req,
       const header_v_t &additional_headers = cors_default_additional_headers) {
    std::string_view origin = req->getHeader("origin");

    bool has_origin = !origin.empty();

#ifdef REQUIRE_ORIGIN_HEADER
    if (!has_origin) {
      auto m = req->getMethod();
      if (m != "get" && m != "head") {
        res->writeStatus(http_status_t.BAD_REQUEST_400);
        res->end("Missing Origin header");

        return {};
      }

      // origin = "*";
      // has_origin = true;
    }
#endif // REQUIRE_ORIGIN_HEADER

    std::string_view host = req->getHeader("host");

    bool has_host = !host.empty();

    bool allow =
        has_origin ? has_host ? (host.find(origin) == 0) : origin == "*" : true;

    if (!allow) {
      for (const std::string &s : conf.cors_enabled_origins) {
        if (origin != s)
          continue;

        allow = true;
        break;
      }
    }

    if (!allow) {
      res->writeStatus(http_status_t.FORBIDDEN_403);
      res->end("Disallowed Origin");
      return {};
    }

    header_v_t headers = {};
    if (has_origin) {
      headers.push_back({"Access-Control-Allow-Origin", std::string(origin)});
    }

    std::string_view req_allow_headers =
        req->getHeader("access-control-request-headers");

    for (const std::pair<std::string, std::string> &s :
         get_cors_headers(req_allow_headers)) {
      headers.push_back(s);
    }

    for (const std::pair<std::string, std::string> &s : additional_headers) {
      headers.push_back(s);
    }

    return headers;
  }

  static inline void set_content_type_json(uws_response_t *res) {
    res->writeHeader(header_key_t.content_type, content_type_t.json);
  }

  static inline void set_content_type_json(http_response_t &hres) {
    hres.headers.emplace_back(header_key_t.content_type, content_type_t.json);
  }

  static inline void write_headers(uws_response_t *res,
                                   const header_v_t &headers) {
    for (const std::pair<std::string, std::string> &s : headers) {
      res->writeHeader(s.first, s.second);
    }
  }

  static inline nlohmann::json parse_json_body(const std::string &body,
                                               http_response_t &hres) {
    if (body.empty()) {
      set_content_type_json(hres);
      hres.set_status(http_status_t.BAD_REQUEST_400);
      hres.set_data(json_response::error(69, "Empty body"));
      return nullptr;
    }

    try {
      return nlohmann::json::parse(body);
    } catch (std::exception &e) {
      log::io() << DEBUG_WHERE << e.what() << "\n";

      set_content_type_json(hres);
      hres.set_status(http_status_t.BAD_REQUEST_400);
      hres.set_data(json_response::error(69, "Malformed body"));

      return nullptr;
    }
  }

  static inline void
  res_handle_body(uws_response_t *res,
                  std::function<void(const std::string &)> &&handler) {
    res->onData([handler, body = std::make_unique<std::string>()](
                    std::string_view chunk, bool is_last) {
      body->append(chunk);

      if (!is_last)
        return;

      handler(*body);
    });
  }

  /**
   * @brief Parse payload to cache data, payload format:
   * - `key`: (string) The unique identifier for the cache entry.
   * - `ttl`: (number) Time-to-live for the cache entry, a duration
   *                   in millisecond eg. 600000 for 10 minutes.
   * - `value`: (string) The data to store in the cache.
   *
   * `key` and `value` must not be empty.
   * If `ttl` is empty then the cache will live forever until the end of the
   * universe.
   */
  static inline std::pair<std::string, cache::data_t>
  parse_to_cache_data(const nlohmann::json &payload, uint64_t ttl_base = 0) {
    if (!payload.is_object())
      throw http_error_t("Malformed data");

    cache::data_t ret;
    std::string key;

    auto ik = payload.find("key");
    if (ik == payload.end() || !ik->is_string() ||
        (key = ik->get<std::string>()).empty()) {
      throw http_error_t("Invalid key");
    }

    auto iv = payload.find("value");
    if (iv == payload.end() || !iv->is_string() ||
        (ret.value = iv->get<std::string>()).empty()) {
      throw http_error_t("Invalid value");
    }

    auto it = payload.find("ttl");
    if (it != payload.end()) {
      if (!it->is_number_unsigned())
        throw http_error_t("Invalid ttl");

      uint64_t ttl = it->get<uint64_t>();

      if (ttl > 0) {
        if (ttl_base == 0)
          ttl_base = util::get_current_ts();

        ret.expires_at = ttl_base + ttl;
      }
    }

    return {key, ret};
  }

  ////////////////////////////////////////

  // util json_response //////////////////

  struct json_response {
    static inline nlohmann::json create_payload(bool s, int c,
                                                const nlohmann::json &d) {
      return {{"success", s}, {"code", c}, {"data", d}};
    }

    static inline nlohmann::json success(const nlohmann::json &d) {
      return create_payload(true, 0, d);
    }

    static inline nlohmann::json error(int c, const std::string &msg) {
      return create_payload(false, c, {{"message", msg}});
    }
  };

  ////////////////////////////////////////

  struct http_handlers {
    static inline int get_cache(http_response_t &hres,
                                const std::string &str_key, sqlite3 *db_conn) {
      auto cached = cache::get(str_key);
      if (!cached.cached()) {
        // key is not in cache
        // try to find it in db and cache it
        cached = db::get_cache(db_conn, str_key);

        auto eat = cached.get_expires_at();
        if (eat != 0) {
          // schedule deletion
          db::delete_cache(str_key, eat);

          // don't response with expired cache
          if (eat <= util::get_current_ts())
            cached.clear();
        }

        if (cached.empty()) {
          cached.mark_cached();
        }

        cache::set(str_key, cached);
      }

      if (cached.expires_at == 1) {
        // cache not found
        hres.set_status(http_status_t.NOT_FOUND_404);
        return 2;
      }

      set_content_type_json(hres);
      hres.set_data(json_response::success(cached.to_json()));
      return 0;
    }

    static inline int
    post_cache(uws_response_t *res, header_v_t &cors_headers,
               endpoint_bench_t &bench,
               post_cache_custom_handler_fn custom_handler = nullptr) {
      bench.cancel();

      auto handle_body = [res, cors_headers, custom_handler,
                          bench](const std::string &body) {
        endpoint_bench_t newbench{bench};
        newbench.cancel(false);

        http_response_t hres(res, cors_headers);

        nlohmann::json body_json = parse_json_body(body, hres);
        if (body_json.is_null())
          return;

        cache_data_t data;

        try {
          data = parse_to_cache_data(body_json);

          // log::io() << DEBUG_WHERE << "data.first(" << data.first
          //           << ") data.second(" << data.second.to_json_str(2) <<
          //           ")\n";

        } catch (http_error_t &e) {
          log::io() << DEBUG_WHERE << "parse_to_cache_data(): " << e.what()
                    << "\n";

          set_content_type_json(hres);
          hres.set_status(http_status_t.BAD_REQUEST_400);
          hres.set_data(json_response::error(69, std::string(e.what())));
          return;
        } catch (std::exception &e) {
          log::io() << DEBUG_WHERE << "parse_to_cache_data(): " << e.what()
                    << "\n";

          hres.set_status(http_status_t.INTERNAL_SERVER_ERROR_500);
          return;
        }

        if (custom_handler && custom_handler(hres, data))
          return;

        // - Sets cache in mem
        // - Schedules query to set cache in db
        // - Mark skip all previous query with the same key
        cache::set(data.first, data.second);
        db::set_cache(data.first, data.second);

        set_content_type_json(hres);
        hres.set_data(json_response::success(data.second.to_json()));
      };

      res_handle_body(res, std::move(handle_body));

      res->onAborted([]() {
        // nothing to do??
      });

      return 0;
    }
  };
};

////////////////////////////////////////

} // namespace ssplus_cache_me::server

#undef _SF_SOURCE_FILE_

#endif // SERVER_H
