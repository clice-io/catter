target("catter-hook-linux")
    set_kind("shared")
    add_includedirs("libhook")
    add_files("libhook/*.cc")
    add_files("libhook/inject/*.cc")
    add_syslinks("dl")

    if is_mode("release") then
        add_ldflags("--version-script=libhook.ver")
        add_cxxflags("-fvisibility=hidden")
        add_cxxflags("-nostdlib++")
    elseif is_mode("debug") then
        add_defines("DEBUG")
    end
