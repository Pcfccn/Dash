"""Generate docs/future/carrier-template.pdf: the 7x9 carrier at 1:1 on A4, to print and lay on
the perfboard. Sheet 1 top (component side), sheet 2 bottom (solder side, mirrored: as the
board lies when you solder it), sheet 3 wire labels X1.1...X5.8.

Usage (from the repo root): python tools/drawings/carrier_template.py docs/future/carrier-template.pdf
Needs PyMuPDF (pip install pymupdf). Geometry and placement are the ones of carrier_board.py /
carrier-board.md: change those first, then this. Print at 100 % ("actual size"); check the 50 mm
ruler with a real one."""
import sys
from pathlib import Path

import pymupdf

OUT = Path(sys.argv[1])

P = 2.54
BW, BH = 70.0, 90.0
COLS, ROWS = 24, 30
X0 = (BW - (COLS - 1) * P) / 2
Y0 = (BH - (ROWS - 1) * P) / 2
COLN = "ABCDEFGHIJKLMNOPQRSTUVWX"
OX, OY = 70.0, 52.0                       # board origin on the A4 sheet, mm

RED, BLK, ORG, GRN, GRY, BLU = "#c62828", "#111", "#e07b00", "#1d5a1d", "#777", "#1f5fbf"

P2_EVEN, P2_ODD, P1_EVEN, P1_ODD = 0, 1, 14, 15
def prow(pin): return (pin + 1) // 2      # header pin -> row index
P1 = {18: "CAN TX", 19: "CAN RX", 28: "ШИМ", 41: "SDI", 43: "SCK"}
P2 = {11: "CS", 15: "GND", 19: "→GPS", 20: "←GPS", 25: "DC", 26: "RST",
      30: "▶", 31: "●", 32: "◀", 38: "габар.", 41: "3V3", 42: "5V", 43: "GND", 44: "GND"}

# terminal blocks: name, first hole (col, row), vertical?, pin labels
BLOCKS = [("X5", 22, 1, True, ["VCC", "GND", "CS", "RST", "DC", "SDI", "SCK", "LED"]),
          ("X4", 22, 15, True, ["▶", "●", "◀", "GND", "ILL+", "—"]),
          ("X1", 2, 29, False, ["+12", "GND"]),
          ("X3", 7, 29, False, ["3V3", "GND", "TX", "RX"]),
          ("X2", 15, 29, False, ["H", "L", "GND"])]
ZONES = [(17, 1, 3, 6, "подсветка", "2N7000, R1–R3"), (17, 9, 3, 5, "кнопки", "3×1 к, 3×100 н"),
         (17, 16, 3, 6, "габариты", "BC547 …"), (4, 25, 10, 1.6, "TVS 1.5KE20A", "полоска к X1.1")]


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Sheet:
    def __init__(self, title, lines):
        self.o = ['<svg xmlns="http://www.w3.org/2000/svg" width="210mm" height="297mm" viewBox="0 0 210 297" '
                  'font-family="DejaVu Sans, Arial, sans-serif">', '<rect width="210" height="297" fill="#fff"/>']
        self.text(12, 14, title, 5.2, "#111", weight="bold")
        for i, ln in enumerate(lines):
            self.text(12, 21 + i * 4.6, ln, 3.2, "#333")

    def add(self, s): self.o.append(s)

    def text(self, x, y, t, size=2.0, color="#222", anchor="start", weight="normal"):
        self.add(f'<text x="{x:.2f}" y="{y:.2f}" font-size="{size}" fill="{color}" text-anchor="{anchor}" '
                 f'font-weight="{weight}">{esc(t)}</text>')

    def line(self, pts, color, w=0.3, dash=None):
        d = " ".join(f"{x:.2f},{y:.2f}" for x, y in pts)
        da = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{w}" '
                 f'stroke-linejoin="round" stroke-linecap="round"{da}/>')

    def rect(self, x, y, w, h, stroke, fill="none", sw=0.3, dash=None, rx=0.6, opacity=1.0):
        da = f' stroke-dasharray="{dash}"' if dash else ""
        op = f' fill-opacity="{opacity}"' if opacity < 1.0 else ""
        self.add(f'<rect x="{x:.2f}" y="{y:.2f}" width="{w:.2f}" height="{h:.2f}" rx="{rx}" fill="{fill}" '
                 f'stroke="{stroke}" stroke-width="{sw}"{da}{op}/>')

    def circle(self, x, y, r, stroke, fill="none", sw=0.15):
        self.add(f'<circle cx="{x:.2f}" cy="{y:.2f}" r="{r}" fill="{fill}" stroke="{stroke}" stroke-width="{sw}"/>')

    def ruler(self, x, y):
        self.line([(x, y), (x + 50, y)], BLK, 0.4)
        for i in range(11):
            self.line([(x + 5 * i, y), (x + 5 * i, y - (2.5 if i % 2 == 0 else 1.5))], BLK, 0.3)
        self.text(x + 25, y + 4.5, "50 мм — проверить настоящей линейкой", 2.6, BLK, "middle")

    def svg(self):
        return "\n".join(self.o + ["</svg>"])


def board(s, mirror):
    """Holes, labels, sockets, blocks and zones; mirror = view from the solder side."""
    def hx(c):
        x = X0 + c * P
        return OX + (BW - x if mirror else x)
    def hy(r): return OY + Y0 + r * P
    s.rect(OX, OY, BW, BH, "#4a7a3a", "#f3f8f1", 0.4, rx=2)
    for c in range(COLS):
        s.text(hx(c), OY - 1.4, COLN[c], 1.7, GRY, "middle")
    for r in range(ROWS):
        s.text(OX - 1.3 if not mirror else OX + BW + 1.3, hy(r) + 0.6, str(r + 1), 1.7, GRY,
               "end" if not mirror else "start")
    for c in range(COLS):
        for r in range(ROWS):
            s.circle(hx(c), hy(r), 0.45, "#9fb89a")
    # WeAct outline (top view only: from below it is hidden by the board)
    if not mirror:
        mid = (hx(P2_EVEN) + hx(P1_ODD)) / 2
        s.rect(mid - 20.3, hy(prow(1)) - 6.3, 40.6, hy(prow(43)) - hy(prow(1)) + 12.6, "#2b4f86",
               dash="1.6 1", sw=0.35)
        s.text(mid, hy(prow(1)) - 3.4, "WeAct — сверху, на гнёздах (выводы 1–4 без гнёзд)", 1.8, "#2b4f86",
               "middle", "bold")
    # sockets: P1 pins 5-44, P2 pins 7-44
    for col, first in ((P2_EVEN, 7), (P2_ODD, 7), (P1_EVEN, 5), (P1_ODD, 5)):
        y0, y1 = hy(prow(first)) - 1.25, hy(prow(43)) + 1.25
        s.rect(hx(col) - 1.25, y0, 2.5, y1 - y0, "#333", "#333", 0.2, rx=0.3, opacity=0.25)
    # used header pins: a dot each, and one label per row, beside the header's inner column
    # (towards the board middle: right of P2 / left of P1 from above, the other way from below)
    for hdr, table, inner_col in ((1, P1, P1_EVEN), (2, P2, P2_ODD)):
        rows = {}
        for pin, sig in sorted(table.items()):
            col = (P1_ODD if pin % 2 else P1_EVEN) if hdr == 1 else (P2_ODD if pin % 2 else P2_EVEN)
            color = RED if sig in ("3V3", "5V") else BLK if sig == "GND" else BLU
            s.circle(hx(col), hy(prow(pin)), 0.75, color, color)
            rows.setdefault(prow(pin), []).append((pin, sig, color))
        to_right = (hdr == 2) != mirror
        for r, items in rows.items():
            txt = f"P{hdr}." + " · ".join(f"{pin} {sig}" for pin, sig, _ in items)
            x = hx(inner_col) + (2.0 if to_right else -2.0)
            s.text(x, hy(r) + 0.6, txt, 1.7, items[0][2], "start" if to_right else "end")
    # terminal blocks, pins stepped 0.6 mm in pairs (carrier-board.md section 3)
    for name, c0, r0, vertical, labels in BLOCKS:
        n = len(labels)
        holes = [round(1.5 * k - 0.25) for k in range(n)]
        span = (n * 3.81 + 0.8)
        mid = ((1.5 * 0 - 0.25) + (1.5 * (n - 1) - 0.25)) / 2 * P
        if vertical:
            x = hx(c0)
            ya = hy(r0) + mid - span / 2
            s.rect(x - 3.5, ya, 7.0, span, GRN, "#3a8f3a", 0.35, opacity=0.15)
            s.text(x, ya - 1.0, name, 2.4, GRN, "middle", "bold")
            for k, h in enumerate(holes):
                y = hy(r0 + h)
                s.circle(x, y, 0.8, GRN, GRN)
                side = 1 if not mirror else -1
                s.text(x + side * 4.3, y + 0.6, f"{k+1} {labels[k]}", 1.6, GRN, "start" if side > 0 else "end")
        else:
            y = hy(r0)
            xa_top = hx(c0) + (mid - span / 2) * (1 if not mirror else -1)
            xb_top = hx(c0) + (mid + span / 2) * (1 if not mirror else -1)
            x_lo, x_hi = min(xa_top, xb_top), max(xa_top, xb_top)
            s.rect(x_lo, y - 3.5, x_hi - x_lo, 7.0, GRN, "#3a8f3a", 0.35, opacity=0.15)
            s.text(x_lo - 1.0, y + 0.8, name, 2.4, GRN, "end", "bold")
            for k, h in enumerate(holes):
                x = hx(c0 + h)
                s.circle(x, y, 0.8, GRN, GRN)
                s.text(x, y + 5.6, f"{k+1}", 1.7, GRN, "middle", "bold")
            s.text((x_lo + x_hi) / 2, y + 7.8, " · ".join(f"{k+1} {lb}" for k, lb in enumerate(labels)),
                   1.45, GRN, "middle")
    # zones for the small parts (top view: where they sit; bottom view: same holes)
    for c0, r0, w, h, name, sub in ZONES:
        xa, xb = hx(c0) - 1.0, hx(c0 + w) + (1.0 if not mirror else -1.0)
        x_lo, x_hi = min(xa, xb), max(xa, xb)
        s.rect(x_lo, hy(r0) - 1.0, x_hi - x_lo, h * P + 2.0, "#b35400", "none", 0.3, "1 0.8")
        s.text((x_lo + x_hi) / 2, hy(r0) + 1.0, name, 1.7, "#b35400", "middle", "bold")
        s.text((x_lo + x_hi) / 2, hy(r0) + 3.0, sub, 1.35, "#b35400", "middle")
    return hx, hy


# ------------------------------------------------------------ sheet 1: top
s1 = Sheet("Плата-носитель 7×9 — шаблон 1:1, вид СВЕРХУ (сторона деталей)", [
    "Печатать в масштабе 100 % («фактический размер», без «подогнать»). Сверить линейку 50 мм внизу.",
    "Положить на макетку отверстиями к отверстиям (колонка A слева, ряд 1 сверху) — и ставить детали по меткам.",
    "Синие точки — используемые выводы WeAct, зелёные — выводы клеммников (уже подогнутые, carrier-board.md §3).",
    "Рыжий пунктир — где ставить мелочь (assembly.md, шаги 4, 7, 8). Ряды 24–25 — снизу идут шины: туда детали не ставить."])
hx, hy = board(s1, mirror=False)
s1.rect(OX + 1, hy(23) - 1.3, BW - 2, 2 * P + 0.1, "#e8a0a0", "#c62828", 0.2, rx=0.4, opacity=0.08)
s1.text(OX + BW / 2, hy(24) + 0.6, "ряды 24–25: шины 3V3 / GND снизу", 1.6, RED, "middle")
s1.ruler(OX + 10, 262)

# ------------------------------------------------------------ sheet 2: bottom
s2 = Sheet("Плата-носитель 7×9 — шаблон 1:1, вид СНИЗУ (сторона пайки, зеркально)", [
    "Так плата лежит, когда её паяешь: колонка A справа, ряд 1 сверху. Подписи выводов WeAct и клеммников — как они",
    "видны снизу. Голые шины: 3V3 (красная) и GND (чёрная). 3V3 у P2.41 сначала уходит в колонку C (изолированной",
    "перемычкой), а потом вниз — чтобы не пройти по P2.43, это GND. Остальные провода — в изоляции,",
    "по таблицам assembly.md."])
hx, hy = board(s2, mirror=True)
# 3V3: P2.41 (B, row idx 21) -> C21 -> C23 -> V23 ; GND: P2.43 (B, 22) -> B24 -> V24
s2.line([(hx(1), hy(21)), (hx(2), hy(21))], RED, 0.6, "0.8 0.5")
s2.line([(hx(2), hy(21)), (hx(2), hy(23)), (hx(21), hy(23))], RED, 0.9)
s2.line([(hx(1), hy(22)), (hx(1), hy(24)), (hx(21), hy(24))], BLK, 0.9)
s2.text(hx(2) - 1.0, hy(20) + 0.6, "↓ 3V3: перемычка в изоляции B22 → C22", 1.45, RED, "end")
s2.ruler(OX + 10, 262)

# ------------------------------------------------------------ sheet 3: labels
WIRES = [("X1.1", "+12 В ACC"), ("X1.2", "масса"),
         ("X2.1", "CAN-H · OBD 6"), ("X2.2", "CAN-L · OBD 14"), ("X2.3", "земля · OBD 5"),
         ("X3.1", "3V3 GPS"), ("X3.2", "GND GPS"), ("X3.3", "TX GPS"), ("X3.4", "RX GPS"),
         ("X4.1", "кнопка ▶"), ("X4.2", "кнопка ●"), ("X4.3", "кнопка ◀"), ("X4.4", "GND кнопок"),
         ("X4.5", "ILL+ 12 В"),
         ("X5.1", "VCC экрана"), ("X5.2", "GND экрана"), ("X5.3", "CS"), ("X5.4", "RESET"), ("X5.5", "DC/RS"),
         ("X5.6", "SDI"), ("X5.7", "SCK"), ("X5.8", "LED")]
s3 = Sheet("Бирки на провода жгута — по две на провод (оба конца)", [
    "Вырезать по пунктиру, сложить пополам по средней линии вокруг провода, сверху — прозрачный скотч или прозрачная",
    "термоусадка. Надпись читается с обеих сторон. Последний ряд — бирки на сами вилки X1…X5.",
    "Таблица проводов — harness.md."])
tags = [w for w in WIRES for _ in (0, 1)] + [(f"X{i}", "вилка") for i in range(1, 6) for _ in (0, 1)]
TW, TH, COLS3 = 30.0, 12.0, 6
x0, y0 = 15.0, 40.0
for i, (lab, sub) in enumerate(tags):
    cx, cy = x0 + (i % COLS3) * TW, y0 + (i // COLS3) * TH
    s3.rect(cx, cy, TW, TH, "#999", "none", 0.2, "1 0.7", rx=0)
    s3.line([(cx + TW / 2, cy + 1), (cx + TW / 2, cy + TH - 1)], "#bbb", 0.15, "0.5 0.5")
    for half in (0, 1):
        hx0 = cx + half * TW / 2 + TW / 4
        s3.text(hx0, cy + 5.6, lab, 3.4, "#111", "middle", "bold")
        s3.text(hx0, cy + 9.0, sub, 1.55, "#333", "middle")

# ------------------------------------------------------------ PDF
out = pymupdf.open()
for sh in (s1, s2, s3):
    src = pymupdf.open(stream=sh.svg().encode("utf-8"), filetype="svg")
    out.insert_pdf(pymupdf.open("pdf", src.convert_to_pdf()))
out.set_metadata({"title": "Dash carrier board 1:1 template", "creator": "tools/drawings/carrier_template.py"})
out.save(OUT)
page = out[0]
print("written", OUT, "pages", len(out), "page size (pt)", round(page.rect.width, 1), round(page.rect.height, 1))
