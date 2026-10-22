"""Generate docs/future/wiring-overview.svg: what connects where, car -> box -> display/GPS.
Source of truth: build-hardware.md (harness X1-X5, power, CAN, backlight driver, 6c buttons /
day-night) and carrier-board.md section 5 (WeAct pins): change those first, then this.

Usage (from the repo root): python tools/drawings/wiring_overview.py docs/future/wiring-overview.svg"""
import sys
from pathlib import Path

OUT = Path(sys.argv[1])
W, H = 1560, 1080
o = []
def a(s): o.append(s)
def esc(t): return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")

RED, BLK, YEL, GRN, ORG, GRY, BLU, WHT = "#d62828", "#222222", "#e0b000", "#2a9d3a", "#f08c00", "#6c757d", "#1f6fd1", "#9aa4ad"

def text(x, y, t, size=12, color="#222", anchor="start", weight="normal"):
    a(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{weight}">{esc(t)}</text>')

def box(x, y, w, h, title, lines=(), stroke="#555", fill="#f7f7f7", tsize=14):
    a(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="1.5"/>')
    text(x + 10, y + 20, title, tsize, "#111", weight="bold")
    for i, ln in enumerate(lines):
        text(x + 10, y + 38 + i * 15, ln, 11, "#333")

def wire(pts, color, width=2.2, dash=None):
    d = " ".join(f"{px},{py}" for px, py in pts)
    da = f' stroke-dasharray="{dash}"' if dash else ""
    a(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{width}" stroke-linejoin="round"{da}/>')

def dot(x, y, color=BLK): a(f'<circle cx="{x}" cy="{y}" r="3.5" fill="{color}"/>')

def net(x, y, name, side="right"):
    """Net tag: name drawn in a small flag, wire end at (x, y)."""
    col = {"3V3": RED, "GND": BLK, "5V": ORG}[name]
    w = 34
    x0 = x if side == "right" else x - w
    a(f'<rect x="{x0}" y="{y-8}" width="{w}" height="16" rx="3" fill="#fff" stroke="{col}" stroke-width="1.5"/>')
    text(x0 + w / 2, y + 4, name, 10, col, "middle", "bold")

ROW, HEAD = 22, 24
def conn(cx, top, name, pins):
    """Connector on the box edge; returns the y of each pin (1-based)."""
    h = HEAD + ROW * len(pins) + 4
    a(f'<rect x="{cx-38}" y="{top}" width="76" height="{h}" rx="4" fill="#3a8f3a" fill-opacity="0.18" stroke="#1d5a1d" stroke-width="1.5"/>')
    text(cx, top + 16, name, 13, "#1d5a1d", "middle", "bold")
    ys = {}
    for i, p in enumerate(pins):
        yy = top + HEAD + ROW * i + 11
        ys[i + 1] = yy
        text(cx, yy + 4, f"{i+1} {p}", 10, "#123", "middle")
    return ys

# ---------------------------------------------------------------- frame
a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans, Arial, sans-serif">')
a('<rect width="100%" height="100%" fill="#fff"/>')
text(20, 30, "Dash — что куда подключать (вариант A, плата-носитель)", 20, "#111", weight="bold")
text(20, 52, "Схема логическая, не в масштабе. Xn.m — маркировка провода в жгуте (build-hardware.md, «Маркировка жгута»). "
     "Выводы WeAct — схема V12 (carrier-board.md §1).", 12, "#555")

BX0, BX1, BY0, BY1 = 400, 1080, 80, 960
a(f'<rect x="{BX0}" y="{BY0}" width="{BX1-BX0}" height="{BY1-BY0}" rx="14" fill="#fbfbf3" stroke="#8a8a5a" stroke-width="2.5" stroke-dasharray="10 6"/>')
text(640, 102, "Коробка Dash 140×82×38 (плата-носитель на гайках M3)", 14, "#6a6a3a", weight="bold")
text(20, 90, "МАШИНА", 13, "#888", weight="bold")
text(1160, 90, "ЭКРАН И GPS", 13, "#888", weight="bold")

# ---------------------------------------------------------------- connectors
x1 = conn(BX0, 150, "X1 питание", ["+12 В ACC", "масса"])
x2 = conn(BX0, 420, "X2 CAN", ["CAN-H", "CAN-L", "GND"])
x4 = conn(BX0, 640, "X4 кнопки", ["▶", "●", "◀", "GND", "ILL+", "запас"])
x5 = conn(BX1, 130, "X5 экран", ["VCC 3V3", "GND", "CS", "RESET", "DC/RS", "SDI", "SCK", "LED"])
x3 = conn(BX1, 560, "X3 GPS", ["3V3", "GND", "TX", "RX"])
LO, LI = BX0 - 38, BX0 + 38       # left connectors: outer / inner edge
RI, RO = BX1 - 38, BX1 + 38       # right connectors: inner / outer edge

# ---------------------------------------------------------------- car side
box(20, 158, 190, 54, "Провод ACC", ["12 В только в ACC / ON"], RED, "#fff5f5")
wire([(210, x1[1]), (LO, x1[1])], RED, 3)
dot(245, x1[1], RED)
text(214, x1[1] - 26, "врезка — пайка", 10, RED)
text(214, x1[1] - 14, "+ термоусадка", 10, RED)
a(f'<rect x="282" y="{x1[1]-9}" width="52" height="18" rx="3" fill="#fff" stroke="{RED}" stroke-width="2"/>')
text(308, x1[1] + 4, "2 А", 11, RED, "middle", "bold")
text(308, x1[1] + 24, "держатель", 10, RED, "middle")
text(308, x1[1] + 36, "у врезки", 10, RED, "middle")

box(20, 262, 210, 50, "Масса кузова", ["болт / штатная точка массы"], BLK, "#f4f4f4")
wire([(230, 287), (345, 287), (345, x1[2]), (LO, x1[2])], BLK, 3)

box(20, 400, 230, 150, "Разъём OBD2", ["под панелью → разветвитель 1 → 2:", "гнездо 1 — сканер (свободно),",
                                      "гнездо 2 — Dash", "", "пин 6 CAN-H, 14 CAN-L,", "5 — сигнальная земля",
                                      "16 (постоянные 12 В) — не брать"], "#7a4fb0", "#f7f2fc")
wire([(250, x2[1]), (LO, x2[1])], YEL, 3)
wire([(250, x2[2]), (LO, x2[2])], GRN, 3)
wire([(250, x2[3]), (LO, x2[3])], BLK, 2.5)
text(305, x2[1] - 8, "6", 10, "#555", "middle"); text(305, x2[2] - 6, "14", 10, "#555", "middle")
text(305, x2[3] - 6, "5", 10, "#555", "middle")
text(20, 568, "CAN-H / CAN-L — витая пара, отвод от разветвителя покороче", 10, "#555")

box(20, 625, 230, 150, "3 кнопки 8 мм (1NO)", ["в штатной заглушке LC100", "контакт 1 каждой → свой провод",
                                         "контакт 2 всех трёх → общий GND", "", "кольцо подсветки 12–24 В — не через",
                                         "Dash: «+» ILL+, «−» ILL− / масса"], GRY, "#f6f7f8")
for k in (1, 2, 3):
    wire([(250, x4[k]), (LO, x4[k])], GRY, 2.2)
wire([(250, x4[4]), (LO, x4[4])], BLK, 2.2)

box(20, 820, 230, 70, "ILL+ / TAIL", ["плюс подсветки, 12 В", "при включённых габаритах"], RED, "#fff5f5")
wire([(250, 855), (330, 855), (330, x4[5]), (LO, x4[5])], RED, 2.2, "7 4")

# ---------------------------------------------------------------- inside the box
# power: TVS + PD65W -> USB-C -> WeAct
node = 462
wire([(LI, x1[1]), (500, x1[1])], RED, 3)
dot(node, x1[1], RED)
wire([(node, x1[1]), (node, 140)], RED, 2)
box(442, 96, 130, 44, "TVS 1.5KE20A", ["полоска (катод) к +12"], RED, "#fff", 12)
net(574, 118, "GND")
wire([(572, 118), (574, 118)], BLK, 2)
wire([(LI, x1[2]), (452, x1[2])], BLK, 2.5)
net(452, x1[2], "GND")
box(500, 160, 120, 70, "PD65W", ["вход 8–30 В", "выход USB-C 5 В"], ORG, "#fff8ef", 13)
wire([(620, 195), (650, 195)], ORG, 5)
text(635, 212, "USB-C", 9, ORG, "middle")

WX0, WX1, WY0, WY1 = 650, 870, 110, 830
a(f'<rect x="{WX0}" y="{WY0}" width="{WX1-WX0}" height="{WY1-WY0}" rx="10" fill="#e9eef8" stroke="#2b4f86" stroke-width="2"/>')
text((WX0 + WX1) / 2, WY0 + 24, "WeAct MiniSTM32H743", 15, "#2b4f86", "middle", "bold")
text((WX0 + WX1) / 2, WY0 + 42, "в гнёздах на плате-носителе", 11, "#2b4f86", "middle")
def wpin_l(y, t): text(WX0 + 8, y + 4, t, 10, "#203a66")
def wpin_r(y, t): text(WX1 - 8, y + 4, t, 10, "#203a66", "end")
wpin_l(195, "USB-C: питание 5 В")

# CAN module
box(470, 430, 140, 112, "SN65HVD230", ["модуль CAN", "VCC — 3V3", "120 Ом на модуле", "СНЯТЬ"], "#7a4fb0", "#f7f2fc", 12)
wire([(LI, x2[1]), (470, x2[1])], YEL, 3)
wire([(LI, x2[2]), (470, x2[2])], GRN, 3)
wire([(LI, x2[3]), (442, x2[3])], BLK, 2.5)
net(442, x2[3], "GND")
wire([(610, 455), (WX0, 455)], "#7a4fb0", 2.2); wpin_l(455, "PD1 · P1.18  CAN TX")
wire([(610, 485), (WX0, 485)], "#7a4fb0", 2.2); wpin_l(485, "PD0 · P1.19  CAN RX")
text(630, 448, "TXD", 9, "#7a4fb0", "middle"); text(630, 478, "RXD", 9, "#7a4fb0", "middle")

# button pull-ups
box(470, 645, 140, 82, "Подтяжки", ["3 × 1 кОм к 3V3", "3 × 100 нФ к GND", "3 × 1 кОм в линию"], GRY, "#f6f7f8", 12)
for k, (y, t) in zip((1, 2, 3), ((660, "PE7 · P2.30  ▶"), (685, "PE8 · P2.31  ●"), (710, "PE9 · P2.32  ◀"))):
    wire([(LI, x4[k]), (470, x4[k])], GRY, 2.2)
    wire([(610, y), (WX0, y)], GRY, 2.2)
    wpin_l(y, t)
wire([(LI, x4[4]), (442, x4[4])], BLK, 2.2)
net(442, x4[4], "GND")

# parking-light detector
box(470, 752, 140, 96, "Датчик габаритов", ["BC547: 47 к, 100 к,", "100 нФ, 1N4148;", "коллектор: 10 к к 3V3"], RED, "#fff5f5", 12)
wire([(LI, x4[5]), (470, x4[5])], RED, 2.2, "7 4")
wire([(610, 775), (WX0, 775)], RED, 2.2)
wpin_l(775, "PE15 · P2.38  габариты")
text(LI + 8, x4[6] + 4, "—", 11, "#888")

# display lines
DISP = {3: (209, "CS · PC0 · P2.11"), 4: (231, "RESET · PC5 · P2.26"), 5: (253, "DC · PC4 · P2.25"),
        6: (275, "SDI · PB15 · P1.41"), 7: (297, "SCK · PB13 · P1.43")}
for k, (y, t) in DISP.items():
    wire([(WX1, y), (RI, x5[k])], GRY, 2.2)
    wpin_r(y, t)
wire([(RI, x5[1]), (1006, x5[1])], RED, 2.2); net(1006, x5[1], "3V3", "left")
wire([(RI, x5[2]), (1006, x5[2])], BLK, 2.2); net(1006, x5[2], "GND", "left")

# backlight driver
box(895, 345, 135, 120, "Подсветка 4\"", ["2N7000 + Si4599", "R1 10 к → 5 В", "R2 1 к ← PA8", "R3 10 к → GND",
                                        "образ ROAD INV"], ORG, "#fff8ef", 12)
wire([(WX1, 370), (895, 370)], ORG, 2.2); wpin_r(370, "PA8 · P1.28  ШИМ")
wire([(WX1, 420), (895, 420)], ORG, 3); wpin_r(420, "5V · P2.42")
wire([(1000, 345), (1000, x5[8]), (RI, x5[8])], ORG, 3)
text(995, 336, "сток Si4599", 9, ORG, "end")

# GPS lines
wire([(RI, x3[1]), (1006, x3[1])], RED, 2.2); net(1006, x3[1], "3V3", "left")
wire([(RI, x3[2]), (1006, x3[2])], BLK, 2.2); net(1006, x3[2], "GND", "left")
wire([(WX1, x3[3]), (RI, x3[3])], BLU, 2.2); wpin_r(x3[3], "PA3 · P2.20  ← TX GPS")
wire([(WX1, x3[4]), (RI, x3[4])], WHT, 2.6); wpin_r(x3[4], "PA2 · P2.19  → RX GPS")

# rails
wpin_r(730, "3V3 · P2.41 → шина 3V3"); wpin_r(752, "GND · P2.43 / P2.44 → шина GND")
net(WX1 + 6, 730, "3V3"); net(WX1 + 6, 752, "GND")
text(WX0 + 10, WY1 - 12, "без гнёзд: выводы 1–4 (P2: 1–6)", 10, "#2b4f86")

# ---------------------------------------------------------------- outside, right
box(1160, 125, 330, 230, "Экран 4\" ILI9488, 320×480", [], "#16a085", "#eefaf6")
for k, nm in zip(range(1, 9), ["VCC", "GND", "CS", "RESET", "DC/RS", "SDI (MOSI)", "SCK", "LED"]):
    col = RED if k == 1 else BLK if k == 2 else ORG if k == 8 else GRY
    wire([(RO, x5[k]), (1160, x5[k])], col, 2.4 if k in (1, 2, 8) else 2.2)
    text(1168, x5[k] + 4, nm, 11, "#0f5f50")
for i, ln in enumerate(["SDO (MISO) — не подключать", "шлейф 8 проводов — как можно короче",
                        "(SPI 18.75 МГц), без разъёма в жгуте", "LED: 5 В через ключ, не 3V3"]):
    text(1280, 175 + i * 16, ln, 10, "#0f5f50")

box(1160, 555, 330, 175, "Плата GPS ATGM336H", [], "#2980b9", "#eef5fc")
for k, nm in zip(range(1, 5), ["VCC — ТОЛЬКО 3V3", "GND", "TX", "RX"]):
    col = RED if k == 1 else BLK if k == 2 else BLU if k == 3 else WHT
    wire([(RO, x3[k]), (1160, x3[k])], col, 2.4)
    text(1168, x3[k] + 4, nm, 11, "#1b4f7a", weight="bold" if k == 1 else "normal")
for i, ln in enumerate(["под лобовым стеклом, антенной вверх", "PPS — не нужен",
                        "TX платы → PA3, RX платы ← PA2", "(перепутаны — на DIAG «GPS -»)"]):
    text(1300, 600 + i * 16, ln, 10, "#1b4f7a")

# ---------------------------------------------------------------- legend
ly = 995
text(20, ly, "Цвета:", 12, "#111", weight="bold")
items = [(RED, "+12 В / 3V3"), (BLK, "масса"), (ORG, "5 В / подсветка"), (YEL, "CAN-H"), (GRN, "CAN-L"),
         (BLU, "GPS TX"), (WHT, "GPS RX"), (GRY, "сигналы")]
lx = 80
for col, nm in items:
    wire([(lx, ly - 4), (lx + 28, ly - 4)], col, 3)
    text(lx + 34, ly, nm, 11, "#333")
    lx += 36 + 9 * len(nm) + 16
for i, ln in enumerate([
        "3V3 / GND / 5V в флажках — шины платы-носителя. X1.2 — масса питания (кузов), X2.3 — опорная земля CAN от OBD 5; на плате обе — одна шина GND.",
        "Пустые выводы и разводка по отверстиям — carrier-board.md §5 и carrier-board.svg. +12 В есть только в X1.1 и X4.5: их метки делать особенно заметными."]):
    text(20, ly + 24 + i * 18, ln, 11, "#444")

a("</svg>")
OUT.write_text("\n".join(o), encoding="utf-8")
print("written", OUT)
