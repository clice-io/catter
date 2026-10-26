"""What the root BUILD.bazel builds with: the flags of catter's sources and
programs, the hook payloads, and the release packages."""

load("@rules_cc//cc:find_cc_toolchain.bzl", "find_cc_toolchain", "use_cc_toolchain")
load("@rules_pkg//pkg:mappings.bzl", "pkg_attributes", "pkg_files")
load("@rules_pkg//pkg:tar.bzl", "pkg_tar")
load("@rules_pkg//pkg:zip.bzl", "pkg_zip")
load("@xclang//bazel:debug_symbols.bzl", "xclang_debug_symbols")
load("//modules:modules.bzl", "MODULE_COPTS")

# What catter's own sources compile with; the libraries keep their own flags.
# Every include names its header by its path under src/.
COPTS = ["-Isrc"] + select({
    "@platforms//os:linux": ["-DCATTER_LINUX"],
    "@platforms//os:macos": ["-DCATTER_MAC"],
    "@platforms//os:windows": [
        "-DCATTER_WINDOWS",
        "-DWIN32_LEAN_AND_MEAN",
        "-DNOMINMAX",
    ],
}) + select({
    "//bazel:dbg": ["-DDEBUG"],
    "//conditions:default": [],
})

# What catter's module units compile with, with a dependency on //modules
# and the cpp_modules feature.
MODULE_UNIT_COPTS = COPTS + MODULE_COPTS

# Link optimizations in optimized builds: what the cases run is what
# releases ship. icf=safe (not =all) so address-taken functions keep C++
# pointer identity.
LINKOPTS = select({
    "//bazel:opt_macos": ["-Wl,--icf=safe"],
    "//bazel:opt": [
        "-Wl,--icf=safe",
        "-Wl,-O2",
    ],
    "//conditions:default": [],
})

def _payload_transition_impl(settings, attr):
    return {
        "//command_line_option:features": [
            feature
            for feature in settings["//command_line_option:features"]
            if feature != "asan"
        ],
        "//command_line_option:platforms": [attr.platform] if attr.platform else settings["//command_line_option:platforms"],
    }

# A payload runs inside every process of the build, next to that program's
# own runtime, which has no ASan; on Windows, Detours' search of the address
# space reads ASan's shadow memory too. It and every library it links build
# without, and for the platform of its DLL.
_payload_transition = transition(
    implementation = _payload_transition_impl,
    inputs = [
        "//command_line_option:features",
        "//command_line_option:platforms",
    ],
    outputs = [
        "//command_line_option:features",
        "//command_line_option:platforms",
    ],
)

def _payload_impl(ctx):
    out = ctx.actions.declare_file(ctx.attr.out)
    ctx.actions.symlink(output = out, target_file = ctx.files.binary[0])
    return [DefaultInfo(files = depset([out]))]

_payload = rule(
    implementation = _payload_impl,
    attrs = {
        "binary": attr.label(cfg = _payload_transition, allow_files = True, mandatory = True),
        "out": attr.string(mandatory = True),
        "platform": attr.label(),
    },
)

def payload(name, binary, file, platform = None, **kwargs):
    """A hook payload: the shared library `binary`, linked without ASan.

    Two targets: `name`, bin/<file>, beside the programs, where catter-proxy
    finds it, and `<name>.stripped`, stripped/<file>, the stripped one the
    package ships. `platform` is the one the library is for, the build's
    when None: each Windows build makes the DLL of every architecture.
    """
    _payload(
        name = name,
        binary = binary,
        out = "bin/" + file,
        platform = platform,
        **kwargs
    )
    _payload(
        name = name + ".stripped",
        binary = binary + ".stripped",
        out = "stripped/" + file,
        platform = platform,
        **kwargs
    )

def _asan_runtime_impl(ctx):
    runtime = [
        file
        for file in find_cc_toolchain(ctx).all_files.to_list()
        if file.basename == ctx.attr.runtime
    ]
    out = ctx.actions.declare_file("bin/" + ctx.attr.runtime)
    ctx.actions.symlink(output = out, target_file = runtime[0])
    return [DefaultInfo(files = depset([out]))]

# The ASan runtime of the toolchain beside the programs, which an ASan
# program of macOS loads from its own directory too: the rpath the link gives
# it names the toolchain where the build ran, which the runner of the unit
# tests and the cases has not. The toolchain puts Windows' beside the
# programs itself.
asan_runtime = rule(
    implementation = _asan_runtime_impl,
    attrs = {"runtime": attr.string(mandatory = True)},
    toolchains = use_cc_toolchain(),
)

def release_packages(programs, payloads):
    """//:package and //:symbols, of the programs (target names) and payloads.

    The release archive is catter/{bin/<programs and payloads>, LICENSE},
    stripped, and the symbol package the GSYM of each program, for
    llvm-gsymutil: the programs of these builds are the ones the cases run.
    """
    pkg_files(
        name = "package_programs",
        srcs = [":%s.stripped" % program for program in programs],
        attributes = pkg_attributes(mode = "0755"),
        prefix = "catter/bin",
        renames = select({
            "@platforms//os:windows": {
                ":%s.stripped" % program: program.rpartition("/")[2] + ".exe"
                for program in programs
            },
            "//conditions:default": {
                ":%s.stripped" % program: program.rpartition("/")[2]
                for program in programs
            },
        }),
    )

    pkg_files(
        name = "package_payloads",
        srcs = payloads,
        attributes = pkg_attributes(mode = "0755"),
        prefix = "catter/bin",
    )

    pkg_files(
        name = "package_license",
        srcs = ["LICENSE"],
        prefix = "catter",
    )

    package = [
        ":package_license",
        ":package_payloads",
        ":package_programs",
    ]

    pkg_tar(
        name = "package_tar",
        srcs = package,
        extension = "tar.gz",
        package_file_name = "catter.tar.gz",
        tags = ["manual"],
    )

    pkg_zip(
        name = "package_zip",
        srcs = package,
        package_file_name = "catter.zip",
        tags = ["manual"],
    )

    native.alias(
        name = "package",
        actual = select({
            "@platforms//os:windows": ":package_zip",
            "//conditions:default": ":package_tar",
        }),
    )

    # The GSYM of each program, and the dSYM it comes from on macOS.
    # --merged-functions keeps the names of the functions ICF folded.
    for program in programs:
        xclang_debug_symbols(
            name = program + "_symbols",
            binary = ":" + program,
            gsymutil_args = ["--merged-functions"],
            tags = ["manual"],
        )

    native.filegroup(
        name = "gsym",
        srcs = [":%s_symbols" % program for program in programs],
        output_group = "gsym",
        tags = ["manual"],
    )

    pkg_tar(
        name = "symbols_tar",
        srcs = [":gsym"],
        extension = "tar.xz",
        package_file_name = "catter-symbol.tar.xz",
        tags = ["manual"],
    )

    pkg_zip(
        name = "symbols_zip",
        srcs = [":gsym"],
        package_file_name = "catter-symbol.zip",
        tags = ["manual"],
    )

    native.alias(
        name = "symbols",
        actual = select({
            "@platforms//os:windows": ":symbols_zip",
            "//conditions:default": ":symbols_tar",
        }),
    )
