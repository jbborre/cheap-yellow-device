"""A tiny supersampled vector toolkit for making sprite art.

Shapes are drawn at 4x the final size into boolean masks, painted with flat
colors or simple cel shading, then averaged down so edges are smooth. The
result is snapped to the display's 256 colors, given a dark outline, trimmed
and saved as a PNG plus a manifest entry for png2header.py.
"""
import json
import math
import os

import numpy as np
from PIL import Image, ImageDraw, ImageFont

import rgb332

S = 4  # Supersampling factor.
ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
ART = os.path.join(ROOT, "art")
manifest = {}

# Gray levels used to paint recolorable areas (car bodies, neon tubes).
BODY_GRAYS = [(70, 70, 70), (140, 140, 140), (200, 200, 200), (245, 245, 245)]
BODY_MARKERS = [0x61, 0xA2, 0xE3, 0xEB]
LIGHT_DIM, LIGHT_BRIGHT = (150, 20, 20), (255, 150, 150)
LIGHT_MARKERS = [0x10, 0x1C]


def mix(a, b, t):
    return tuple(int(round(a[i] + (b[i] - a[i]) * t)) for i in range(3))


def lighter(c, t=0.35):
    return mix(c, (255, 255, 255), t)


def darker(c, t=0.4):
    return mix(c, (0, 0, 0), t)


def ang(a):
    """Unit vector at angle a degrees from straight down, positive toward +x."""
    r = math.radians(a)
    return np.array([math.sin(r), math.cos(r)])


def font(size):
    try:
        return ImageFont.load_default(size=size)
    except TypeError:
        return ImageFont.load_default()


class Canvas:
    def __init__(self, w, h, ax=0, ay=0):
        self.w, self.h, self.ax, self.ay = w, h, ax, ay
        self.rgba = np.zeros((h * S, w * S, 4), np.float32)
        self.body = np.zeros((h * S, w * S), np.float32)
        self.light = np.zeros((h * S, w * S), np.float32)

    # Coordinates are final-size pixels relative to the anchor.
    def p(self, x, y):
        return ((self.ax + x) * S, (self.ay + y) * S)

    def _mask(self, fn):
        img = Image.new("L", (self.w * S, self.h * S), 0)
        fn(ImageDraw.Draw(img))
        return np.asarray(img) > 127

    def ellipse(self, cx, cy, rx, ry):
        x0, y0 = self.p(cx - rx, cy - ry)
        x1, y1 = self.p(cx + rx, cy + ry)
        return self._mask(lambda d: d.ellipse([x0, y0, x1, y1], fill=255))

    def circle(self, cx, cy, r):
        return self.ellipse(cx, cy, r, r)

    def poly(self, pts):
        q = [self.p(x, y) for x, y in pts]
        return self._mask(lambda d: d.polygon(q, fill=255))

    def rect(self, x, y, w, h):
        return self.poly([(x, y), (x + w, y), (x + w, y + h), (x, y + h)])

    def rrect(self, x, y, w, h, r):
        x0, y0 = self.p(x, y)
        x1, y1 = self.p(x + w, y + h)
        return self._mask(lambda d: d.rounded_rectangle([x0, y0, x1, y1], radius=r * S, fill=255))

    def capsule(self, a, b, r):
        a, b = np.asarray(a, float), np.asarray(b, float)
        pa, pb = self.p(*a), self.p(*b)
        def fn(d):
            d.line([pa, pb], fill=255, width=max(1, int(2 * r * S)))
            for q in (pa, pb):
                d.ellipse([q[0] - r * S, q[1] - r * S, q[0] + r * S, q[1] + r * S], fill=255)
        return self._mask(fn)

    def line(self, a, b, width):
        pa, pb = self.p(*a), self.p(*b)
        return self._mask(lambda d: d.line([pa, pb], fill=255, width=max(1, int(width * S))))

    def polyline(self, pts, width):
        q = [self.p(x, y) for x, y in pts]
        return self._mask(lambda d: d.line(q, fill=255, width=max(1, int(width * S)), joint="curve"))

    def text(self, s, x, y, size, anchor="mm"):
        f = font(int(size * S))
        q = self.p(x, y)
        return self._mask(lambda d: d.text(q, s, font=f, fill=255, anchor=anchor))

    def everything(self):
        return np.ones((self.h * S, self.w * S), bool)

    # ---- Painting ----
    def paint(self, m, color, alpha=1.0, body=False, light=False):
        if m is None or not m.any():
            return
        c = np.array(color[:3], np.float32) / 255
        old = self.rgba[m]
        a = alpha
        out_a = a + old[:, 3] * (1 - a)
        safe = np.maximum(out_a, 1e-6)[:, None]
        rgb = (c[None, :] * a + old[:, :3] * old[:, 3:4] * (1 - a)) / safe
        self.rgba[m] = np.concatenate([rgb, out_a[:, None]], axis=1)
        if alpha >= 0.5:
            self.body[m] = 1.0 if body else 0.0
            self.light[m] = 1.0 if light else 0.0

    def shade(self, m, base, hi=None, lo=None, k=1.2, body=False):
        """Base color with a light rim (top left) and a shadow rim (bottom right)."""
        if hi is None:
            hi = lighter(base)
        if lo is None:
            lo = darker(base)
        self.paint(m, base, body=body)
        d = int(k * S)
        self.paint(m & ~shift(m, -d, -d), lo, body=body)
        self.paint(m & ~shift(m, max(1, d * 2 // 3), max(1, d * 2 // 3)), hi, body=body)

    def gradient(self, m, top, bottom, y0, y1, alpha=1.0):
        """Vertical gradient between final-size rows y0 and y1 (relative to the anchor)."""
        if not m.any():
            return
        ys = np.nonzero(m)[0] / S - self.ay
        t = np.clip((ys - y0) / max(1e-6, y1 - y0), 0, 1)[:, None]
        a, b = np.array(top, np.float32) / 255, np.array(bottom, np.float32) / 255
        rgb = a + (b - a) * t
        old = self.rgba[m]
        out_a = alpha + old[:, 3] * (1 - alpha)
        mixed = (rgb * alpha + old[:, :3] * old[:, 3:4] * (1 - alpha)) / np.maximum(out_a, 1e-6)[:, None]
        self.rgba[m] = np.concatenate([mixed, out_a[:, None]], axis=1)
        self.body[m] = 0
        self.light[m] = 0

    def glow(self, cx, cy, r, color, strength=0.7):
        """Soft radial glow blended over what's there."""
        H, W = self.rgba.shape[:2]
        x0, y0 = self.p(cx, cy)
        yy, xx = np.mgrid[0:H, 0:W]
        d = np.sqrt((xx - x0) ** 2 + (yy - y0) ** 2) / (r * S)
        a = np.clip(1 - d, 0, 1) ** 2 * strength
        m = a > 0.004
        c = np.array(color, np.float32) / 255
        old = self.rgba[m]
        aa = a[m][:, None]
        out_a = aa[:, 0] + old[:, 3] * (1 - aa[:, 0])
        rgb = (c[None, :] * aa + old[:, :3] * old[:, 3:4] * (1 - aa)) / np.maximum(out_a, 1e-6)[:, None]
        self.rgba[m] = np.concatenate([rgb, out_a[:, None]], axis=1)

    # ---- Output ----
    def finish(self, dither=False, outline=None, opaque=False):
        h, w = self.h, self.w
        blocks = self.rgba.reshape(h, S, w, S, 4)
        a = blocks[..., 3].mean(axis=(1, 3))
        rgb = (blocks[..., :3] * blocks[..., 3:4]).sum(axis=(1, 3)) / np.maximum(blocks[..., 3].sum(axis=(1, 3)), 1e-6)[..., None]
        rgb = np.clip(rgb * 255, 0, 255)
        body = self.body.reshape(h, S, w, S).mean(axis=(1, 3))
        light = self.light.reshape(h, S, w, S).mean(axis=(1, 3))
        solid = np.ones((h, w), bool) if opaque else a >= 0.5
        index = rgb332.dithered(rgb) if dither else rgb332.nearest(rgb)
        lum = rgb.mean(axis=2)
        b = solid & (body >= 0.5)
        if b.any():
            level = np.digitize(lum, [105, 170, 222])
            index[b] = np.array(BODY_MARKERS, np.uint8)[level[b]]
        l = solid & (light >= 0.5)
        if l.any():
            index[l] = np.where(lum[l] > 120, LIGHT_MARKERS[1], LIGHT_MARKERS[0]).astype(np.uint8)
        if outline is not None:
            grow = solid.copy()
            grow[1:] |= solid[:-1]; grow[:-1] |= solid[1:]
            grow[:, 1:] |= solid[:, :-1]; grow[:, :-1] |= solid[:, 1:]
            ring = grow & ~solid
            index[ring] = rgb332.nearest(np.array([[outline]], np.float32))[0, 0]
            solid = grow
        return index, solid


def shift(m, dx, dy):
    out = np.zeros_like(m)
    H, W = m.shape
    xs, xd = (slice(0, W - dx), slice(dx, W)) if dx >= 0 else (slice(-dx, W), slice(0, W + dx))
    ys, yd = (slice(0, H - dy), slice(dy, H)) if dy >= 0 else (slice(-dy, H), slice(0, H + dy))
    out[yd, xd] = m[ys, xs]
    return out


def save(canvas, group, name, path, dither=False, outline=None, trim=True, opaque=False, anchor=None):
    index, solid = canvas.finish(dither=dither, outline=outline, opaque=opaque)
    ax, ay = (canvas.ax, canvas.ay) if anchor is None else anchor
    if trim and solid.any():
        ys, xs = np.nonzero(solid)
        y0, y1, x0, x1 = ys.min(), ys.max() + 1, xs.min(), xs.max() + 1
        index, solid = index[y0:y1, x0:x1], solid[y0:y1, x0:x1]
        ax, ay = ax - x0, ay - y0
    rgb = rgb332.to_rgb(index)
    rgba = np.dstack([rgb, np.where(solid, 255, 0).astype(np.uint8)])
    full = os.path.join(ART, path)
    os.makedirs(os.path.dirname(full), exist_ok=True)
    Image.fromarray(rgba, "RGBA").save(full)
    manifest.setdefault(group, []).append({"file": path, "name": name, "anchor": [int(ax), int(ay)]})


def write_manifest():
    with open(os.path.join(ART, "manifest.json"), "w") as f:
        json.dump(manifest, f, indent=1)
