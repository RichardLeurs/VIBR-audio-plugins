#!/usr/bin/env python3
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
"""Renders the TR-8 drum machine front panel (2400 x 1400) used as the plugin background: lime satin body (Rhythm group),
black panels TEMPO / PATTERN / CONTROLS / VOICES / STEPS, tinted voice columns, coloured buttons, printed legends.
The editor works in half-size units (1200 x 700); every position here is twice the editor's. Needs numpy + Pillow.
Usage: make_background.py out.png"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H, S = 2400, 1400, 2
rng = np.random.default_rng(8)
BODY = (165, 201, 67)                          # lime #A5C943 (Rhythm group)
ACC = BODY
INK = (238, 238, 232)
DARK = (27, 33, 13)
PANEL = (24, 25, 28)
FONT = '/System/Library/Fonts/Avenir Next.ttc'
def font(size, idx=0): return ImageFont.truetype(FONT, int(size * S), index=idx)

# ----- layout (editor units are half of these) -----
PT1, PB1 = 230, 620
P_TEMPO, P_PATTERN, P_CTRL = (40, PT1, 620, PB1), (640, PT1, 1560, PB1), (1580, PT1, 2360, PB1)
P_VOICES = (40, 650, 2360, 1135)
P_STEPS = (40, 1160, 2360, 1350)
KIT = (560, 56, 660, 70)
DISP = (80, 290, 300, 100)
GRP = {  # instrument colours
    'red': (232, 54, 40), 'orange': (242, 138, 56), 'amber': (240, 190, 50), 'cream': (244, 238, 220), 'teal': (110, 205, 215)}
VOICES = [("BD", "BASS DRUM", 'red'), ("SD", "SNARE", 'orange'), ("LT", "LOW TOM", 'amber'), ("MT", "MID TOM", 'amber'), ("HT", "HI TOM", 'amber'),
          ("RS", "RIMSHOT", 'cream'), ("CP", "HAND CLAP", 'cream'), ("CB", "COWBELL", 'cream'), ("CY", "CYMBAL", 'teal'),
          ("OH", "OPEN HAT", 'teal'), ("CH", "CLOSED HAT", 'teal')]
def colx(i): return 146 + 211 * i
ROWS = (865, 965, 1065)
KR_V = 28

def comp(layer):
    global img
    img = Image.alpha_composite(img, layer)
def new(): return Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))

def shadow(cx, cy, r, dx=5, dy=9, blur=9, alpha=150):
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).ellipse([(cx + dx - r) * S, (cy + dy - r * 0.95) * S, (cx + dx + r) * S, (cy + dy + r) * S], fill=alpha)
    m = m.filter(ImageFilter.GaussianBlur(blur * S))
    layer = new(); layer.putalpha(m); comp(layer)

def text(x, y, s, fnt, fill=INK, anchor='mm', spacing=0):
    m = Image.new('L', (W * S, H * S), 0)
    dr = ImageDraw.Draw(m)
    if spacing:
        total = sum(dr.textlength(ch, font=fnt) + spacing * S for ch in s) - spacing * S
        px = x * S - total / 2 if anchor[0] == 'm' else (x * S - total if anchor[0] == 'r' else x * S)
        for ch in s:
            dr.text((px, y * S), ch, font=fnt, fill=255, anchor='l' + anchor[1]); px += dr.textlength(ch, font=fnt) + spacing * S
    else:
        dr.text((x * S, y * S), s, font=fnt, fill=255, anchor=anchor)
    ink = np.asarray(m, np.float32) / 255 * (0.90 + 0.10 * rng.random(m.size[::-1]).astype(np.float32))
    layer = Image.new('RGBA', (W * S, H * S), tuple(fill) + (0,))
    layer.putalpha(Image.fromarray((ink * 255).astype(np.uint8)))
    comp(layer)

def line(pts, fill=INK, width=2, alpha=190):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.line([(px * S, py * S) for px, py in pts], fill=tuple(fill) + (alpha,), width=int(width * S), joint='curve')
    comp(layer)

# ----- base surface: satin powder coat -----
yy, xx = np.mgrid[0:H * S, 0:W * S].astype(np.float32)
xn, yn = xx / (W * S), yy / (H * S)
light = 1.22 - 0.55 * np.hypot(xn - 0.28, (yn - 0.10) * 0.8)
sheen = 0.10 * np.exp(-((xn * 0.55 + yn * 0.45 - 0.42) / 0.10) ** 2)
blot = Image.fromarray((rng.random((H // 6, W // 6)) * 255).astype(np.uint8)).resize((W * S, H * S), Image.BICUBIC).filter(ImageFilter.GaussianBlur(40 * S))
blot = (np.asarray(blot, np.float32) / 255 - 0.5) * 0.10
grain = rng.normal(0, 0.028, (H * S, W * S)).astype(np.float32)
lum = light + sheen + blot + grain
d = np.minimum(np.minimum(xx, yy), np.minimum(W * S - 1 - xx, H * S - 1 - yy)) / S
dirn = 0.65 + 0.35 * np.clip((1.0 - xn) * 0.5 + (1.0 - yn) * 0.5, 0, 1)
lum = lum + 0.55 * np.exp(-((d - 5) / 3.2) ** 2) * dirn
lumn = lum / np.median(lum)
sw = np.array(BODY, np.float32)[None, None, :]
rgb = np.where(lumn[:, :, None] <= 1.0, sw * lumn[:, :, None], sw + (255 - sw) * (1.0 - 1.0 / np.maximum(lumn, 1.0))[:, :, None])
img = Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')

# ----- title -----
text(46, 110, "TR-8", font(140, 9), DARK, 'lm')
text(2360, 86, "RHYTHM COMPOSER", font(40, 5), DARK, 'rm', spacing=6)
text(2360, 134, "11 VOICES  ·  16 STEPS  ·  A / B", font(34, 5), DARK, 'rm', spacing=6)
line([(40, 208), (2360, 208)], DARK, 2, 150)

# ----- panels -----
def panel(box, title, fill=PANEL):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([v * S for v in box], radius=34 * S, fill=tuple(fill) + (255,))
    dr.rounded_rectangle([v * S for v in box], radius=34 * S, outline=INK + (150,), width=2 * S)
    comp(layer)
    cx = (box[0] + box[2]) / 2
    f = font(36, 0)
    pw = sum(ImageDraw.Draw(Image.new('L', (8, 8))).textlength(ch, font=f) + 3 * S for ch in title) / S - 3 + 76
    pill = new(); pd = ImageDraw.Draw(pill)
    pd.rounded_rectangle([(cx - pw / 2) * S, (box[1] - 22) * S, (cx + pw / 2) * S, (box[1] + 22) * S], radius=22 * S, fill=tuple(fill) + (255,))
    comp(pill)
    text(cx, box[1] + 1, title, f, ACC, spacing=3)

panel(P_TEMPO, "TEMPO")
panel(P_PATTERN, "PATTERN")
panel(P_CTRL, "CONTROLS")
panel(P_VOICES, "VOICES")
panel(P_STEPS, "STEP SEQUENCER")

# ----- kit selector socket (live text) -----
text(KIT[0] - 20, KIT[1] + 35, "KIT", font(34, 5), DARK, 'rm', spacing=4)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle([KIT[0] * S, KIT[1] * S, (KIT[0] + KIT[2]) * S, (KIT[1] + KIT[3]) * S], radius=16 * S, fill=(8, 10, 12, 255), outline=(255, 255, 255, 70), width=2 * S)
comp(layer)

# ----- knobs: recess + scale -----
def scale(x, y, ticks, a0, a1, r, big_at=None, labels=None, lab_r=None, lab_font=24, t0=20, t1=31, t2=36, rim=12):
    shadow(x, y, r + rim, 5, 9, 9, 170)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(x - r - rim) * S, (y - r - rim) * S, (x + r + rim) * S, (y + r + rim) * S], outline=(0, 0, 0, 120), width=3 * S)
    dr.ellipse([(x - r - rim - 3) * S, (y - r - rim - 3) * S, (x + r + rim + 3) * S, (y + r + rim + 3) * S], outline=(255, 255, 255, 40), width=2 * S)
    comp(layer)
    n = ticks - 1
    for k in range(ticks):
        a = math.radians(a0 + (a1 - a0) * k / (n if n else 1))
        big = k in (0, n // 2, n) if big_at is None else k in big_at
        r0, r1 = r + t0, r + (t2 if big else t1)
        line([(x + r0 * math.sin(a), y - r0 * math.cos(a)), (x + r1 * math.sin(a), y - r1 * math.cos(a))], INK, 3 if big else 2, 230)
        if labels:
            lr = lab_r or (r + 56)
            text(x + lr * math.sin(a), y - lr * math.cos(a), labels[k], font(lab_font, 2), INK, 'mm', spacing=0)

def name(x, y, s, size=30): text(x, y, s, font(size, 0), INK, 'mm', spacing=2)

# TEMPO panel
name(510, 280, "FINE", 28)
scale(510, 350, 11, -135, 135, 36, t0=14, t1=22, t2=26, rim=8)
scale(200, 510, 11, -135, 135, 58)
text(200 - 78, 604, "40", font(26, 2), INK, 'mm')
text(200 + 78, 604, "300", font(26, 2), INK, 'mm')
text(330, 412, "BPM", font(28, 2), INK, 'mm', spacing=2)
layer = new(); dr = ImageDraw.Draw(layer)                                # display recess
dr.rounded_rectangle([DISP[0] * S, DISP[1] * S, (DISP[0] + DISP[2]) * S, (DISP[1] + DISP[3]) * S], radius=14 * S, fill=(8, 5, 5, 255), outline=(255, 255, 255, 70), width=2 * S)
comp(layer)

# PATTERN panel
name(715, 292, "ACCENT", 28);        scale(715, 450, 11, -135, 135, 44)
name(925, 292, "PATTERN", 28)
scale(925, 450, 6, -90, 61, 44, big_at=[0, 5], labels=["CLEAR", "1ST", "2ND", "MAN", "PLAY", "COMP"], lab_r=100, lab_font=20)
name(1170, 292, "AUTO FILL", 28)
scale(1170, 450, 6, -90, 62, 44, big_at=[0, 5], labels=["MAN", "16", "12", "8", "4", "2"], lab_r=96, lab_font=22)
name(1410, 292, "INSTRUMENT", 28)
scale(1410, 450, 12, 180, 510, 46, big_at=[0], labels=["AC", "BD", "SD", "LT", "MT", "HT", "RS", "CP", "CB", "CY", "OH", "CH"], lab_r=100, lab_font=22)

# ----- buttons -----
def button(cx, cy, w, h, top, bot, label=None, label_fill=DARK, round_=False, size=28):
    sm = Image.new('L', (W * S, H * S), 0)
    sb = [(cx - w / 2 + 3) * S, (cy - h / 2 + 7) * S, (cx + w / 2 + 3) * S, (cy + h / 2 + 7) * S]
    if round_: ImageDraw.Draw(sm).ellipse(sb, fill=130)
    else: ImageDraw.Draw(sm).rounded_rectangle(sb, radius=min(14, h / 3) * S, fill=130)
    sm = sm.filter(ImageFilter.GaussianBlur(5 * S))
    sl = new(); sl.putalpha(sm); comp(sl)
    m = Image.new('L', (W * S, H * S), 0)
    box = [(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S]
    if round_: ImageDraw.Draw(m).ellipse(box, fill=255)
    else: ImageDraw.Draw(m).rounded_rectangle(box, radius=min(14, h / 3) * S, fill=255)
    t = np.clip((yy - box[1]) / (box[3] - box[1]), 0, 1)[:, :, None]
    col = np.array(top, np.float32) * (1 - t) + np.array(bot, np.float32) * t
    layer = Image.fromarray(np.dstack([col, np.full(t.shape[:2], 255, np.float32)]).astype(np.uint8), 'RGBA')
    layer.putalpha(m); comp(layer)
    ol = new(); od = ImageDraw.Draw(ol)
    if round_: od.ellipse(box, outline=(255, 255, 255, 110), width=2 * S)
    else: od.rounded_rectangle(box, radius=min(14, h / 3) * S, outline=(255, 255, 255, 110), width=2 * S)
    comp(ol)
    if label: text(cx, cy + 1, label, font(size, 0), label_fill, spacing=3)

button(480, 455, 190, 66, (96, 226, 120), (36, 150, 66), "START / STOP", size=22)
button(480, 550, 190, 66, (250, 200, 90), (205, 130, 30), "TAP", size=28)
button(2150, 520, 76, 76, (110, 170, 250), (40, 90, 190), None, round_=True)
button(2290, 520, 76, 76, (245, 90, 80), (170, 30, 30), None, round_=True)
text(2150, 585, "SHUFFLE", font(24, 2), INK, 'mm', spacing=2)
text(2290, 585, "CLEAR", font(24, 2), INK, 'mm', spacing=2)

# ----- CONTROLS panel: switches and lamps -----
def slot_v(cx, cy, w, h):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S], radius=w / 2 * S, fill=(5, 8, 14, 245), outline=(255, 255, 255, 70), width=2 * S)
    comp(layer)
def socket(cx, cy, r=14):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(cx - r - 3) * S, (cy - r - 3) * S, (cx + r + 3) * S, (cy + r + 3) * S], fill=(12, 14, 20, 255), outline=(255, 255, 255, 70), width=2 * S)
    comp(layer)

name(1680, 300, "PRE-SCALE", 26)
slot_v(1680, 445, 36, 200)
for k, (lab, sub) in enumerate((("1", "1/8T"), ("2", "1/16T"), ("3", "1/16"), ("4", "1/32"))):
    y = 370 + 50 * k
    text(1722, y, lab, font(30, 2), INK, 'lm')
    text(1750, y + 2, sub, font(20, 2), INK, 'lm')

name(1960, 300, "BASIC VARIATION", 26)
slot_v(1960, 400, 180, 46)
for dx, lab in ((-50, "A"), (0, "AB"), (50, "B")):
    text(1960 + dx, 448, lab, font(28, 2), INK, 'mm')
socket(1920, 350, 10); socket(2000, 350, 10)

name(1960, 495, "I / F VARIATION", 26)
slot_v(1960, 555, 180, 46)
text(1960 - 100, 555, "A", font(28, 2), INK, 'rm'); text(1960 + 100, 555, "B", font(28, 2), INK, 'lm')

name(2270, 300, "PART", 26)
socket(2270, 360, 10); socket(2270, 430, 10)
text(2300, 360, "1ST", font(26, 2), INK, 'lm'); text(2300, 430, "2ND", font(26, 2), INK, 'lm')
text(2150, 460, "", font(20))

# ----- VOICES panel: tinted columns, chips, knob scales, legends -----
for i, (code, full, g) in enumerate(VOICES):
    x = colx(i); col = GRP[g]
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(x - 100) * S, 678 * S, (x + 100) * S, 1122 * S], radius=24 * S, fill=col + (34,))
    comp(layer)
    button(x, 708, 150, 46, tuple(min(255, c + 20) for c in col), tuple(int(c * 0.78) for c in col), code, size=30)
    text(x, 752, full, font(21, 2), INK, 'mm', spacing=1)
knob_rows = {  # instrument index -> {row index: label}
    0: {0: "LEVEL", 1: "TONE", 2: "DECAY"}, 1: {0: "LEVEL", 1: "TONE", 2: "SNAPPY"}, 2: {0: "LEVEL", 1: "TUNING"}, 3: {0: "LEVEL", 1: "TUNING"},
    4: {0: "LEVEL", 1: "TUNING"}, 5: {0: "LEVEL"}, 6: {0: "LEVEL"}, 7: {0: "LEVEL"}, 8: {0: "LEVEL", 1: "TONE", 2: "DECAY"}, 9: {0: "LEVEL", 1: "DECAY"}, 10: {0: "LEVEL"}}
for i, rows in knob_rows.items():
    x = colx(i)
    for r_, lab in rows.items():
        y = ROWS[r_]
        scale(x - 40, y, 9, -135, 135, KR_V, big_at=[0, 8], t0=8, t1=14, t2=18, rim=5)
        text(x + 12, y, lab, font(22, 2), INK, 'lm', spacing=1)
for i, (up, down) in {2: ("CONGA", "TOM"), 3: ("CONGA", "TOM"), 4: ("CONGA", "TOM"), 5: ("CLAVES", "RIMSHOT"), 6: ("MARACAS", "CLAP")}.items():
    x = colx(i)
    slot_v(x - 44, ROWS[2], 30, 64)
    text(x - 6, ROWS[2] - 18, up, font(19, 2), INK, 'lm'); text(x - 6, ROWS[2] + 18, down, font(19, 2), INK, 'lm')

# ----- STEPS panel: coloured step buttons with numbers -----
STEP_COL = [GRP['red']] * 4 + [GRP['orange']] * 4 + [GRP['amber']] * 4 + [GRP['cream']] * 4
for k in range(16):
    x = 112.5 + 145 * k
    c = STEP_COL[k]
    socket(x, 1212, 11)
    button(x, 1276, 118, 60, tuple(min(255, v + 25) for v in c), tuple(int(v * 0.75) for v in c))
    text(x, 1328, str(k + 1), font(26, 2), INK, 'mm')

import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'common'))
from vibr_credit import credit
credit(text, new, comp, S, 2360, 1376, font(28, 2), DARK, 28)

out = img.convert('RGB').resize((W, H), Image.LANCZOS)
arr = np.asarray(out, np.float32) + rng.normal(0, 1.3, (H, W, 1)).astype(np.float32)
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(sys.argv[1] if len(sys.argv) > 1 else 'background.png')
print('ok')
