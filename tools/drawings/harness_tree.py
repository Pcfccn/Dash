"""Generate docs/future/harness.svg: the harness as a tree, box -> car / display / GPS / buttons.

Usage (from the repo root): python tools/drawings/harness_tree.py docs/future/harness.svg
Source of truth: docs/future/harness.md and the X1-X5 table in build-hardware.md."""
import sys
from pathlib import Path

OUT = Path(sys.argv[1])
W, H = 1320, 800
o = []
def a(s): o.append(s)
def esc(t): return t.replace("&", "&amp;").replace("<", "&lt;").replace(">", "&gt;")
RED, BLK, YEL, GRN, ORG, GRY, BLU, PUR = "#d62828", "#222", "#d9a400", "#2a9d3a", "#ee8a00", "#6c757d", "#1f6fd1", "#7a4fb0"

def text(x, y, t, size=12, color="#222", anchor="start", weight="normal"):
    a(f'<text x="{x}" y="{y}" font-size="{size}" fill="{color}" text-anchor="{anchor}" font-weight="{weight}">{esc(t)}</text>')

def box(x, y, w, h, title, lines=(), stroke="#555", fill="#f7f7f7"):
    a(f'<rect x="{x}" y="{y}" width="{w}" height="{h}" rx="8" fill="{fill}" stroke="{stroke}" stroke-width="1.6"/>')
    text(x + 10, y + 20, title, 13, "#111", weight="bold")
    for i, ln in enumerate(lines):
        text(x + 10, y + 38 + i * 15, ln, 11, "#333")

def cable(pts, colors, label_at=None, label=(), lcolor="#333"):
    """A bundle: thin coloured lines side by side, each segment offset across its own direction."""
    n = len(colors)
    for (x1, y1), (x2, y2) in zip(pts, pts[1:]):
        horiz = abs(x2 - x1) >= abs(y2 - y1)
        for i, c in enumerate(colors):
            off = (i - (n - 1) / 2) * 3.2
            dx, dy = (0, off) if horiz else (off, 0)
            a(f'<line x1="{x1 + dx}" y1="{y1 + dy}" x2="{x2 + dx}" y2="{y2 + dy}" stroke="{c}" '
              'stroke-width="2.2" stroke-linecap="round"/>')
    if label_at:
        lx, ly = label_at
        for i, ln in enumerate(label):
            text(lx, ly + i * 15, ln, 11, lcolor, weight="bold" if i == 0 else "normal")

def plug(x, y, name, vertical=False):
    w, h = (26, 40) if vertical else (40, 26)
    a(f'<rect x="{x - w/2}" y="{y - h/2}" width="{w}" height="{h}" rx="4" fill="#e3f1e3" stroke="#1d5a1d" stroke-width="1.5"/>')
    text(x, y + 4, name, 11, "#1d5a1d", "middle", "bold")

a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" font-family="DejaVu Sans, Arial, sans-serif">')
a('<rect width="100%" height="100%" fill="#fff"/>')
text(20, 30, "Жгут Dash: пять веток от коробки", 20, "#111", weight="bold")
text(20, 52, "Длины мерить по месту по будущей трассе (шнурком), + 15 см запаса. Исключение — экран: шлейф X5 не длиннее 15–20 см.", 12, "#555")

# the box
BX, BY, BW, BH = 520, 300, 280, 170
a(f'<rect x="{BX}" y="{BY}" width="{BW}" height="{BH}" rx="12" fill="#fbfbf3" stroke="#8a8a5a" stroke-width="2.5"/>')
text(BX + BW / 2, BY + 60, "Коробка Dash", 16, "#6a6a3a", "middle", "bold")
text(BX + BW / 2, BY + 82, "140 × 82 × 38", 12, "#6a6a3a", "middle")
text(BX + BW / 2, BY + 100, "провода — через отверстия", 11, "#6a6a3a", "middle")
text(BX + BW / 2, BY + 114, "в стенке, высоко (25–30 мм от дна)", 11, "#6a6a3a", "middle")
plug(BX, BY + 40, "X1"); plug(BX, BY + 130, "X2")
plug(BX + BW, BY + 40, "X5"); plug(BX + BW, BY + 100, "X3")
plug(BX + BW / 2, BY + BH, "X4", True)

# X1 power
box(30, 90, 250, 92, "Провод ACC", ["найти мультиметром:", "12 В в ACC и ON, 0 В без ключа", "врезка: пайка, без разреза провода"], RED, "#fff5f5")
box(30, 230, 250, 60, "Масса кузова", ["болт на металле, краску зачистить"], BLK, "#f4f4f4")
a(f'<rect x="300" y="142" width="74" height="26" rx="4" fill="#fff" stroke="{RED}" stroke-width="2"/>')
text(337, 160, "2 А", 12, RED, "middle", "bold")
text(337, 186, "держатель", 10, RED, "middle"); text(337, 198, "у врезки", 10, RED, "middle")
cable([(280, 155), (300, 155)], [RED])
cable([(374, 155), (440, 155), (440, BY + 34), (BX - 20, BY + 34)], [RED], (452, 196), ["X1.1 красный", "+12 В ACC"], RED)
cable([(280, 262), (420, 262), (420, BY + 46), (BX - 20, BY + 46)], [BLK], (300, 252), ["X1.2 чёрный — масса"], BLK)

# X2 CAN
box(30, 420, 270, 132, "Разветвитель OBD2 (Y)", ["штекер — в разъём OBD машины", "гнездо 1 — свободно для сканера",
                                                  "гнездо 2 — Dash: штекер OBD2 «папа»", "или хвост разветвителя:",
                                                  "пины 6, 14, 5"], PUR, "#f7f2fc")
cable([(300, 486), (450, 486), (450, BY + 130), (BX - 20, BY + 130)], [YEL, GRN, BLK], (30, 576),
      ["X2: 3 провода", "X2.1 жёлтый — пин 6 CAN-H", "X2.2 зелёный — пин 14 CAN-L", "X2.3 чёрный — пин 5 земля",
       "жёлтый и зелёный — витая пара,", "3–4 витка на 10 см"], PUR)

# X5 display
box(1010, 90, 290, 110, "Экран 4\" ILI9488", ["8 проводов, ≤ 15–20 см (SPI 18.75 МГц)", "коробка — прямо за экраном",
                                             "к выводам экрана — пайкой", "+ термоусадка, без разъёма"], "#16a085", "#eefaf6")
cable([(BX + BW + 20, BY + 40), (900, BY + 40), (900, 160), (1010, 160)], [RED, BLK, GRY, GRY, GRY, GRY, GRY, ORG],
      (735, 232), ["X5: 8 проводов", "X5.1 VCC … X5.8 LED"], "#0f5f50")

# X3 GPS
box(1010, 330, 290, 110, "Плата GPS ATGM336H", ["под лобовым стеклом, антенной вверх,", "не под металлом и тонировкой с фольгой",
                                               "VCC — только 3V3", "до 1.5 м, подальше от силовых проводов"], BLU, "#eef5fc")
cable([(BX + BW + 20, BY + 100), (1010, BY + 100)], [RED, BLK, BLU, GRY], (840, BY + 128),
      ["X3: 4 провода", "1 3V3 · 2 GND · 3 TX · 4 RX"], BLU)

# X4 buttons + ILL
box(470, 610, 380, 160, "Заглушка LC100: 3 кнопки 8 мм (1NO)", ["контакт 1 кнопок ▶ / ● / ◀ → X4.1 / X4.2 / X4.3",
                                                               "контакт 2 всех трёх — вместе → X4.4 (GND)",
                                                               "кольцо подсветки 12–24 В — НЕ через Dash:",
                                                               "  «+» → ILL+, «−» → ILL− (гаснет реостатом)",
                                                               "  или «−» на массу (всегда ярко)",
                                                               "разъём 6.3 мм 8P у заглушки — по желанию"], GRY, "#f6f7f8")
cable([(BX + BW / 2, BY + BH + 20), (BX + BW / 2, 610)], [GRY, GRY, GRY, BLK], (BX + BW / 2 + 14, 548),
      ["X4.1–X4.4: 4 провода к кнопкам"], GRY)
box(960, 620, 340, 120, "ILL+ (габариты)", ["12 В при включённых габаритах, при любом", "положении реостата (плюс, не ILL−)",
                                            "удобно взять у разъёма соседней", "штатной кнопки с подсветкой"], RED, "#fff5f5")
cable([(960, 680), (900, 680), (900, 520), (BX + BW / 2 + 30, 520), (BX + BW / 2 + 30, BY + BH + 20)], [RED],
      (910, 506), ["X4.5 красный — ILL+"], RED)
text(BX + BW / 2 - 16, BY + BH + 44, "X4.6 — запас", 10, "#888", "end")

a("</svg>")
OUT.write_text("\n".join(o), encoding="utf-8")
print("written", OUT)
