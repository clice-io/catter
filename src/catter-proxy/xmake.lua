add_deps("libutil")
add_deps("librpc")

includes("libhook")


target("catter-proxy")
    set_kind("binary")
    add_includedirs(".")
    add_includedirs("src")
    add_files("src/*.cc")
    add_deps("libhook")
