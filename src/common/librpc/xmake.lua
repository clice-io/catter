target("librpc")
    set_kind("static")
    add_includedirs("include", {public = true})
    add_files("src/*.cc")
