Build as CI does, so its Bazel cache hits: pixi run -e dev build RelWithDebInfo
(or Debug; user.bazelrc in ~/src points Bazel at the cache CI's main run
saved); the programs are built into build/<type>/bin/bin. Windows programs build
on a linux box, with -- --platforms=@xclang//platforms:x86_64-w64-mingw32
(aarch64-w64-mingw32 for ARM64) after the type.
