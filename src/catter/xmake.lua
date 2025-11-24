includes("libqjs")


target("catter")
    set_kind("binary")
    add_includedirs("src")
    add_files("src/*.cc")
    add_deps("libqjs")
