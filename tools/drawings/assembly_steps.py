"""Generate docs/future/assembly/step-*.svg: the carrier board, one subsystem per step,
every part with its legs and every wire numbered (the tables are in assembly.md).

Usage (from the repo root): python tools/drawings/assembly_steps.py docs/future/assembly
Source of truth: carrier-board.md section 5 (wire list, WeAct pins) and build-hardware.md
(harness X1-X5, backlight driver, 6c buttons and day/night). Change those first, then this.

TO-92 pinouts, flat side towards you, legs down, left to right:
2N7000 S G D (onsemi), BC547 C B E (onsemi)."""
import sys
from pathlib import Path

OUT = Path(sys.argv[1])
OUT.mkdir(parents=True, exist_ok=True)

RED, BLK, YEL, GRN, ORG, GRY, BLU, PUR, LGR = ("#d62828", "#222", "#d9a400", "#2a9d3a", "#ee8a00",
                                              "#6c757d", "#1f6fd1", "#7a4fb0", "#9aa4ad")


def esc(t):
    return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")


class Svg:
    def __init__(self, w, h, title, sub=""):
        self.w, self.h, self.o = w, h, []
        self.add(f'<svg xmlns="http://www.w3.org/2000/svg" width="{w}" height="{h}" viewBox="0 0 {w} {h}" '
                 'font-family="DejaVu Sans, Arial, sans-serif">')
        self.add('<rect width="100%" height="100%" fill="#fff"/>')
        self.text(20, 30, title, 18, "#111", weight="bold")
        if sub:
            self.text(20, 52, sub, 12, "#555")

    def add(self, s):
        self.o.append(s)

    def text(self, x, y, t, size=12, color="#222", anchor="start", weight="normal"):
        self.add(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" '
                 f'font-weight="{weight}">{esc(t)}</text>')

    def line(self, pts, color, width=2.4, dash=None):
        d = " ".join(f"{px},{py}" for px, py in pts)
        da = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<polyline points="{d}" fill="none" stroke="{color}" stroke-width="{width}" '
                 f'stroke-linejoin="round" stroke-linecap="round"{da}/>')

    def dot(self, x, y, color=BLK):
        self.add(f'<circle cx="{x}" cy="{y}" r="4" fill="{color}"/>')

    def rect(self, x, y, w, h, stroke="#555", fill="#f7f7f7", rx=6, width=1.5, dash=None):
        da = f' stroke-dasharray="{dash}"' if dash else ""
        self.add(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="{rx}" fill="{fill}" stroke="{stroke}" '
                 f'stroke-width="{width}"{da}/>')

    def num(self, x, y, n):
        """Wire number, matching the table in assembly.md."""
        self.add(f'<circle cx="{x}" cy="{y}" r="10" fill="#fff" stroke="#111" stroke-width="1.5"/>')
        self.text(x, y + 4, str(n), 11, "#111", "middle", "bold")

    def note(self, x, y, lines, color="#333", size=12):
        for i, ln in enumerate(lines):
            self.text(x, y + i * (size + 5), ln, size, color)

    def save(self, name):
        self.add("</svg>")
        (OUT / name).write_text("\n".join(self.o), encoding="utf-8")
        print("written", OUT / name)


# ------------------------------------------------------------------ parts
KEY = "ключ"


def term(s, x, y, name, pins, sub="клеммник"):
    """Vertical terminal block. Returns {pin: (left_x, right_x, y)}. A pin named KEY is the key:
    its socket pin is pulled out and its plug cavity blocked (carrier-board.md section 3), no wire."""
    h = 26 + 28 * len(pins)
    s.rect(x, y, 56, h, "#1d5a1d", "#e3f1e3", 4)
    s.text(x + 28, y - 18, name, 14, "#1d5a1d", "middle", "bold")
    s.text(x + 28, y - 5, sub, 10, "#1d5a1d", "middle")
    out = {}
    for i, p in enumerate(pins):
        yy = y + 22 + 28 * i
        if p == KEY:
            s.add(f'<rect x="{x + 4}" y="{yy - 10}" width="48" height="20" rx="3" fill="#fff" stroke="#c0392b" '
                  'stroke-width="1.2" stroke-dasharray="3 2"/>')
            s.text(x + 28, yy + 4, f"{i+1} ✕ ключ", 10, "#c0392b", "middle", "bold")
        else:
            s.text(x + 28, yy + 4, f"{i+1} {p}", 10, "#123", "middle")
        out[i + 1] = (x, x + 56, yy)
    return out


def weact(s, x, y, rows, side="right", w=176):
    """WeAct header pins: rows = [(key, signal, header pin)], one row every 28 px. Returns {key: (x, y)}."""
    h = 30 + 28 * len(rows)
    s.rect(x, y, w, h, "#2b4f86", "#e9eef8", 6)
    s.text(x + w / 2, y + 18, "WeAct (гребёнка)", 11, "#2b4f86", "middle", "bold")
    out = {}
    for i, (key, sig, pin) in enumerate(rows):
        yy = y + 38 + 28 * i
        if side == "right":
            s.text(x + 10, yy + 4, sig, 12, "#203a66", weight="bold")
            s.text(x + w - 14, yy + 4, pin, 11, "#203a66", "end")
            s.dot(x + w, yy, "#2b4f86")
            out[key] = (x + w, yy)
        else:
            s.text(x + 14, yy + 4, pin, 11, "#203a66")
            s.text(x + w - 10, yy + 4, sig, 12, "#203a66", "end", "bold")
            s.dot(x, yy, "#2b4f86")
            out[key] = (x, yy)
    return out


def res(s, a, b, label, color=BLK):
    """Resistor between two points on one axis; label beside the body."""
    (x1, y1), (x2, y2) = a, b
    if y1 == y2:
        m = (x1 + x2) / 2
        s.line([(x1, y1), (m - 22, y1)], color)
        s.line([(m + 22, y1), (x2, y2)], color)
        s.rect(m - 22, y1 - 8, 44, 16, "#333", "#fff3c4", 2)
        s.text(m, y1 - 13, label, 11, "#333", "middle", "bold")
    else:
        m = (y1 + y2) / 2
        s.line([(x1, y1), (x1, m - 22)], color)
        s.line([(x1, m + 22), (x2, y2)], color)
        s.rect(x1 - 8, m - 22, 16, 44, "#333", "#fff3c4", 2)
        s.text(x1 + 14, m + 4, label, 11, "#333", weight="bold")


def cap(s, a, b, label):
    """Ceramic capacitor (no polarity), vertical."""
    (x, y1), (_, y2) = a, b
    m = (y1 + y2) / 2
    s.line([(x, y1), (x, m - 5)], BLK)
    s.line([(x, m + 5), (x, y2)], BLK)
    s.line([(x - 12, m - 5), (x + 12, m - 5)], BLK, 3)
    s.line([(x - 12, m + 5), (x + 12, m + 5)], BLK, 3)
    s.text(x + 16, m + 4, label, 11, "#333", weight="bold")


def diode(s, top, bottom, label, cathode="top", lines=()):
    """Vertical diode body with its band (cathode)."""
    (x, y1), (_, y2) = top, bottom
    m = (y1 + y2) / 2
    s.line([(x, y1), (x, m - 18)], BLK)
    s.line([(x, m + 18), (x, y2)], BLK)
    s.rect(x - 9, m - 18, 18, 36, "#333", "#d8d8d8", 3)
    by = m - 18 if cathode == "top" else m + 10
    s.add(f'<rect x="{x-9}" y="{by}" width="18" height="8" fill="#555"/>')
    s.text(x + 16, m - 4, label, 11, "#333", weight="bold")
    for i, ln in enumerate(lines):
        s.text(x + 16, m + 11 + i * 14, ln, 10, "#444")


def to92(s, x, y, name, legs, notes=()):
    """TO-92 seen from its flat side, legs down. Returns leg ends {name: (x, y)}."""
    s.add(f'<path d="M{x-26},{y+40} L{x-26},{y+12} Q{x-26},{y} {x-14},{y} L{x+14},{y} '
          f'Q{x+26},{y} {x+26},{y+12} L{x+26},{y+40} Z" fill="#2d2d2d" stroke="#000"/>')
    s.text(x, y + 25, name, 10, "#fff", "middle", "bold")
    out = {}
    for i, lg in enumerate(legs):
        lx = x - 16 + 16 * i
        s.line([(lx, y + 40), (lx, y + 70)], "#777", 3)
        s.add(f'<rect x="{lx-7}" y="{y+47}" width="14" height="15" rx="2" fill="#fff" stroke="#777"/>')
        s.text(lx, y + 59, lg, 10, "#111", "middle", "bold")
        out[lg] = (lx, y + 70)
    for i, ln in enumerate(notes):
        s.text(x + 34, y + 14 + i * 14, ln, 10, "#444")
    return out


def module(s, x, y, w, h, name, pins, color="#555", fill="#f7f7f7", lines=()):
    """pins = [(key, side, y_offset, label)]. Returns {key: (x, y)}."""
    s.rect(x, y, w, h, color, fill, 8)
    s.text(x + w / 2, y + 20, name, 13, "#111", "middle", "bold")
    for i, ln in enumerate(lines):
        s.text(x + w / 2, y + h - 12 - 14 * (len(lines) - 1 - i), ln, 10, "#444", "middle")
    out = {}
    for key, side, dy, label in pins:
        py = y + dy
        if side == "left":
            s.dot(x, py, color)
            s.text(x + 8, py + 4, label, 11, "#222")
            out[key] = (x, py)
        else:
            s.dot(x + w, py, color)
            s.text(x + w - 8, py + 4, label, 11, "#222", "end")
            out[key] = (x + w, py)
    return out


def bus(s, x1, x2, y, name):
    col = {"3V3": RED, "GND": BLK, "5V": ORG}[name]
    s.line([(x1, y), (x2, y)], col, 4)
    s.text(x1, y - 7, f"шина {name}", 10, col, weight="bold")


def side_labels(s, x_board, x_harness, y=78):
    s.text(x_board, y, "НА ПЛАТЕ", 11, "#888", weight="bold")
    s.text(x_harness, y, "В ЖГУТЕ (вне коробки)", 11, "#888", weight="bold")


def check(s, y, lines):
    s.rect(20, y, s.w - 40, 22 + 17 * len(lines), "#2a9d3a", "#f1faf2", 6)
    s.text(32, y + 18, "Проверка", 12, "#1d6a2a", weight="bold")
    for i, ln in enumerate(lines):
        s.text(32, y + 36 + 17 * i, ln, 12, "#1d4d26")


# ------------------------------------------------------------------ step 1: power
s = Svg(1100, 440, "Шаг 1. Питание: X1 → TVS → PD65W → USB-C → WeAct",
        "Предохранитель 2 А — в жгуте, у самой врезки в провод ACC. На плате — клеммник X1, TVS и PD65W.")
s.text(20, 78, "В ЖГУТЕ (вне коробки)", 11, "#888", weight="bold")
s.text(390, 78, "НА ПЛАТЕ / В КОРОБКЕ", 11, "#888", weight="bold")
x1 = term(s, 300, 100, "X1", ["+12 В", KEY, "масса"])
s.rect(20, 102, 130, 38, RED, "#fff5f5"); s.note(30, 118, ["провод ACC", "(12 В в ACC/ON)"], "#7a1010", 10)
s.rect(170, x1[1][2] - 9, 50, 18, RED, "#fff", 3); s.text(195, x1[1][2] + 4, "2 А", 11, RED, "middle", "bold")
s.line([(150, x1[1][2]), (170, x1[1][2])], RED, 3); s.line([(220, x1[1][2]), (300, x1[1][2])], RED, 3)
s.num(260, x1[1][2] - 14, 1)
s.rect(20, 160, 130, 38, BLK, "#f2f2f2"); s.note(30, 176, ["масса кузова", "(болт)"], "#111", 10)
s.line([(150, 179), (240, 179), (240, x1[3][2]), (300, x1[3][2])], BLK, 3)
s.num(270, x1[3][2] + 16, 2)
pd = module(s, 600, 96, 180, 96, "PD65W", [("in+", "left", 26, "вход +"), ("in-", "left", 54, "вход −"),
                                          ("usb", "right", 40, "USB-C")], ORG, "#fff8ef",
            ["подписи входа —", "как на самой плате"])
y1, y2 = x1[1][2], x1[3][2]
s.line([(356, y1), (pd["in+"][0], pd["in+"][1])], RED, 3); s.dot(430, y1, RED)
s.num(530, y1 - 14, 3)
gy = 300                                  # ground below the +12 line: nothing crosses
s.line([(356, y2), (380, y2), (380, gy), (560, gy), (560, pd["in-"][1]), pd["in-"]], BLK, 3)
s.num(650, gy - 14, 4)
diode(s, (430, y1), (430, gy), "TVS 1.5KE20A", "top", ["полоска — вверх,", "к +12 (X1.1)"])
s.dot(430, gy, BLK); s.num(408, 200, 5)
s.line([(480, gy), (480, 350)], BLK, 2.4); s.dot(480, gy, BLK)
s.text(488, 362, "↓ к шине GND (шаг 2)", 11, BLK, weight="bold"); s.num(500, 332, 7)
s.rect(880, 110, 180, 64, "#2b4f86", "#e9eef8"); s.note(892, 132, ["WeAct MiniSTM32H743", "разъём USB-C"], "#203a66", 12)
s.line([(pd["usb"][0], pd["usb"][1]), (880, pd["usb"][1])], ORG, 7)
s.num(830, pd["usb"][1] - 16, 6)
s.text(830, pd["usb"][1] + 24, "кабель USB-C", 10, ORG, "middle")
check(s, 385, ["От лабораторного БП 12 В на X1 (+ на 1, − на 3): на выходе USB-C 5.0–5.2 В. TVS холодный. WeAct пока не подключать."])
s.save("step-1-power.svg")

# ------------------------------------------------------------------ step 2: rails
s = Svg(1100, 380, "Шаг 2. Шины 3V3, GND и 5 В от WeAct",
        "Голый лужёный провод по нижней стороне платы: 3V3 вдоль ряда 24, GND вдоль ряда 25 (carrier-board.svg). Каждая шина — от одной точки.")
w = weact(s, 40, 90, [("3v3", "3V3", "P2.41"), ("5v", "5V", "P2.42"), ("gnd", "GND", "P2.43"), ("gnd2", "GND", "P2.44")])
bus(s, w["3v3"][0], 820, w["3v3"][1], "3V3"); s.num(300, w["3v3"][1] - 18, 1)
s.line([w["5v"], (420, w["5v"][1])], ORG, 4)
s.text(430, w["5v"][1] + 4, "5 В — только к Si4599 и R1 (шаг 4), больше никуда", 11, ORG, weight="bold"); s.num(300, w["5v"][1] - 16, 2)
bus(s, w["gnd"][0], 820, w["gnd"][1], "GND"); s.num(380, w["gnd"][1] - 16, 3)
s.text(w["gnd2"][0] + 12, w["gnd2"][1] + 4, "P2.44 (и P2.15) — не подключать: шина GND берётся с одной точки", 10, "#555")
s.note(830, w["3v3"][1] - 4, ["→ X5.1 экран, CAN 3V3, X3.1 GPS,", "   3 × 1 к кнопок, 10 к габаритов"], RED, 11)
s.note(830, w["gnd"][1] - 4, ["→ X1.3, X2.4, X3.2, X4.4, X5.2, CAN GND,", "   2N7000 S, BC547 E, R3, 100 к, 1N4148,",
                              "   4 × 100 нФ"], BLK, 11)
check(s, 300, ["Прозвонка до установки WeAct: 3V3 ↔ GND и 5 В ↔ GND — не звонятся (нет КЗ).",
               "После установки WeAct от USB: на шине 3V3 — 3.3 В, на 5 В (P2.42) — ~4.6–4.8 В: 5 В USB идут через диод D10 на WeAct."])
s.save("step-2-rails.svg")

# ------------------------------------------------------------------ step 3: display
s = Svg(1100, 520, "Шаг 3. Экран 4\": X5 (8 конт.)",
        "На плате — 7 проводов к X5, восьмой придёт из шага 4. В жгуте — 8 проводов от X5 к экрану, как можно короче.")
side_labels(s, 40, 640)
x5 = term(s, 520, 100, "X5", ["VCC", "GND", "CS", "RESET", "DC", "SDI", "SCK", "LED"])
w = weact(s, 40, x5[3][2] - 38, [("cs", "PC0", "P2.11"), ("rst", "PC5", "P2.26"), ("dc", "PC4", "P2.25"),
                                 ("sdi", "PB15", "P1.41"), ("sck", "PB13", "P1.43")])
bus(s, 300, 470, x5[1][2], "3V3"); s.line([(470, x5[1][2]), (520, x5[1][2])], RED, 2.4); s.num(495, x5[1][2] - 14, 1)
bus(s, 300, 470, x5[2][2], "GND"); s.line([(470, x5[2][2]), (520, x5[2][2])], BLK, 2.4); s.num(495, x5[2][2] + 14, 2)
for n, k in zip((3, 4, 5, 6, 7), ("cs", "rst", "dc", "sdi", "sck")):
    yy = x5[n][2]
    s.line([w[k], (520, yy)], GRY)
    s.num(400, yy, n)
s.line([(330, x5[8][2]), (520, x5[8][2])], ORG, 2.6, "6 4")
s.text(320, x5[8][2] + 4, "выход подсветки: сток Si4599 (шаг 4)", 11, ORG, "end", weight="bold"); s.num(495, x5[8][2] - 14, 8)
dm = module(s, 830, 92, 230, 300, "Экран 4\" ILI9488",
            [(k, "left", 30 + 28 * i, lbl) for i, (k, lbl) in enumerate(
                [(1, "VCC"), (2, "GND"), (3, "CS"), (4, "RESET"), (5, "DC/RS"), (6, "SDI (MOSI)"), (7, "SCK"),
                 (8, "LED"), (9, "SDO (MISO)")])],
            "#16a085", "#eefaf6", ["порядок выводов сверить", "с надписями на плате экрана"])
for k in range(1, 9):
    col = RED if k == 1 else BLK if k == 2 else ORG if k == 8 else GRY
    s.line([(576, x5[k][2]), (700, x5[k][2]), (700, dm[k][1]), dm[k]], col)
s.text(dm[9][0] - 10, dm[9][1] + 4, "не подключать", 10, "#a00", "end")
check(s, 420, ["Прозвонка: каждый вывод экрана звонится со своим выводом WeAct (CS–PC0, RESET–PC5, DC–PC4, SDI–PB15, SCK–PB13),",
               "VCC — с шиной 3V3, GND — с GND; соседние провода X5 между собой не звонятся.",
               "До шага 4: ток в вывод LED при 3.3 В и при 5 В. Единицы мА — LED только вход управления: записать (build-hardware.md §3)."])
s.save("step-3-display.svg")

# ------------------------------------------------------------------ step 4: backlight driver
s = Svg(1100, 650, "Шаг 4. Подсветка экрана: 2N7000 + Si4599 (+ 3 резистора)",
        "Si4599 уже работает на макетке: исток на 5 В, сток на LED экрана, затвор — к PA8. Затвор теперь не к PA8, а к узлу A.")
w5 = weact(s, 30, 84, [("5v", "5V", "P2.42")])
si = module(s, 720, 92, 230, 120, "Модуль Si4599 (P-канал)",
            [("s", "left", 30, "исток S"), ("g", "left", 90, "затвор G"), ("d", "right", 60, "сток D")],
            "#16a085", "#eefaf6", ["те же выводы, что сейчас на макетке"])
n5 = (330, si["s"][1])
s.line([w5["5v"], (w5["5v"][0] + 20, w5["5v"][1]), (w5["5v"][0] + 20, n5[1]), si["s"]], ORG, 3); s.dot(*n5, ORG)
s.num(560, n5[1] - 14, 1)
nA = (600, si["g"][1])
res(s, n5, (330, si["g"][1]), "R1 10 к", ORG)
s.num(305, (n5[1] + si["g"][1]) / 2, 2)
s.line([(330, si["g"][1]), si["g"]], GRY); s.dot(*nA); s.text(nA[0] - 6, nA[1] - 8, "A", 12, "#111", "end", "bold")
s.num(660, si["g"][1] - 14, 3)
q = to92(s, 470, 240, "2N7000", ["S", "G", "D"], [])
s.text(420, 262, "плоской стороной к себе,", 10, "#444", "end")
s.text(420, 276, "ножки вниз: S G D", 10, "#444", "end")
s.line([q["D"], (q["D"][0], q["D"][1] + 10), (nA[0], q["D"][1] + 10), nA], GRY); s.num(nA[0] + 18, 300, 4)
gnd_y = 500
s.line([q["S"], (q["S"][0], gnd_y)], BLK); s.num(q["S"][0] - 18, 430, 5)
nB = (560, 450)
s.line([q["G"], (q["G"][0], nB[1]), nB], GRY); s.dot(*nB); s.text(nB[0], nB[1] - 10, "B", 12, "#111", "middle", "bold")
s.num(q["G"][0] + 20, 420, 6)
wpa = weact(s, 860, nB[1] - 38, [("pa8", "PA8", "P1.28")], "left", 190)
res(s, nB, wpa["pa8"], "R2 1 к", GRY); s.num(760, nB[1] + 18, 7)
res(s, nB, (nB[0], gnd_y), "R3 100 к"); s.num(nB[0] - 20, 478, 8)
bus(s, 380, 640, gnd_y, "GND")
s.line([si["d"], (1060, si["d"][1])], ORG, 3); s.num(1000, si["d"][1] - 14, 9)
s.text(1060, si["d"][1] + 20, "→ X5.8 (LED экрана)", 11, ORG, "end", weight="bold")
s.note(30, 170, ["Прошивка для этой платы —", "только ROAD INV:", "build.py -D BACKLIGHT_ACTIVE_LOW=0", "",
                 "Обычный образ здесь гасит", "экран при «100 %»."], "#7a3a00", 11)
check(s, 560, ["До прошивки: PA8 не звонится с затвором Si4599 напрямую — только через R2 1 к на G 2N7000.",
               "После прошивки ROAD INV: в режиме яркости 10 / 50 / 100 % подсветка меняется в ту же сторону, 2N7000 и Si4599 не греются."])
s.save("step-4-backlight.svg")

# ------------------------------------------------------------------ step 5: CAN
s = Svg(1100, 400, "Шаг 5. CAN: модуль SN65HVD230 → X2 → разветвитель OBD2",
        "На модуле снять резистор 120 Ом (маркировка 121, у выводов CANH/CANL): в машине шина уже согласована.")
side_labels(s, 40, 760)
w = weact(s, 40, 128, [("tx", "PD1", "P1.18"), ("rx", "PD0", "P1.19")])
cm = module(s, 380, 92, 220, 170, "SN65HVD230",
            [("3v3", "left", 34, "3V3"), ("gnd", "left", 62, "GND"), ("ctx", "left", 90 + 2, "CTX (TX, D)"),
             ("crx", "left", 118 + 2, "CRX (RX, R)"), ("h", "right", 62, "CANH"), ("l", "right", 90, "CANL")],
            PUR, "#f7f2fc", ["120 Ом (121) — снять"])
bus(s, 250, 350, cm["3v3"][1], "3V3"); s.line([(350, cm["3v3"][1]), cm["3v3"]], RED); s.num(365, cm["3v3"][1] - 14, 1)
bus(s, 250, 350, cm["gnd"][1], "GND"); s.line([(350, cm["gnd"][1]), cm["gnd"]], BLK)
s.line([w["tx"], (300, w["tx"][1]), (300, cm["ctx"][1]), cm["ctx"]], PUR); s.num(330, cm["ctx"][1] - 13, 3)
s.line([w["rx"], (320, w["rx"][1]), (320, cm["crx"][1]), cm["crx"]], PUR); s.num(345, cm["crx"][1] + 13, 4)
s.num(365, cm["gnd"][1] + 13, 2)
x2 = term(s, 680, 140, "X2", ["CAN-H", "CAN-L", KEY, "GND"])
s.line([cm["h"], (640, cm["h"][1]), (640, x2[1][2]), (680, x2[1][2])], YEL, 3); s.num(655, x2[1][2] - 30, 5)
s.line([cm["l"], (630, cm["l"][1]), (630, x2[2][2]), (680, x2[2][2])], GRN, 3); s.num(615, x2[2][2] + 12, 6)
s.line([(600, x2[4][2]), (680, x2[4][2])], BLK); s.text(596, x2[4][2] + 4, "шина GND", 10, BLK, "end", weight="bold")
s.num(655, x2[4][2] + 16, 7)
ob = module(s, 880, 120, 190, 130, "Разветвитель OBD2",
            [(6, "left", 42, "пин 6  CAN-H"), (14, "left", 70, "пин 14 CAN-L"), (5, "left", 98, "пин 5  земля")],
            PUR, "#f7f2fc", ["гнездо для Dash"])
for k, n, col, xv in ((1, 6, YEL, 800), (2, 14, GRN, 812), (4, 5, BLK, 824)):
    s.line([(736, x2[k][2]), (xv, x2[k][2]), (xv, ob[n][1]), ob[n]], col, 3)
s.text(830, 296, "CAN-H и CAN-L скрутить; от разветвителя до X2 ≤ 30 см", 10, "#555", "middle")
check(s, 320, ["Ключ выключен: между пинами 6 и 14 разветвителя ~60 Ом (две штатные оконечные). Если ~40 Ом — 120 Ом на модуле не снят.",
               "Модуль: 3V3 ↔ GND не звонятся; CTX звонится с PD1, CRX — с PD0."])
s.save("step-5-can.svg")

# ------------------------------------------------------------------ step 6: GPS
s = Svg(1100, 400, "Шаг 6. GPS: X3 (4 конт.) → плата ATGM336H",
        "TX и RX перекрещиваются: передатчик модуля → вход МК (PA3), выход МК (PA2) → приёмник модуля. В каждой линии — 100 Ом у X3.")
side_labels(s, 40, 640)
x3 = term(s, 500, 110, "X3", ["3V3", "GND", "TX", "RX"])
w = weact(s, 40, x3[3][2] - 38, [("rx", "PA3", "P2.20"), ("tx", "PA2", "P2.19")])
bus(s, 300, 450, x3[1][2], "3V3"); s.line([(450, x3[1][2]), (500, x3[1][2])], RED); s.num(475, x3[1][2] - 14, 1)
bus(s, 300, 450, x3[2][2], "GND"); s.line([(450, x3[2][2]), (500, x3[2][2])], BLK); s.num(475, x3[2][2] + 14, 2)
# 100 R in line, at X3 (zone "GPS", cols R-U, rows 26-27): limits what the 1.5 m lead brings in
res(s, (330, x3[3][2]), (470, x3[3][2]), "100 Ом", LGR); s.line([w["rx"], (330, x3[3][2])], LGR, 2.8)
s.line([(470, x3[3][2]), (500, x3[3][2])], LGR, 2.8); s.num(300, x3[3][2] - 14, 3)
s.text(228, x3[3][2] - 8, "вход ←", 10, "#555")
res(s, (330, x3[4][2]), (470, x3[4][2]), "", BLU); s.line([w["tx"], (330, x3[4][2])], BLU)
s.text(400, x3[4][2] + 24, "100 Ом", 11, "#333", "middle", "bold")
s.line([(470, x3[4][2]), (500, x3[4][2])], BLU); s.num(300, x3[4][2] + 16, 4)
s.text(228, x3[4][2] + 20, "выход →", 10, BLU)
g = module(s, 820, 92, 240, 190, "Плата GPS ATGM336H",
           [("vcc", "left", 40, "VCC — только 3V3!"), ("gnd", "left", 68, "GND"), ("tx", "left", 96, "TX"),
            ("rx", "left", 124, "RX"), ("pps", "left", 152, "PPS — не нужен")],
           BLU, "#eef5fc", ["под стеклом, антенной вверх"])
for k, key, col, xv in ((1, "vcc", RED, 700), (2, "gnd", BLK, 712), (3, "tx", LGR, 724), (4, "rx", BLU, 736)):
    s.line([(556, x3[k][2]), (xv, x3[k][2]), (xv, g[key][1]), g[key]], col, 2.6)
s.text(640, x3[4][2] + 40, "в жгуте: TX — белый, RX — синий", 10, "#555", "middle")
check(s, 320, ["Прозвонка: X3.3 ↔ PA3 и X3.4 ↔ PA2 — по ~100 Ом. VCC платы GPS звонится с шиной 3V3, а не с 5 В.",
               "После прошивки: на DIAG «GPS ?» → «V…» → «спутники/скорость». «GPS -» — скорее всего, перепутаны TX и RX."])
s.save("step-6-gps.svg")

# ------------------------------------------------------------------ step 7: buttons
s = Svg(1100, 650, "Шаг 7. Кнопки ▶ ● ◀: X4.1–X4.4",
        "Каждая кнопка — одна цепочка: провод от X4 → узел (1 кОм к 3V3, 100 нФ к GND) → 1 кОм в линию → PE-вывод WeAct.")
s.text(20, 78, "В ЖГУТЕ (вне коробки)", 11, "#888", weight="bold")
s.text(420, 78, "НА ПЛАТЕ", 11, "#888", weight="bold")
x4 = term(s, 260, 100, "X4", ["▶", "●", "◀", "GND", "ILL+", KEY])
bt = module(s, 20, 96, 190, 140, "3 кнопки 8 мм (1NO)",
            [(1, "right", 26, "▶ контакт 1"), (2, "right", 54, "● контакт 1"), (3, "right", 82, "◀ контакт 1"),
             (4, "right", 110, "контакт 2 всех трёх")], GRY, "#f6f7f8")
for k in (1, 2, 3, 4):
    s.line([bt[k], (260, x4[k][2])], BLK if k == 4 else GRY)
s.note(20, 262, ["вторые контакты трёх кнопок", "соединить вместе → X4.4", "",
                 "кольцо подсветки (12–24 В) — не через Dash:", "«+» к ILL+, «−» к ILL− или к массе",
                 "(harness.md, X4)"], "#444", 10)
s.text(330, x4[5][2] + 4, "→ шаг 8", 10, "#888")
rows = [(1, 180, "PE7", "P2.30", 470), (2, 320, "PE8", "P2.31", 450), (3, 460, "PE9", "P2.32", 430)]
n = 1
for k, y, sig, pin, vx in rows:
    s.line([(316, x4[k][2]), (vx, x4[k][2]), (vx, y), (540, y)], GRY); s.dot(540, y)
    s.num((vx + 540) / 2, y - 12, n)
    res(s, (540, y), (540, y - 70), "1 к", RED); s.num(518, y - 35, n + 1)
    s.line([(500, y - 70), (580, y - 70)], RED, 4); s.text(500, y - 77, "шина 3V3", 10, RED, weight="bold")
    s.dot(620, y)
    cap(s, (620, y), (620, y + 60), "100 нФ"); s.num(600, y + 30, n + 2)
    s.line([(580, y + 60), (660, y + 60)], BLK, 4); s.text(666, y + 64, "шина GND", 10, BLK, weight="bold")
    ww = weact(s, 860, y - 38, [(sig, sig, pin)], "left", 170)
    # series 1 k between the node and the pin: with the 100 nF it filters what the
    # 1.5 m harness picks up (ESD, induced spikes) before it reaches the MCU pin
    s.line([(540, y), (690, y)], GRY)
    res(s, (690, y), (800, y), "1 к", GRY); s.num(745, y + 24, n + 3)
    s.line([(800, y), ww[sig]], GRY)
    n += 4
s.line([(316, x4[4][2]), (340, x4[4][2])], BLK); s.text(344, x4[4][2] + 4, "шина GND", 10, BLK, weight="bold")
s.num(330, x4[4][2] - 14, 13)
check(s, 580, ["Кнопка отпущена: на PE7/PE8/PE9 ~3.3 В. Нажата: ~0 В. PE-выводы не звонятся ни между собой, ни с GND.",
               "1 кОм в линию — между узлом (подтяжка + 100 нФ) и выводом, не между X4 и узлом."])
s.save("step-7-buttons.svg")

# ------------------------------------------------------------------ step 8: parking lights
s = Svg(1100, 470, "Шаг 8. Габариты: X4.5 → BC547 → PE15",
        "Габариты включены → 12 В на ILL+ → BC547 открыт → на PE15 ноль → прошивка включает ночной профиль.")
s.text(20, 78, "В ЖГУТЕ (вне коробки)", 11, "#888", weight="bold")
s.text(420, 78, "НА ПЛАТЕ", 11, "#888", weight="bold")
x4 = term(s, 260, 100, "X4", ["▶", "●", "◀", "GND", "ILL+", KEY])
s.rect(20, x4[5][2] - 22, 190, 44, RED, "#fff5f5"); s.note(30, x4[5][2] - 4, ["провод ILL+ / TAIL", "(+12 В при габаритах)"], "#7a1010", 10)
s.line([(210, x4[5][2]), (260, x4[5][2])], RED, 3); s.num(235, x4[5][2] - 14, 1)
for k in (1, 2, 3, 4):
    s.text(330, x4[k][2] + 4, "шаг 7", 10, "#aaa")
yB = x4[5][2]
nB = (520, yB)
res(s, (316, yB), nB, "47 к", RED); s.num(418, yB + 18, 2); s.dot(*nB)
s.text(nB[0] + 4, yB - 10, "база", 10, "#111")
gnd = 400
res(s, nB, (nB[0], gnd), "100 к"); s.num(nB[0] - 18, 320, 3)
s.line([nB, (580, yB)], BLK); s.dot(580, yB)
cap(s, (580, yB), (580, gnd), "100 нФ"); s.num(562, 360, 4)
s.line([(580, yB), (670, yB)], BLK); s.dot(670, yB)
diode(s, (670, yB), (670, gnd), "1N4148", "top", ["полоска — вверх,", "к базе"]); s.num(652, 290, 5)
q = to92(s, 760, 110, "BC547", ["C", "B", "E"])
s.line([q["B"], (q["B"][0], yB), (670, yB)], BLK)
s.line([q["E"], (q["E"][0] + 14, q["E"][1]), (q["E"][0] + 14, gnd)], BLK); s.num(q["E"][0] + 30, 300, 6)
bus(s, 490, 820, gnd, "GND")
s.line([q["C"], (q["C"][0], q["C"][1] + 6), (700, q["C"][1] + 6), (700, 92), (880, 92)], GRY); s.dot(820, 92)
s.num(700, 140, 7)
wpe = weact(s, 880, 54, [("pe15", "PE15", "P2.38")], "left", 190)
s.line([(880, 92), wpe["pe15"]], GRY)
res(s, (820, 92), (820, 160 + 60), "10 к", RED)
s.line([(812, 220), (870, 220)], RED, 4); s.text(876, 224, "шина 3V3", 10, RED, weight="bold"); s.num(802, 128, 8)
s.text(690, 150, "плоской стороной к себе,", 10, "#444", "end")
s.text(690, 164, "ножки вниз: C B E", 10, "#444", "end")
check(s, 420, ["БП 12 В на X4.5 (минус на GND): на PE15 ~0 В. Без него ~3.3 В. При 9 В — тоже ноль."])
s.save("step-8-parking-lights.svg")
