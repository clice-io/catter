
target("libcommand")
    set_kind("static")
    add_files("src/*.cc")
    add_includedirs("src", {public = true})
