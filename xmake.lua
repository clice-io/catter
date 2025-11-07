set_project("catter")
add_rules("mode.debug", "mode.release")
set_allowedplats("windows", "linux")

set_languages("c++23")
set_toolchains("clang")

option("dev", {default = true})
if has_config("dev") then
   add_rules("plugin.compile_commands.autoupdate", {outputdir = "build", lsp = "clangd"})
end

if is_plat("windows") then
    add_requires("microsoft-detours")
end

if is_plat("windows") then
    target("catter-hook64")
        set_kind("shared")
        add_includedirs("src")
        add_files("src/hook/windows/hook.cpp")
        add_syslinks("user32")
        add_packages("microsoft-detours")
end

if is_plat("linux") then
    target("catter-hook-linux")
        set_kind("shared")
        add_includedirs("src/hook/linux/libhook")
        add_files("src/hook/linux/libhook/*.cc")
        add_files("src/hook/linux/libhook/inject/*.cc")
        if is_mode("debug") then
            add_defines("DEBUG")
        end
end



target("catter")
    set_kind("binary")
    add_includedirs("src")
    add_files("src/main.cpp")
    if is_plat("windows") then
        add_files("src/hook/windows/impl.cpp")
        add_packages("microsoft-detours")
    end
    if is_plat("linux") then
        add_files("src/hook/linux/*.cc")
    end
