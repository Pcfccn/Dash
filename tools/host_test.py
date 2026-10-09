#!/usr/bin/env python3
"""Build and run the host-side unit tests in tests/host.

    python tools/host_test.py              # finds a compiler, builds, runs
    python tools/host_test.py --cc clang

Compiler lookup: --cc, $CC, gcc/clang on PATH, then zig (the `ziglang` pip
package, in this interpreter or ~/tools/hostcc-venv). UndefinedBehaviorSanitizer
is always on; AddressSanitizer is added where the toolchain supports it
(not on Windows).
"""
import argparse
import importlib.util
import os
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build" / "host"
TESTS = sorted((ROOT / "tests" / "host").glob("test_*.c"))


def find_cc(arg):
    if arg:
        return arg.split()
    if os.environ.get("CC"):
        return os.environ["CC"].split()
    for name in ("gcc", "clang"):
        if shutil.which(name):
            return [name]
    if importlib.util.find_spec("ziglang"):
        return [sys.executable, "-m", "ziglang", "cc"]
    venv_py = Path.home() / "tools" / "hostcc-venv" / ("Scripts/python.exe" if os.name == "nt" else "bin/python")
    if venv_py.exists():
        return [str(venv_py), "-m", "ziglang", "cc"]
    sys.exit("no host C compiler found (install gcc/clang, or: pip install ziglang)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc")
    args = ap.parse_args()
    cc = find_cc(args.cc)
    OUT.mkdir(parents=True, exist_ok=True)

    san = ["-fsanitize=undefined", "-fsanitize-trap=undefined"]
    if os.name != "nt":
        san = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]

    failed = []
    for t in TESTS:
        exe = OUT / (t.stem + (".exe" if os.name == "nt" else ""))
        cmd = [*cc, "-std=gnu11", "-g", "-O1", "-Wall", "-Wextra", *san,
               f"-I{ROOT / 'tests/host/mock'}", f"-I{ROOT / 'Core/Inc'}",
               str(t), "-o", str(exe), "-lm"]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f"BUILD FAILED: {t.name}\n{r.stderr}")
            failed.append(t.name)
            continue
        if r.stderr.strip():
            print(f"{t.name} warnings:\n{r.stderr}")
        print(f"== {t.name}", flush=True)
        if subprocess.run([str(exe)]).returncode != 0:
            failed.append(t.name)
    if failed:
        sys.exit(f"\nFAILED: {', '.join(failed)}")
    print("\nall host tests passed")


if __name__ == "__main__":
    main()
