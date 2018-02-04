# Binaries.

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//ai:random",
        "//game:game",
        "//util:http_server",
        "@com_google_absl//absl/memory",
    ],
    linkopts = ["-lglog -lgflags -lmicrohttpd -ljsoncpp"],
)

cc_binary(
    name = "train",
    srcs = ["train_main.cc"],
    deps = [
        "//ai:random",
        "//game:game",
        "@com_google_absl//absl/memory",
    ],
    linkopts = ["-lglog -lgflags"],
)