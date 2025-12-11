set_project("catter")

add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

add_requires("spdlog", {system = false, version = "1.15.3", configs = {header_only = false, std_format = true, noexcept = true}})


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



includes("src/common/libutil")
includes("src/common/libconfig")


includes("src/catter")
includes("src/catter-proxy")
