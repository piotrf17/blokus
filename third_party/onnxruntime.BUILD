# BUILD file for the prebuilt ONNX Runtime CPU release (see MODULE.bazel).

load("@rules_cc//cc:cc_import.bzl", "cc_import")
load("@rules_cc//cc:cc_library.bzl", "cc_library")

# The file name matches the library's SONAME, so that the dynamic linker finds
# it next to the binary.
cc_import(
    name = "onnxruntime_shared",
    shared_library = "lib/libonnxruntime.so.1",
)

cc_library(
    name = "onnxruntime",
    hdrs = glob(["include/**/*.h", "include/**/*.inc"]),
    includes = ["include"],
    visibility = ["//visibility:public"],
    deps = [":onnxruntime_shared"],
)
