# BlokusZero: network inputs, inference, and (later) search and self-play.
# The Python training code is in zero/train/, outside Bazel.

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")
load("@protobuf//bazel:cc_proto_library.bzl", "cc_proto_library")
load("@protobuf//bazel:proto_library.bzl", "proto_library")

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

cc_library(
    name = "policy_ai",
    srcs = ["policy_ai.cc"],
    hdrs = ["policy_ai.h"],
    deps = [
        ":evaluator",
        "//game",
        "//game:board",
        "//game:player",
    ],
)

cc_test(
    name = "policy_ai_test",
    srcs = ["policy_ai_test.cc"],
    data = ["testdata/tiny.onnx"],
    deps = [
        ":evaluator",
        ":policy_ai",
        "//ai:random",
        "//game",
        "//game:board",
        "//game:game_runner",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "search",
    srcs = ["search.cc"],
    hdrs = ["search.h"],
    deps = [
        ":evaluator",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/log:check",
    ],
)

cc_test(
    name = "search_test",
    srcs = ["search_test.cc"],
    data = ["testdata/tiny.onnx"],
    deps = [
        ":evaluator",
        ":search",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
        "@googletest//:gtest_main",
    ],
)

cc_library(
    name = "zero_ai",
    srcs = ["zero_ai.cc"],
    hdrs = ["zero_ai.h"],
    deps = [
        ":evaluator",
        ":search",
        "//game",
        "//game:board",
        "//game:player",
    ],
)

cc_test(
    name = "zero_ai_test",
    srcs = ["zero_ai_test.cc"],
    data = ["testdata/tiny.onnx"],
    deps = [
        ":evaluator",
        ":zero_ai",
        "//ai:random",
        "//game:game_runner",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status:statusor",
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

proto_library(
    name = "training_proto",
    srcs = ["training.proto"],
)

cc_proto_library(
    name = "training_cc_proto",
    deps = [":training_proto"],
)

cc_library(
    name = "shard",
    srcs = ["shard.cc"],
    hdrs = ["shard.h"],
    deps = [
        ":features",
        ":training_cc_proto",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/types:span",
        "@bagz",
    ],
)

cc_test(
    name = "shard_test",
    srcs = ["shard_test.cc"],
    deps = [
        ":features",
        ":shard",
        ":training_cc_proto",
        "//game",
        "//game:board",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/types:span",
        "@googletest//:gtest_main",
    ],
)

cc_binary(
    name = "generate",
    srcs = ["generate_main.cc"],
    deps = [
        ":shard",
        ":training_cc_proto",
        "//ai:mcts",
        "//game",
        "@abseil-cpp//absl/flags:flag",
        "@abseil-cpp//absl/flags:parse",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/log:globals",
        "@abseil-cpp//absl/log:initialize",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/time",
    ],
)
