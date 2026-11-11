#!/usr/bin/env python3
# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
"""Renders the CS-3 flanger front panel (1840 x 890) used as the plugin background: magenta satin body, three black panels
(FLANGE, SWEEP, MODE), printed legends. Control positions match PluginEditor.cpp. Needs numpy + Pillow.
Usage: make_background.py out.png"""
import math, sys
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

W, H, S = 1840, 890, 2
rng = np.random.default_rng(15)
BODY = (216, 190, 53)                          # yellow #D8BE35 (palette: Sustainer)
ACC = BODY                                    # panel titles use the plugin's body colour
INK = (238, 238, 232)
DARK = (33, 29, 8)                            # print on the body
PANEL = (24, 25, 28)
CHROME = [(0, (250, 250, 252)), (0.35, (190, 194, 202)), (0.7, (110, 114, 124)), (1, (60, 62, 70))]

# ----- layout (shared with PluginEditor.cpp); same margins and gaps as the DM-1 (see ../style.md) -----
PT, PB = 268, 752
P1, P2 = (40, PT, 910, PB), (930, PT, 1800, PB)      # side margins 40, panel gap 20
KY, KR = 532, 100                             # knob row and knob radius
NAME_Y = 348                                  # knob name above the knob
KNOBS = [(305, "LEVEL", "MIN", "MAX"), (645, "TONE", "LO", "HI"), (1195, "ATTACK", "MIN", "MAX"), (1535, "SUSTAIN", "MIN", "MAX")]
CHECK = (560, 87)
LED = (P2[2] - 35, 87)                        # power slide switch (top right)

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
AMBER = (240, 214, 90)                       # lime accent on the black panels
DARK = (33, 29, 8)                            # print on the body
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
text(P1[0] + 6, 110, "CS-3", font(140, 9), DARK, 'lm')
text(P2[2] - 190, 86, "COMPRESSION SUSTAINER", font(40, 5), DARK, 'rm', spacing=6)
text(P2[2] - 190, 134, "LEVEL  \u00b7  TONE  \u00b7  ATTACK  \u00b7  SUSTAIN", font(34, 5), DARK, 'rm', spacing=6)
line([(P1[0], 208), (P2[2], 208)], DARK, 2, 150)

# ----- black panels with a titled outline -----
def panel(box, title):
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

panel(P1, "OUTPUT")
panel(P2, "COMPRESSION")

# ----- power: horizontal slide switch (top right), ball left = OFF, right = ON -----
def power_switch(rx, ry):
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.rounded_rectangle([(rx - 33) * S, (ry - 17) * S, (rx + 33) * S, (ry + 17) * S], radius=16 * S, fill=(6, 10, 18, 235))
    dr.rounded_rectangle([(rx - 35) * S, (ry - 19) * S, (rx + 35) * S, (ry + 19) * S], radius=18 * S, outline=(255, 255, 255, 60), width=2 * S)
    comp(layer)
    text(rx + 35, ry + 50, "OFF / ON", font(26, 2), DARK, 'rm', spacing=2)   # right-aligned with the panels and the rule
power_switch(LED[0], LED[1])

# ----- knobs: recess, scale, legends -----
def scale(x, y, ticks, a0, a1, r=KR):
    shadow(x, y, r + 14, 6, 11, 10, 170)
    layer = new(); dr = ImageDraw.Draw(layer)
    dr.ellipse([(x - r - 14) * S, (y - r - 14) * S, (x + r + 14) * S, (y + r + 14) * S], outline=(0, 0, 0, 120), width=3 * S)
    dr.ellipse([(x - r - 17) * S, (y - r - 17) * S, (x + r + 17) * S, (y + r + 17) * S], outline=(255, 255, 255, 40), width=2 * S)
    comp(layer)
    for k in range(ticks):
        a = math.radians(a0 + (a1 - a0) * k / (ticks - 1))
        big = k in (0, (ticks - 1) // 2, ticks - 1)
        r0, r1 = r + 24, r + (44 if big else 38)
        line([(x + r0 * math.sin(a), y - r0 * math.cos(a)), (x + r1 * math.sin(a), y - r1 * math.cos(a))], INK, 3 if big else 2, 230)

for x, name, lo, hi in KNOBS:
    scale(x, KY, 11, -154, 154)
    text(x, NAME_Y, name, font(36, 0), INK, spacing=2)
    text(x - 80, KY + 160, lo, font(28, 2), INK, spacing=1)
    text(x + 80, KY + 160, hi, font(28, 2), INK, spacing=1)

# ----- CHECK lamp socket (the lamp is drawn live) -----
shadow(CHECK[0], CHECK[1], 22, 3, 5, 5, 140)
layer = new(); dr = ImageDraw.Draw(layer)
dr.ellipse([(CHECK[0] - 22) * S, (CHECK[1] - 22) * S, (CHECK[0] + 22) * S, (CHECK[1] + 22) * S], fill=(20, 20, 22, 255), outline=(255, 255, 255, 70), width=2 * S)
comp(layer)
text(CHECK[0], CHECK[1] + 50, "CHECK", font(26, 2), DARK, 'mm', spacing=2)

# ----- credit, bottom right (right edge lined up with the panels) -----
text(P2[2], PB + 70, "Made by Richard Leurs", font(28, 2), DARK, 'rm', spacing=2)

out = img.convert('RGB').resize((W, H), Image.LANCZOS)
arr = np.asarray(out, np.float32) + rng.normal(0, 1.3, (H, W, 1)).astype(np.float32)
Image.fromarray(np.clip(arr, 0, 255).astype(np.uint8)).save(sys.argv[1] if len(sys.argv) > 1 else 'background.png')
print('ok')
