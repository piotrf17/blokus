# BlokusZero: network inputs, inference, and (later) search and self-play.
# The Python training code is in zero/train/, outside Bazel.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "features",
    srcs = ["features.cc"],
    hdrs = ["features.h"],
    deps = [
        "//game",
        "//game:board",
        "//game:defs",
        "//game:tile",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/types:span",
    ],
)

cc_test(
    name = "features_test",
    srcs = ["features_test.cc"],
    deps = [
        ":features",
        "//game",
        "//game:board",
        "//game:defs",
        "//game:tile",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "evaluator",
    srcs = ["evaluator.cc"],
    hdrs = ["evaluator.h"],
    deps = [
        ":features",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@onnxruntime",
    ],
)

cc_test(
    name = "evaluator_test",
    srcs = ["evaluator_test.cc"],
    data = [
        "testdata/tiny.onnx",
        "testdata/tiny_expected.txt",
    ],
    deps = [
        ":evaluator",
        ":features",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "evaluator_benchmark",
    srcs = ["evaluator_benchmark.cc"],
    deps = [
        ":evaluator",
        ":features",
        "//game",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/types:span",
    ],
)
