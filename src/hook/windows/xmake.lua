add_requires("microsoft-detours")

target("catter-hook64")
    set_kind("shared")
    add_includedirs("../../../src")
    add_files("hook.cpp")
    add_syslinks("user32", "advapi32")
    add_packages("microsoft-detours")
