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
    python tools/build.py -D BACKLIGHT_ACTIVE_LOW=0 --flash   # build, then flash that image
    python tools/build.py --lint       # extra -O2 warnings over Core/Src, then build

-D NAME=VALUE (repeatable) adds a define on top of the project's own, for an
explicit experiment build. It goes to its own build-<name>/ directory, so the
normal build/ is never silently a variant (the dependency check cannot see a
changed -D).

--lint first compiles every Core/Src file once more at -O2 with
-Warray-bounds=2 / -Wstringop-overflow=4 / -Wextra, output discarded, and stops
on any warning. GCC only runs those checks with optimisation on, so the -O0
Debug build cannot see e.g. an index past the end of an array: page[3] holding
four page handles went through the normal build with 0 warnings and broke page
switching (2026-10-22). The image itself is still the -O0 Debug build.

--flash writes the image just built over ST-LINK (SWD) with STM32CubeProgrammer
(STM32_Programmer_CLI on PATH, else C:/ST/STM32CubeCLT_*/STM32CubeProgrammer/bin),
verifies it and resets the board: the variant's build-<name>/ path is worked out
here, so the VS Code task does not have to.

The toolchain is looked up in --toolchain, then $ARM_GCC_DIR, then PATH, then
~/tools/arm-gnu-toolchain-*/bin. Warnings in Core/ (our code) are listed at the
end; vendor warnings (HAL, FreeRTOS, LVGL) are only counted.
"""
import argparse
import concurrent.futures
import hashlib
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
] + [f"-D{d}" for d in DEFINES] + [f"-I../{i}" for i in INCLUDES] + [
    "-Igen",   # <out>/gen/fw_version.h (git hash, see write_version_header)
]

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


def flags_stamp(bindir):
    """Compiler version + every compile flag. The .d files only know sources
    and headers, so without this a changed flag or toolchain would leave old
    objects in place (review C7, build)."""
    r = subprocess.run([tool(bindir, "gcc"), "--version"], capture_output=True, text=True)
    ver = (r.stdout.splitlines() or ["?"])[0]
    blob = "\n".join([ver, " ".join(CFLAGS), " ".join(ASFLAGS)])
    return hashlib.sha256(blob.encode()).hexdigest()


def write_version_header():
    """gen/fw_version.h for Core/Inc/fw_info.h: the commit, and whether the
    firmware sources differ from it. Rewritten only when that changes, so only
    the files that include it recompile."""
    def git(*a):
        try:
            r = subprocess.run(["git", *a], capture_output=True, text=True, cwd=ROOT)
        except OSError:
            return None
        return r.stdout.strip() if r.returncode == 0 else None
    sha = git("rev-parse", "--short=7", "HEAD") or "nogit"
    # Only what goes into the image counts, not e.g. editor settings.
    dirty = git("status", "--porcelain", "--", "Core", "Drivers", "Middlewares",
                "STM32H743VITX_FLASH.ld")
    text = ("/* generated by tools/build.py: do not edit */\n"
            f'#define FW_GIT       "{sha}"\n'
            f"#define FW_GIT_DIRTY {1 if dirty else 0}\n")
    f = OUT / "gen" / "fw_version.h"
    f.parent.mkdir(parents=True, exist_ok=True)
    if not f.exists() or f.read_text() != text:
        f.write_text(text)
    return sha + ("+" if dirty else "")


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
    ap.add_argument("--flash", action="store_true", help="flash the image over ST-LINK once it has built")
    ap.add_argument("--lint", action="store_true", help="-O2 bounds/overflow warnings over Core/Src first")
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
    stamp, want = OUT / "flags.stamp", flags_stamp(bindir)
    if not stamp.exists() or stamp.read_text().strip() != want:
        if (OUT / "obj").exists():
            print("compiler or flags changed since the last build: rebuilding everything")
            shutil.rmtree(OUT / "obj")
        stamp.write_text(want + "\n")
    print(f"firmware {write_version_header()}")

    if args.lint:
        lint(bindir)

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
    if args.flash:
        flash(elf)


LINT_FLAGS = ["-O2", "-Wextra", "-Wno-unused-parameter", "-Warray-bounds=2",
              "-Wstringop-overflow=4", "-Wformat-truncation=1"]


def lint(bindir):
    srcs = sorted((ROOT / "Core" / "Src").glob("*.c"))
    def one(src):
        rel = Path(os.path.relpath(src, OUT)).as_posix()
        r = subprocess.run([tool(bindir, "gcc"), *CFLAGS, *LINT_FLAGS, "-c", rel, "-o", os.devnull],
                           capture_output=True, text=True, cwd=OUT)
        return src, r.returncode, r.stderr
    bad = []
    with concurrent.futures.ThreadPoolExecutor(os.cpu_count() or 4) as ex:
        for src, rc, err in ex.map(one, srcs):
            # only our own lines: vendor headers may warn at -O2
            ours = [ln for ln in err.splitlines() if "Core/" in ln and ("warning:" in ln or "error:" in ln)]
            if rc != 0 or ours:
                bad.append((src.relative_to(ROOT).as_posix(), err))
    print(f"lint: {len(srcs)} Core/Src files at -O2, {len(bad)} with findings")
    for rel, err in bad:
        print(f"\n{rel}:\n{err}")
    if bad:
        sys.exit("lint failed")


def find_programmer():
    exe = "STM32_Programmer_CLI" + (".exe" if os.name == "nt" else "")
    found = shutil.which(exe)
    if found:
        return found
    for d in sorted(Path("C:/ST").glob("STM32CubeCLT_*/STM32CubeProgrammer/bin"), reverse=True):
        if (d / exe).exists():
            return str(d / exe)
    sys.exit("STM32_Programmer_CLI not found (install STM32CubeCLT or STM32CubeProgrammer)")


def flash(elf):
    print(f"flashing {elf.relative_to(ROOT).as_posix()} over ST-LINK")
    r = subprocess.run([find_programmer(), "-c", "port=SWD", "-w", str(elf), "-v", "-rst"])
    if r.returncode != 0:
        # "No debug probe detected" with the probe plugged in usually means
        # stlinkserver.exe (or an open debug session) holds it.
        sys.exit("FLASH FAILED")


if __name__ == "__main__":
    main()
