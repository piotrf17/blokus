# Blokus AI

Experiments with creating an AI for the game Blokus.

## Building

This project is built with bazel. Most dependencies are specified as
http_archive dependencies in the WORKSPACE file, but there are a few
external dependencies. These can be installed (on Debian/Ubuntu) via:

```bash
$ sudo apt-get install libjsoncpp-dev libmicrohttpd-dev libgoogle-perftools-dev
```

Note that perftools is only required if running benchmarks.

Afterwards, to try playing the AI, use:

```bash
$ bazel build -c opt main:blokus
$ bazel-bin/main/blokus --alsologtostderr
```

and follow the provided link to the server.
