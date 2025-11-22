add_deps("libutil")
add_deps("librpc")

target("libhook")
    set_kind("static")
    add_includedirs(".")
    if is_plat("windows") then
        add_files("impl/windows/impl.cc")
    elseif is_plat("linux", "macosx") then
        add_includedirs("../libhook/src/")
        add_files("impl/linux-mac/*.cc")
    end
    if is_plat("windows") then
        add_packages("microsoft-detours")
    end


target("catter-proxy")
    set_kind("binary")
    add_includedirs(".")
    add_includedirs("src")
    add_files("src/*.cc")
    add_deps("libhook")
