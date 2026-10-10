#!/usr/bin/env python3
"""Command-line firmware build without STM32CubeIDE.

Mirrors the Debug configuration in .cproject (same defines, include paths,
CPU/FPU flags, -O0 -g3 — Debug sets no optimization level, so CubeIDE uses its
-O0 default — linker script, nano specs). CubeIDE remains the
reference build: if compiler options change there, change them here too.

    python tools/build.py              # incremental build -> build/Dash.elf
    python tools/build.py --clean      # wipe build/ first
    python tools/build.py --toolchain C:/path/to/arm-gnu-toolchain/bin
    python tools/build.py -D OILP_DPID_ENABLE=1   # experiment build -> build-oilp_dpid_enable_1/

-D NAME=VALUE (repeatable) adds a define on top of the project's own, for an
explicit experiment build. It goes to its own build-<name>/ directory, so the
normal build/ is never silently a variant (the dependency check cannot see a
changed -D).

The toolchain is looked up in --toolchain, then $ARM_GCC_DIR, then PATH, then
~/tools/arm-gnu-toolchain-*/bin. Warnings in Core/ (our code) are listed at the
end; vendor warnings (HAL, FreeRTOS, LVGL) are only counted.
"""
import argparse
import concurrent.futures
import os
import re
import shutil
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
OUT = ROOT / "build"

SRC_DIRS = ["Core", "Drivers", "Middlewares"]

DEFINES = ["DEBUG", "USE_PWR_LDO_SUPPLY", "USE_HAL_DRIVER", "STM32H743xx"]

INCLUDES = [
    "Core/Inc",
    "Drivers/CMSIS/RTOS2/Include",
    "Middlewares/lvgl",
    "Drivers/STM32H7xx_HAL_Driver/Inc",
    "Drivers/STM32H7xx_HAL_Driver/Inc/Legacy",
    "Middlewares/Third_Party/FreeRTOS/Source/include",
    "Middlewares/Third_Party/FreeRTOS/Source/CMSIS_RTOS_V2",
    "Middlewares/Third_Party/FreeRTOS/Source/portable/GCC/ARM_CM4F",
    "Drivers/CMSIS/Device/ST/STM32H7xx/Include",
    "Drivers/CMSIS/Include",
]

CPU = ["-mcpu=cortex-m7", "-mfpu=fpv5-d16", "-mfloat-abi=hard", "-mthumb"]

CFLAGS = CPU + [
    "-std=gnu11", "-g3", "-O0",
    "-ffunction-sections", "-fdata-sections", "-fstack-usage",
    "-Wall", "--specs=nano.specs",
] + [f"-D{d}" for d in DEFINES] + [f"-I../{i}" for i in INCLUDES]

ASFLAGS = CPU + ["-g3", "-DDEBUG", "-x", "assembler-with-cpp", "--specs=nano.specs"]

LDSCRIPT = ROOT / "STM32H743VITX_FLASH.ld"


def find_toolchain(arg):
    candidates = []
    if arg:
        candidates.append(Path(arg))
    if os.environ.get("ARM_GCC_DIR"):
        candidates.append(Path(os.environ["ARM_GCC_DIR"]))
    on_path = shutil.which("arm-none-eabi-gcc")
    if on_path:
        candidates.append(Path(on_path).parent)
    candidates += sorted((Path.home() / "tools").glob("arm-gnu-toolchain-*/bin"), reverse=True)
    for c in candidates:
        gcc = c / ("arm-none-eabi-gcc.exe" if os.name == "nt" else "arm-none-eabi-gcc")
        if gcc.exists():
            return c
    sys.exit("arm-none-eabi-gcc not found (use --toolchain or ARM_GCC_DIR)")


def tool(bindir, name):
    return str(bindir / (f"arm-none-eabi-{name}.exe" if os.name == "nt" else f"arm-none-eabi-{name}"))


def sources():
    out = []
    for d in SRC_DIRS:
        for ext in ("*.c", "*.s"):
            out += (ROOT / d).rglob(ext)
    return sorted(out)


def obj_for(src):
    return OUT / "obj" / src.relative_to(ROOT).with_suffix(".o")


def up_to_date(src, obj):
    dep = obj.with_suffix(".d")
    if not obj.exists() or not dep.exists():
        return False
    t = obj.stat().st_mtime
    text = dep.read_text(errors="replace").replace("\\\n", " ")
    for line in text.splitlines():
        # "target: deps" — split on a colon followed by whitespace/end so a
        # Windows drive letter ("C:/...") is not mistaken for the separator.
        parts = re.split(r":(?:\s|$)", line, maxsplit=1)
        if len(parts) < 2:
            continue
        for p in parts[1].split():
            f = Path(p)
            if not f.is_absolute():          # deps are relative to build/
                f = OUT / f
            if f.exists() and f.stat().st_mtime > t:
                return False
    return src.stat().st_mtime <= t


def compile_one(bindir, src):
    obj = obj_for(src)
    if up_to_date(src, obj):
        return src, None, ""
    obj.parent.mkdir(parents=True, exist_ok=True)
    flags = ASFLAGS if src.suffix == ".s" else CFLAGS
    # Compile from build/ with "../Core/..." paths, exactly like CubeIDE does
    # from Debug/: __FILE__ (embedded in LVGL log strings) then has the same
    # length, so the image size is comparable with a CubeIDE build.
    rel = Path(os.path.relpath(src, OUT)).as_posix()
    cmd = [tool(bindir, "gcc"), *flags, "-MMD", "-MP", "-c", rel, "-o", str(obj)]
    r = subprocess.run(cmd, capture_output=True, text=True, cwd=OUT)
    return src, r.returncode, r.stderr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--toolchain")
    ap.add_argument("--clean", action="store_true")
    ap.add_argument("--jobs", type=int, default=os.cpu_count() or 4)
    ap.add_argument("-D", dest="defines", action="append", default=[], metavar="NAME=VALUE")
    args = ap.parse_args()

    global OUT, CFLAGS
    if args.defines:
        tag = "_".join(re.sub(r"[^A-Za-z0-9]+", "_", d).lower() for d in args.defines)
        OUT = ROOT / f"build-{tag}"
        CFLAGS = CFLAGS + [f"-D{d}" for d in args.defines]
        print(f"experiment build: {' '.join('-D' + d for d in args.defines)} -> {OUT.name}/")

    bindir = find_toolchain(args.toolchain)
    if args.clean and OUT.exists():
        shutil.rmtree(OUT)
    OUT.mkdir(exist_ok=True)

    srcs = sources()
    failed, core_warn, vendor_warn, built = [], [], 0, 0
    with concurrent.futures.ThreadPoolExecutor(args.jobs) as ex:
        for src, rc, err in ex.map(lambda s: compile_one(bindir, s), srcs):
            if rc is None:
                continue
            built += 1
            rel = src.relative_to(ROOT).as_posix()
            if rc != 0:
                failed.append((rel, err))
            elif "warning:" in err:
                if rel.startswith("Core/"):
                    core_warn.append((rel, err))
                else:
                    vendor_warn += err.count("warning:")

    print(f"compiled {built} of {len(srcs)} files")
    for rel, err in failed:
        print(f"\nERROR in {rel}:\n{err}")
    if failed:
        sys.exit(f"{len(failed)} file(s) failed to compile")

    elf = OUT / "Dash.elf"
    # Response file: ~650 object paths overflow the Windows command-line limit.
    # Forward slashes, because gcc treats a backslash in @file as an escape.
    objlist = OUT / "objects.list"
    objlist.write_text("\n".join(f'"{obj_for(s).as_posix()}"' for s in srcs) + "\n")
    ldcmd = [tool(bindir, "gcc"), *CPU, "-T", str(LDSCRIPT), f"@{objlist.as_posix()}", "-o", str(elf),
             "--specs=nosys.specs", "--specs=nano.specs", "-static",
             "-Wl,--gc-sections", f"-Wl,-Map={OUT / 'Dash.map'}",
             "-Wl,--start-group", "-lc", "-lm", "-Wl,--end-group"]
    r = subprocess.run(ldcmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.exit(f"LINK FAILED:\n{r.stderr}")
    if r.stderr.strip():
        print(f"linker:\n{r.stderr}")

    for rel, err in core_warn:
        print(f"\nwarning in {rel}:\n{err}")
    print(f"\nwarnings: {sum(e.count('warning:') for _, e in core_warn)} in Core/, {vendor_warn} in vendor code")
    print(subprocess.run([tool(bindir, "size"), str(elf)], capture_output=True, text=True).stdout)


if __name__ == "__main__":
    main()
