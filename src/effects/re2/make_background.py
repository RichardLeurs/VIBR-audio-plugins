#!/usr/bin/env python3
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
"""Renders the RE-2 front panel (2610 x 1138) used as the plugin background: turquoise satin body, black panels,
printed legends. Control positions match PluginEditor.cpp (see ../style.md). Needs numpy + Pillow.
Usage: make_background.py out.png"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H, S = 2610, 1138, 2
rng = np.random.default_rng(202)
BODY = (49, 85, 181)                          # royal blue #3155B5 (palette)
ACC = BODY                                    # panel titles use the plugin's body colour
INK = (238, 238, 232)
DARK = (244, 246, 250)                            # print on the body
PANEL = (24, 25, 28)
CHROME = [(0, (250, 250, 252)), (0.35, (190, 194, 202)), (0.7, (110, 114, 124)), (1, (60, 62, 70))]

# ----- layout (shared with PluginEditor.cpp): side margins 40, panel gaps 20, see ../style.md -----
PT, PB = 268, 1000
P1, P2, P3 = (40, PT, 900, PB), (920, PT, 1560, PB), (1580, PT, 2570, PB)
ROW1, ROW2, KR = 458, 790, 72                 # knob rows, knob radius
LEFT = [(250, ROW2, "SATURATION"), (690, ROW2, "WOW &|FLUTTER")]
RIGHT = [(1775, ROW1, "BASS"), (2075, ROW1, "TREBLE"), (2375, ROW1, "REVERB|VOLUME"),
         (1775, ROW2, "REPEAT RATE"), (2075, ROW2, "INTENSITY"), (2375, ROW2, "ECHO|VOLUME")]
MODE = (1240, 700)                            # 12-position selector, 1 at -150 ... 12 at 180 (straight down)
MODE_R = 100
# LED strips on the left panel: row A (memory, decorative) and row B (input + tape, live)
STRIP_A, STRIP_B = (70, 300, 870, 468), (70, 486, 870, 654)      # the MEMORY row and the INPUT / TAPE row (dividers are drawn under them)
A_CAP, A_LED = 330, 386                      # lamps / buttons sit 56 px under their captions
B_CAP, B_LED = 516, 574
MEM_X = [150, 260, 370, 480, 590]; MEM_BTN = 780      # same x as the TAPE button below
GUITAR, LINE, IN_BTN = (MEM_X[0], B_LED), (MEM_X[1], B_LED), (390, B_LED)      # GUITAR / LINE lamps in line with MANUAL / 1 above
NEW, AGED, TAPE_BTN = (550, B_LED), (660, B_LED), (780, B_LED)
HEAD_X = [1240 - 165, 1240 - 55, 1240 + 55, 1240 + 165]; HEAD_LED = 400
POWER = (P3[2] - 35, 87)                      # power slide switch (top right)

FONT = '/System/Library/Fonts/Avenir Next.ttc'
COND = '/System/Library/Fonts/Avenir Next Condensed.ttc'
def font(size, idx=0, path=FONT): return ImageFont.truetype(path, int(size * S), index=idx)

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
AMBER = (196, 238, 74)                       # lime accent on the black panels
DARK = (244, 246, 250)                            # print on the body
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
edge = 0.55 * np.exp(-((d - 5) / 3.2) ** 2) * dirn          # bright rim only: no dark falloff or dark outline round the border
lum = lum + edge
lumn = lum / np.median(lum)                                                    # 1.0 = the swatch colour exactly
sw = np.array(BODY, np.float32)[None, None, :]
rgb = np.where(lumn[:, :, None] <= 1.0, sw * lumn[:, :, None], sw + (255 - sw) * (1.0 - 1.0 / np.maximum(lumn, 1.0))[:, :, None])   # highlights fade toward white, hue kept
rgb = np.clip(rgb, 0, 255)
img = Image.fromarray(rgb.astype(np.uint8), 'RGB').convert('RGBA')

def comp(layer):
    global img
    img = Image.alpha_composite(img, layer)

def new(): return Image.new('RGBA', (W * S, H * S), (0, 0, 0, 0))

def shadow(cx, cy, r, dx=5, dy=9, blur=9, alpha=150):
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).ellipse([(cx + dx - r) * S, (cy + dy - r * 0.95) * S, (cx + dx + r) * S, (cy + dy + r) * S], fill=alpha)
    m = m.filter(ImageFilter.GaussianBlur(blur * S))
    layer = new(); layer.putalpha(m); comp(layer)

def disc(cx, cy, r, stops, hl=(-0.35, -0.45), spread=1.6):
    x0, y0, x1, y1 = int((cx - r - 2) * S), int((cy - r - 2) * S), int((cx + r + 2) * S), int((cy + r + 2) * S)
    gy, gx = np.mgrid[y0:y1, x0:x1].astype(np.float32)
    t = np.hypot(gx - (cx + hl[0] * r) * S, gy - (cy + hl[1] * r) * S) / (spread * r * S)
    ts = [s[0] for s in stops]
    out = np.stack([np.interp(t, ts, [s[1][k] for s in stops]) for k in range(3)], -1)
    a = np.clip(r * S - np.hypot(gx - cx * S, gy - cy * S) + 0.5, 0, 1)
    layer = new(); layer.paste(Image.fromarray(np.dstack([out, a * 255]).astype(np.uint8), 'RGBA'), (x0, y0)); comp(layer)

def rrect_grad(x0, y0, x1, y1, r, top, bot):
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).rounded_rectangle([x0 * S, y0 * S, x1 * S, y1 * S], radius=r * S, fill=255)
    t = np.clip((yy - y0 * S) / ((y1 - y0) * S), 0, 1)[:, :, None]
    col = np.array(top, np.float32) * (1 - t) + np.array(bot, np.float32) * t
    layer = Image.fromarray(np.dstack([col, np.full(t.shape[:2], 255, np.float32)]).astype(np.uint8), 'RGBA')
    layer.putalpha(m); comp(layer)





# ----- title -----
text(P1[0] + 6, 110, "RE-2", font(140, 9), DARK, 'lm')
text(P3[2] - 190, 86, "TAPE ECHO  +  SATURATION", font(40, 5), DARK, 'rm', spacing=6)
text(P3[2] - 190, 134, "ANALOG-STYLE SPACE ECHO", font(34, 5), DARK, 'rm', spacing=6)
line([(P1[0], 208), (P3[2], 208)], DARK, 2, 150)

def panel(box, title, pw):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([v * S for v in box], radius=34 * S, fill=PANEL + (255,))
    comp(layer)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([v * S for v in box], radius=34 * S, outline=INK + (150,), width=2 * S)
    comp(layer)
    cx = (box[0] + box[2]) / 2
    f = font(36, 0)
    pw = sum(ImageDraw.Draw(Image.new('L', (8, 8))).textlength(ch, font=f) + 3 * S for ch in title) / S - 3 + 76   # fit the text + padding
    pill = new(); pd = ImageDraw.Draw(pill)
    pd.rounded_rectangle([(cx - pw / 2) * S, (box[1] - 22) * S, (cx + pw / 2) * S, (box[1] + 22) * S], radius=22 * S, fill=PANEL + (255,))
    comp(pill)
    text(cx, box[1] + 1, title, font(36, 0), ACC, spacing=3)




panel(P1, "INPUT", 150)
panel(P2, "MODE", 150)
panel(P3, "ECHO", 150)
line([(P3[0] + 34, (ROW1 + ROW2) / 2 + 28), (P3[2] - 34, (ROW1 + ROW2) / 2 + 28)], INK, 1.5, 70)       # divider between the two knob rows

def led_bezel(lx, ly, lit=None):
    shadow(lx, ly, 20, 3, 6, 5, 160)
    disc(lx, ly, 21, CHROME, (-0.4, -0.5), 1.6)
    disc(lx, ly, 14, [(0, (20, 20, 22)), (1, (4, 4, 5))], (0, 0), 1.2)
    if lit:
        glow = new(); gd = ImageDraw.Draw(glow)
        gd.ellipse([(lx - 36) * S, (ly - 36) * S, (lx + 36) * S, (ly + 36) * S], fill=lit + (70,))
        comp(glow.filter(ImageFilter.GaussianBlur(10 * S)))
        disc(lx, ly, 11, [(0, (210, 255, 220)), (0.5, lit), (1, tuple(int(c * 0.5) for c in lit))], (-0.3, -0.4), 1.4)
    else:
        disc(lx, ly, 11, [(0, (70, 74, 76)), (1, (22, 24, 25))], (-0.3, -0.4), 1.4)

def pushbutton(bx, by, w=112, h=58):
    m = Image.new('L', (W * S, H * S), 0)
    ImageDraw.Draw(m).rounded_rectangle([(bx - w / 2 - 6) * S, (by - h / 2 - 2) * S, (bx + w / 2 + 10) * S, (by + h / 2 + 12) * S], radius=14 * S, fill=170)
    m = m.filter(ImageFilter.GaussianBlur(8 * S)); lay = new(); lay.putalpha(m); comp(lay)
    rrect_grad(bx - w / 2 - 6, by - h / 2 - 6, bx + w / 2 + 6, by + h / 2 + 6, 16, (236, 238, 244), (92, 96, 106))
    rrect_grad(bx - w / 2, by - h / 2, bx + w / 2, by + h / 2, 11, (30, 31, 34), (8, 8, 10))


# ----- power: horizontal slide switch (top right), ball left = OFF, right = ON -----
def hswitch(rx, ry, left, right):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(rx - 33) * S, (ry - 17) * S, (rx + 33) * S, (ry + 17) * S], radius=16 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(rx - 35) * S, (ry - 19) * S, (rx + 35) * S, (ry + 19) * S], radius=18 * S, outline=(255, 255, 255, 60), width=2 * S)
    comp(layer)
    text(rx + 35, ry + 50, left + " / " + right, font(26, 2), DARK, 'rm', spacing=2)   # right-aligned with the panels and the rule


hswitch(POWER[0], POWER[1], "OFF", "ON")

# ----- LED strips (left panel): raised dark-grey panels (lighter than the black panel). Row A: MANUAL / 1-4 + MEMORY (decorative); row B: GUITAR / LINE + INPUT | NEW / AGED + TAPE -----
SINK = INK                                                   # white print on the black panel
# the rows sit directly on the black panel; a thin divider line under each row separates them (same line style as the other panels)
for y in (STRIP_A[3] + 9, STRIP_B[3] + 2):
    line([(P1[0] + 34, y), (P1[2] - 34, y)], INK, 1.5, 90)

for i, (x, lab) in enumerate(zip(MEM_X, ["MANUAL", "1", "2", "3", "4"])):
    text(x, A_CAP, lab, font(30, 2), SINK, spacing=1)
    led_bezel(x, A_LED, None)                                  # the editor lights the memory LEDs
text(MEM_BTN, A_CAP, "MEMORY", font(30, 2), SINK, spacing=1)
pushbutton(MEM_BTN, A_LED)
text(MEM_BTN, A_LED + 56, "(WRITE: HOLD)", font(22, 2), SINK)

text(GUITAR[0], B_CAP, "GUITAR", font(30, 2), SINK, spacing=1); led_bezel(*GUITAR)
text(LINE[0], B_CAP, "LINE", font(30, 2), SINK, spacing=1); led_bezel(*LINE)
text(IN_BTN[0], B_CAP, "INPUT", font(30, 2), SINK, spacing=1); pushbutton(*IN_BTN)
line([(473, B_LED - 40), (473, B_LED + 40)], SINK, 2, 170)
text(NEW[0], B_CAP, "NEW", font(30, 2), SINK, spacing=1); led_bezel(*NEW)
text(AGED[0], B_CAP, "AGED", font(30, 2), SINK, spacing=1); led_bezel(*AGED)
text(TAPE_BTN[0], B_CAP, "TAPE", font(30, 2), SINK, spacing=1); pushbutton(*TAPE_BTN)

# ----- head LEDs (middle panel) -----
text(1240, 342, "HEAD", font(28, 2), INK, spacing=3)
line([(HEAD_X[0] - 30, 342), (1240 - 56, 342)], INK, 1.5, 120)
line([(1240 + 56, 342), (HEAD_X[3] + 30, 342)], INK, 1.5, 120)
for x, n in zip(HEAD_X, "1234"):
    text(x, 372, n, font(30, 2), INK)
    led_bezel(x, HEAD_LED + 22)

# ----- knobs: recess, scale, legends -----
def scale(x, y, r=KR, ticks=11, a0=-135, a1=135, labels=True):
    shadow(x, y, r + 14, 6, 11, 10, 170)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(x - r - 14) * S, (y - r - 14) * S, (x + r + 14) * S, (y + r + 14) * S], outline=(0, 0, 0, 120), width=3 * S)
    dr.ellipse([(x - r - 17) * S, (y - r - 17) * S, (x + r + 17) * S, (y + r + 17) * S], outline=(255, 255, 255, 40), width=2 * S)
    comp(layer)
    for k in range(ticks):
        a = math.radians(a0 + (a1 - a0) * k / (ticks - 1))
        big = ticks == 12 or k in (0, (ticks - 1) // 2, ticks - 1)
        r0, r1 = r + 24, r + (44 if big else 38)
        line([(x + r0 * math.sin(a), y - r0 * math.cos(a)), (x + r1 * math.sin(a), y - r1 * math.cos(a))], INK, 3 if big else 2, 230)
    if labels:
        for k, lab in ((0, "0"), (ticks - 1, "10")):
            a = math.radians(a0 + (a1 - a0) * k / (ticks - 1))
            text(x + (r + 66) * math.sin(a), y - (r + 66) * math.cos(a), lab, font(28, 2), INK)

def name_under(x, y, name):
    parts = name.split('|')
    for i, part in enumerate(parts):
        text(x, y + KR + 82 + 36 * i - 18 * (len(parts) - 1), part, font(36, 0), INK, spacing=2)

for x, y, name in LEFT + RIGHT:
    scale(x, y)
    name_under(x, y, name)

# ----- mode selector: 1-12, 30 degrees apart; 1 at -150, 11 at +150, 12 straight down -----
mx, my = MODE
scale(mx, my, MODE_R, ticks=12, a0=-150, a1=180, labels=False)
for k in range(12):
    a = math.radians(-150 + 30 * k)
    rr = MODE_R + 82
    text(mx + rr * math.sin(a), my - rr * math.cos(a), str(k + 1), font(40, 0), INK)

# the printed arc round positions 8-12 (as on the original unit), with a short tick at each end
ARC_INK = (150, 152, 150)                      # gray line (same gray as the RE-1 mode print)
R_ARC = MODE_R + 128
layer = new(); dr = ImageDraw.Draw(layer)
dr.arc([(mx - R_ARC) * S, (my - R_ARC) * S, (mx + R_ARC) * S, (my + R_ARC) * S], 54 - 90, 180 - 90, fill=ARC_INK + (215,), width=int(3 * S))
for deg in (54, 180):
    a = math.radians(deg)
    dr.line([((mx + (R_ARC - 9) * math.sin(a)) * S, (my - (R_ARC - 9) * math.cos(a)) * S), ((mx + (R_ARC + 9) * math.sin(a)) * S, (my - (R_ARC + 9) * math.cos(a)) * S)], fill=ARC_INK + (215,), width=int(3 * S))
comp(layer)

# ----- credit, bottom right (right edge lined up with the panels) -----
text(P3[2], PB + 70, "Made by Richard Leurs", font(28, 2), DARK, 'rm', spacing=2)

out = img.convert('RGB').resize((W, H), Image.LANCZOS)
arr = np.asarray(out, np.float32) + rng.normal(0, 1.3, (H, W, 1)).astype(np.float32)
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(sys.argv[1] if len(sys.argv) > 1 else 'background.png')
print('ok')
