#!/usr/bin/env python3
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
"""Renders the MA-1 front panel (2400 x 1246) used as the plugin background: red-coral satin body (Amp & Cabinet group),
black panels GATE / BOOST / AMP / CABINET / EQ / OUTPUT, printed legends. Positions match PluginEditor.cpp. Needs numpy + Pillow.
Usage: make_background.py TAB out.png   (TAB 0 = AMP tab: GATE, TIGHT BOOST, AMP; 1 = CABINET tab: CABINET, EQ, OUTPUT)"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H, S = 2400, 1246, 2                      # H = the tall canvas everything is drawn on first; the tab image is cut from it (see the end)
H_FINAL = 856                                # final image height
TAB = int(sys.argv[1]) if len(sys.argv) > 2 else 0
rng = np.random.default_rng(8)
BODY = (217, 76, 72)                          # coral red #D94C48 (palette: IR Loader)
ACC = BODY
INK = (238, 238, 232)
DARK = (33, 12, 11)
PANEL = (24, 25, 28)
FONT = '/System/Library/Fonts/Avenir Next.ttc'
def font(size, idx=0): return ImageFont.truetype(FONT, int(size * S), index=idx)

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

# ----- base surface: satin powder coat (made at the final size) -----
def make_body(Hn):
    yy_, xx_ = np.mgrid[0:Hn * S, 0:W * S].astype(np.float32)
    xn, yn = xx_ / (W * S), yy_ / (Hn * S)
    light = 1.22 - 0.55 * np.hypot(xn - 0.28, (yn - 0.10) * 0.8)
    sheen = 0.10 * np.exp(-((xn * 0.55 + yn * 0.45 - 0.42) / 0.10) ** 2)
    blot = Image.fromarray((rng.random((Hn // 6, W // 6)) * 255).astype(np.uint8)).resize((W * S, Hn * S), Image.BICUBIC).filter(ImageFilter.GaussianBlur(40 * S))
    blot = (np.asarray(blot, np.float32) / 255 - 0.5) * 0.10
    grain = rng.normal(0, 0.028, (Hn * S, W * S)).astype(np.float32)
    lum = light + sheen + blot + grain
    d = np.minimum(np.minimum(xx_, yy_), np.minimum(W * S - 1 - xx_, Hn * S - 1 - yy_)) / S
    dirn = 0.65 + 0.35 * np.clip((1.0 - xn) * 0.5 + (1.0 - yn) * 0.5, 0, 1)
    lum = lum + 0.55 * np.exp(-((d - 5) / 3.2) ** 2) * dirn
    lumn = lum / np.median(lum)
    sw = np.array(BODY, np.float32)[None, None, :]
    rgb = np.where(lumn[:, :, None] <= 1.0, sw * lumn[:, :, None], sw + (255 - sw) * (1.0 - 1.0 / np.maximum(lumn, 1.0))[:, :, None])
    return Image.fromarray(np.clip(rgb, 0, 255).astype(np.uint8), 'RGB').convert('RGBA')

BODY_IMG = make_body(H_FINAL)
yy, xx = np.mgrid[0:H * S, 0:W * S].astype(np.float32)     # coordinates of the tall canvas (the push-button gradients use them)
img = Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))     # transparent content layer: the panels and legends are drawn on it first

# ----- layout (positions shared with PluginEditor.cpp) -----
PT1, PB1, PT2, PB2 = 268, 658, 718, 1108
P_GATE, P_BOOST, P_AMP = (40, PT1, 360, PB1), (380, PT1, 860, PB1), (880, PT1, 2360, PB1)
P_CAB, P_EQ, P_OUT = (40, PT2, 1216, PB2), (1236, PT2, 1916, PB2), (1936, PT2, 2360, PB2)
KR = 36

# ----- title -----
text(46, 110, "MA-1", font(140, 9), DARK, 'lm')
text(2360 - 190, 86, "METAL AMP & CABINET", font(40, 5), DARK, 'rm', spacing=6)
text(2360 - 190, 134, "HIGH-GAIN METAL", font(34, 5), DARK, 'rm', spacing=6)
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

panel(P_GATE, "GATE"); panel(P_BOOST, "TIGHT BOOST"); panel(P_AMP, "AMP"); panel(P_CAB, "CABINET"); panel(P_EQ, "EQ"); panel(P_OUT, "OUTPUT")

# power switch
rx, ry = 2325, 87
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle([(rx - 33) * S, (ry - 17) * S, (rx + 33) * S, (ry + 17) * S], radius=16 * S, fill=(6, 10, 18, 235))
dr.rounded_rectangle([(rx - 35) * S, (ry - 19) * S, (rx + 35) * S, (ry + 19) * S], radius=18 * S, outline=(255, 255, 255, 60), width=2 * S)
comp(layer)
text(rx + 35, ry + 50, "OFF / ON", font(26, 2), DARK, 'rm', spacing=2)

def scale(x, y, ticks, a0, a1, r, big_at=None, labels=None, lab_r=None, lab_font=22, t0=16, t1=24, t2=30, rim=10):
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
            lr = (lab_r[k] if isinstance(lab_r, (list, tuple)) else lab_r) or (r + 56)
            text(x + lr * math.sin(a), y - lr * math.cos(a), labels[k], font(lab_font, 2), INK, 'mm')

def name(x, y, s, size=28): text(x, y, s, font(size, 0), INK, 'mm', spacing=2)

def pushbutton(cx, cy, label, w=128, h=44):
    sm = Image.new('L', (W * S, H * S), 0)
    box = [(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S]
    ImageDraw.Draw(sm).rounded_rectangle([box[0] + 3 * S, box[1] + 7 * S, box[2] + 3 * S, box[3] + 7 * S], radius=12 * S, fill=130)
    sl = new(); sl.putalpha(sm.filter(ImageFilter.GaussianBlur(5 * S))); comp(sl)
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).rounded_rectangle(box, radius=12 * S, fill=255)
    t = np.clip((yy - box[1]) / (box[3] - box[1]), 0, 1)[:, :, None]
    col = np.array((70, 72, 78), np.float32) * (1 - t) + np.array((34, 35, 40), np.float32) * t
    layer = Image.fromarray(np.dstack([col, np.full(t.shape[:2], 255, np.float32)]).astype(np.uint8), 'RGBA')
    layer.putalpha(m); comp(layer)
    ol = new(); ImageDraw.Draw(ol).rounded_rectangle(box, radius=12 * S, outline=(255, 255, 255, 110), width=2 * S); comp(ol)
    text(cx + 14, cy + 1, label, font(22, 0), INK, spacing=1)
    # LED socket on the left (the lamp is drawn live)
    sock = new(); ImageDraw.Draw(sock).ellipse([(cx - w / 2 + 22 - 12) * S, (cy - 12) * S, (cx - w / 2 + 22 + 12) * S, (cy + 12) * S], fill=(12, 14, 20, 255), outline=(255, 255, 255, 70), width=2 * S); comp(sock)

def lamp_socket(x, y, r=14):
    shadow(x, y, r + 4, 2, 4, 4, 130)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(x - r - 3) * S, (y - r - 3) * S, (x + r + 3) * S, (y + r + 3) * S], fill=(12, 14, 20, 255), outline=(255, 255, 255, 70), width=2 * S)
    comp(layer)

MM = ("MIN", "MAX")
def knob(x, y, nm, lab=MM, r=KR, size=24, dx=40):
    scale(x, y, 11, -154, 154, r)
    name(x, y - 98, nm, size)
    if lab:
        text(x - dx, y + 76, lab[0], font(19, 2), INK, 'mm'); text(x + dx, y + 76, lab[1], font(19, 2), INK, 'mm')

# GATE
knob(115, 488, "THRESH", size=22); knob(260, 488, "RELEASE", ("FAST", "SLOW"), size=22)
text(290, 642, "OPEN", font(19, 2), INK, 'mm'); lamp_socket(290, 610, 12)
pushbutton(150, 613, "GATE")
# BOOST
knob(460, 488, "DRIVE"); knob(610, 488, "LEVEL"); knob(760, 488, "TIGHT", ("LOW", "HIGH"))
pushbutton(620, 613, "BOOST")
# AMP
scale(1040, 488, 4, -67.5, 67.5, 46, big_at=[0, 3], labels=["CRUNCH", "MODERN", "BRUTAL", "ULTRA"], lab_r=[114, 134, 134, 114], lab_font=17, t0=20, t1=28, t2=34)
name(1040, 330, "VOICING", 26)
for x, nm in ((1230, "GAIN"), (1372, "BASS"), (1514, "MIDDLE"), (1656, "TREBLE"), (1798, "PRESENCE"), (1940, "RESONANCE"), (2082, "SAG"), (2224, "MASTER")):
    knob(x, 488, nm, size=21, dx=36)
pushbutton(1230, 613, "BRIGHT"); pushbutton(1372, 613, "DEEP")
# CABINET
text(104, 761, "IR A", font(24, 2), INK, 'lm', spacing=2); text(474, 761, "IR B", font(24, 2), INK, 'lm', spacing=2)
for x in (104, 474):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([x * S, 783 * S, (x + 330) * S, 833 * S], radius=14 * S, fill=(8, 10, 12, 255), outline=(255, 255, 255, 70), width=2 * S)
    comp(layer)
knob(194, 958, "LEVEL A"); knob(564, 958, "LEVEL B")
knob(874, 958, "LOW CUT", ("LOW", "HIGH")); knob(1074, 958, "HIGH CUT", ("DARK", "BRIGHT"))
pushbutton(1074, 808, "CAB")
# EQ
knob(1366, 958, "LOW", ("-", "+"), dx=36); knob(1506, 958, "MID", ("-", "+"), dx=36); knob(1646, 958, "FREQ", ("LO", "HI"), dx=36); knob(1786, 958, "HIGH", ("-", "+"), dx=36)
# OUTPUT
knob(2068, 958, "INPUT"); knob(2228, 958, "OUTPUT")
pushbutton(2068, 808, "LIMIT")
text(2228, 780, "LIMITING", font(20, 2), INK, 'mm'); lamp_socket(2228, 812, 12)

# ----- cut the tab image out of the tall content layer and put it on the body -----
# header (everything above the rule) + one row of panels, moved up/down so it sits under the tab bar; PluginEditor.cpp uses the same offsets
final = BODY_IMG.copy()
def band(y0, y1, dy):
    final.alpha_composite(img.crop((0, y0 * S, W * S, y1 * S)), dest=(0, (y0 + dy) * S))
band(0, 236, 0)
if TAB == 0:
    band(236, 676, 60)          # GATE / TIGHT BOOST / AMP: panel top 268 -> 328
else:
    band(686, 1126, -390)       # CABINET / EQ / OUTPUT: panel top 718 -> 328
img, H = final, H_FINAL

# ----- tab buttons: below the rule, centred on the window (PluginEditor.cpp: kTabBtn); the editor lights the selected one -----
for lab, tx in (("AMP", 1095), ("CABINET", 1305)):
    ty = 250
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(tx - 95) * S, (ty - 22) * S, (tx + 95) * S, (ty + 22) * S], radius=13 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(tx - 97) * S, (ty - 24) * S, (tx + 97) * S, (ty + 24) * S], radius=15 * S, outline=(255, 255, 255, 60), width=2 * S)
    comp(layer)
    text(tx, ty + 1, lab, font(24, 0), INK, spacing=3)
line([(40, 289), (2360, 289)], DARK, 2, 150)          # second rule: under the tab buttons

import os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'common'))
from vibr_credit import credit
credit(text, new, comp, S, 2360, 788, font(28, 2), DARK, 28)

out = img.convert('RGB').resize((W, H), Image.LANCZOS)
arr = np.asarray(out, np.float32) + rng.normal(0, 1.3, (H, W, 1)).astype(np.float32)
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(sys.argv[2] if len(sys.argv) > 2 else 'background.png')
print('ok')
