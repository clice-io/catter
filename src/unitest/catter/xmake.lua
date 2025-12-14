target("ut-catter")
    set_default(false)
    set_kind("binary")
    add_files("main.cc", "tests/*.cc")
    add_packages("boost_ut")
    add_deps("catter-core", "libconfig", "libutil")

    add_defines(format([[JS_TEST_PATH="%s"]], path.unix(path.join(os.projectdir(), "api/output/test/"))))
    add_rules("build.js", {js_target = "build-js-test"})
    add_files("../../../api/src/*.ts", "../../../api/test/*.ts", "../../../api/test/res/**/*.txt")

    add_tests("default")
