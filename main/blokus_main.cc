// Serves Blokus games in the browser. Run from the repo root, so that it
// finds the web client in server/static:
//
//   bazel build -c opt //main:blokus && bazel-bin/main/blokus
//
// then open http://localhost:7777.
#include <pthread.h>

#include <csignal>
#include <string>
#include <thread>

#include "absl/flags/flag.h"
#include "absl/flags/parse.h"
#include "absl/log/check.h"
#include "absl/log/flags.h"
#include "absl/log/globals.h"
#include "absl/log/initialize.h"
#include "absl/log/log.h"

#include "server/game_manager.h"
#include "server/web_server.h"

ABSL_FLAG(std::string, host, "127.0.0.1",
          "Address to listen on. Use 0.0.0.0 to accept connections from "
          "other machines.");
ABSL_FLAG(int, port, 7777, "Port to listen on.");
ABSL_FLAG(std::string, data_dir, "games",
          "Directory to save games in, one JSON file each.");
ABSL_FLAG(std::string, static_dir, "server/static",
          "Directory with the web client's files.");

int main(int argc, char** argv) {
  // Show INFO logs by default. --stderrthreshold overrides this.
  absl::SetStderrThreshold(absl::LogSeverityAtLeast::kInfo);
  absl::ParseCommandLine(argc, argv);
  absl::InitializeLog();

  // Handle SIGINT and SIGTERM on a thread of their own, so that shutdown can
  // stop the server cleanly. Block them before starting any other threads,
  // which inherit the mask.
  sigset_t signals;
  sigemptyset(&signals);
  sigaddset(&signals, SIGINT);
  sigaddset(&signals, SIGTERM);
  CHECK_EQ(pthread_sigmask(SIG_BLOCK, &signals, nullptr), 0);

  blokus::GameManager manager({.data_dir = absl::GetFlag(FLAGS_data_dir)});
  blokus::WebServer server(&manager,
                           {.static_dir = absl::GetFlag(FLAGS_static_dir)});

  std::thread([&] {
    int signal;
    sigwait(&signals, &signal);
    LOG(INFO) << "Got signal " << signal << ", shutting down";
    // Closing the games first ends any long-polls, so the server can stop
    // without waiting for them.
    manager.CloseGames();
    server.Stop();
  }).detach();

  const std::string host = absl::GetFlag(FLAGS_host);
  const int port = absl::GetFlag(FLAGS_port);
  LOG(INFO) << "Serving on http://" << host << ":" << port;
  if (!server.Listen(host, port)) {
    LOG(ERROR) << "Couldn't listen on " << host << ":" << port;
    return 1;
  }
  return 0;
}
