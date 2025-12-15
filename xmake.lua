set_project("catter")

add_rules("mode.debug", "mode.release", "mode.releasedbg")
set_allowedplats("windows", "linux", "macosx")

option("dev", {default = true})
option("test", {default = true})

if has_config("dev") then
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
end

if is_plat("macosx") then
    -- https://conda-forge.org/docs/maintainer/knowledge_base/#newer-c-features-with-old-sdk
    add_defines("_LIBCPP_DISABLE_AVAILABILITY=1")
    add_ldflags("-fuse-ld=lld")
    add_shflags("-fuse-ld=lld")

    local opt = {configs = {
        ldflags = "-fuse-ld=lld",
        shflags = "-fuse-ld=lld",
        cxflags = "-D_LIBCPP_DISABLE_AVAILABILITY=1"
    }}
    add_requireconfs("quickjs-ng", opt)
    add_requireconfs("libuv", opt)
    add_requireconfs("spdlog", opt)
end

add_requires("spdlog", {system = false, version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
if has_config("test") then
    add_requires("boost_ut", {system = false, version = "v2.3.1"})
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
end

includes(
    "src/common",

    "src/catter",
    "src/catter-proxy",

    "src/unitest/catter"
)

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
                })
            end
        end, {
            files = sourcebatch.sourcefiles,
            dependfile = target:dependfile(objectfile),
            changed = target:is_rebuilt(),
        })
    end)
