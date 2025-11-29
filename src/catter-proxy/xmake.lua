includes("libhook")


target("catter-proxy")
    set_kind("binary")
    add_deps("libhook")
    add_deps("libutil")
    add_deps("librpc")
    add_deps("libconfig")
    add_includedirs("src")
    add_files("src/*.cc")
