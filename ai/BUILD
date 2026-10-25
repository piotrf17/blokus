# AI players.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mcts",
    srcs = ["mcts.cc"],
    hdrs = ["mcts.h"],
    deps = [
        "//game:player",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "mcts_test",
    srcs = ["mcts_test.cc"],
    deps = [
        ":mcts",
        "//game",
        "//game:game_runner",
        "//game:player",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
   name = "mcts_benchmark",
   srcs = ["mcts_benchmark.cc"],
   deps = [
       ":mcts",
        "@google_benchmark//:benchmark",
   ],
   linkopts = ["-lprofiler"],
)

cc_library(
    name = "random",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    deps = [
        "//game:player",
    ],
)