"""How the third-party libraries' module of modules/ is built and used."""

# What every module unit compiles with: the sandbox of an importer holds no
# module sources, so the BMI carries them, and a reduced BMI drops the
# global module fragment's declarations the purview never names, partial
# specializations among them.
MODULE_COPTS = [
    "-Xclang",
    "-fmodules-embed-all-files",
    "-fno-modules-reduced-bmi",
]

# The libraries whose headers deps.cppm wraps, as `clice modularize` found
# them; which target provides an include root is the build's knowledge.
LIBRARIES = [
    "@cpptrace",
    "@kotatsu//:async",
    "@kotatsu//:cli",
    "@kotatsu//:cli_table",
    "@kotatsu//:codec_json",
    "@kotatsu//:fs",
    "@kotatsu//:ipc",
    "@kotatsu//:ipc_codec",
    "@kotatsu//:ipc_codec_json",
    "@kotatsu//:ipc_json",
    "@kotatsu//:meta",
    "@kotatsu//:support",
    "@kotatsu//:sys",
    "@kotatsu//:zest",
    "@spdlog",
]
