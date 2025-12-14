add_requires("quickjs-ng", {system = false, version = "v0.11.0"})

target("catter-core")
    -- use object, avoid register invalid
    set_kind("object")
    add_includedirs("src", {public = true})
    add_packages("quickjs-ng", {public = true})

    add_deps("libutil")

    add_files("src/**.cc")

    add_files("../../api/src/*.ts", {always_added = true})
    add_rules("build.js", {js_target = "build-js-lib", js_file = "api/output/lib/lib.js"})

target("catter")
    set_kind("binary")
    add_deps("catter-core")
    add_files("main.cc")
