# Binaries.

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//ai:random",
        "//game:game_runner",
        "//util:http_server",
        "@com_google_absl//absl/memory",
  	"@com_google_absl//absl/strings",
	"@com_github_gflags_gflags//:gflags",
	"@com_google_glog//:glog",
    ],
    linkopts = ["-lmicrohttpd -ljsoncpp"],
)

cc_binary(
    name = "train",
    srcs = ["train_main.cc"],
    deps = [
        "//ai:mcts",
        "//ai:random",
        "//game:game_runner",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
        "@com_github_gflags_gflags//:gflags",
	"@com_google_glog//:glog",
    ],
    linkopts = ["-lprofiler"],
)