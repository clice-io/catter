# ruff: noqa: F821
import sys
import os
import subprocess
import json
import platform
import glob
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


def prepend_path(dir_path: str) -> None:
    current_path = config.environment.get("PATH", os.environ.get("PATH", ""))
    config.environment["PATH"] = os.pathsep.join(filter(None, [dir_path, current_path]))


def find_windows_msvc_asan_dir() -> str | None:
    asan_dll = "clang_rt.asan_dynamic-x86_64.dll"
    host_candidates = [
        ("Hostx64", "x64"),
        ("Hostx86", "x64"),
    ]

    vctools_dir = os.environ.get("VCToolsInstallDir")
    if vctools_dir:
        for host_arch, target_arch in host_candidates:
            candidate = os.path.join(
                vctools_dir, "bin", host_arch, target_arch, asan_dll
            )
            if os.path.isfile(candidate):
                return os.path.dirname(candidate)

    program_files_x86 = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = os.path.join(
        program_files_x86, "Microsoft Visual Studio", "Installer", "vswhere.exe"
    )
    if os.path.isfile(vswhere):
        install_path = run(
            f'"{vswhere}" -latest -products * '
            "-requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 "
            "-property installationPath"
        ).strip()
        if install_path:
            for host_arch, target_arch in host_candidates:
                pattern = os.path.join(
                    install_path,
                    "VC",
                    "Tools",
                    "MSVC",
                    "*",
                    "bin",
                    host_arch,
                    target_arch,
                    asan_dll,
                )
                matches = sorted(glob.glob(pattern), reverse=True)
                if matches:
                    return os.path.dirname(matches[0])

    return None


def find_windows_asan_dir(compiler: str) -> str | None:
    compiler_name = os.path.basename(compiler).lower()

    if "cl" in compiler_name and "clang" not in compiler_name:
        return find_windows_msvc_asan_dir()

    return None


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
        if project_mode == "debug":
            compiler = hook_config["compilers"][0]["program"]
            asan_dir = find_windows_asan_dir(compiler)
            if asan_dir:
                prepend_path(asan_dir)
    case "Linux":
        if project_mode == "debug":
            compiler = hook_config["compilers"][0]["program"]
            if "g++" in compiler and "clang" not in compiler:
                asan_path = run(f"{compiler} -print-file-name=libasan.so").strip()
                hook_path = f"LD_PRELOAD={asan_path} {hook_path}"

config.substitutions.append(("%it_catter_hook", hook_path))
config.substitutions.append(("%it_catter_proxy", proxy_path))
