set_policy("build.ccache", true)

target("hello")
    set_kind("binary")
    add_files("src/*.cc")
    add_includedirs("include")
    add_defines("CATTER_TEST=1")
