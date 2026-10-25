# Blokus AI

Experiments with creating an AI for the game Blokus.

## Building

This project is built with [bazelisk](https://github.com/bazelbuild/bazelisk)
(installed as `bazel`), which downloads the Bazel version pinned in
`.bazelversion`. Most dependencies are Bazel modules listed in `MODULE.bazel`,
but a few are system libraries. These can be installed (on Debian/Ubuntu) via:

```bash
$ sudo apt-get install libjsoncpp-dev libmicrohttpd-dev libgoogle-perftools-dev
```

Note that perftools is only required if running benchmarks.

Afterwards, to try playing the AI, use:

```bash
$ bazel build -c opt main:blokus
$ bazel-bin/main/blokus --stderrthreshold=0
```

and open http://localhost:7777 in a browser. Run the binary from the repo
root, because it serves the web client files by relative path.

To run the tests:

```bash
$ bazel test //...
```
