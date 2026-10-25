# Various utilities.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "http_server",
    srcs = ["http_server.cc"],
    hdrs = ["http_server.h"],
    deps = [
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
    ],
)
