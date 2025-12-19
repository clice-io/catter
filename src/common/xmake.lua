


add_requires("libuv", {version = "v1.51.0"})

target("common")
    set_kind("static")
    add_includedirs("src", {public = true})
    add_files("src/**.cc")
    add_packages("libuv", {public = true})
    add_packages("spdlog", {public = true})
