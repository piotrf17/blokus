# Various utilities.

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "http_server",
    srcs = ["http_server.cc"],
    hdrs = ["http_server.h"],
    deps = [
        ":stringpiece",
    ],
)
  

# TODO(piotrf): start using absl::string_view.
cc_library(
    name = "stringpiece",
    srcs = ["stringpiece.cc"],
    hdrs = ["stringpiece.h"],
)