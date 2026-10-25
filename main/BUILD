# Binaries.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//ai:mcts",
        "//ai:random",
        "//game:game_runner",
        "//util:http_server",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/memory",
          "@abseil-cpp//absl/strings",
    ],
    linkopts = ["-lmicrohttpd -ljsoncpp"],
)

cc_binary(
    name = "train",
    srcs = ["train_main.cc"],
    deps = [
        "//ai:mcts",
        "//ai:random",
        "//game:game_runner",
        "//game:logger",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",        
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/memory",
        "@abseil-cpp//absl/time",
    ],
    linkopts = ["-lprofiler"],
)