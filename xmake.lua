set_project("catter")

set_languages("c++23")

add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})


add_requires("microsoft-detours")

target("catter-hook64")
    set_kind("shared")
    add_files("src/hook.cpp")
    add_syslinks("user32")

    add_packages("microsoft-detours")

target("catter")
    set_kind("binary")
    add_files("src/main.cpp")

    add_packages("microsoft-detours")
