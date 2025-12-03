add_requires("quickjs-ng")

target("libqjs")
    set_kind("static")
    add_packages("quickjs-ng", {public = true})
    add_includedirs("include", {public = true})
    add_deps("libutil")
    add_files("src/*.cc")
