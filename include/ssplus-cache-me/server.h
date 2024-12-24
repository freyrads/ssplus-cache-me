#ifndef SERVER_H
#define SERVER_H

#include "nlohmann/json.hpp"
#include "ssplus-cache-me/cache.h"
#include "ssplus-cache-me/db.h"
#include "ssplus-cache-me/debug.h"
#include "ssplus-cache-me/log.h"
#include "ssplus-cache-me/server_config.h"
#include "uWebSockets/src/App.h"
#include <sqlite3.h>
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

  // http_response_t /////////////////////

  struct http_response_t {
    uws_response_t *res;
    const char *status;

    header_v_t headers;
    std::string data;

    http_response_t() : res(nullptr), status(http_status_t.OK_200) {}

    explicit http_response_t(uws_response_t *_res)
        : res(_res), status(http_status_t.OK_200) {}

    explicit http_response_t(uws_response_t *_res, const header_v_t &_headers)
        : res(_res), status(http_status_t.OK_200), headers(_headers) {}

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
  };

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
    sapp = new uWS::TemplatedApp<WITH_SSL>{};

    register_routes();

    sapp->listen(conf.port, [this](us_listen_socket_t *listen_socket) {
      if (listen_socket) {
        slisten_socket = listen_socket;
        log::io() << get_id_for_log() << "Listening on port " << conf.port
                  << "\n";
      } else
        log::io() << DEBUG_WHERE << get_id_for_log()
                  << "Listening socket is null\n";
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
      auto cors_headers = cors(res, req,
                               {
                                   {"Access-Control-Max-Age", CORS_VALID_FOR},
                               });

      if (cors_headers.empty())
        return;

      res->writeStatus(http_status_t.NO_CONTENT_204);
      write_headers(res, cors_headers);
      res->end();
    };

    sapp->any("/*", any_any);
    sapp->options("/*", options_cors);
    sapp->head("/*", options_cors);

    auto get_cache = [this](uws_response_t *res, uws_request_t *req) {
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

      auto cached = cache::get(str_key);
      if (!cached.cached()) {
        // key is not in cache
        // try to find it in db and cache it
        cached = db::get_cache(db_conn, str_key);

        if (cached.empty()) {
          cached.mark_cached();
        }

        auto eat = cached.get_expires_at();
        if (eat != 0) {
          // schedule deletion
          db::delete_cache(str_key, eat);
        }

        cache::set(str_key, cached);
      }

      if (cached.expires_at == 1) {
        // cache not found
        hres.set_status(http_status_t.NOT_FOUND_404);
        return;
      }

      set_content_type_json(hres);
      hres.set_data(json_response::success(cached.to_json()));
    };

    auto post_cache = [this](uws_response_t *res, uws_request_t *req) {
      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      std::shared_ptr<std::string> body = std::make_shared<std::string>();

      res->onData(
          [res, cors_headers, body](std::string_view chunk, bool is_last) {
            body->append(chunk);

            if (!is_last)
              return;

            // handle body
            http_response_t hres(res, cors_headers);

            nlohmann::json body_json = parse_json_body(*body, hres);
            if (body_json.is_null())
              return;

            // TODO
            log::io() << DEBUG_WHERE << body_json.dump(2) << "\n";
          });

      res->onAborted([]() {
        // nothing to do??
      });
    };

    auto get_post_cache = [this](uws_response_t *res, uws_request_t *req) {
      auto cors_headers = cors(res, req);
      if (cors_headers.empty())
        return;

      http_response_t hres(res, cors_headers);
    };

    auto delete_cache = [this](uws_response_t *res, uws_request_t *req) {
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

  static inline nlohmann::json parse_json_body(std::string &body,
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
};

////////////////////////////////////////

} // namespace ssplus_cache_me::server

#undef _SF_SOURCE_FILE_

#endif // SERVER_H
