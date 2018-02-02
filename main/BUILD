# Binaries.

cc_binary(
    name = "blokus",
    srcs = ["blokus_main.cc"],
    deps = [
        "//game:game",
        "//util:http_server",
    ],
    linkopts = ["-lglog -lgflags -lmicrohttpd -ljsoncpp"],
)