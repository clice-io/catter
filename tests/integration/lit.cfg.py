# ruff: noqa: F821
import sys
import os
import subprocess
import json
from typing import Callable
import lit.formats


def get_cmd_output(cmd: str, fn: Callable[[str], str]) -> str:
    res = ""
    try:
        res = fn(subprocess.getoutput(cmd))
    except Exception as e:
        print(f"Error parsing output of  {cmd}: {e}")
    if not res:
        raise RuntimeError(f"Could not find info from {cmd}")
    return res


config.name = "Catter Integration Test"
config.test_format = lit.formats.ShTest(True)
config.suffixes = [".js", ".c", ".cpp", ".cc", ".ts"]

project_root = get_cmd_output(
    "xmake show --json", lambda r: json.loads(r)["project"]["projectdir"]
)

config.test_source_root = os.path.join(project_root, "tests", "integration", "test")
config.test_exec_root = os.path.join(project_root, "build", "lit-tests")

is_macos = sys.platform == "darwin"
is_linux = sys.platform.startswith("linux")

if is_linux or is_macos:
    hook_path = get_cmd_output(
        "xmake show -t catter-hook-unix --json", lambda r: json.loads(r)["targetfile"]
    )
    hook_path = os.path.join(project_root, hook_path)
    mode = get_cmd_output(
        "xmake show --json", lambda r: json.loads(r)["project"]["mode"]
    )

    preload_var_name = "DYLD_INSERT_LIBRARIES" if is_macos else "LD_PRELOAD"

    proxy_script = os.path.join(project_root, "scripts", "src", "fake-catter-proxy.ts")
    catter_proxy_cmd = "%t"

    key_parent_id = "__key_catter_command_id_v1"
    key_proxy_path = "__key_catter_proxy_path_v1"

    config.substitutions.append(("%catter-proxy", catter_proxy_cmd))
    config.substitutions.append(("%catter-hook-path", hook_path))
    config.substitutions.append(("%filecheck", "FileCheck"))

    # in debug mode, we need asan
    if is_macos:
        config.substitutions.append(("%cc", "clang++ -std=c++23 -fuse-ld=lld %s -o %t"))
    else:
        config.substitutions.append(("%cc", "g++ -std=c++23 %s -o %t"))
        if mode == "debug":
            asan_path = get_cmd_output("g++ -print-file-name=libasan.so", lambda x: x)
            hook_path = f"{asan_path}:{hook_path}"

    inject_cmd_str = (
        f"{preload_var_name}={hook_path} "
        f"{key_proxy_path}='{catter_proxy_cmd}' "
        f"{key_parent_id}="
    )

    config.substitutions.append(("%inject-hook", inject_cmd_str))

else:
    config.substitutions.append(("%cc", "cl %s"))
