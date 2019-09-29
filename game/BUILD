# Game logic and data structures.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "board",
    srcs = ["board.cc"],
    hdrs = ["board.h"],
    deps = [
        ":tile",
  	"@com_google_absl//absl/strings",
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
)

cc_library(
    name = "player",
    hdrs = ["player.h"],
    deps = [
        ":game",
    ],
)