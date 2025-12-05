
add_requires("libuv", {system = false, version = "v1.51.0"})

target("librpc")
    set_kind("static")
    add_packages("libuv", {public = true})
    add_includedirs("include", {public = true})
    add_files("src/*.cc")
