# Binaries.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//server:game_manager",
        "//server:web_server",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:flags",
        "@abseil-cpp//absl/log:globals",
        "@abseil-cpp//absl/log:initialize",
    ],
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

cc_binary(
    name = "match",
    srcs = ["match_main.cc"],
    deps = [
        "//ai:mcts",
        "//ai:random",
        "//game",
        "//game:game_runner",
        "//game:player",
        "//zero:evaluator",
        "//zero:policy_ai",
        "//zero:search",
        "//zero:zero_ai",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/time",
    ],
)
