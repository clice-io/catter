set_version("0.1.0")
set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("mingw", "linux", "macosx")

set_languages("c++23")
-- Sources and narrow string literals are UTF-8, the encoding catter's strings
-- hold on every platform, as kotatsu's do.
set_encodings("utf-8")

option("dev", {default = true})
option("test", {default = false})

local prefix_includedirs = {}

local function collect_prefix_includedirs(prefix)
    local include_candidates = {
        path.join(prefix, "include"),
        path.join(prefix, "Library", "include"),
    }

    for _, includedir in ipairs(include_candidates) do
        if os.isdir(includedir) then
            table.insert(prefix_includedirs, includedir)
        end
    end
end

local function add_local_prefix_includedirs()
    for _, includedir in ipairs(prefix_includedirs) do
        add_includedirs(includedir)
    end
end

local conda_prefix = os.getenv("CONDA_PREFIX")
if conda_prefix then
    collect_prefix_includedirs(conda_prefix)
else
    local pixi_dev_prefix = path.join(os.projectdir(), ".pixi", "envs", "dev")
    local pixi_default_prefix = path.join(os.projectdir(), ".pixi", "envs", "default")

    if os.isdir(pixi_dev_prefix) then
        collect_prefix_includedirs(pixi_dev_prefix)
    elseif os.isdir(pixi_default_prefix) then
        collect_prefix_includedirs(pixi_default_prefix)
    end
end


if has_config("dev") then
    -- Don't fetch system package
    set_policy("package.install_only", true)
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})

    -- compiler-rt has no ASan for arm64 Windows.
    if is_mode("debug") and not (is_plat("mingw") and is_arch("arm64")) then
        set_policy("build.sanitizer.address", true)
    end
end

if is_plat("macosx") then
    -- https://conda-forge.org/docs/maintainer/knowledge_base/#newer-c-features-with-old-sdk
    set_toolchains("clang")
    set_config("ranlib", "/usr/bin/ranlib")
    add_defines("_LIBCPP_DISABLE_AVAILABILITY=1")
    add_ldflags("-fuse-ld=lld")
    add_shflags("-fuse-ld=lld")

    add_requireconfs("**|cmake", {configs = {
        ldflags = "-fuse-ld=lld",
        shflags = "-fuse-ld=lld",
        cxflags = "-D_LIBCPP_DISABLE_AVAILABILITY=1",
    }})
end


if is_mode("debug") then
    add_defines("DEBUG")
end


if is_mode("debug") and is_plat("linux", "macosx") then
    -- hook.so will use a static lib to log in debug mode
    add_cxxflags("-fPIC")
end

if is_plat("linux") then
    add_defines("CATTER_LINUX")
elseif is_plat("macosx") then
    add_defines("CATTER_MAC")
elseif is_plat("mingw") then
    add_defines("CATTER_WINDOWS")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

-- The architectures of the processes Windows runs on x64 and ARM64, each
-- with a hook DLL catter-hook-<arch>.dll that every Windows build makes:
-- catter-proxy injects into a process the one of its machine
-- (src/capture/inject/windows/inject.cc).
local windows_hook_archs = is_plat("mingw") and {"x64", "arm64"} or {}

-- The name xmake gives a hook's architecture.
local function xmake_arch(arch)
    return arch == "x64" and "x86_64" or arch
end

-- Detours searches the address space for the data it leaves in a process,
-- ASan's shadow memory included, and an instrumented read of it aborts: never
-- instrument it, nor the kota::sys and kota::fs a hook DLL links, built for the
-- DLL's architecture without the rest of kotatsu.
for _, arch in ipairs(windows_hook_archs) do
    add_requires("detours~" .. arch, {arch = xmake_arch(arch), configs = {asan = false}})
    add_requires("kotatsu~" .. arch, {arch = xmake_arch(arch), configs = {
        asan = false,
        async = false,
        deco = false,
        ztest = false,
        codec_simdjson = false,
    }})
end

add_requires("spdlog", {version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
add_requires("kotatsu")


target("support")
    set_kind("static")
    add_includedirs("src", {public = true})
    add_files("src/support/*.cc")
    add_packages("spdlog", {public = true})
    add_packages("kotatsu", {public = true})

target("command")
    set_kind("static")
    add_local_prefix_includedirs()
    add_includedirs("src", {public = true})
    add_files("src/command/**.cc")
    add_packages("kotatsu", {public = true})

target("session")
    set_kind("static")
    add_local_prefix_includedirs()
    add_files("src/session/*.cc")
    add_deps("support", {public = true})

target("capture")
    set_kind("static")
    add_local_prefix_includedirs()
    add_files("src/capture/*.cc")
    add_deps("session", {public = true})

-- Launching a command with the hook, which catter-proxy does.
target("capture-inject")
    set_kind("static")
    add_local_prefix_includedirs()
    add_deps("session", {public = true})
    if is_plat("mingw") then
        add_files("src/capture/inject/windows/*.cc")
        add_packages("detours~" .. (is_arch("arm64", "aarch64") and "arm64" or "x64"), {public = true})
    elseif is_plat("linux", "macosx") then
        add_files("src/capture/inject/unix/*.cc")
    end

target("protocol")
    set_kind("static")
    add_local_prefix_includedirs()
    add_files("src/protocol/*.cc|schema.cc")
    add_deps("command", "session", {public = true})

target("feature")
    set_kind("static")
    add_local_prefix_includedirs()
    add_files("src/feature/*.cc")
    add_deps("command", "session", {public = true})

target("catter")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("capture", "protocol", "feature")
    add_files("src/driver/*.cc")


target("catter-protocol-schema")
    set_kind("binary")
    -- Prints the schema the SDKs' protocol types are generated from.
    add_local_prefix_includedirs()
    add_deps("support")
    add_files("src/protocol/schema.cc")

for _, arch in ipairs(windows_hook_archs) do
target("catter-hook-" .. arch)
    set_kind("shared")
    set_arch(xmake_arch(arch))
    -- Beside catter-proxy, which injects it, whatever its architecture.
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)")
    -- MinGW would name it lib*.dll.
    set_prefixname("")
    -- It runs inside every process of the build, and Detours trips ASan (see
    -- its add_requires): no sanitizer, and kotatsu of the DLL's architecture
    -- rather than catter's support.
    set_policy("build.sanitizer.address", false)
    add_includedirs("src")
    add_files("src/hook/windows/*.cc")
    add_packages("detours~" .. arch, "kotatsu~" .. arch)
    add_cxxflags("-fno-rtti")
    -- lld's MinGW driver takes a .def input as the only exports.
    add_shflags("src/hook/windows/exports.def", {force = true})
end


target("catter-hook-unix")
    set_default(is_plat("linux", "macosx"))
    set_kind("shared")
    add_local_prefix_includedirs()
    add_includedirs("src")
    add_packages("kotatsu")

    if is_mode("debug") then
        add_deps("support")
    end

    add_cxxflags("-fvisibility=hidden")
    add_cxxflags("-fvisibility-inlines-hidden")
    add_cxflags("-ffunction-sections", "-fdata-sections")

    add_files("src/hook/unix/**.cc")

    if is_plat("linux") then
        add_shflags("-static-libstdc++", {force = true})
        add_shflags("-static-libgcc", {force = true})

        add_shflags("-Wl,--version-script=src/hook/unix/inject/exports.map")
        add_syslinks("dl")
        add_shflags("-Wl,--gc-sections", {force = true})
    elseif is_plat("macosx") then
        -- set_policy("check.auto_ignore_flags", false)
        -- A private libc++, force-loaded below; xclang's libc++.a carries
        -- libc++abi as well.
        add_shflags("-nostdlib++", {force = true})
        add_syslinks("System")
        add_shflags("-fuse-ld=lld")
        add_shflags("-Wl,-exported_symbols_list,/dev/null", {public = true, force = true})
        add_shflags("-Wl,-dead_strip", {force = true})
    end
    on_load(function (target)
        if is_plat("macosx") then
          local libcxx_lib = os.iorun("clang++ -print-file-name=libc++.a"):trim()
          target:add("shflags", "-Wl,-force_load," .. libcxx_lib, {force = true})
        end
    end
    )

target("catter-proxy")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("capture-inject")
    add_files("src/proxy/*.cc")

rule("unittests-main")
    on_load(function (target)
        target:add("includedirs", "unittests")
        target:add("files", "unittests/main.cc")
        target:add("packages", "kotatsu")
    end)

target("unittests")
    set_default(has_config("test"))
    set_kind("binary")
    add_local_prefix_includedirs()
    add_rules("unittests-main")

    add_files("unittests/**.cc|main.cc|hook/**.cc")
    add_deps("capture", "protocol", "feature")

    add_tests("default")


target("unittests-hook-unix")
    set_default(has_config("test") and (is_plat("linux", "macosx")))
    set_kind("binary")
    add_rules("unittests-main")

    if is_plat("linux") then
        add_syslinks("dl")
    end
    add_files("src/hook/unix/*.cc")

    add_files("unittests/hook/unix/**.cc")

    add_deps("support")

    if is_plat("linux", "macosx") then
        add_tests("default")
    end

target("unittests-hook-windows")
    set_default(has_config("test") and is_plat("mingw"))
    set_kind("binary")
    add_rules("unittests-main")

    add_files("src/hook/windows/util.cc")
    add_files("unittests/hook/windows/**.cc")

    add_deps("support")

    if is_plat("mingw") then
        add_tests("default")
    end

package("detours")
    set_homepage("https://github.com/microsoft/Detours")
    set_license("MIT")

    set_urls("https://github.com/microsoft/Detours.git")
    -- main of 2026-09-29, with #388 (hooking ARM64EC functions).
    add_versions("2026.09.29", "1dbd4ea281d73d478aa8d09a8b83fbd265f715f7")

    on_install("mingw", function (package)
        -- The module walks start from (PBYTE)NULL + 64K: arithmetic on a null
        -- pointer, undefined, from which clang -O2 concludes the code
        -- unreachable once it inlines the first call, and deletes
        -- DetourUpdateProcessWithDll and DetourFindPayloadEx. MSVC does not.
        for _, file in ipairs({"src/creatwth.cpp", "src/modules.cpp"}) do
            io.replace(file,
                       "PBYTE pbLast = (PBYTE)hModuleLast + MM_ALLOCATION_GRANULARITY;",
                       "PBYTE pbLast = (PBYTE)((ULONG_PTR)hModuleLast + MM_ALLOCATION_GRANULARITY);",
                       {plain = true})
        end
        io.writefile("xmake.lua", [[
            add_rules("mode.release", "mode.debug")
            target("detours")
                set_kind("static")
                -- creatwth.cpp includes uimports.cpp once per architecture.
                add_files("src/*.cpp|uimports.cpp")
                add_headerfiles("src/detours.h", "src/detver.h")
                add_defines("WIN32_LEAN_AND_MEAN")
                -- The 0ui64 literals of the ARM64 code. detours.h turns
                -- __try/__except into nothing for __GNUC__ compilers.
                add_cxxflags("-fms-extensions")
        ]])
        import("package.tools.xmake").install(package)
    end)

package("kotatsu")
    set_homepage("https://clice.io")
    set_license("Apache-2.0")

    set_urls("https://github.com/clice-io/kotatsu.git")
    -- version from `git rev-list --count HEAD`
    add_versions("244", "08de62dfdb48b7228506a32d6e9acc1a604e72d9")

    -- The modules catter consumes, which the hook DLLs leave out. Configs, not
    -- only install options, so that they are part of the package's hash and a
    -- cached build of others is not reused.
    add_configs("async", {description = "Build kota::async and kota::ipc, on libuv.", default = true, type = "boolean"})
    add_configs("deco", {description = "Build the option parser.", default = true, type = "boolean"})
    add_configs("ztest", {description = "Build zest, the test framework.", default = true, type = "boolean"})
    -- The JSON codec and kota::ipc::JSONPeer, which catter speaks to handlers
    -- with.
    add_configs("codec_simdjson", {description = "Build the JSON codec.", default = true, type = "boolean"})

    -- What kotatsu's sys target links, which its install does not record.
    if is_plat("mingw") then
        add_syslinks("advapi32", "ntdll", "ole32", "shell32")
    end

    on_load(function (package)
        if package:config("async") then
            package:add("deps", "libuv v1.52.0")
        end
        if package:config("ztest") then
            package:add("deps", "cpptrace v1.0.4")
        end
        if package:config("codec_simdjson") then
            package:add("deps", "simdjson v4.2.4")
        end
    end)

    on_install(function (package)
        local configs = {}
        -- Build the dependency with a plain consumer config so we do not pull
        -- in kotatsu's repo-local dev toolchain tweaks during package install.
        configs.dev = false
        configs.test = false
        -- Pinned, so that upstream default flips do not silently drop features
        -- catter depends on.
        configs.async = package:config("async")
        configs.deco = package:config("deco")
        configs.ztest = package:config("ztest")
        -- http defaults on upstream and would pull in libcurl and OpenSSL.
        configs.http = false
        configs.codec_simdjson = package:config("codec_simdjson")
        import("package.tools.xmake").install(package, configs, {target = "kotatsu"})
    end)


includes("@builtin/xpack")

xpack("catter")
    set_basename("catter-$(version)-$(plat)-$(arch)")
    set_title("Catter")
    set_description("A tool for intercepting and analyzing build processes")
    set_licensefile("LICENSE")
    set_homepage("https://clice.io")
    -- set_iconfile()
    set_formats("nsis", "zip", "targz")
    add_targets("catter", "catter-proxy")
    for _, arch in ipairs(windows_hook_archs) do
        add_targets("catter-hook-" .. arch)
    end
    if is_plat("linux", "macosx") then
        add_targets("catter-hook-unix")
    end

    set_libdir("bin") -- put hook dlls in bin for easier loading
    -- xmake's NSIS script installs into the 64-bit Program Files and
    -- registry view only for the architecture it calls x64; MinGW's are
    -- x86_64 and arm64, both 64-bit.
    if is_plat("mingw") then
        set_specvar("PACKAGE_ARCH", "x64")
    end
