
if is_mode("debug") then
    add_defines("DEBUG")
end

if is_plat("linux") then
    add_defines("CATTER_LINUX")
elseif is_plat("macosx") then
    add_defines("CATTER_MAC")
elseif is_plat("windows") then
    add_defines("CATTER_WINDOWS")
end

if is_plat("windows") then
    includes("src/windows")
elseif is_plat("linux", "macosx") then
    includes("src/linux")
end
