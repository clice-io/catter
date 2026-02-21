# ruff: noqa: F821
import sys
import os
import subprocess
import json
import platform
import lit.formats
from lit.llvm import config as cfg


def run(cmd: str) -> str:
    process = subprocess.run(cmd, shell=True, capture_output=True, text=True)

    if process.returncode != 0:
        error_msg = f"Command '{cmd}' failed with exit code {process.returncode}:\n{process.stderr}"
        print(error_msg, file=sys.stderr)
        raise RuntimeError(error_msg)
    return process.stdout


def run_with_json(cmd: str) -> dict:
    return json.loads(run(cmd))


llvm_config = cfg.LLVMConfig(lit_config, config)

config.name = "Catter Integration Test"
config.test_format = lit.formats.ShTest(True)
config.suffixes = [".test", ".cc"]

project_config = run_with_json("xmake show --json")

project_root = project_config["project"]["projectdir"]
project_mode = project_config["project"]["mode"]

config.test_source_root = os.path.normpath(f"{project_root}/tests/integration/test")
config.test_exec_root = os.path.normpath(f"{project_root}/build/lit-tests")

hook_config = run_with_json("xmake show -t it-catter-hook --json")
hook_path = os.path.join(project_root, hook_config["targetfile"])

proxy_config = run_with_json("xmake show -t it-catter-proxy --json")
proxy_path = os.path.join(project_root, proxy_config["targetfile"])

match platform.system():
    case "Windows":
        config.test_format = lit.formats.ShTest(False)
    case "Linux":
        if project_mode == "debug":
            compiler = hook_config["compilers"][0]["program"]
            if "g++" in compiler and "clang" not in compiler:
                asan_path = run(f"{compiler} -print-file-name=libasan.so").strip()
                hook_path = f"LD_PRELOAD={asan_path} {hook_path}"

config.substitutions.append(("%it_catter_hook", hook_path))
config.substitutions.append(("%it_catter_proxy", proxy_path))
