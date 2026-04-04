set_version("0.1.0")
set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

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
        else
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
elseif is_plat("windows") then
    add_defines("CATTER_WINDOWS")
    add_requires("minhook", {version = "v1.3.4"})
end

-- do
--     local toolchain = get_config("toolchain")
--     if toolchain == "msvc" or toolchain == "clang-cl" then
--         add_cxxflags("/W3", "/WX", {force = true})
--     else
--         add_cxxflags("-Wall", "-Werror", {force = true})
--     end
-- end

add_requires("quickjs-ng", {version = "v0.11.0"})
add_requires("spdlog", {version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
add_requires("eventide")


target("common")
    set_kind("static")
    add_local_prefix_includedirs()
    add_includedirs("src/common", {public = true})
    add_files("src/common/**.cc")

    add_packages("spdlog", {public = true})
    add_packages("eventide", {public = true})

target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_local_prefix_includedirs()
    add_includedirs("src/catter/core", {public = true})
    add_packages("quickjs-ng", {public = true})

    add_deps("common")

    add_files("src/catter/core/**.cc")

    add_files("api/src/**.ts", {always_added = true})
    add_rules("build.js", {js_target = "build-js-lib", js_file = "api/output/lib/lib.js"})

target("catter")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("catter-core")
    add_files("src/catter/main.cc")




target("catter-hook-win64")
    set_default(is_plat("windows"))
    set_kind("shared")
    add_local_prefix_includedirs()
    add_includedirs("src/catter-hook/")
    add_files("src/catter-hook/win/payload/*.cc")
    add_syslinks("user32", "advapi32")
    add_packages("minhook")
    local toolchain = get_config("toolchain")
    if toolchain == "msvc" or toolchain == "clang-cl" then
        add_cxxflags("/GR-")
        add_shflags("/DEF:src/catter-hook/win/payload/exports.def")
    else
        add_cxxflags("-fno-exceptions", "-fno-rtti")
        add_shflags("-Wl,/DEF:src/catter-hook/win/payload/exports.def")
    end


target("catter-hook-unix")
    set_default(is_plat("linux", "macosx"))
    set_kind("shared")
    add_local_prefix_includedirs()

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
        add_shflags("-nostdlib++", {force = true})
        add_syslinks("System")
        add_syslinks("c++abi")
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
    if is_plat("windows") then
        add_files("src/catter-hook/win/*.cc")
    elseif is_plat("linux", "macosx") then
        add_files("src/catter-hook/unix/impl.cc")
    end

target("catter-proxy")
    set_kind("binary")
    add_local_prefix_includedirs()
    add_deps("common", "catter-hook")
    add_includedirs("src/catter-proxy/")
    add_files("src/catter-proxy/**.cc")


rule("ut-base")
    on_load(function (target)
        target:add("includedirs", "tests/unit/base/")
        target:add("files", "tests/unit/base/**.cc")
        target:add("packages", "eventide")
    end)

target("ut-common")
    set_default(has_config("test"))
    set_kind("binary")
    add_local_prefix_includedirs()
    add_rules("ut-base")

    add_files("tests/unit/common/**.cc")

    add_deps("common")
    add_tests("default")

target("ut-catter")
    set_default(has_config("test"))
    set_kind("binary")
    add_local_prefix_includedirs()
    add_rules("ut-base")

    add_files("tests/unit/catter/**.cc")
    add_deps("catter-core", "common")

    add_defines(format([[JS_TEST_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/"))))
    add_defines(format([[JS_TEST_RES_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/res"))))
    add_rules("build.js", {js_target = "build-js-test"})
    add_files("api/src/**.ts", "api/test/**.ts")

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

    add_files("tests/unit/catter-hook/unix/**.cc")

    add_deps("common")

    if is_plat("linux", "macosx") then
        add_tests("default")
    end

target("ut-catter-hook-win64")
    set_default(has_config("test") and is_plat("windows"))
    set_kind("binary")
    add_rules("ut-base")

    add_includedirs("src/catter-hook/")
    add_files("src/catter-hook/win/payload/resolver.cc")
    add_files("src/catter-hook/win/payload/util.cc")
    add_files("tests/unit/catter-hook/win/**.cc")

    add_deps("common")

    if is_plat("windows") then
        add_tests("default")
    end

target("it-catter-hook")
    set_default(has_config("test"))
    set_kind("binary")
    add_files("tests/integration/test/catter-hook.cc")
    add_deps("catter-hook", "common")

target("it-catter-proxy")
    set_default(has_config("test"))
    set_kind("binary")
    add_files("tests/integration/test/catter-proxy.cc")
    add_deps("common", "catter-core")

rule("build.js")
    set_extensions(".ts", ".d.ts", ".js", ".txt")

    on_build_files(function (target, sourcebatch, opt)
        -- ref xmake/rules/utils/bin2obj/utils.lua
        import("utils.binary.bin2obj")
        import("lib.detect.find_tool")
        import("core.project.depend")
        import("utils.progress")

        local js_target = target:extraconf("rules", "build.js", "js_target")
        local js_file = target:extraconf("rules", "build.js", "js_file")

        local pnpm = assert(find_tool("pnpm") or find_tool("pnpm.cmd") or find_tool("pnpm.bat"), "pnpm not found!")

        local format
        if target:is_plat("windows", "mingw", "msys", "cygwin") then
            format = "coff"
        elseif target:is_plat("macosx", "iphoneos", "watchos", "appletvos") then
            format = "macho"
        else
            format = "elf"
        end

        local objectfile
        if js_file then
            objectfile = target:objectfile(js_file)
            os.mkdir(path.directory(objectfile))
            table.insert(target:objectfiles(), objectfile)
        end

        depend.on_changed(function()
            progress.show(opt.progress or 0, "${color.build.object}Building js target %s", js_target)
            os.vrunv(pnpm.program, {"run", js_target})

            if js_file then
                progress.show(opt.progress or 0, "${color.build.object}generating.bin2obj %s", js_file)
                bin2obj(js_file, objectfile, {
                    format = format,
                    arch = target:arch(),
                    plat = target:plat(),
                    zeroend = true
                })
            end
        end, {
            files = sourcebatch.sourcefiles,
            dependfile = target:dependfile(objectfile),
            changed = target:is_rebuilt(),
        })
    end)

package("eventide")
    set_homepage("https://clice.io")
    set_license("Apache-2.0")

    set_urls("https://github.com/clice-io/eventide.git")
    -- version from `git rev-list --count HEAD`
    add_versions("104", "191e46355d2bc958fc19379a99b9a2b8b77f2963")

    add_deps("libuv v1.52.0")
    add_deps("cpptrace v1.0.4")

    on_install(function (package)
        if package:has_tool("cxx", "cl", "clang_cl") then
            package:add("cxxflags", "/Zc:__cplusplus", "/Zc:preprocessor")
        end

        local configs = {}
        -- Build the dependency with a plain consumer config so we do not pull
        -- in eventide's repo-local dev toolchain tweaks during package install.
        configs.dev = false
        configs.test = false
        if package:is_plat("macosx") then
            local conda_prefix = os.getenv("CONDA_PREFIX")
            if conda_prefix then
                local bindir = path.join(conda_prefix, "bin")
                local libdir = path.join(conda_prefix, "lib")
                local mode = package:is_debug() and "debug" or "release"
                local builddir = package:builddir()
                -- Pixi's clang cfg injects `.pixi/.../include`; disable that default
                -- config just for eventide's package install and keep the rest explicit.
                local argv = {
                    "f", "-y", "-c",
                    "--plat=" .. package:plat(),
                    "--arch=" .. package:arch(),
                    "--mode=" .. mode,
                    "--kind=" .. (package:config("shared") and "shared" or "static"),
                    "--builddir=" .. builddir,
                    "--cc=" .. path.join(bindir, "clang"),
                    "--cxx=" .. path.join(bindir, "clang++"),
                    "--ld=" .. path.join(bindir, "clang++"),
                    "--sh=" .. path.join(bindir, "clang++"),
                    "--ar=" .. path.join(bindir, "llvm-ar"),
                    "--ranlib=" .. path.join(bindir, "llvm-ranlib"),
                    "--cflags=--no-default-config",
                    "--cxflags=--no-default-config -D_LIBCPP_DISABLE_AVAILABILITY=1",
                    "--ldflags=--no-default-config -L" .. libdir .. " -Wl,-rpath," .. libdir,
                    "--shflags=--no-default-config -L" .. libdir .. " -Wl,-rpath," .. libdir,
                    "--dev=false",
                    "--test=false"
                }
                if package:config("asan") then
                    table.insert(argv, "--policies=build.sanitizer.address")
                end
                os.vrunv("xmake", argv, {curdir = package:sourcedir()})
                os.mkdir(path.join(builddir, ".deps", "eventide", package:plat(), package:arch(), mode))
                os.vrunv("xmake", {"build", "eventide"}, {curdir = package:sourcedir()})
                os.vrunv("xmake", {"install", "-y", "--packages=n", "-o", package:installdir(), "eventide"}, {curdir = package:sourcedir()})
                return
            end
            import("package.tools.xmake").install(package, configs, {target = "eventide"})
        else
            import("package.tools.xmake").install(package, configs)
        end
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
    if is_plat("windows") then
        add_targets("catter-hook-win64")
    elseif is_plat("linux", "macosx") then
        add_targets("catter-hook-unix")
    end

    set_libdir("bin") -- put hook dlls in bin for easier loading
