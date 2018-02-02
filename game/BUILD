# Game logic and data structures.

package(default_visibility = ["//visibility:public"])

cc_library(
   name = "board",
   srcs = ["board.cc"],
   hdrs = ["board.h"],
   deps = [
       ":tile",
   ]
)

cc_library(
    name = "game",
    srcs = ["game.cc"],
    hdrs = ["game.h"],
    deps = [
        ":board",
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
)