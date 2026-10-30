# Bagz (https://github.com/google-deepmind/bagz), which upstream builds with
# CMake. This is its C++ core with the local file system: the CMake targets
# bagz_internal, bagz_file_system, bagz_file_system_posix, bagz_file and
# bagz_core, without the GCS file system or the Python bindings.

load("@rules_cc//cc:cc_library.bzl", "cc_library")

cc_library(
    name = "bagz",
    srcs = [
        "src/bagz_index.cc",
        "src/bagz_multi_index.cc",
        "src/bagz_reader.cc",
        "src/bagz_writer.cc",
        "src/file/file.cc",
        "src/file/file_system/file_system.cc",
        "src/file/file_system/pread_file.cc",
        "src/file/file_system/shard_spec.cc",
        "src/file/file_system/string_pread_file.cc",
        "src/file/file_systems/posix/posix_file_system.cc",
        "src/file/registry/file_system_registry.cc",
        "src/file/registry/register_file_systems.cc",
        "src/internal/bagz_shard_reader.cc",
        "src/internal/limits_name.cc",
        "src/internal/parallel_do.cc",
        "src/internal/records_limits.cc",
        "src/internal/shard_indexing.cc",
        "src/internal/zstd_compressor.cc",
        "src/internal/zstd_decompressor.cc",
    ],
    hdrs = glob(
        ["src/**/*.h"],
        exclude = [
            "src/file/file_systems/gcs/**",
            "src/python/**",
        ],
    ),
    visibility = ["//visibility:public"],
    deps = [
        "@abseil-cpp//absl/algorithm:container",
        "@abseil-cpp//absl/base",
        "@abseil-cpp//absl/base:core_headers",
        "@abseil-cpp//absl/base:no_destructor",
        "@abseil-cpp//absl/base:nullability",
        "@abseil-cpp//absl/cleanup",
        "@abseil-cpp//absl/container:flat_hash_map",
        "@abseil-cpp//absl/container:inlined_vector",
        "@abseil-cpp//absl/functional:any_invocable",
        "@abseil-cpp//absl/functional:function_ref",
        "@abseil-cpp//absl/log",
        "@abseil-cpp//absl/log:absl_check",
        "@abseil-cpp//absl/log:check",
        "@abseil-cpp//absl/status",
        "@abseil-cpp//absl/status:statusor",
        "@abseil-cpp//absl/strings",
        "@abseil-cpp//absl/strings:str_format",
        "@abseil-cpp//absl/synchronization",
        "@abseil-cpp//absl/time",
        "@abseil-cpp//absl/types:span",
        "@abseil-cpp//absl/utility",
        "@zstd",
    ],
)
