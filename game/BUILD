# Game logic and data structures.

load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "board",
    srcs = ["board.cc"],
    hdrs = ["board.h"],
    deps = [
        ":defs",
        ":tile",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
          "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "board_test",
    srcs = ["board_test.cc"],
    deps = [
         ":board",
           "@abseil-cpp//absl/strings",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "defs",
    srcs = ["defs.cc"],
    hdrs = ["defs.h"],
    deps = [
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/strings:str_format",
    ],
)

cc_test(
    name = "defs_test",
    srcs = ["defs_test.cc"],
    deps = [
        ":defs",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "logger",
    srcs = ["logger.cc"],
    hdrs = ["logger.h"],
    deps = [
         ":game_log_cc_proto",
         ":game_runner",
         "@abseil-cpp//absl/log:check",
    ],
)

cc_test(
    name = "logger_test",
    srcs = ["logger_test.cc"],
    deps = [
        ":game",
        ":game_log_cc_proto",
        ":game_runner",
        ":logger",
        ":player",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "game",
    srcs = ["game.cc"],
    hdrs = ["game.h"],
    deps = [
        ":board",
        "@abseil-cpp//absl/log:check",
    ],
)

cc_test(
    name = "game_test",
    srcs = ["game_test.cc"],
    deps = [
        ":game",
        "@googletest//:gtest_main",
    ],
)

proto_library(
    name = "game_log_proto",
    srcs = ["game_log.proto"],
)

cc_proto_library(
    name = "game_log_cc_proto",
    deps = [":game_log_proto"],
)

cc_library(
    name = "game_runner",
    srcs = ["game_runner.cc"],
    hdrs = ["game_runner.h"],
    deps = [
        ":game",
        ":player",
        "@abseil-cpp//absl/log:check",
    ],
)

cc_test(
    name = "game_runner_test",
    srcs = ["game_runner_test.cc"],
    deps = [
        ":game",
        ":game_runner",
        ":player",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "tile",
    srcs = ["tile.cc"],
    hdrs = ["tile.h"],
    deps = [
        ":defs",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
          "@abseil-cpp//absl/types:span",
    ]
)

cc_test(
    name = "tile_test",
    srcs = ["tile_test.cc"],
    deps = [
        ":tile",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "player",
    hdrs = ["player.h"],
    deps = [
        ":game",
    ],
)
