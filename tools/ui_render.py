#!/usr/bin/env python3
"""Render the cluster pages on the PC, without the board.

    python tools/ui_render.py                    # -> build/render/<page>.png
    python tools/ui_render.py -D UI_PALETTE_TN=0 # any -D the firmware takes
    python tools/ui_render.py --scale 2
    python tools/ui_render.py --m32              # 32-bit build: LVGL heap use as on the MCU

Compiles the real cluster_ui.c, its fonts and the pure modules it reads
(alert, trip_stats, speed_cal, perf, odo) plus LVGL itself for the host, with
the hardware-facing functions stubbed in tools/ui_render/stubs.c, fills
g_obd with one scene (tools/ui_render/main.c) and writes each page as a PNG
exactly 320x480 (or scaled). Positions, fonts, colours and text overflow
are what the panel gets; the TN panel's own look (angle, gamma) is not.
It also prints the LVGL heap use after building every page. On a 64-bit PC
pointers are 8 bytes and every object is bigger than on the Cortex-M7, so
that figure is too high; --m32 builds for 32-bit x86 (zig or clang with a
32-bit runtime), where it comes out close to the MCU's.

Compiler: the same lookup as host_test.py (gcc/clang, else zig). PNG: written
here (zlib), no third-party package. LVGL objects are rebuilt when lv_conf.h,
LVGL's version or the flags change (conf.stamp in the object directory).
"""
import argparse
import concurrent.futures as cf
import hashlib
import os
import shutil
import struct
import subprocess
import sys
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from host_test import find_cc  # noqa: E402

ROOT = Path(__file__).resolve().parent.parent
LVGL = ROOT / "Middlewares" / "lvgl"
HERE = ROOT / "tools" / "ui_render"
APP_SRC = ["cluster_ui.c", "alert.c", "trip_stats.c", "speed_cal.c", "perf.c", "odo.c"]


def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    if r.returncode != 0:
        sys.stderr.write(" ".join(map(str, cmd)) + "\n" + r.stdout + r.stderr)
        raise SystemExit(1)
    return r


def write_png(path, rgb565, w, h, scale):
    """320x480 little-endian RGB565 -> 8-bit RGB PNG, nearest-neighbour scaled."""
    rows = bytearray()
    for y in range(h):
        line = bytearray()
        for x in range(w):
            v = rgb565[2 * (y * w + x)] | (rgb565[2 * (y * w + x) + 1] << 8)
            r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
            line += bytes((r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)) * scale
        for _ in range(scale):
            rows += b"\x00" + line
    def chunk(tag, data):
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)
    png = (b"\x89PNG\r\n\x1a\n"
           + chunk(b"IHDR", struct.pack(">IIBBBBB", w * scale, h * scale, 8, 2, 0, 0, 0))
           + chunk(b"IDAT", zlib.compress(bytes(rows), 9)) + chunk(b"IEND", b""))
    path.write_bytes(png)


def lvgl_stamp(cc, base):
    """What the LVGL objects depend on beyond their own .c: the project's
    lv_conf.h, LVGL's version header, the compiler and the flags."""
    h = hashlib.sha256()
    for f in (ROOT / "Middlewares" / "lv_conf.h", LVGL / "lv_version.h"):
        if f.exists():
            h.update(f.read_bytes())
    h.update(" ".join([*cc, *base]).encode())
    return h.hexdigest()


def compile_one(cc, flags, src, obj):
    if obj.exists() and obj.stat().st_mtime > src.stat().st_mtime:
        return obj
    obj.parent.mkdir(parents=True, exist_ok=True)
    run([*cc, *flags, "-c", str(src), "-o", str(obj)])
    return obj


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cc")
    ap.add_argument("-D", dest="defines", action="append", default=[], metavar="NAME=VALUE")
    ap.add_argument("--scale", type=int, default=1)
    ap.add_argument("--m32", action="store_true", help="32-bit build (heap use as on the MCU)")
    args = ap.parse_args()
    cc = find_cc(args.cc)
    arch = []
    if args.m32:
        arch = ["-target", "x86-windows-gnu"] if "ziglang" in cc else ["-m32"]

    tag = "_".join(d.lower().replace("=", "_") for d in args.defines + (["m32"] if args.m32 else []))
    out = ROOT / "build" / ("render" + ("-" + tag if tag else ""))
    lv_obj_dir = ROOT / "build" / ("render-lvgl" + ("-m32" if args.m32 else ""))
    base = ["-std=gnu11", "-O1", "-g0", "-w", *arch, f"-I{LVGL}"]

    # LVGL: compiled once, shared by every variant — again after a change to
    # lv_conf.h (object mtimes alone cannot see it: LV_MEM_SIZE, features).
    stamp, want = lv_obj_dir / "conf.stamp", lvgl_stamp(cc, base)
    if lv_obj_dir.exists() and (not stamp.exists() or stamp.read_text().strip() != want):
        print("lv_conf.h, LVGL or the flags changed: rebuilding the LVGL objects")
        shutil.rmtree(lv_obj_dir)
    lv_obj_dir.mkdir(parents=True, exist_ok=True)
    stamp.write_text(want + "\n")
    lv_srcs = sorted((LVGL / "src").rglob("*.c"))
    with cf.ThreadPoolExecutor(max_workers=os.cpu_count() or 4) as ex:
        lv_objs = list(ex.map(
            lambda s: compile_one(cc, base, s, lv_obj_dir / (s.relative_to(LVGL).with_suffix(".o"))),
            lv_srcs))

    # The app: always rebuilt (small), with the requested defines.
    app_flags = base + ["-DUI_RENDER=1", f"-I{HERE}", f"-I{ROOT / 'Core/Inc'}",
                        f"-I{ROOT / 'tests/host/mock'}"] + [f"-D{d}" for d in args.defines]
    srcs = [ROOT / "Core/Src" / s for s in APP_SRC]
    srcs += sorted((ROOT / "Core/Src").glob("montserrat_*.c"))
    srcs += [HERE / "main.c", HERE / "stubs.c"]
    out.mkdir(parents=True, exist_ok=True)
    app_objs = []
    for s in srcs:
        o = out / "obj" / (s.stem + ".o")
        if o.exists():
            o.unlink()
        app_objs.append(compile_one(cc, app_flags, s, o))

    exe = out / ("ui_render" + (".exe" if os.name == "nt" else ""))
    rsp = out / "objs.rsp"          # Windows caps a command line at 32 K
    rsp.write_text("\n".join(o.as_posix() for o in app_objs + lv_objs))
    run([*cc, *arch, f"@{rsp}", "-o", str(exe), "-lm"])
    print(run([str(exe), str(out)]).stdout, end="")

    pages = []
    for raw in sorted(out.glob("*.rgb565")):
        png = raw.with_suffix(".png")
        write_png(png, raw.read_bytes(), 320, 480, max(1, args.scale))
        raw.unlink()
        pages.append(png)
    for p in pages:
        print(p)


if __name__ == "__main__":
    main()
