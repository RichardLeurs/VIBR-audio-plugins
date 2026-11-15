# VIBR audio plugins | Copyright © Richard Leurs 2026 | GPL3.0 License
# The credit line "Made by Richard Leurs | VIBR" + the small VIBR logo (the rainbow V of assets/icon.svg, transparent background),
# right-aligned on `right`, centred on the line `y`.  Used by every make_background.py.
import numpy as np
from PIL import Image, ImageDraw

# the two arms of the V in the 64 x 64 viewBox of icon.svg: (x1, y1, x2, y2, colour stops), stroke 12, round caps
_ARMS = [((10, 12), (32, 54), [(0, (255, 45, 85)), (.34, (255, 138, 0)), (.67, (255, 214, 10)), (1, (48, 224, 96))]),
         ((32, 54), (54, 12), [(0, (48, 224, 96)), (.34, (0, 212, 255)), (.67, (61, 123, 255)), (1, (162, 89, 255))])]

_O = 1.6                                                       # outline width in viewBox units

def vibr_logo(px, outline):
    """RGBA image (px x px) of the rainbow V with a thin outline, on a transparent background."""
    k = px / (56.0 + 2 * _O)                                   # the V spans 4 .. 60 (stroke included), plus the outline
    out = Image.new('RGBA', (px, px), (0, 0, 0, 0))
    m = Image.new('L', (px, px), 0)
    d = ImageDraw.Draw(m)
    for a, b, _ in _ARMS:
        P = [((x - 4 + _O) * k, (y - 4 + _O) * k) for x, y in (a, b)]
        w = (12 + 2 * _O) * k
        d.line(P, fill=255, width=round(w))
        for cx, cy in P: d.ellipse([cx - w / 2, cy - w / 2, cx + w / 2, cy + w / 2], fill=255)
    edge = Image.new('RGBA', (px, px), tuple(outline) + (255,))
    edge.putalpha(m)
    out = Image.alpha_composite(out, edge)
    yy, xx = np.mgrid[0:px, 0:px].astype(np.float32)
    for (x1, y1), (x2, y2), stops in _ARMS:
        P = [((x - 4 + _O) * k, (y - 4 + _O) * k) for x, y in ((x1, y1), (x2, y2))]
        m = Image.new('L', (px, px), 0)
        d = ImageDraw.Draw(m)
        w = 12 * k
        d.line([P[0], P[1]], fill=255, width=round(w))
        for cx, cy in P: d.ellipse([cx - w / 2, cy - w / 2, cx + w / 2, cy + w / 2], fill=255)
        (ax, ay), (bx, by) = P
        t = np.clip(((xx - ax) * (bx - ax) + (yy - ay) * (by - ay)) / ((bx - ax) ** 2 + (by - ay) ** 2), 0, 1)
        rgb = np.zeros((px, px, 3), np.float32)
        for c in range(3):
            rgb[..., c] = np.interp(t, [s[0] for s in stops], [s[1][c] for s in stops])
        arm = Image.fromarray(rgb.astype(np.uint8)).convert('RGBA')
        arm.putalpha(m)
        out = Image.alpha_composite(out, arm)
    return out

def credit(text, new, comp, S, right, y, fnt, fill, size, spacing=2):
    right = round(right)
    L = round(size * 1.0)                                       # logo height in image px
    gap = round(size * 0.45)
    text(right - L - gap, y, "Made by Richard Leurs | VIBR", fnt, fill, 'rm', spacing=spacing)
    layer = new()
    layer.paste(vibr_logo(L * S, fill).resize((L * S, L * S), Image.LANCZOS), ((right - L) * S, round((y - L / 2) * S)))
    comp(layer)
