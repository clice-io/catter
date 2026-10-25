set_version("0.1.0")
set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "mingw", "linux", "macosx")

set_languages("c++23")
-- Sources and narrow string literals are UTF-8, the encoding catter's strings
-- hold on every platform (src/common/util/encoding.h).
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

    local toolchain = get_config("toolchain")

    if is_mode("debug") then
        if is_plat("windows") then
            if toolchain == "msvc" then
                set_policy("build.sanitizer.address", true)
            end
        elseif not is_plat("mingw") then
            -- xclang has no ASan runtime for MinGW.
            set_policy("build.sanitizer.address", true)
        end

    end


    if is_plat("windows") then
        set_runtimes("MD")
        if toolchain == "clang" then
            add_ldflags("-fuse-ld=lld-link")
            add_shflags("-fuse-ld=lld-link")
        elseif toolchain == "clang-cl" then
            set_toolset("ld", "lld-link")
            set_toolset("sh", "lld-link")
        end
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
elseif is_plat("windows", "mingw") then
    add_defines("CATTER_WINDOWS")
    add_defines("WIN32_LEAN_AND_MEAN", "NOMINMAX")
end

-- The architectures of the processes Windows runs on x64 and ARM64, each
-- with a hook DLL catter-hook-<arch>.dll that every Windows build makes:
-- catter-proxy injects into a process the one of its machine (win/inject.cc).
local windows_hook_archs = is_plat("windows", "mingw") and {"x64", "arm64"} or {}

-- The name xmake gives a hook's architecture on the platform.
local function xmake_arch(arch)
    return (arch == "x64" and is_plat("mingw")) and "x86_64" or arch
end

-- Detours searches the address space for the data it leaves in a process,
-- ASan's shadow memory included, and an instrumented read of it aborts: never
-- instrument it.
for _, arch in ipairs(windows_hook_archs) do
    add_requires("detours~" .. arch, {arch = xmake_arch(arch), configs = {asan = false}})
end

-- do
--     local toolchain = get_config("toolchain")
--     if toolchain == "msvc" or toolchain == "clang-cl" then
--         add_cxxflags("/W3", "/WX", {force = true})
--     else
--         add_cxxflags("-Wall", "-Werror", {force = true})
--     end
-- end

add_requires("spdlog", {version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
add_requires("kotatsu")


target("common-option")
    set_kind("static")
    add_local_prefix_includedirs()
    add_includedirs("src/common", {public = true})
    add_files("src/common/option/**.cc")
    add_packages("kotatsu", {public = true})

target("common-util")
    set_kind("static")
    add_includedirs("src/common", {public = true})
    add_files("src/common/util/**.cc")
    add_packages("spdlog", {public = true})
    add_packages("kotatsu", {public = true})

target("common-config")
    set_kind("headeronly")
    add_includedirs("src/common", {public = true})
    add_packages("kotatsu", {public = true})

target("common-winapi")
    set_default(is_plat("windows", "mingw"))
    set_kind("static")
    add_includedirs("src/common", {public = true})
    add_files("src/common/winapi/**.cc")
    add_syslinks("user32", "advapi32")

target("common-resolver")
    set_kind("static")
    add_local_prefix_includedirs()
    add_includedirs("src/common", {public = true})
    if is_plat("windows", "mingw") then
        add_files("src/common/resolver/win.cc")
    else
        add_files("src/common/resolver/unix.cc")
    end

    if is_plat("windows", "mingw") then
        add_deps("common-winapi", {public = true})
    end

target("common")
    set_kind("static")
    add_includedirs("src/common", {public = true})

    if is_plat("windows", "mingw") then
        add_deps("common-winapi", {public = true})
    end

    add_deps("common-option", "common-util", "common-config", "common-resolver", { public = true })

target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_local_prefix_includedirs()
    add_includedirs("src/catter/core", {public = true})

    add_deps("common")

    add_files("src/catter/core/**.cc")

target("catter")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("catter-core")
    add_files("src/catter/main.cc")
    -- main is wmain on Windows (src/catter/main.cc).
    if is_plat("mingw") then
        add_ldflags("-municode", {force = true})
    end


target("catter-protocol-schema")
    set_kind("binary")
    -- Prints the schema the SDKs' protocol types are generated from.
    add_local_prefix_includedirs()
    add_includedirs("src/catter/core")
    add_deps("common-util")
    add_files("src/catter-protocol-schema/main.cc")

for _, arch in ipairs(windows_hook_archs) do
target("catter-hook-" .. arch)
    set_kind("shared")
    set_arch(xmake_arch(arch))
    -- Beside catter-proxy, which injects it, whatever its architecture.
    set_targetdir("$(builddir)/$(plat)/$(arch)/$(mode)")
    -- MinGW would name it lib*.dll.
    set_prefixname("")
    -- It runs inside every process of the build, and Detours trips ASan (see
    -- its add_requires): no sanitizer, and the resolver compiled in, of the
    -- DLL's architecture, rather than linked from common-resolver.
    set_policy("build.sanitizer.address", false)
    add_includedirs("src/catter-hook/", "src/common")
    add_files("src/catter-hook/win/payload/*.cc")
    add_files("src/common/resolver/win.cc", "src/common/winapi/**.cc")
    add_syslinks("user32", "advapi32")
    add_packages("detours~" .. arch)
    local toolchain = get_config("toolchain")
    if toolchain == "msvc" or toolchain == "clang-cl" then
        -- Nor the VC runtime DLLs, which a process of the architecture may
        -- lack.
        set_runtimes("MT")
        add_cxxflags("/GR-")
        add_shflags("/DEF:src/catter-hook/win/payload/exports.def")
    elseif is_plat("mingw") then
        add_cxxflags("-fno-rtti")
        -- lld's MinGW driver takes a .def input as the only exports.
        add_shflags("src/catter-hook/win/payload/exports.def", {force = true})
    else
        add_cxxflags("-fno-exceptions", "-fno-rtti")
        add_shflags("-Wl,/DEF:src/catter-hook/win/payload/exports.def")
    end
end


target("catter-hook-unix")
    set_default(is_plat("linux", "macosx"))
    set_kind("shared")
    add_local_prefix_includedirs()
    add_deps("common-resolver")

    if is_mode("debug") then
        add_deps("common")
    end

    add_cxxflags("-fvisibility=hidden")
    add_cxxflags("-fvisibility-inlines-hidden")
    add_cxflags("-ffunction-sections", "-fdata-sections")

    add_includedirs("src/catter-hook/")
    add_includedirs("src/catter-hook/unix/payload/")
    add_files("src/catter-hook/unix/payload/**.cc")

    if is_plat("linux") then
        add_shflags("-static-libstdc++", {force = true})
        add_shflags("-static-libgcc", {force = true})

        add_shflags("-Wl,--version-script=src/catter-hook/unix/payload/inject/exports.map")
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

target("catter-hook")
    set_kind("object")
    add_local_prefix_includedirs()
    add_includedirs("src/catter-hook/", {public = true})
    add_deps("common")
    if is_plat("windows", "mingw") then
        add_files("src/catter-hook/win/*.cc")
        add_packages("detours~" .. (is_arch("arm64", "aarch64") and "arm64" or "x64"), {public = true})
    elseif is_plat("linux", "macosx") then
        add_files("src/catter-hook/unix/impl.cc")
    end

target("catter-proxy")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("common", "catter-hook")
    add_includedirs("src/catter-proxy/")
    add_files("src/catter-proxy/**.cc")
    -- main is wmain on Windows (src/catter-proxy/main.cc).
    if is_plat("mingw") then
        add_ldflags("-municode", {force = true})
    end

rule("ut-base")
    on_load(function (target)
        target:add("includedirs", "unittests/base/")
        target:add("files", "unittests/base/**.cc")
        target:add("packages", "kotatsu")
    end)

target("ut-common")
    set_default(has_config("test"))
    set_kind("binary")
    add_local_prefix_includedirs()
    add_rules("ut-base")

    add_files("unittests/common/**.cc")

    add_deps("common")
    add_tests("default")

target("ut-catter")
    set_default(has_config("test"))
    set_kind("binary")
    add_local_prefix_includedirs()
    add_rules("ut-base")

    add_files("unittests/catter/**.cc")
    add_deps("catter-core", "common")

    add_tests("default")


target("ut-catter-hook-unix")
    set_default(has_config("test") and (is_plat("linux", "macosx")))
    set_kind("binary")
    add_rules("ut-base")

    if is_plat("linux") then
        add_syslinks("dl")
    end
    add_includedirs("src/catter-hook/", { public = true })
    add_includedirs("src/catter-hook/unix/payload/", { public = true })
    add_files("src/catter-hook/unix/payload/*.cc")

    add_files("unittests/catter-hook/unix/**.cc")
    add_files("unittests/catter-hook/shared/**.cc")

    add_deps("common")

    if is_plat("linux", "macosx") then
        add_tests("default")
    end

target("ut-catter-hook-win")
    set_default(has_config("test") and is_plat("windows", "mingw"))
    set_kind("binary")
    add_rules("ut-base")

    add_includedirs("src/catter-hook/")
    add_files("src/catter-hook/win/payload/util.cc")
    add_files("unittests/catter-hook/win/**.cc")
    add_files("unittests/catter-hook/shared/**.cc")

    add_deps("common")

    if is_plat("windows", "mingw") then
        add_tests("default")
    end

package("detours")
    set_homepage("https://github.com/microsoft/Detours")
    set_license("MIT")

    set_urls("https://github.com/microsoft/Detours.git")
    -- main of 2026-09-29, with #388 (hooking ARM64EC functions).
    add_versions("2026.09.29", "1dbd4ea281d73d478aa8d09a8b83fbd265f715f7")

    on_install("windows", "mingw", function (package)
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
                if is_plat("windows") then
                    -- No default C runtime, as Detours' own build: the hook
                    -- links the static one, catter-proxy the DLL.
                    add_cxflags("/Zl")
                else
                    -- The 0ui64 literals of the ARM64 code. detours.h turns
                    -- __try/__except into nothing for __GNUC__ compilers.
                    add_cxxflags("-fms-extensions")
                end
        ]])
        import("package.tools.xmake").install(package)
    end)

package("kotatsu")
    set_homepage("https://clice.io")
    set_license("Apache-2.0")

    set_urls("https://github.com/clice-io/kotatsu.git")
    -- version from `git rev-list --count HEAD`
    add_versions("210", "4f5fc11be4133d6de4525b5c7556a45de59d0be1")

    add_deps("libuv v1.52.0")
    add_deps("cpptrace v1.0.4")
    -- The JSON codec and kota::ipc::JsonPeer, which catter speaks to handlers
    -- with. A config, not only an install option, so that it is part of the
    -- package's hash and a cached build without it is not reused.
    add_configs("codec_simdjson", {description = "Build the JSON codec.", default = true, type = "boolean"})
    add_deps("simdjson v4.2.4")

    on_install(function (package)
        if package:has_tool("cxx", "cl", "clang_cl") then
            package:add("cxxflags", "/Zc:__cplusplus", "/Zc:preprocessor")
        end

        local configs = {}
        -- Build the dependency with a plain consumer config so we do not pull
        -- in kotatsu's repo-local dev toolchain tweaks during package install.
        configs.dev = false
        configs.test = false
        -- Pin the kotatsu submodules catter actually consumes so upstream
        -- default flips do not silently drop features we depend on.
        configs.async = true
        configs.deco = true
        configs.ztest = true
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
