set_project("catter")
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
        add_includedirs("src/hook/linux")
        add_files("src/hook/linux/libhook/*.cc")
        add_cxxflags("-ffreestanding")
end



target("catter")
    set_kind("binary")
    add_includedirs("src")
    add_files("src/main.cpp")
    if is_plat("windows") then
        add_files("src/hook/windows/impl.cpp")
        add_packages("microsoft-detours")
    end
