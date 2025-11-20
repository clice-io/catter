add_requires("quickjs-ng")

target("libqjs")
    set_kind("static")
    add_packages("quickjs-ng", {public = true})
    add_includedirs("src", {public = true})
    add_files("src/*.cc")
