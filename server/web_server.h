#ifndef BLOKUS_SERVER_WEB_SERVER_H_
#define BLOKUS_SERVER_WEB_SERVER_H_

#include <memory>
#include <string>

#include "absl/time/time.h"
#include "httplib.h"

#include "server/game_manager.h"

namespace blokus {

// Serves the game API and the web client over HTTP.
//
// API (JSON in and out; errors are {"error": "..."} with status 400 for a bad
// request, 404 for an unknown game, and 409 for a stale version or a move
// out of turn):
//   GET    /api/presets               AI presets (PresetsToJson)
//   GET    /api/tiles                 tile shapes (TilesToJson)
//   GET    /api/games                 summaries, most recent first
//   POST   /api/games                 new game from a config
//                                     (GameConfigFromJson); returns its
//                                     state, with status 201
//   GET    /api/games/<id>            state (GameSession::State). With
//                                     ?wait_version=N, waits (up to
//                                     `long_poll_timeout`) for a version
//                                     after N
//   DELETE /api/games/<id>
//   GET    /api/games/<id>/legal_moves
//   POST   /api/games/<id>/moves      {"version": N, "tile": t, "cells": [...]}
//                                     or {"version": N, "pass": true};
//                                     returns the new state
//   POST   /api/games/<id>/undo       {"version": N}; returns the new state
//
// Web client, from `static_dir`:
//   GET /               index.html
//   GET /game/<id>      game.html
//   GET /static/...     any file
class WebServer {
 public:
  struct Options {
    std::string static_dir = "server/static";
    absl::Duration long_poll_timeout = absl::Seconds(25);
  };

  // `manager` must outlive the server.
  WebServer(GameManager* manager, Options options);

  // Serves on `host`:`port` until Stop(). Returns false if it can't listen.
  bool Listen(const std::string& host, int port);

  // Like Listen, but on any free port, which it returns, or -1 on failure.
  // Call ListenAfterBind to serve.
  int BindToAnyPort(const std::string& host);
  bool ListenAfterBind();

  // Stops serving. Safe to call from any thread.
  void Stop();

 private:
  void AddRoutes();

  GameManager* const manager_;
  const Options options_;
  // The responses that never change.
  const std::string presets_;
  const std::string tiles_;
  httplib::Server server_;
};

}  // namespace blokus

#endif  // BLOKUS_SERVER_WEB_SERVER_H_
