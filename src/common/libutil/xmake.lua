
add_requires("libuv", {system = false, version = "v1.51.0"})

target("libutil")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cc")
    add_deps("librpc")
    add_deps("libconfig")
    add_packages("libuv", {public = true})
    add_packages("spdlog", {public = true})
