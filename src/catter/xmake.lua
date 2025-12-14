includes("libqjs")

target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_includedirs("src", {public = true})
    add_files("src/**.cc")
    add_deps("libqjs")

    add_files("../../api/src/*.ts", {always_added = true})
    add_rules("build.js", {js_target = "build-js-lib", js_file = "api/output/lib/lib.js"})

target("catter")
    set_kind("binary")
    add_deps("catter-core")
    add_files("main.cc")
