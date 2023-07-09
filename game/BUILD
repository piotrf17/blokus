# Game logic and data structures.

package(default_visibility = ["//visibility:public"])

load("@rules_proto//proto:defs.bzl", "proto_library")

cc_library(
    name = "board",
    srcs = ["board.cc"],
    hdrs = ["board.h"],
    deps = [
        ":defs",
        ":tile",
        "@com_google_absl//absl/log",
  	    "@com_google_absl//absl/strings",
	    "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "board_test",
    srcs = ["board_test.cc"],
    deps = [
     	":board",
       	"@com_google_absl//absl/strings",
	    "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "defs",
    srcs = ["defs.cc"],
    hdrs = ["defs.h"],
    deps = [
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "logger",
    srcs = ["logger.cc"],
    hdrs = ["logger.h"],
    deps = [
         ":game_log_cc_proto",
    	 ":game_runner",
         "@com_google_absl//absl/log:check",
    ],
)

cc_library(
    name = "game",
    srcs = ["game.cc"],
    hdrs = ["game.h"],
    deps = [
        ":board",
        "@com_google_absl//absl/log:check",
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
        "@com_google_absl//absl/log:check",
    ],
)

cc_library(
    name = "tile",
    srcs = ["tile.cc"],
    hdrs = ["tile.h"],
    deps = [
        ":defs",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
      	"@com_google_absl//absl/types:span",
    ]
)

cc_test(
    name = "tile_test",
    srcs = ["tile_test.cc"],
    deps = [
        ":tile",
	    "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "player",
    hdrs = ["player.h"],
    deps = [
        ":game",
    ],
)
