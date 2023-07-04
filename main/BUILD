# Binaries.

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//ai:mcts",
        "//ai:random",
        "//game:game_runner",
        "//util:http_server",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:flags",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/memory",
  	    "@com_google_absl//absl/strings",
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
	    "@com_google_absl//absl/flags:flag",
	    "@com_google_absl//absl/flags:parse",        
        "@com_google_absl//absl/log",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/log:flags",
        "@com_google_absl//absl/log:initialize",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/time",
    ],
    linkopts = ["-lprofiler"],
)