#!/usr/bin/env python3
"""Use the workspace's CHGame tools and native RISC-V compiler for Tetris."""
from pathlib import Path
import os
import platform
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parent
(ROOT / ".cache/tmp").mkdir(parents=True, exist_ok=True)
env = dict(os.environ)
env.update({
    "ARDUINO_CONFIG_FILE": str(ROOT / "arduino-cli.yaml"),
    "ARDUINO_DIRECTORIES_DATA": str(ROOT / ".arduino15"),
    "ARDUINO_DIRECTORIES_DOWNLOADS": str(ROOT / ".arduino15/staging"),
    "ARDUINO_DIRECTORIES_USER": str(ROOT / "sketchbook"),
    "ARDUINO_BUILD_CACHE_PATH": str(ROOT / ".arduino15/cache"),
    "UV_CACHE_DIR": str(ROOT / ".cache/uv"),
    "TMPDIR": str(ROOT / ".cache/tmp"),
})
args = sys.argv[1:] or ["build"]
native_builder = ROOT / "tools/build_current_gcc.py"
native_mac = platform.system() == "Darwin" and platform.machine() == "arm64"
if args[0] == "native-toolchain":
    raise SystemExit(subprocess.call([sys.executable, str(native_builder), "--activate", *args[1:]],
                                    env=env, cwd=ROOT))
if args == ["setup"]:
    if sys.version_info < (3, 12):
        raise SystemExit("Setup requires Python 3.12 or later for the pinned dependencies.")
    for program in ("git", "uv", "arduino-cli"):
        if not shutil.which(program):
            raise SystemExit(f"Install {program} and add it to PATH before setup.")

    def run(*command):
        subprocess.run(command, cwd=ROOT, env=env, check=True)

    run("git", "submodule", "update", "--init", "--", "CHGame")
    if not (ROOT / ".venv/bin/python").exists():
        run("uv", "venv", "--python", sys.executable, str(ROOT / ".venv"))
    run("uv", "pip", "install", "--python", str(ROOT / ".venv/bin/python"),
        "--requirement", str(ROOT / "requirements.txt"))
    run("arduino-cli", "core", "update-index")
    run("arduino-cli", "core", "install", "CHGame:ch32v@0.3.0")
    if native_mac:
        run(sys.executable, str(native_builder), "--activate")
    raise SystemExit(0)
needs_compiler = (args[0] in ("build", "upload", "card") or
                  args[0] == "export" and "--no-build" not in args or
                  args[0] == "run" and "--device" in args)
if native_mac and needs_compiler:
    native = ROOT / ".toolchains/current/native"
    if not (native / "chgame-current.json").exists():
        raise SystemExit("Run python3 tetris.py native-toolchain first.")
    overrides = ROOT / ".arduino15/packages/CHGame/hardware/ch32v/0.3.0/platform.local.txt"
    if not overrides.exists() or str(native / "bin") not in overrides.read_text():
        subprocess.run([sys.executable, str(ROOT / "tools/build_current_gcc.py"), "--stage", "verify", "--activate"],
                       cwd=ROOT, env=env, check=True)
python = Path(os.environ.get("CHGAME_PYTHON", ROOT / ".venv/bin/python"))
if not python.exists():
    python = Path(sys.executable)
raise SystemExit(subprocess.call([str(python), str(ROOT / "CHGame/tools/chgame.py"),
                                "--sketch", str(ROOT / "Tetris"), *args], env=env, cwd=ROOT))
