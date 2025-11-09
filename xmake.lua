set_project("catter")

add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux")

set_languages("c++23")

option("dev", {default = true})
if has_config("dev") then
    set_policy("build.ccache", true)
    add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
end

if is_plat("windows") then
    add_requires("microsoft-detours")
end

if is_plat("linux") then
    includes("src/hook/linux")
end

target("catter-hook64")
    set_kind("shared")
    set_enabled(is_plat("windows"))
    add_includedirs("src")
    add_files("src/hook/windows/hook.cpp")
    add_syslinks("user32")
    add_packages("microsoft-detours")

target("catter")
    set_kind("binary")
    add_includedirs("src")
    add_files("src/main.cpp")
    if is_plat("windows") then
        add_files("src/hook/windows/impl.cpp")
    elseif is_plat("linux") then
        add_files("src/hook/linux/*.cc")
    end

    if is_plat("windows") then
        add_packages("microsoft-detours")
    end
