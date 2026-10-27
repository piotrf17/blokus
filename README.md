# Blokus AI

Experiments with creating an AI for the game Blokus.

## Building

This project is built with [bazelisk](https://github.com/bazelbuild/bazelisk)
(installed as `bazel`), which downloads the Bazel version pinned in
`.bazelversion`. Most dependencies are Bazel modules listed in `MODULE.bazel`.
The one system library is gperftools, needed for the benchmark and self-play
binaries. On Debian/Ubuntu:

```bash
$ sudo apt-get install libgoogle-perftools-dev
```

Afterwards, to try playing the AI, use:

```bash
$ bazel build -c opt main:blokus
$ bazel-bin/main/blokus
```

and open http://localhost:7777 in a browser. Run the binary from the repo
root, because it serves the web client files by relative path. Games are
saved in `games/`.

To run the tests:

```bash
$ bazel test //...
```
