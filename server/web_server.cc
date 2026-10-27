#include "server/web_server.h"

#include <exception>
#include <fstream>
#include <sstream>
#include <utility>

#include "absl/log/log.h"
#include "absl/status/status.h"
#include "absl/strings/numbers.h"
#include "nlohmann/json.hpp"

#include "server/config.h"
#include "server/json.h"

namespace blokus {
namespace {

using ::nlohmann::json;

// Long-polls hold a thread each, so allow plenty.
constexpr int kNumThreads = 32;

void SendJson(httplib::Response& res, const json& body, int status = 200) {
  res.status = status;
  res.set_content(body.dump(), "application/json");
}

void SendError(httplib::Response& res, int status, const std::string& error) {
  SendJson(res, {{"error", error}}, status);
}

void SendStatus(httplib::Response& res, const absl::Status& status) {
  switch (status.code()) {
    case absl::StatusCode::kInvalidArgument:
      return SendError(res, 400, std::string(status.message()));
    case absl::StatusCode::kNotFound:
      return SendError(res, 404, std::string(status.message()));
    case absl::StatusCode::kFailedPrecondition:
      return SendError(res, 409, std::string(status.message()));
    default:
      return SendError(res, 500, std::string(status.message()));
  }
}

// Parses the request body as a JSON object, or sends a 400 and returns a
// discarded value.
json ParseBody(const httplib::Request& req, httplib::Response& res) {
  json body = json::parse(req.body, nullptr, /*allow_exceptions=*/false);
  if (body.is_discarded() || !body.is_object()) {
    SendError(res, 400, "The body must be a JSON object");
    return json(json::value_t::discarded);
  }
  return body;
}

// Reads the body's "version", or sends a 400 and returns false.
bool ReadVersion(const json& body, httplib::Response& res, int* version) {
  if (!body.contains("version") || !body["version"].is_number_integer()) {
    SendError(res, 400, "The body needs an integer \"version\"");
    return false;
  }
  *version = body["version"].get<int>();
  return true;
}

void SendFile(httplib::Response& res, const std::string& path,
              const char* content_type) {
  std::ifstream file(path);
  if (!file) {
    res.status = 404;
    res.set_content("Not found", "text/plain");
    return;
  }
  std::stringstream contents;
  contents << file.rdbuf();
  res.set_content(contents.str(), content_type);
}

}  // namespace

WebServer::WebServer(GameManager* manager, Options options)
    : manager_(manager),
      options_(std::move(options)),
      presets_(PresetsToJson().dump()),
      tiles_(TilesToJson().dump()) {
  server_.new_task_queue = [] { return new httplib::ThreadPool(kNumThreads); };
  server_.set_exception_handler(
      [](const httplib::Request& req, httplib::Response& res,
         std::exception_ptr exception) {
        std::string what = "unknown exception";
        try {
          std::rethrow_exception(exception);
        } catch (const std::exception& e) {
          what = e.what();
        } catch (...) {
        }
        LOG(ERROR) << req.method << " " << req.path << ": " << what;
        SendError(res, 500, "Internal error");
      });
  AddRoutes();
}

bool WebServer::Listen(const std::string& host, int port) {
  return server_.listen(host, port);
}

int WebServer::BindToAnyPort(const std::string& host) {
  return server_.bind_to_any_port(host);
}

bool WebServer::ListenAfterBind() { return server_.listen_after_bind(); }

void WebServer::Stop() { server_.stop(); }

void WebServer::AddRoutes() {
  // A game's id in a URL path.
  static constexpr char kId[] = "([0-9a-f]+)";
  using Req = const httplib::Request&;
  using Res = httplib::Response&;

  server_.Get("/api/presets", [this](Req, Res res) {
    res.set_content(presets_, "application/json");
  });
  server_.Get("/api/tiles", [this](Req, Res res) {
    res.set_content(tiles_, "application/json");
  });

  server_.Get("/api/games", [this](Req, Res res) {
    SendJson(res, manager_->ListGames());
  });
  server_.Post("/api/games", [this](Req req, Res res) {
    const json body = ParseBody(req, res);
    if (body.is_discarded()) return;
    absl::StatusOr<std::shared_ptr<GameSession>> game =
        manager_->CreateGame(body);
    if (!game.ok()) return SendStatus(res, game.status());
    SendJson(res, (*game)->State(), 201);
  });

  // Returns the game named in the path, or sends a 404 and returns null.
  auto find_game = [this](Req req, Res res) {
    std::shared_ptr<GameSession> game = manager_->GetGame(req.matches[1]);
    if (game == nullptr) SendError(res, 404, "No such game");
    return game;
  };

  server_.Get(std::string("/api/games/") + kId, [this, find_game](Req req,
                                                                  Res res) {
    const std::shared_ptr<GameSession> game = find_game(req, res);
    if (game == nullptr) return;
    if (!req.has_param("wait_version")) return SendJson(res, game->State());
    int version;
    if (!absl::SimpleAtoi(req.get_param_value("wait_version"), &version)) {
      return SendError(res, 400, "\"wait_version\" must be an integer");
    }
    SendJson(res, game->WaitForState(version, options_.long_poll_timeout));
  });
  server_.Delete(std::string("/api/games/") + kId, [this](Req req, Res res) {
    const absl::Status status = manager_->DeleteGame(req.matches[1]);
    if (!status.ok()) return SendStatus(res, status);
    SendJson(res, json::object());
  });
  server_.Get(std::string("/api/games/") + kId + "/legal_moves",
              [find_game](Req req, Res res) {
                const std::shared_ptr<GameSession> game =
                    find_game(req, res);
                if (game == nullptr) return;
                SendJson(res, game->LegalMoves());
              });
  server_.Get(std::string("/api/games/") + kId + "/position",
              [find_game](Req req, Res res) {
                const std::shared_ptr<GameSession> game =
                    find_game(req, res);
                if (game == nullptr) return;
                int num_moves;
                if (!absl::SimpleAtoi(req.get_param_value("moves"),
                                      &num_moves)) {
                  return SendError(res, 400, "\"moves\" must be an integer");
                }
                absl::StatusOr<json> position = game->Position(num_moves);
                if (!position.ok()) return SendStatus(res, position.status());
                SendJson(res, *position);
              });
  server_.Post(std::string("/api/games/") + kId + "/moves",
               [find_game](Req req, Res res) {
                 const std::shared_ptr<GameSession> game =
                     find_game(req, res);
                 if (game == nullptr) return;
                 const json body = ParseBody(req, res);
                 int version;
                 if (body.is_discarded() || !ReadVersion(body, res, &version)) {
                   return;
                 }
                 const absl::Status status = game->PlayMove(version, body);
                 if (!status.ok()) return SendStatus(res, status);
                 SendJson(res, game->State());
               });
  server_.Post(std::string("/api/games/") + kId + "/undo",
               [find_game](Req req, Res res) {
                 const std::shared_ptr<GameSession> game =
                     find_game(req, res);
                 if (game == nullptr) return;
                 const json body = ParseBody(req, res);
                 int version;
                 if (body.is_discarded() || !ReadVersion(body, res, &version)) {
                   return;
                 }
                 const absl::Status status = game->Undo(version);
                 if (!status.ok()) return SendStatus(res, status);
                 SendJson(res, game->State());
               });

  // The web client.
  server_.Get("/", [this](Req, Res res) {
    SendFile(res, options_.static_dir + "/index.html", "text/html");
  });
  server_.Get(std::string("/game/") + kId, [this](Req, Res res) {
    SendFile(res, options_.static_dir + "/game.html", "text/html");
  });
  if (!server_.set_mount_point("/static", options_.static_dir)) {
    LOG(WARNING) << "Static directory " << options_.static_dir
                 << " doesn't exist";
  }
}

}  // namespace blokus
