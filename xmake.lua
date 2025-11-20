set_project("catter")

add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux", "macosx")

set_languages("c++23")

option("dev", {default = true})
if has_config("dev") then
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
end


includes("src/libhook")
includes("src/libcommand")
includes("src/catter")
includes("src/catter-proxy")
