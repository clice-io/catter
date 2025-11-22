target("libutil")
    set_kind("static")
    add_includedirs("..", {public = true})
    add_files("*.cc")
