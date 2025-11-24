set_project("catter")

add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

option("dev", {default = true})
if has_config("dev") then
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
end

if is_mode("debug") then
    add_defines("DEBUG")
end

if is_plat("linux") then
    add_defines("CATTER_LINUX")
elseif is_plat("macosx") then
    add_defines("CATTER_MAC")
elseif is_plat("windows") then
    add_defines("CATTER_WINDOWS")
end


includes("src/common/librpc")
includes("src/common/libutil")

includes("src/catter")
includes("src/catter-proxy")
