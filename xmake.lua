set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

option("dev", {default = true})
option("test", {default = true})

if has_config("dev") then
    -- Don't fetch system package
    set_policy("package.install_only", true)
    set_policy("build.ccache", true)
    if is_mode("debug") then
        set_policy("build.sanitizer.address", true)
    end

    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})

    if is_plat("windows") then
        set_runtimes("MD")

        local toolchain = get_config("toolchain")
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
    add_defines("_LIBCPP_DISABLE_AVAILABILITY=1")
    add_ldflags("-fuse-ld=lld")
    add_shflags("-fuse-ld=lld")

    add_requireconfs("**|cmake", {configs = {
        ldflags = "-fuse-ld=lld",
        shflags = "-fuse-ld=lld",
        cxflags = "-D_LIBCPP_DISABLE_AVAILABILITY=1",
    }})
end


if is_mode("debug") and is_plat("linux", "macosx") then
    -- hook.so will use a static lib to log in debug mode
    add_defines("DEBUG")
    add_cxxflags("-fPIC")
end

if is_plat("linux") then
    add_defines("CATTER_LINUX")
elseif is_plat("macosx") then
    add_defines("CATTER_MAC")
elseif is_plat("windows") then
    add_defines("CATTER_WINDOWS")
    add_requires("microsoft-detours", {version = "2023.6.8"})
end


add_requires("quickjs-ng", {version = "v0.11.0"})
add_requires("spdlog", {version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
if has_config("test") then
    add_requires("eventide")
end

target("common")
    set_kind("static")
    add_includedirs("src/common", {public = true})
    add_files("src/common/**.cc")

    add_packages("spdlog", {public = true})
    add_packages("eventide", {public = true})

target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_includedirs("src/catter/core", {public = true})
    add_packages("quickjs-ng", {public = true})

    add_deps("common")

    add_files("src/catter/core/**.cc")

    add_files("api/src/*.ts", {always_added = true})
    add_rules("build.js", {js_target = "build-js-lib", js_file = "api/output/lib/lib.js"})

target("catter")
    set_kind("binary")
    add_deps("catter-core")
    add_files("src/catter/main.cc")




target("catter-hook-win64")
    set_default(is_plat("windows"))
    set_kind("shared")
    add_includedirs("src/catter-hook/")
    add_files("src/catter-hook/win/payload/main.cc")
    add_syslinks("user32", "advapi32")
    add_packages("microsoft-detours")
    add_cxxflags("-fno-exceptions", "-fno-rtti")




target("catter-hook-unix")
    set_default(is_plat("linux", "macosx"))
    set_kind("shared")

    if is_mode("debug") then
        add_deps("common")
    end

    add_cxxflags("-fvisibility=hidden")
    add_cxxflags("-fvisibility-inlines-hidden")
    add_cxflags("-ffunction-sections", "-fdata-sections")

    add_includedirs("src/catter-hook/")
    add_includedirs("src/catter-hook/linux-mac/payload/")
    add_files("src/catter-hook/linux-mac/payload/**.cc")

    if is_plat("linux") then
        add_shflags("-static-libstdc++", {force = true})
        add_shflags("-static-libgcc", {force = true})

        add_shflags("-Wl,--version-script=src/catter-hook/linux-mac/payload/inject/exports.map")
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
    add_includedirs("src/catter-hook/", {public = true})
    add_deps("common")
    if is_plat("windows") then
        add_files("src/catter-hook/win/impl.cc")
        add_packages("microsoft-detours")
    elseif is_plat("linux", "macosx") then
        add_files("src/catter-hook/linux-mac/impl.cc")
    end

target("catter-proxy")
    set_kind("binary")
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
    set_default(false)
    set_kind("binary")
    add_rules("ut-base")

    add_files("tests/unit/common/**.cc")

    add_deps("common")
    add_tests("default")

target("ut-catter")
    set_default(false)
    set_kind("binary")
    add_rules("ut-base")

    add_files("tests/unit/catter/**.cc")
    add_deps("catter-core", "common")

    add_defines(format([[JS_TEST_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/"))))
    add_defines(format([[JS_TEST_RES_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/res"))))
    add_rules("build.js", {js_target = "build-js-test"})
    add_files("api/src/*.ts", "api/test/*.ts")

    add_tests("default")


target("ut-catter-hook-unix")
    set_default(false)
    set_kind("binary")
    add_rules("ut-base")

    if is_plat("linux") then
        add_syslinks("dl")
    end
    add_includedirs("src/catter-hook/", { public = true })
    add_includedirs("src/catter-hook/linux-mac/payload/", { public = true })
    add_files("src/catter-hook/linux-mac/payload/*.cc")

    add_files("tests/unit/catter-hook/linux-mac/**.cc")

    add_deps("common")

    if is_plat("linux", "macosx") then
        add_tests("default")
    end

target("ut-catter-hook-win64")
    set_default(false)
    set_kind("binary")
    add_rules("ut-base")

    if is_plat("windows") then
        -- skip
    elseif is_plat("linux", "macosx") then
        add_deps("ut-catter-hook-unix")
    end
    add_files("tests/unit/catter-hook/win/**.cc")

    add_deps("common")

    if is_plat("windows") then
        add_tests("default")
    end

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
    add_versions("34", "500ea2f8a5fde57637560159753ce0b3522b6c83")

    add_deps("libuv 1.51.0")
    add_deps("cpptrace v1.0.4")

    on_install(function (package)
        if package:has_tool("cxx", "cl", "clang_cl") then
            package:add("cxxflags", "/Zc:__cplusplus", "/Zc:preprocessor")
        end

        local configs = {}
        configs.dev = has_config("dev")
        configs.test = false
        import("package.tools.xmake").install(package, configs)
    end)
