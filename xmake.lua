set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

option("dev", {default = true})
option("test", {default = true})

if has_config("dev") then
    -- Don't fetch system package
    set_policy("package.install_only", true)
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
    if is_plat("windows") then
        set_runtimes("MD")
    end
    if is_mode("debug") then
        set_policy("build.sanitizer.address", true)
    end
end

if is_plat("macosx") then
    -- https://conda-forge.org/docs/maintainer/knowledge_base/#newer-c-features-with-old-sdk
    add_defines("_LIBCPP_DISABLE_AVAILABILITY=1")
    add_ldflags("-fuse-ld=lld")
    add_shflags("-fuse-ld=lld")

    add_requireconfs("**|cmake", {configs = {
        ldflags = "-fuse-ld=lld",
        shflags = "-fuse-ld=lld",
        cxflags = "-D_LIBCPP_DISABLE_AVAILABILITY=1",
    }})
end

set_languages("c++23")

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

add_requires("libuv", {version = "v1.51.0"})
add_requires("quickjs-ng", {version = "v0.11.0"})
add_requires("spdlog", {version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
if has_config("test") then
    add_requires("boost_ut", {version = "v2.3.1"})
end

target("common")
    set_kind("static")
    add_includedirs("src/common", {public = true})
    add_files("src/common/**.cc")
    add_packages("libuv", {public = true})
    add_packages("spdlog", {public = true})

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


target("ut-catter")
    set_default(false)
    set_kind("binary")
    add_files("tests/unit/catter/**.cc")
    add_packages("boost_ut")
    add_deps("catter-core", "common")

    add_defines(format([[JS_TEST_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/"))))
    add_rules("build.js", {js_target = "build-js-test"})
    add_files("api/src/*.ts", "api/test/*.ts", "api/test/res/**/*.txt")

    add_tests("default")


if is_plat("windows") then
    target("catter-hook-win64")
        set_kind("shared")
        add_includedirs("src/catter-hook/")
        add_files("src/catter-hook/win/payload/main.cc")
        add_syslinks("user32", "advapi32")
        add_packages("microsoft-detours")
        add_cxxflags("-fno-exceptions -fno-rtti")

elseif is_plat("linux", "macosx") then
    target("catter-hook-unix")
        set_kind("shared")
        if is_mode("debug") then
            add_deps("common")
        end

        add_includedirs("src/catter-hook/")
        add_includedirs("src/catter-hook/linux-mac/payload/")
        add_files("src/catter-hook/linux-mac/payload/**.cc")
        add_syslinks("dl")
        if is_mode("release") then
            add_cxxflags("-fvisibility=hidden")
            add_cxxflags("-nostdlib++")
        end
end

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
    add_files("src/catter-proxy/main.cc", "src/catter-proxy/constructor.cc")

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
