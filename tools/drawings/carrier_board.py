"""Generate docs/future/carrier-board.svg — top view of the 7x9 cm carrier.

Usage (from the repo root): python tools/drawings/carrier_board.py docs/future/carrier-board.svg
Keep it in step with carrier-board.md (pinout, layout, wire list)."""
import sys
from pathlib import Path

OUT = Path(sys.argv[1])
S = 8.0                      # px per mm
P = 2.54
BW, BH = 70.0, 90.0          # board mm
COLS, ROWS = 24, 30
X0 = (BW - (COLS - 1) * P) / 2          # first hole column, mm
Y0 = (BH - (ROWS - 1) * P) / 2
OX, OY = 40, 70              # board origin on the canvas, px
W, H = 1180, 860

def hx(c): return OX + (X0 + c * P) * S
def hy(r): return OY + (Y0 + r * P) * S
COLN = "ABCDEFGHIJKLMNOPQRSTUVWX"

out = []
def a(s): out.append(s)

a(f'<svg xmlns="http://www.w3.org/2000/svg" width="{W}" height="{H}" viewBox="0 0 {W} {H}" '
  'font-family="DejaVu Sans, Arial, sans-serif">')
a('<rect width="100%" height="100%" fill="#ffffff"/>')
a(f'<text x="{OX}" y="28" font-size="20" font-weight="bold" fill="#1a1a1a">Dash — плата-носитель 7×9 см, вид сверху (сторона компонентов)</text>')
a(f'<text x="{OX}" y="50" font-size="13" fill="#555">Сетка 2.54 мм, 24 × 30 отверстий. WeAct MiniSTM32H743 сверху на гнёздах; распиновка — схема WeAct V12 (Header 22X2 P1 / P2).</text>')

# board
a(f'<rect x="{OX}" y="{OY}" width="{BW*S}" height="{BH*S}" rx="10" fill="#e9f3e6" stroke="#4a7a3a" stroke-width="2"/>')
# column / row labels
for c in range(COLS):
    a(f'<text x="{hx(c)}" y="{OY - 6}" font-size="9" fill="#777" text-anchor="middle">{COLN[c]}</text>')
for r in range(ROWS):
    a(f'<text x="{OX - 8}" y="{hy(r) + 3}" font-size="9" fill="#777" text-anchor="end">{r + 1}</text>')
# holes
for c in range(COLS):
    for r in range(ROWS):
        a(f'<circle cx="{hx(c):.1f}" cy="{hy(r):.1f}" r="3" fill="#ffffff" stroke="#b8c9b0" stroke-width="1"/>')

# WeAct outline: headers P2 at cols A(even, outer) B(odd, inner); P1 at O(even, inner) P(odd, outer);
# pin pair k (pins 2k-1, 2k) at row k (0-based row index k, k=1..22 -> rows 2..23)
P2_EVEN, P2_ODD, P1_EVEN, P1_ODD = 0, 1, 14, 15
def prow(pin): return (pin + 1) // 2      # pin 1/2 -> row index 1
mid_x = (hx(P2_EVEN) + hx(P1_ODD)) / 2
wx0, wx1 = mid_x - 20.3 * S, mid_x + 20.3 * S
wy0 = hy(prow(1)) - 6.3 * S
wy1 = hy(prow(43)) + 6.3 * S
a(f'<rect x="{wx0:.1f}" y="{wy0:.1f}" width="{wx1-wx0:.1f}" height="{wy1-wy0:.1f}" rx="6" '
  'fill="#2b4f86" fill-opacity="0.10" stroke="#2b4f86" stroke-width="2" stroke-dasharray="8 5"/>')
a(f'<text x="{mid_x:.1f}" y="{wy1 - 22:.1f}" font-size="15" fill="#2b4f86" text-anchor="middle" font-weight="bold">WeAct MiniSTM32H743 — сверху, на гнёздах</text>')
a(f'<text x="{mid_x:.1f}" y="{wy1 - 7:.1f}" font-size="11" fill="#2b4f86" text-anchor="middle">под ней — только провода</text>')
a(f'<text x="{hx(P2_ODD) + 8:.1f}" y="{hy(prow(1)) - 14:.1f}" font-size="11" fill="#2b4f86">P2</text>')
a(f'<text x="{hx(P1_EVEN) - 22:.1f}" y="{hy(prow(1)) - 14:.1f}" font-size="11" fill="#2b4f86">P1</text>')

# sockets: pins 5..44 (rows of pins 5/6 .. 43/44) — 4 strips of 20 from the two 1x40
for col in (P2_EVEN, P2_ODD, P1_EVEN, P1_ODD):
    first = 7 if col in (P2_EVEN, P2_ODD) else 5      # P2: strips of 19 (cut loses one)
    ytop, ybot = hy(prow(first)) - 4, hy(prow(43)) + 4
    a(f'<rect x="{hx(col)-4:.1f}" y="{ytop:.1f}" width="8" height="{ybot-ytop:.1f}" fill="#222" fill-opacity="0.55" rx="2"/>')
# pins 1..4 without sockets
for col in (P2_EVEN, P2_ODD, P1_EVEN, P1_ODD):
    for pin_row in ((prow(1), prow(3), prow(5)) if col in (P2_EVEN, P2_ODD) else (prow(1), prow(3))):
        a(f'<circle cx="{hx(col):.1f}" cy="{hy(pin_row):.1f}" r="3.5" fill="none" stroke="#c0392b" stroke-width="1.5" stroke-dasharray="2 2"/>')

P1 = {1:"GND",2:"5V",3:"PE1",4:"PE0",5:"PB9",6:"PB8",7:"PB7",8:"PB6",9:"PB5",10:"PB4",11:"PB3",12:"PD7",
      13:"PD6",14:"PD5",15:"PD4",16:"PD3",17:"PD2",18:"PD1",19:"PD0",20:"PC12",21:"PC11",22:"PC10",23:"PA15",
      24:"PA12",25:"PA11",26:"PA10",27:"PA9",28:"PA8",29:"PC9",30:"PC8",31:"PC7",32:"PC6",33:"PD15",34:"PD14",
      35:"PD13",36:"PD12",37:"PD11",38:"PD10",39:"PD9",40:"PD8",41:"PB15",42:"PB14",43:"PB13",44:"PB12"}
P2 = {1:"GND",2:"3V3",3:"PE2",4:"PE3",5:"PE4",6:"PE5",7:"PE6",8:"VBAT",9:"PC13",10:"NRST",11:"PC0",12:"PC1",
      13:"PC2",14:"PC3",15:"GND",16:"VREF+",17:"PA0",18:"PA1",19:"PA2",20:"PA3",21:"PA4",22:"PA5",23:"PA6",
      24:"PA7",25:"PC4",26:"PC5",27:"PB0",28:"PB1",29:"PB2",30:"PE7",31:"PE8",32:"PE9",33:"PE10",34:"PE11",
      35:"PE12",36:"PE13",37:"PE14",38:"PE15",39:"PB10",40:"PB11",41:"3V3",42:"5V",43:"GND",44:"GND"}
USED = {  # (header, pin): (label, color)
    (1,18): ("PD1 CAN TX", "#8e44ad"), (1,19): ("PD0 CAN RX", "#8e44ad"),
    (1,28): ("PA8 подсветка", "#d35400"), (1,41): ("PB15 MOSI", "#16a085"), (1,43): ("PB13 SCK", "#16a085"),
    (2,11): ("PC0 CS", "#16a085"), (2,25): ("PC4 DC", "#16a085"), (2,26): ("PC5 RST", "#16a085"),
    (2,19): ("PA2 TX→GPS", "#2980b9"), (2,20): ("PA3 RX←GPS", "#2980b9"),
    (2,30): ("PE7 ▶", "#c0392b"), (2,31): ("PE8 ●", "#c0392b"), (2,32): ("PE9 ◀", "#c0392b"),
    (2,38): ("PE15 габариты", "#c0392b"),
    (2,41): ("3V3", "#e67e22"), (2,42): ("5V", "#e74c3c"), (2,43): ("GND", "#000"), (2,44): ("GND", "#000"),
    (2,15): ("GND", "#000"),
}
def pin_xy(hdr, pin):
    if hdr == 1: col = P1_ODD if pin % 2 else P1_EVEN
    else:        col = P2_ODD if pin % 2 else P2_EVEN
    return hx(col), hy(prow(pin))
rows = {}
for (hdr, pin), (lab, colr) in sorted(USED.items()):
    x, y = pin_xy(hdr, pin)
    a(f'<circle cx="{x:.1f}" cy="{y:.1f}" r="4.5" fill="{colr}"/>')
    rows.setdefault((hdr, prow(pin)), []).append((pin, lab, colr, y))
for (hdr, r), items in rows.items():
    y = items[0][3]
    txt = " · ".join(f"{pin} {lab}" for pin, lab, _, _ in items)
    colr = items[0][2]
    if hdr == 2:
        a(f'<text x="{hx(P2_ODD) + 9:.1f}" y="{y + 3.5:.1f}" font-size="9" fill="{colr}">P2.{txt}</text>')
    else:
        a(f'<text x="{hx(P1_EVEN) - 9:.1f}" y="{y + 3.5:.1f}" font-size="9" fill="{colr}" text-anchor="end">P1.{txt}</text>')

# terminal blocks KF2EDGV-3.81 (KEFA drawing): body (n*3.81 + 0.8) x 7.0 mm, pins 0.8 sq x 3.7 mm.
# On the 2.54 grid every pin is stepped 0.635 mm (a quarter hole), pairs 1-2, 3-4.. toward each other:
# pin k (0-based) sits at 1.5k - 0.25 holes from the first hole and lands in hole round(1.5k - 0.25)
# -> holes 1,2, 4,5, 7,8, 10,11 (1-based). The plug (11.1 mm deep) is shown dashed, overhanging outward.
def block(name, c0, r0, vertical, labels, color):
    n = len(labels)
    orig = [1.5 * k - 0.25 for k in range(n)]           # in holes, from the first hole
    hole = [round(o) for o in orig]
    mid = (orig[0] + orig[-1]) / 2 * P                  # mm from the first hole
    half = (n * 3.81 + 0.8) / 2
    if vertical:
        x = hx(c0)
        y_at = lambda h: hy(r0) + h * P * S
        y0, y1 = hy(r0) + (mid - half) * S, hy(r0) + (mid + half) * S
        a(f'<rect x="{x - 3.5*S:.1f}" y="{y0:.1f}" width="{11.1*S:.1f}" height="{y1-y0:.1f}" rx="3" fill="none" '
          'stroke="#1d5a1d" stroke-width="1.2" stroke-dasharray="5 4"/>')
        a(f'<rect x="{x - 3.5*S:.1f}" y="{y0:.1f}" width="{7*S:.1f}" height="{y1-y0:.1f}" rx="3" fill="#3a8f3a" fill-opacity="0.80" stroke="#1d5a1d"/>')
        for i, lab in enumerate(labels):
            yo, yh = y_at(orig[i]), y_at(hole[i])
            a(f'<line x1="{x:.1f}" y1="{yo:.1f}" x2="{x:.1f}" y2="{yh:.1f}" stroke="#222" stroke-width="2"/>')
            a(f'<rect x="{x-2:.1f}" y="{yo-2:.1f}" width="4" height="4" fill="#222"/>')
            a(f'<circle cx="{x:.1f}" cy="{yh:.1f}" r="3.6" fill="#f1c40f" stroke="#222" stroke-width="1"/>')
            a(f'<text x="{x + 8.1*S:.1f}" y="{yh + 3.5:.1f}" font-size="10" fill="#1a1a1a">{name}.{i+1} {lab}</text>')
        a(f'<text x="{x:.1f}" y="{y0 - 6:.1f}" font-size="12" font-weight="bold" fill="{color}" text-anchor="middle">{name}</text>')
    else:
        y = hy(r0)
        x_at = lambda h: hx(c0) + h * P * S
        x0, x1 = hx(c0) + (mid - half) * S, hx(c0) + (mid + half) * S
        a(f'<rect x="{x0:.1f}" y="{y - 3.5*S:.1f}" width="{x1-x0:.1f}" height="{11.1*S:.1f}" rx="3" fill="none" '
          'stroke="#1d5a1d" stroke-width="1.2" stroke-dasharray="5 4"/>')
        a(f'<rect x="{x0:.1f}" y="{y - 3.5*S:.1f}" width="{x1-x0:.1f}" height="{7*S:.1f}" rx="3" fill="#3a8f3a" fill-opacity="0.80" stroke="#1d5a1d"/>')
        for i, lab in enumerate(labels):
            xo, xh = x_at(orig[i]), x_at(hole[i])
            a(f'<line x1="{xo:.1f}" y1="{y:.1f}" x2="{xh:.1f}" y2="{y:.1f}" stroke="#222" stroke-width="2"/>')
            a(f'<rect x="{xo-2:.1f}" y="{y-2:.1f}" width="4" height="4" fill="#222"/>')
            a(f'<circle cx="{xh:.1f}" cy="{y:.1f}" r="3.6" fill="#f1c40f" stroke="#222" stroke-width="1"/>')
        a(f'<text x="{x0 - 5:.1f}" y="{y + 4:.1f}" font-size="12" font-weight="bold" fill="{color}" text-anchor="end">{name}</text>')
        ly = y - 3.5*S - 6
        a(f'<text x="{(x0+x1)/2:.1f}" y="{ly:.1f}" font-size="9" fill="#1a1a1a" text-anchor="middle">'
          + " · ".join(f"{i+1} {lab}" for i, lab in enumerate(labels)) + '</text>')

# right edge, vertical: column W (index 22), plug overhang toward the board edge
block("X5", 22, 1, True, ["VCC 3V3","GND","CS","RST","DC","SDI","SCK","LED"], "#16a085")
block("X4", 22, 15, True, ["▶","●","◀","GND","ILL+","запас"], "#c0392b")
# bottom edge, horizontal: last row (30), plug overhang toward the board edge
block("X1", 2, 29, False, ["+12","GND"], "#e74c3c")
block("X3", 7, 29, False, ["3V3","GND","TX","RX"], "#2980b9")
block("X2", 15, 29, False, ["CAN-H","CAN-L","GND"], "#8e44ad")

# small parts in the free right column (cols R..U)
def part(c0, r0, w, h, text, color):
    x, y = hx(c0) - 4, hy(r0) - 4
    a(f'<rect x="{x:.1f}" y="{y:.1f}" width="{w*P*S+8:.1f}" height="{h*P*S+8:.1f}" rx="4" fill="{color}" fill-opacity="0.15" stroke="{color}" stroke-width="1.5"/>')
    for i, line in enumerate(text.split("|")):
        a(f'<text x="{x + 5:.1f}" y="{y + 14 + i*12:.1f}" font-size="10" fill="{color}">{line}</text>')
part(17, 1, 3, 6, "Подсветка|2N7000|R1 10k→5V|R2 1k←PA8|R3 10k→GND", "#d35400")
part(17, 9, 3, 5, "Кнопки|3 × 1k→3V3|3 × 100n|→GND", "#c0392b")
# rows 17-23 only: row 24 carries the bare 3V3 bus underneath (row 25 GND)
part(17, 16, 3, 6, "Габариты|BC547|47k←ILL+|100k, 100n|1N4148|10k→3V3", "#c0392b")
part(4, 25, 10, 1.6, "TVS 1.5KE20A: полоска (катод) к X1.1 +12, к входу PD65W", "#e74c3c")

a(f'<text x="{hx(5):.1f}" y="{hy(1) + 2:.1f}" font-size="1" fill="#fff"> </text>')

# legend / notes on the right
lx = OX + BW * S + 110
notes = [
 ("Гнёзда под WeAct", "#222"),
 ("Из двух 1×40 (C9811): 2 полоски по 20 (выводы 5–44)", "#333"),
 ("(P1) и 2 по 19 (P2, выводы 7–44). Красный пунктир —", "#333"),
 ("без гнёзд: GND / 5V / 3V3 есть ниже (P2.41–44).", "#333"),
 ("", ""),
 ("Клеммники 3.81 мм на сетке 2.54", "#222"),
 ("Каждый вывод — ступенькой 0.6 мм (■ → ●): пары 1–2,", "#333"),
 ("3–4, 5–6, 7–8 навстречу друг другу; занято 1, 2, 4, 5,", "#333"),
 ("7, 8, 10, 11-е отверстие ряда. Вывод 0.8 × 0.8 мм: если", "#333"),
 ("не входит в отверстие — рассверлить до Ø1.4.", "#333"),
 ("Пунктир — вилка (11.1 мм), висит выше 10 мм.", "#333"),
 ("Провода из вилки выходят вверх: плата на гайке M3", "#333"),
 ("(2.4 мм), не на стойках 8 мм — иначе не влезет по высоте.", "#333"),
 ("", ""),
 ("Вне платы (в коробке, на скотче VHB)", "#222"),
 ("PD65W: вход ← X1 через TVS; USB-C → WeAct.", "#333"),
 ("SN65HVD230: 3V3, GND, TX←PD1, RX→PD0, CANH/L → X2.", "#333"),
 ("Si4599: исток 5V (P2.42), затвор ← драйвер, сток → X5.8.", "#333"),
 ("GPS — на своей плате под стеклом, кабель → X3.", "#333"),
 ("", ""),
 ("Перед пайкой", "#222"),
 ("Сверить с шелкографией WeAct: P1.1 = GND, P1.2 = 5V,", "#333"),
 ("P2.1 = GND, P2.2 = 3V3 (схема V12). Ряды: у P1 нечётные", "#333"),
 ("снаружи, у P2 — внутри (вид сверху).", "#333"),
 ("Полный список проводов — carrier-board.md.", "#333"),
]
for i, (t, colr) in enumerate(notes):
    if not t: continue
    bold = ' font-weight="bold"' if colr == "#222" else ""
    a(f'<text x="{lx}" y="{OY + 10 + i*18}" font-size="13" fill="{colr}"{bold}>{t}</text>')

a('</svg>')
OUT.write_text("\n".join(out), encoding="utf-8")
print("written", OUT)
