add_requires("quickjs-ng", {system = false, version = "v0.11.0"})

target("libqjs")
    set_kind("static")
    add_packages("quickjs-ng", {public = true})
    add_includedirs("include", {public = true})
    add_deps("libutil")
    add_files("src/*.cc")
