# The web server for playing games in a browser.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "json",
    srcs = ["json.cc"],
    hdrs = ["json.h"],
    deps = [
        "//game",
        "//game:board",
        "//game:defs",
        "//game:tile",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "json_test",
    srcs = ["json_test.cc"],
    deps = [
        ":json",
        "//game",
        "//game:board",
        "//game:tile",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "config",
    srcs = ["config.cc"],
    hdrs = ["config.h"],
    deps = [
        "//ai:mcts",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "config_test",
    srcs = ["config_test.cc"],
    deps = [
        ":config",
        "@abseil-cpp//absl/status",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "game_session",
    srcs = ["game_session.cc"],
    hdrs = ["game_session.h"],
    deps = [
        ":config",
        ":json",
        "//ai:mcts",
        "//game",
        "//game:player",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/time",
        "@nlohmann_json//:json",
    ],
)

cc_library(
    name = "test_ai",
    testonly = True,
    hdrs = ["test_ai.h"],
    deps = [
        ":game_session",
        "//game",
        "//game:player",
    ],
)

cc_test(
    name = "game_session_test",
    srcs = ["game_session_test.cc"],
    deps = [
        ":game_session",
        ":test_ai",
        "//game:board",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/time",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "game_manager",
    srcs = ["game_manager.cc"],
    hdrs = ["game_manager.h"],
    deps = [
        ":config",
        ":game_session",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@nlohmann_json//:json",
    ],
)

cc_test(
    name = "game_manager_test",
    srcs = ["game_manager_test.cc"],
    deps = [
        ":game_manager",
        ":test_ai",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/time",
        "@googletest//:gtest_main",
    ],
)
