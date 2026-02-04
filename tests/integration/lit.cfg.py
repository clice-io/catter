# ruff: noqa: F821
import sys
import os
import lit.formats
import subprocess


config.name = "Catter Integration Test"
config.test_format = lit.formats.ShTest(True)
config.suffixes = [".js", ".c", ".cpp", ".cc", ".ts"]

project_root = os.path.abspath(os.path.join(__file__, "..", "..", ".."))

config.test_source_root = os.path.join(project_root, "tests", "integration", "test")
config.test_exec_root = os.path.join(project_root, "build", "lit-tests")

is_macos = sys.platform == "darwin"
is_linux = sys.platform.startswith("linux")

if is_linux or is_macos:
    hook_path = ""
    try:
        xmake_output = subprocess.getoutput("xmake show -t catter-hook-unix")

        lib_suffix = ".dylib" if is_macos else ".so"

        for line in xmake_output.splitlines():
            line = line.strip()
            if "targetfile" in line:
                raw_path = (
                    line.split(":", 1)[1].split(lib_suffix)[0].strip() + lib_suffix
                )
                hook_path = os.path.join(project_root, raw_path)
                break
    except Exception as e:
        print(f"Error parsing xmake output: {e}")

    if not hook_path:
        raise RuntimeError(
            "Could not find catter-hook-unix target file path from xmake"
        )

    preload_var_name = "DYLD_INSERT_LIBRARIES" if is_macos else "LD_PRELOAD"

    proxy_script = os.path.join(project_root, "scripts", "src", "fake-catter-proxy.ts")
    catter_proxy_cmd = "%t"

    key_parent_id = "__key_catter_command_id_v1"
    key_proxy_path = "__key_catter_proxy_path_v1"

    config.substitutions.append(("%catter-proxy", catter_proxy_cmd))
    config.substitutions.append(("%catter-hook-path", hook_path))

    if is_macos:
        config.substitutions.append(("%cc", "clang++ -std=c++23 -fuse-ld=lld %s -o %t"))
    else:
        config.substitutions.append(("%cc", "g++ -std=c++23 %s -o %t"))

    inject_cmd_str = (
        f"{preload_var_name}={hook_path} "
        f"{key_proxy_path}='{catter_proxy_cmd}' "
        f"{key_parent_id}="
    )

    config.substitutions.append(("%inject-hook", inject_cmd_str))

else:
    config.substitutions.append(("%cc", "cl %s"))
