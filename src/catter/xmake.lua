includes("libqjs")


target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_rules("js-lib")
    add_files("../../api/src/*.ts")
    add_includedirs("src", {public = true})
    add_files("src/**/*.cc")
    add_files("src/*.cc")
    add_deps("libqjs")
     before_build(function (target)
        local data = io.readfile("api/output/lib/lib.js")
        io.print("src/common/libconfig/include/libconfig/js-lib.inc", "R\"(\n%s\n)\"", data)
    end)

target("catter")
    set_kind("binary")
    add_deps("catter-core")
    add_files("main.cc")
