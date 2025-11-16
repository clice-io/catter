target("catter-hook")
    set_kind("shared")
    add_includedirs("libhook")
    add_includedirs("common")
    add_files("libhook/*.cc")
    add_files("libhook/inject/*.cc")
    add_syslinks("dl")
    if is_mode("release") then
        add_cxxflags("-fvisibility=hidden")
        add_cxxflags("-nostdlib++")
    end
