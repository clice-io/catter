# ripgrep

A Rust program whose dependency pcre2-sys builds C in its build script: the
cc crate compiles PCRE2's sources and archives them with `ar`, and rustc
links the build scripts and the program itself through `cc`.
