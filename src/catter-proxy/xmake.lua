
if is_plat("windows") then
    add_requires("microsoft-detours", {system = false, version = "2023.6.8"})

    target("catter-hook64")
        set_kind("shared")
        add_includedirs("src")
        add_files("payload/win/main.cc")
        add_syslinks("user32", "advapi32")
        add_packages("microsoft-detours")
        add_cxxflags("-fno-exceptions -fno-rtti")

elseif is_plat("linux", "macosx") then
    target("catter-hook-unix")
        set_kind("shared")
        if is_mode("debug") then
            add_deps("libutil")
        end
        add_deps("libconfig")
        add_includedirs("src")
        add_includedirs("payload/linux-mac")
        add_files("payload/linux-mac/*.cc")
        add_files("payload/linux-mac/inject/*.cc")
        add_syslinks("dl")
        if is_mode("release") then
            add_cxxflags("-fvisibility=hidden")
            add_cxxflags("-nostdlib++")
        end
end

target("catter-proxy")
    set_kind("binary")
    add_deps("libutil")
    add_deps("libconfig")
    add_includedirs("src")
    add_files("src/main.cc", "src/constructor.cc")
    if is_plat("windows") then
        add_files("src/hook/win/impl.cc")
        add_packages("microsoft-detours")
    elseif is_plat("linux", "macosx") then
        add_files("src/hook/linux-mac/impl.cc")
    end
