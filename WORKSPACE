load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# TODO(piotrf): use an http_archive
local_repository(
  # Name of the Abseil repository. This name is defined within Abseil's
  # WORKSPACE file, in its `workspace()` metadata
  name = "com_google_absl",

  # NOTE: Bazel paths must be absolute paths. E.g., you can't use ~/Source
  path = "/home/piotrf/source/abseil-cpp",
)

http_archive(
  name = "com_google_googletest",
  url = "https://github.com/google/googletest/archive/release-1.10.0.zip",
  sha256 = "94c634d499558a76fa649edb13721dce6e98fb1e7018dfaeba3cd7a083945e91",
  strip_prefix = "googletest-release-1.10.0",
)

http_archive(
 name = "com_google_benchmark",
 url = "https://github.com/google/benchmark/archive/v1.5.0.zip",
 sha256 = "2d22dd3758afee43842bb504af1a8385cccb3ee1f164824e4837c1c1b04d92a0",
 strip_prefix = "benchmark-1.5.0",
)