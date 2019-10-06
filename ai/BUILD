# AI players.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "mcts",
    srcs = ["mcts.cc"],
    hdrs = ["mcts.h"],
    deps = [
        "//game:player",
	"@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "random",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    deps = [
        "//game:player",
    ],
)