#!/usr/bin/env python3
"""Render the cluster pages on the PC, without the board.

    python tools/ui_render.py                    # -> build/render/<page>.png
    python tools/ui_render.py -D UI_PALETTE_TN=0 # any -D the firmware takes
    python tools/ui_render.py --scale 2

Compiles the real cluster_ui.c, its fonts and the pure modules it reads
(alert, trip_stats, speed_cal, perf, odo) plus LVGL itself for the host, with
the hardware-facing functions stubbed in tools/ui_render/stubs.c, fills
g_obd with one scene (tools/ui_render/main.c) and writes each page as a PNG
exactly 320x480 (or scaled). Positions, fonts, colours and text overflow
are what the panel gets; the TN panel's own look (angle, gamma) is not.

Compiler: the same lookup as host_test.py (gcc/clang, else zig). PNG: Pillow.
"""
import argparse
import concurrent.futures as cf
import os
import subprocess
import sys
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
    args = ap.parse_args()
    cc = find_cc(args.cc)

    tag = "_".join(d.lower().replace("=", "_") for d in args.defines)
    out = ROOT / "build" / ("render" + ("-" + tag if tag else ""))
    lv_obj_dir = ROOT / "build" / "render-lvgl"
    base = ["-std=gnu11", "-O1", "-g0", "-w", f"-I{LVGL}"]

    # LVGL: compiled once, shared by every variant.
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
    run([*cc, f"@{rsp}", "-o", str(exe), "-lm"])
    run([str(exe), str(out)])

    from PIL import Image
    pages = []
    for raw in sorted(out.glob("*.rgb565")):
        data = raw.read_bytes()
        w, h = 320, 480
        img = Image.new("RGB", (w, h))
        px = img.load()
        for y in range(h):
            for x in range(w):
                v = data[2 * (y * w + x)] | (data[2 * (y * w + x) + 1] << 8)
                r, g, b = (v >> 11) & 0x1F, (v >> 5) & 0x3F, v & 0x1F
                px[x, y] = (r << 3 | r >> 2, g << 2 | g >> 4, b << 3 | b >> 2)
        if args.scale > 1:
            img = img.resize((w * args.scale, h * args.scale), Image.NEAREST)
        png = raw.with_suffix(".png")
        img.save(png)
        raw.unlink()
        pages.append(png)
    for p in pages:
        print(p)


if __name__ == "__main__":
    main()
