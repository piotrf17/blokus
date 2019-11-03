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

cc_binary(
   name = "mcts_benchmark",
   srcs = ["mcts_benchmark.cc"],
   deps = [
       ":mcts",
	"@com_google_benchmark//:benchmark",
   ],
   linkopts = ["-lglog -lgflags -lprofiler"],
)

cc_library(
    name = "random",
    srcs = ["random.cc"],
    hdrs = ["random.h"],
    deps = [
        "//game:player",
    ],
)