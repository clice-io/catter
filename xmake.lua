set_project("catter")

add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

add_requires("spdlog", {system = false, version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})
add_requires("boost_ut", {system = false, version = "2.3.1"})

option("dev", {default = true})
if has_config("dev") then
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
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
end

rule("js-lib")
    set_extensions(".ts", ".d.ts", ".js")
    on_build_file(function (target, sourcefile, opt)
        import("core.project.depend")
        import("utils.progress")

        depend.on_changed(function ()
            progress.show(opt.progress, "${color.build.target}build js-lib %s", sourcefile)
            os.run("pnpm run build-js-lib")
        end, {files = sourcefile})
    end)



includes("src/common/librpc")
includes("src/common/libutil")
includes("src/common/libconfig")


includes("src/catter")
includes("src/catter-proxy")

-- unitest
includes("src/unitest/")
