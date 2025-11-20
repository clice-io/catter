
if is_mode("debug") then
    add_defines("DEBUG")
end

if is_plat("linux") then
    add_defines("CATTER_LINUX")
elseif is_plat("macosx") then
    add_defines("CATTER_MAC")
end

if is_plat("windows") then
    includes("src/windows")
elseif is_plat("linux", "macosx") then
    includes("src/linux")
end

target("libhook")
    set_kind("static")
    add_includedirs("src", {public = true})
    if is_plat("windows") then
        add_files("src/windows/impl.cc")
    elseif is_plat("linux", "macosx") then
        add_files("src/linux/*.cc")
    end

    if is_plat("windows") then
        add_packages("microsoft-detours")
    end

    add_deps("libcommand")
