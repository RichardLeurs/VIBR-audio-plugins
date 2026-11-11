#!/usr/bin/env python3
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
"""Renders the DD-1 pedal front panel (1560 x 900) used as the plugin background.
Photo-style finish: satin lime anodised body with black panels, bevelled edges, printed legends.
Needs numpy + Pillow.  Usage: make_background.py out.png"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H, S = 2690, 740, 2                      # image size and supersampling
rng = np.random.default_rng(7)

# ----- layout (shared with PluginEditor.cpp) -----
CX = [800, 1560, 2430]       # delay A, delay B, shimmer (the stereo panel sits between A and B)
ROW1, ROW2 = 365, 585
BIG, SMALL = 44, 34
PANEL_T, PANEL_B, PANEL_HW = 192, 680, 230
GH = 7.5                                    # panel gaps are 15 px (= 10 px on screen, as on the DM-1 and CE-1): panels grow 7.5 px on their inner sides
PANEL_L, PANEL_R = 30, 2660                 # left edge of the TEMPO panel, right edge of the SHIMMER panel
FOOT = (780, 805)
LED = (PANEL_R - 27, 67)
KNOBS = []   # (cx, cy, r, name, kind, labels)
DIV = ["FREE", "1/1", "1/2", "1/4.", "1/4", "1/4T", "1/8.", "1/8", "1/8T", "1/16"]
DIV_B = ["2/1", "3/2", "1/1", "3/4", "2/3", "1/2", "3/8", "1/3", "1/4", "1/8"]      # ratios of delay A's time
INT = ["-OCT", "+5TH", "+OCT", "+OCT+5", "+2OCT"]
for i in (0, 1):
    c = CX[i]
    if i == 0:      # delay A: division + time
        KNOBS += [(c - 110, ROW1, BIG, "DIVISION", "sel", DIV),
                  (c + 70, ROW1, SMALL, "LOW CUT", "knob", None), (c + 170, ROW1, SMALL, "HI CUT", "knob", None)]
    else:           # delay B: no time knob ("=A" copies delay A's time); the repeat filter (A + B) sits in its place
        KNOBS += [(c - 110, ROW1, BIG, "DIVISION", "sel", DIV_B),
                  (c + 70, ROW1, SMALL, "LOW CUT", "knob", None), (c + 170, ROW1, SMALL, "HI CUT", "knob", None)]
    KNOBS += [(c - 162, ROW2, SMALL, "REPEATS", "knob", None), (c - 54, ROW2, SMALL, "LEVEL", "knob", None),
              (c + 54, ROW2, SMALL, "MOD", "knob", None), (c + 162, ROW2, SMALL, "RATE", "knob", None)]
c = CX[2]
KNOBS += [(c - 110, ROW1, BIG, "INTERVAL", "sel", INT), (c + 110, ROW1, BIG, "DECAY", "knob", None),
          (c - 162, ROW2, SMALL, "SHIMMER", "knob", None), (c - 54, ROW2, SMALL, "TONE", "knob", None),
          (c + 54, ROW2, SMALL, "LEVEL", "knob", None)]
ROUTE = (CX[2] + 162, ROW2)
BOOST_CX = 420
FX_BOX = (1820 - GH, PANEL_T, 2170 + GH, PANEL_B)         # ambience panel (chorus + reverb), between delay B and the shimmer
FX_CX = 1995
FXR = 34
MINI = 34
# knob rows line up with the neighbouring panels (row 1 = ROW1, row 2 = ROW2)
KNOBS += [(FX_CX - 108, ROW1, FXR, "RATE", "knob", None), (FX_CX, ROW1, FXR, "DEPTH", "knob", None), (FX_CX + 108, ROW1, FXR, "LEVEL", "knob", None),
          (FX_CX - 108, ROW2, FXR, "DECAY", "knob", None), (FX_CX, ROW2, FXR, "TONE", "knob", None), (FX_CX + 108, ROW2, FXR, "LEVEL", "knob", None)]
SPREAD = (1180, ROW1)                      # level with the knobs in the neighbouring panels
PING = [(1125, ROW2), (1235, ROW2)]        # side by side, level with the ROUTING switch
BOOST_SW = (BOOST_CX, 335)                 # same height as the SOURCE switch
KNOBS.append((BOOST_CX, 450, SMALL, "DRIVE", "knob", None))      # same height and size as the TIME knob
KNOBS.append((BOOST_CX - 54, 590, MINI, "TONE", "knob", None))
KNOBS.append((BOOST_CX + 54, 590, MINI, "LEVEL", "knob", None))
STEREO_BOX = (1060 - GH, PANEL_T, 1300 + GH, PANEL_B)      # TEMPO, BOOST and STEREO panels share one width (240)
TEMPO_BOX = (30, PANEL_T, 270 + GH, PANEL_B)
BOOST_BOX = (300 - GH, PANEL_T, 540 + GH, PANEL_B)
BPM = (150, 246)
SYNC = (150, 335)
TAPPAD = (150, 595)
KNOBS.append((150, 450, SMALL, "TIME", "knob", None))      # the time of delay A lives in the tempo panel
KNOBS.append((SPREAD[0], SPREAD[1], SMALL, "SPREAD", "knob", None))

FONT = '/System/Library/Fonts/Avenir Next.ttc'
COND = '/System/Library/Fonts/Avenir Next Condensed.ttc'
def font(size, idx=0, path=FONT): return ImageFont.truetype(path, int(size * S), index=idx)
def pill_width(title, size, spacing, pad=38):
    """width of the title pill: the text (with its letter-spacing) plus pad on each side"""
    d = ImageDraw.Draw(Image.new('L', (8, 8)))
    return sum(d.textlength(ch, font=font(size, 0)) + spacing * S for ch in title) / S - spacing + 2 * pad


# ----- base surface: satin powder coat -----
yy, xx = np.mgrid[0:H * S, 0:W * S].astype(np.float32)
xn, yn = xx / (W * S), yy / (H * S)
light = 1.22 - 0.55 * np.hypot(xn - 0.28, (yn - 0.10) * 0.8)                 # soft key light from top left
sheen = 0.10 * np.exp(-((xn * 0.55 + yn * 0.45 - 0.42) / 0.10) ** 2)          # broad satin highlight
blot = Image.fromarray((rng.random((H // 6, W // 6)) * 255).astype(np.uint8)).resize((W * S, H * S), Image.BICUBIC).filter(ImageFilter.GaussianBlur(40 * S))
blot = (np.asarray(blot, np.float32) / 255 - 0.5) * 0.10
grain = rng.normal(0, 0.028, (H * S, W * S)).astype(np.float32)
lum = light + sheen + blot + grain
d = np.minimum(np.minimum(xx, yy), np.minimum(W * S - 1 - xx, H * S - 1 - yy)) / S    # distance to the edge, px
dirn = 0.65 + 0.35 * np.clip((1.0 - xn) * 0.5 + (1.0 - yn) * 0.5 - 0.0, 0, 1) * 1.0   # top-left faces the light
edge = 0.55 * np.exp(-((d - 5) / 3.2) ** 2) * dirn          # bright rim only: no dark falloff or dark outline round the border
lum = lum + edge
BODY = (82, 118, 229)                          # blue #5276E5 (palette)
lumn = lum / np.median(lum)                                                    # 1.0 = the swatch colour exactly
sw = np.array(BODY, np.float32)[None, None, :]
rgb = np.where(lumn[:, :, None] <= 1.0, sw * lumn[:, :, None], sw + (255 - sw) * (1.0 - 1.0 / np.maximum(lumn, 1.0))[:, :, None])   # highlights fade toward white, hue kept
rgb = np.clip(rgb, 0, 255)
img = Image.fromarray(rgb.astype(np.uint8), 'RGB').convert('RGBA')


def comp(layer):
    global img
    img = Image.alpha_composite(img, layer)

def new(): return Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))

def shadow(cx, cy, r, dx=5, dy=9, blur=9, alpha=150, shape='ellipse'):
    m = Image.new('L', (W * S, H * S), 0)
    dr = ImageDraw.Draw(m)
    dr.ellipse([(cx + dx - r) * S, (cy + dy - r * 0.95) * S, (cx + dx + r) * S, (cy + dy + r * 1.0) * S], fill=alpha)
    m = m.filter(ImageFilter.GaussianBlur(blur * S))
    layer = new(); layer.putalpha(m); comp(layer)

def disc(cx, cy, r, stops, hl=(-0.35, -0.45), spread=1.6):
    """radial-gradient disc; stops = [(t, (r,g,b))] with t 0 at the highlight point, 1 at distance spread*r"""
    x0, y0, x1, y1 = int((cx - r - 2) * S), int((cy - r - 2) * S), int((cx + r + 2) * S), int((cy + r + 2) * S)
    gy, gx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    t = np.hypot(gx - (cx + hl[0] * r) * S, gy - (cy + hl[1] * r) * S) / (spread * r * S)
    ts = [s[0] for s in stops]
    out = np.stack([np.interp(t, ts, [s[1][k] for s in stops]) for k in range(3)], -1)
    a = np.clip(r * S - np.hypot(gx - cx * S, gy - cy * S) + 0.5, 0, 1)
    tile = np.dstack([out, a * 255]).astype(np.uint8)
    layer = new(); layer.paste(Image.fromarray(tile, 'RGBA'), (x0, y0)); comp(layer)

CHROME = [(0, (250, 250, 252)), (0.35, (190, 194, 202)), (0.7, (110, 114, 124)), (1, (60, 62, 70))]
INK = (238, 238, 232)
AMBER = BODY                                  # panel titles use the plugin's body colour
DARK = (13, 18, 33)                            # print on the body
PANEL = (27, 28, 31)                         # charcoal

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
    ink = np.asarray(m, np.float32) / 255 * (0.90 + 0.10 * rng.random(m.size[::-1]).astype(np.float32))   # slightly uneven ink
    layer = Image.new('RGBA', (W * S, H * S), fill + (0,))
    layer.putalpha(Image.fromarray((ink * 255).astype(np.uint8)))
    comp(layer)

def line(pts, fill=INK, width=2, alpha=190):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.line([(px * S, py * S) for px, py in pts], fill=fill + (alpha,), width=int(width * S), joint='curve')
    comp(layer)

# ----- printed panels -----
for i, (cx, name, col) in enumerate(zip(CX, ["DELAY  A", "DELAY  B", "SHIMMER"], [INK, INK, AMBER])):
    layer = new(); dr = ImageDraw.Draw(layer)
    bl, br = cx - PANEL_HW - GH, (PANEL_R if i == 2 else cx + PANEL_HW + GH)
    box = [bl * S, PANEL_T * S, br * S, PANEL_B * S]
    cx = (bl + br) / 2                                  # title centred on the panel
    pw = pill_width(name, 28, 2)
    dr.rounded_rectangle(box, radius=26 * S, fill=PANEL + (255,))
    comp(layer)
    # outline with a gap for the header
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle(box, radius=26 * S, outline=col + (150,), width=2 * S)
    gap = new(); gd = ImageDraw.Draw(gap)
    layer_np = np.asarray(layer).copy()
    comp(layer)
    # erase the line behind the header by re-drawing the surface strip: simple approach is a dark pill under the title
    pill = new(); pd = ImageDraw.Draw(pill)
    pd.rounded_rectangle([(cx - pw / 2) * S, (PANEL_T - 17) * S, (cx + pw / 2) * S, (PANEL_T + 17) * S], radius=17 * S, fill=PANEL + (255,))
    comp(pill)
    text(cx, PANEL_T + 1, name, font(28, 0), AMBER, spacing=2)

# ----- stereo panel (spread + ping-pong) -----
layer = new(); dr = ImageDraw.Draw(layer)
box = [v * S for v in STEREO_BOX]
dr.rounded_rectangle(box, radius=26 * S, fill=PANEL + (255,))
comp(layer)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle(box, radius=26 * S, outline=INK + (150,), width=2 * S)
comp(layer)
pill = new(); pd = ImageDraw.Draw(pill)
sx = (STEREO_BOX[0] + STEREO_BOX[2]) // 2
pw = pill_width("STEREO", 28, 2)
pd.rounded_rectangle([(sx - pw / 2) * S, (STEREO_BOX[1] - 17) * S, (sx + pw / 2) * S, (STEREO_BOX[1] + 17) * S], radius=17 * S, fill=PANEL + (255,))
comp(pill)
text(sx, STEREO_BOX[1] + 1, "STEREO", font(28, 0), AMBER, spacing=2)

# ----- repeat filter legend (inside the delay B panel) -----
for cxx in CX[:2]:
    text(cxx + 120, 290, "REPEAT FILTER", font(14, 2), INK, spacing=2)
    line([(cxx + 22, 301), (cxx + 218, 301)], INK, 1.2, 120)

# ----- tempo panel (bpm, source switch, time, tap pad) -----
layer = new(); dr = ImageDraw.Draw(layer)
box = [v * S for v in TEMPO_BOX]
dr.rounded_rectangle(box, radius=26 * S, fill=PANEL + (255,))
comp(layer)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle(box, radius=26 * S, outline=INK + (150,), width=2 * S)
comp(layer)
tx = (TEMPO_BOX[0] + TEMPO_BOX[2]) // 2
pill = new(); pd = ImageDraw.Draw(pill)
pw = pill_width("TEMPO", 28, 2)
pd.rounded_rectangle([(tx - pw / 2) * S, (TEMPO_BOX[1] - 17) * S, (tx + pw / 2) * S, (TEMPO_BOX[1] + 17) * S], radius=17 * S, fill=PANEL + (255,))
comp(pill)
text(tx, TEMPO_BOX[1] + 1, "TEMPO", font(28, 0), AMBER, spacing=2)

# ----- boost panel -----
layer = new(); dr = ImageDraw.Draw(layer)
box = [v * S for v in BOOST_BOX]
dr.rounded_rectangle(box, radius=26 * S, fill=PANEL + (255,))
comp(layer)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle(box, radius=26 * S, outline=INK + (150,), width=2 * S)
comp(layer)
pill = new(); pd = ImageDraw.Draw(pill)
pw = pill_width("BOOST", 28, 2)
pd.rounded_rectangle([(BOOST_CX - pw / 2) * S, (BOOST_BOX[1] - 17) * S, (BOOST_CX + pw / 2) * S, (BOOST_BOX[1] + 17) * S], radius=17 * S, fill=PANEL + (255,))
comp(pill)
text(BOOST_CX, BOOST_BOX[1] + 1, "BOOST", font(28, 0), AMBER, spacing=2)

# ----- chorus + reverb panel -----
layer = new(); dr = ImageDraw.Draw(layer)
box = [v * S for v in FX_BOX]
dr.rounded_rectangle(box, radius=26 * S, fill=PANEL + (255,))
comp(layer)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle(box, radius=26 * S, outline=INK + (150,), width=2 * S)
comp(layer)
pill = new(); pd = ImageDraw.Draw(pill)
pw = pill_width("AMBIENCE", 28, 2)
pd.rounded_rectangle([(FX_CX - pw / 2) * S, (FX_BOX[1] - 17) * S, (FX_CX + pw / 2) * S, (FX_BOX[1] + 17) * S], radius=17 * S, fill=PANEL + (255,))
comp(pill)
text(FX_CX, FX_BOX[1] + 1, "AMBIENCE", font(28, 0), AMBER, spacing=2)
for label, ly in (("CHORUS", 268), ("REVERB", 492)):
    text(FX_CX, ly, label, font(15, 2), INK, spacing=3)
    line([(FX_BOX[0] + 24, ly), (FX_CX - 52, ly)], INK, 1.2, 120)
    line([(FX_CX + 52, ly), (FX_BOX[2] - 24, ly)], INK, 1.2, 120)

# ----- title -----
text(PANEL_L + 6, 84, "DD-1", font(108, 9), DARK, 'lm')
text(PANEL_R - 146, 66, "DUAL DELAY  +  SHIMMER REVERB", font(31, 5), DARK, 'rm', spacing=5)       # description: two lines on the right, left of the power switch (as on the DM-1 and CE-1)
text(PANEL_R - 146, 103, "ANALOG-STYLE DELAY MACHINE", font(26, 5), DARK, 'rm', spacing=5)
line([(PANEL_L, 160), (PANEL_R, 160)], DARK, 1.5, 150)

# ----- time displays (ms), one per delay -----
DISP_Y, DISP_W, DISP_H = 246, 176, 44
def lcd(cx, cy, w=DISP_W, h=DISP_H):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(cx - w / 2 - 6) * S, (cy - h / 2 - 6) * S, (cx + w / 2 + 6) * S, (cy + h / 2 + 6) * S], radius=12 * S, fill=(255, 255, 255, 22))
    dr.rounded_rectangle([(cx - w / 2 - 3) * S, (cy - h / 2 - 3) * S, (cx + w / 2 + 3) * S, (cy + h / 2 + 3) * S], radius=10 * S, fill=(0, 0, 0, 200))
    dr.rounded_rectangle([(cx - w / 2) * S, (cy - h / 2) * S, (cx + w / 2) * S, (cy + h / 2) * S], radius=8 * S, fill=(12, 6, 3, 255))
    comp(layer)
for cx in CX[:2]:
    lcd(cx, DISP_Y)

# ----- knobs: shadow, recess ring, scale, legends -----
for (x, y, r, name, kind, labels) in KNOBS:
    shadow(x, y, r + 2, 6, 11, 9, 170)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(x - r - 5) * S, (y - r - 5) * S, (x + r + 5) * S, (y + r + 5) * S], outline=(0, 0, 0, 110), width=3 * S)
    dr.ellipse([(x - r - 7) * S, (y - r - 7) * S, (x + r + 7) * S, (y + r + 7) * S], outline=(255, 255, 255, 40), width=2 * S)
    comp(layer)
    if kind == 'knob':
        for k in range(11):
            a = math.radians(-135 + 27 * k)
            r0, r1 = r + 9, r + (17 if k in (0, 5, 10) else 14)
            line([(x + r0 * math.sin(a), y - r0 * math.cos(a)), (x + r1 * math.sin(a), y - r1 * math.cos(a))], INK, 2 if k in (0, 5, 10) else 1.4, 230)
        for k, lab in ((0, "0"), (10, "10")):
            a = math.radians(-135 + 27 * k)
            text(x + (r + 27) * math.sin(a), y - (r + 27) * math.cos(a), lab, font(14, 2), INK)
    else:
        n = len(labels)
        step = 270.0 / (n - 1)
        for k, lab in enumerate(labels):
            a = math.radians(-135 + step * k)
            s_, c_ = math.sin(a), math.cos(a)
            line([(x + (r + 8) * s_, y - (r + 8) * c_), (x + (r + 15) * s_, y - (r + 15) * c_)], INK, 2, 230)
            R = r + 21
            if s_ > 0.1:   text(x + R * s_, y - R * c_, lab, font(15, 2), INK, 'lm')
            elif s_ < -0.1: text(x + R * s_, y - R * c_, lab, font(15, 2), INK, 'rm')
            else:          text(x + R * s_, y - R * c_ - 4, lab, font(15, 2), INK, 'mm')
    text(x, y + r + 40, name, font(17, 0), INK, spacing=2)

# ----- lever switches (routing, ping-pong) -----
def lever(rx, ry, name, top, bottom, name_y, side=False):
    shadow(rx, ry, 24, 4, 8, 7, 150)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(rx - 17) * S, (ry - 33) * S, (rx + 17) * S, (ry + 33) * S], radius=16 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(rx - 19) * S, (ry - 35) * S, (rx + 19) * S, (ry + 35) * S], radius=18 * S, outline=(255, 255, 255, 60), width=2 * S)
    comp(layer)
    text(rx, name_y, name, font(17, 0), INK, spacing=2)
    if side:
        text(rx + 28, ry - 17, top, font(14, 2), INK, 'lm')
        text(rx + 28, ry + 17, bottom, font(14, 2), INK, 'lm')
    else:
        text(rx, ry - 51, top, font(14, 2), INK)
        text(rx, ry + 54, bottom, font(14, 2), INK)

lever(ROUTE[0], ROUTE[1], "ROUTING", "SERIES", "PARALLEL", ROUTE[1] - 72)
for (px, py), n in zip(PING, ("PING A", "PING B")):
    lever(px, py, n, "ON", "OFF", py - 72)

# ----- chrome footswitches + LED bezels -----
def footswitch(fx, fy, k=1.0):
    shadow(fx, fy, 66 * k, 7 * k, 14 * k, 12 * k, 190)
    disc(fx, fy, 66 * k, [(0, (225, 228, 235)), (0.5, (150, 154, 165)), (1, (52, 54, 62))], (-0.3, -0.4), 1.5)           # washer
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(fx - 55 * k) * S, (fy - 55 * k) * S, (fx + 55 * k) * S, (fy + 55 * k) * S], outline=(30, 32, 38, 230), width=int(3 * S))
    comp(layer)
    disc(fx, fy, 49 * k, CHROME, (-0.4, -0.5), 1.7)                                                                      # cap
    disc(fx, fy, 38 * k, [(0, (255, 255, 255)), (0.5, (205, 209, 217)), (1, (120, 124, 134))], (-0.35, -0.45), 1.4)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(fx - 49 * k) * S, (fy - 49 * k) * S, (fx + 49 * k) * S, (fy + 49 * k) * S], outline=(255, 255, 255, 110), width=int(2 * S))
    comp(layer)

def led_bezel(lx, ly):
    shadow(lx, ly, 20, 3, 6, 5, 160)
    disc(lx, ly, 21, CHROME, (-0.4, -0.5), 1.6)
    disc(lx, ly, 14, [(0, (20, 20, 22)), (1, (4, 4, 5))], (0, 0), 1.2)


# ----- power: horizontal slide switch (top right), ball left = OFF, right = ON -----
def power_switch(rx, ry):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(rx - 25.4) * S, (ry - 13.1) * S, (rx + 25.4) * S, (ry + 13.1) * S], radius=12.3 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(rx - 26.9) * S, (ry - 14.6) * S, (rx + 26.9) * S, (ry + 14.6) * S], radius=13.9 * S, outline=(255, 255, 255, 60), width=int(1.5 * S))
    comp(layer)
    text(rx + 27, ry + 38, "OFF / ON", font(20, 2), DARK, 'rm', spacing=2)   # right-aligned with the panels and the rule
power_switch(LED[0], LED[1])
# tempo panel contents
lcd(BPM[0], BPM[1], 168, 44)
def hswitch(rx, ry, name, left, right):
    """horizontal slide switch: ball left or right"""
    shadow(rx, ry, 30, 4, 8, 7, 150)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(rx - 33) * S, (ry - 17) * S, (rx + 33) * S, (ry + 17) * S], radius=16 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(rx - 35) * S, (ry - 19) * S, (rx + 35) * S, (ry + 19) * S], radius=18 * S, outline=(255, 255, 255, 60), width=2 * S)
    comp(layer)
    text(rx, ry - 40, name, font(17, 0), INK, spacing=2)
    text(rx - 31, ry + 36, left, font(14, 2), INK)
    text(rx + 31, ry + 36, right, font(14, 2), INK)

hswitch(SYNC[0], SYNC[1], "SOURCE", "HOST", "TAP")           # ball left = HOST, right = TAP
hswitch(BOOST_SW[0], BOOST_SW[1], "ENGAGE", "OFF", "ON")     # ball left = off, right = on
# tap pad: small rectangular chrome bezel round a black well; the editor draws the pad that lights up
def rrect_grad(x0, y0, x1, y1, r, top, bot):
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=255)
    t = np.clip((yy - y0 * S) / ((y1 - y0) * S), 0, 1)[:, :, None]
    col = np.array(top, np.float32) * (1 - t) + np.array(bot, np.float32) * t
    layer = Image.fromarray(np.dstack([col, np.full(t.shape[:2], 255, np.float32)]).astype(np.uint8), 'RGBA')
    layer.putalpha(m)
    comp(layer)

px, py = TAPPAD
bw, bh = 112, 72
m = Image.new('L', (W * S, H * S), 0)
ImageDraw.Draw(m).rounded_rectangle([(px - bw / 2 + 4) * S, (py - bh / 2 + 9) * S, (px + bw / 2 + 4) * S, (py + bh / 2 + 9) * S], radius=15 * S, fill=170)
m = m.filter(ImageFilter.GaussianBlur(8 * S)); lay = new(); lay.putalpha(m); comp(lay)                     # shadow
rrect_grad(px - bw / 2, py - bh / 2, px + bw / 2, py + bh / 2, 15, (236, 238, 244), (92, 96, 106))          # chrome bezel
rrect_grad(px - bw / 2 + 6, py - bh / 2 + 6, px + bw / 2 - 6, py + bh / 2 - 6, 10, (8, 8, 10), (3, 3, 4))   # well
text(px, py + 62, "TAP", font(22, 0), INK, spacing=4)

# ----- presets panel (top, centred): replaced by the MEMORY panel below; kept here, commented out -----
# PRE_BOX = (900, 30, 1790, 134)
# PRE_CX, PRE_Y = (PRE_BOX[0] + PRE_BOX[2]) // 2, (PRE_BOX[1] + PRE_BOX[3]) // 2
# layer = new(); dr = ImageDraw.Draw(layer)
# dr.rounded_rectangle([v * S for v in PRE_BOX], radius=22 * S, fill=PANEL + (255,))
# comp(layer)
# layer = new(); dr = ImageDraw.Draw(layer)
# dr.rounded_rectangle([v * S for v in PRE_BOX], radius=22 * S, outline=INK + (150,), width=2 * S)
# comp(layer)
# text(PRE_BOX[0] + 84, PRE_Y, "PRESETS", font(22, 0), AMBER, spacing=3)
# lcd(PRE_CX + 10, PRE_Y, 400, 58)
# for bx, left in ((PRE_CX - 250, True), (PRE_CX + 270, False)):
#     rrect_grad(bx - 36, PRE_Y - 28, bx + 36, PRE_Y + 28, 12, (236, 238, 244), (92, 96, 106))
#     rrect_grad(bx - 31, PRE_Y - 23, bx + 31, PRE_Y + 23, 9, (170, 174, 184), (120, 124, 134))
#     sgn = -1 if left else 1
#     layer = new(); dr = ImageDraw.Draw(layer)
#     dr.polygon([((bx + sgn * 9) * S, PRE_Y * S), ((bx - sgn * 7) * S, (PRE_Y - 12) * S), ((bx - sgn * 7) * S, (PRE_Y + 12) * S)], fill=(25, 27, 32, 235))
#     comp(layer)
#

# ----- memory panel (top, centred, same box as the old presets panel): MANUAL / 1-4 lamps + MEMORY button (as on the RE-2) -----
MEM_BOX = (900, 30, 1790, 134)
MEM_Y = 70                                             # lamp / button row; captions sit below
MEM_LED_X = [1150, 1255, 1360, 1465, 1570]             # PluginEditor.cpp: kMemLedX
MEM_BTN = (1705, MEM_Y)                                # PluginEditor.cpp: kMemoryButton
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle([v * S for v in MEM_BOX], radius=22 * S, fill=PANEL + (255,))
comp(layer)
layer = new(); dr = ImageDraw.Draw(layer)
dr.rounded_rectangle([v * S for v in MEM_BOX], radius=22 * S, outline=INK + (150,), width=2 * S)
comp(layer)
text(MEM_BOX[0] + 100, (MEM_BOX[1] + MEM_BOX[3]) // 2, "MEMORY", font(22, 0), AMBER, spacing=3)
for x, lab in zip(MEM_LED_X, ["MANUAL", "1", "2", "3", "4"]):
    led_bezel(x, MEM_Y)                                # the editor lights the lamps
    text(x, MEM_Y + 40, lab, font(17, 2), INK, spacing=1)
layer = new()
m = Image.new('L', (W * S, H * S), 0)
ImageDraw.Draw(m).rounded_rectangle([(MEM_BTN[0] - 56 - 4) * S, (MEM_BTN[1] - 20) * S, (MEM_BTN[0] + 56 + 8) * S, (MEM_BTN[1] + 34) * S], radius=12 * S, fill=170)
m = m.filter(ImageFilter.GaussianBlur(6 * S)); lay = new(); lay.putalpha(m); comp(lay)
rrect_grad(MEM_BTN[0] - 48, MEM_BTN[1] - 24, MEM_BTN[0] + 48, MEM_BTN[1] + 24, 12, (236, 238, 244), (92, 96, 106))
rrect_grad(MEM_BTN[0] - 43, MEM_BTN[1] - 19, MEM_BTN[0] + 43, MEM_BTN[1] + 19, 8, (30, 31, 34), (8, 8, 10))
text(MEM_BTN[0], MEM_Y + 40, "HOLD = WRITE", font(15, 2), INK, spacing=1)

# ----- standard margins (see ../style.md): more room under the rule and under the panels -----
# body texture rows are mirror-tiled and inserted in two places that only contain plain body; everything below the
# rule moves down by GAP_TOP (PluginEditor.cpp adds the same offset).
GAP_TOP, GAP_BOT, CUT_TOP, CUT_BOT = 14, 46, 168, 694
def tiled(arr, y0, y1, n):
    src = arr[y0 * S:y1 * S]; r = len(src)
    idx = np.arange(n * S) % (2 * r)
    return src[np.where(idx >= r, 2 * r - 1 - idx, idx)]
arr = np.asarray(img)
arr = np.concatenate([arr[:CUT_TOP * S], tiled(arr, 164, 173, GAP_TOP), arr[CUT_TOP * S:(CUT_BOT) * S],
                      tiled(arr, 688, 698, GAP_BOT), arr[CUT_BOT * S:]], axis=0)
H += GAP_TOP + GAP_BOT
img = Image.fromarray(arr, 'RGBA')

# ----- credit, bottom right (right edge lined up with the panels) -----
text(PANEL_R, PANEL_B + GAP_TOP + 54, "Made by Richard Leurs", font(21, 2), DARK, 'rm', spacing=2)

out = img.convert('RGB').resize((W, H), Image.LANCZOS)
arr = np.asarray(out, np.float32) + rng.normal(0, 1.3, (H, W, 1)).astype(np.float32)          # fine photo grain
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(sys.argv[1] if len(sys.argv) > 1 else 'background.png')
print('ok')
