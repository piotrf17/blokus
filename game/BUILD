# Game logic and data structures.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "board",
    srcs = ["board.cc"],
    hdrs = ["board.h"],
    deps = [
        ":defs",
        ":tile",
  	"@com_google_absl//absl/container:flat_hash_set",
  	"@com_google_absl//absl/strings",
	"@com_google_absl//absl/strings:str_format",
	"@com_google_glog//:glog",
    ],
)

cc_test(
    name = "board_test",
    srcs = ["board_test.cc"],
    deps = [
     	":board",
   	"@com_google_absl//absl/strings",
   	"@com_google_glog//:glog",
	"@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "defs",
    srcs = ["defs.cc"],
    hdrs = ["defs.h"],
    deps = [
        "@com_google_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "game",
    srcs = ["game.cc"],
    hdrs = ["game.h"],
    deps = [
        ":board",
    ],
)

cc_library(
    name = "game_runner",
    srcs = ["game_runner.cc"],
    hdrs = ["game_runner.h"],
    deps = [
        ":game",
	":player",
    ],
)

cc_library(
    name = "tile",
    srcs = ["tile.cc"],
    hdrs = ["tile.h"],
    deps = [
        ":defs",
    	"@com_google_glog//:glog",
    ]
)

cc_test(
    name = "tile_test",
    srcs = ["tile_test.cc"],
    deps = [
        ":tile",
	"@com_google_glog//:glog",
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
