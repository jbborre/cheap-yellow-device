"""Road Trip Rally art: sky panoramas, roadside scenery and cars.

Car bodies and neon tubes are painted in marker grays (body=True) so the
game can recolor them at runtime; tail lights use the light markers so they
can glow when braking.
"""
import math
import random

import numpy as np

from draw import BODY_GRAYS, LIGHT_BRIGHT, LIGHT_DIM, Canvas, darker, lighter, mix, save

GROUP = "RoadTripArt"
INK = (16, 10, 30)
W = 400  # Sky panoramas wrap around every 400 pixels.


def wrapped(fn):
    for dx in (-W, 0, W):
        fn(dx)


def cloud(c, x, y, s, color=(255, 255, 255), shade=(200, 210, 235)):
    def one(dx):
        puffs = (c.circle(x + dx, y, s) | c.circle(x + dx + s, y + s * 0.35, s * 0.7) | c.circle(x + dx - s, y + s * 0.35, s * 0.7)
                 | c.ellipse(x + dx, y + s * 0.6, s * 1.9, s * 0.45))
        c.paint(puffs, shade)
        c.paint(puffs & ~c.ellipse(x + dx + s * 0.2, y + s * 0.95, s * 2.2, s * 0.6), color)
    wrapped(one)


def sky_coast():
    c = Canvas(W, 120)
    rng = random.Random(1)
    c.gradient(c.everything(), (60, 130, 255), (190, 225, 255), 0, 104)
    c.glow(300, 40, 40, (255, 255, 200), 0.8)
    c.paint(c.circle(300, 40, 13), (255, 250, 170))
    for x, y, s in ((40, 30, 8), (150, 50, 10), (250, 24, 7), (350, 60, 9)):
        cloud(c, x, y, s)
    sea = c.rect(0, 102, W, 18)
    c.gradient(sea, (20, 100, 220), (10, 70, 170), 102, 120)
    for x, w, h in ((60, 40, 8), (210, 26, 5), (330, 50, 10)):
        def isl(dx, x=x, w=w, h=h):
            c.shade(c.ellipse(x + dx, 103, w, h), (60, 150, 70), k=1)
            c.paint(c.capsule((x + dx, 103 - h), (x + dx + 3, 103 - h - 9), 0.8), (110, 70, 20))
            for a in (-2.5, -1.6, -0.6, 0.2):
                c.paint(c.line((x + dx + 3, 103 - h - 9), (x + dx + 3 + math.cos(a) * 7, 103 - h - 9 + abs(math.sin(a)) * 3 + 2), 1.2), (30, 120, 40))
        wrapped(isl)
    c.paint(c.poly([(150, 101), (162, 101), (159, 104), (152, 104)]) | c.poly([(156, 100), (156, 90), (162, 100)]), (255, 255, 255))
    for _ in range(60):
        x, y = rng.uniform(0, W), rng.uniform(105, 119)
        for dx in (0, -W):
            c.paint(c.line((x + dx, y), (x + dx + rng.uniform(4, 9), y), 0.7), (200, 230, 255), alpha=0.8)
    save(c, GROUP, "skyCoast", "roadtrip/sky_coast.png", dither=True, trim=False, opaque=True)


def mesa(c, x, base, w, h, color, rim):
    def one(dx):
        m = c.poly([(x + dx - 14, base), (x + dx, base - h), (x + dx + w, base - h), (x + dx + w + 14, base)])
        c.paint(m, color)
        c.paint(m & c.poly([(x + dx + w * 0.6, base - h), (x + dx + w, base - h), (x + dx + w + 14, base), (x + dx + w * 0.7, base)]), darker(color, 0.2))
        for k in range(1, 4):
            c.paint(m & c.rect(x + dx - 20, base - h + k * h / 4, w + 40, 1), darker(color, 0.15))
        c.paint(m & c.rect(x + dx - 20, base - h, w + 40, 2), rim)
    wrapped(one)


def sky_canyon():
    c = Canvas(W, 120)
    c.gradient(c.everything(), (255, 130, 60), (255, 220, 150), 0, 110)
    c.glow(110, 66, 50, (255, 240, 170), 0.8)
    c.paint(c.circle(110, 66, 18), (255, 245, 180))
    for x, w, h in ((20, 70, 34), (160, 50, 26), (280, 90, 40)):
        mesa(c, x, 120, w, h, (215, 130, 100), (240, 160, 120))
    for x, w, h in ((80, 60, 52), (220, 40, 30), (330, 50, 60)):
        mesa(c, x, 120, w, h, (180, 70, 40), (230, 120, 70))
    save(c, GROUP, "skyCanyon", "roadtrip/sky_canyon.png", dither=True, trim=False, opaque=True)


def mountain(c, x, base, h, w, color, snow):
    def one(dx):
        m = c.poly([(x + dx - w, base), (x + dx, base - h), (x + dx + w, base)])
        c.paint(m, color)
        c.paint(m & c.poly([(x + dx, base - h), (x + dx + w, base), (x + dx + w * 0.2, base)]), darker(color, 0.18))
        cap = m & c.poly([(x + dx - w * 0.3, base - h * 0.7), (x + dx - w * 0.12, base - h * 0.62), (x + dx, base - h * 0.72),
                          (x + dx + w * 0.15, base - h * 0.6), (x + dx + w * 0.3, base - h * 0.7), (x + dx, base - h - 2)])
        c.paint(cap, snow)
        c.paint(cap & c.poly([(x + dx, base - h), (x + dx + w, base), (x + dx, base)]), darker(snow, 0.12))
    wrapped(one)


def sky_peaks():
    c = Canvas(W, 120)
    rng = random.Random(4)
    c.gradient(c.everything(), (100, 170, 255), (225, 250, 255), 0, 110)
    for x, y, s in ((90, 26, 7), (270, 40, 9)):
        cloud(c, x, y, s)
    for x, h, w in ((30, 70, 60), (150, 86, 70), (260, 64, 56), (350, 80, 64)):
        mountain(c, x, 120, h, w, (140, 165, 205), (245, 250, 255))
    for x, h, w in ((90, 50, 50), (210, 58, 54), (320, 44, 46)):
        mountain(c, x, 120, h, w, (80, 110, 150), (230, 240, 250))
    for i in range(48):
        x, h = i * W / 48 + rng.uniform(-2, 2), rng.uniform(7, 13)
        def tree(dx, x=x, h=h):
            c.paint(c.poly([(x + dx - 3, 120), (x + dx, 120 - h), (x + dx + 3, 120)]), (20, 80, 50))
        wrapped(tree)
    save(c, GROUP, "skyPeaks", "roadtrip/sky_peaks.png", dither=True, trim=False, opaque=True)


def sky_farm():
    c = Canvas(W, 120)
    rng = random.Random(6)
    c.gradient(c.everything(), (255, 175, 110), (255, 250, 175), 0, 110)
    c.glow(250, 50, 36, (255, 255, 210), 0.7)
    c.paint(c.circle(250, 50, 12), (255, 250, 200))
    for x, y, s in ((60, 30, 8), (170, 20, 6), (330, 34, 9)):
        cloud(c, x, y, s, shade=(240, 200, 180))
    for x, w, h, col in ((50, 110, 24, (120, 175, 80)), (220, 130, 30, (110, 165, 70)), (360, 90, 20, (120, 175, 80))):
        wrapped(lambda dx, x=x, w=w, h=h, col=col: c.paint(c.ellipse(x + dx, 120, w, h), col))
    for x, w, h, col in ((130, 120, 16, (80, 150, 40)), (300, 110, 14, (90, 155, 45))):
        def hill(dx, x=x, w=w, h=h, col=col):
            m = c.ellipse(x + dx, 120, w, h)
            c.paint(m, col)
            for k in range(-6, 7):
                c.paint(m & c.line((x + dx + k * 14, 120), (x + dx + k * 8, 104), 1.0), (200, 200, 70), alpha=0.6)
        wrapped(hill)
    # Windmill and a little barn on the hill.
    wx, wy = 210, 92
    c.paint(c.poly([(wx - 3, wy + 14), (wx + 3, wy + 14), (wx + 1, wy - 8), (wx - 1, wy - 8)]), (230, 230, 230))
    for k in range(4):
        a = k * math.pi / 2 + 0.3
        c.paint(c.capsule((wx, wy - 8), (wx + math.cos(a) * 11, wy - 8 + math.sin(a) * 11), 1.2), (250, 250, 250))
    c.paint(c.rect(60, 98, 14, 8) | c.poly([(58, 98), (76, 98), (67, 92)]), (190, 40, 40))
    for x in (40, 110, 330, 380):
        wrapped(lambda dx, x=x: c.paint(c.circle(x + dx, 104 + rng.uniform(-2, 2), 4), (40, 110, 40)))
    save(c, GROUP, "skyFarm", "roadtrip/sky_farm.png", dither=True, trim=False, opaque=True)


def sky_night():
    c = Canvas(W, 120)
    rng = random.Random(8)
    c.gradient(c.everything(), (0, 0, 40), (90, 0, 120), 0, 116)
    for _ in range(70):
        x, y = rng.uniform(0, W), rng.uniform(0, 80)
        c.paint(c.circle(x, y, rng.uniform(0.35, 0.7)), (255, 255, 255), alpha=rng.uniform(0.5, 1))
    c.glow(70, 30, 26, (200, 200, 255), 0.5)
    c.paint(c.circle(70, 30, 10), (255, 255, 225))
    c.paint(c.circle(74, 27, 9), (40, 10, 70))  # Crescent.
    c.glow(200, 120, 160, (255, 60, 200), 0.35)
    x = 0
    while x < W:
        w, h = rng.randint(22, 40), rng.randint(26, 74)
        def tower(dx, x=x, w=w, h=h, seed=rng.random()):
            r2 = random.Random(seed)
            c.paint(c.rect(x + dx, 120 - h, w, h), (36, 0, 70))
            for wy in range(120 - h + 4, 116, 6):
                for wx in range(int(x + dx + 3), int(x + dx + w - 3), 5):
                    if r2.random() < 0.35:
                        c.paint(c.rect(wx, wy, 2, 2.5), (255, 220, 110) if r2.random() < 0.8 else (110, 255, 255))
            if r2.random() < 0.3:
                c.paint(c.rect(x + dx + w / 2, 120 - h - 8, 1, 8), (90, 90, 110))
                c.paint(c.circle(x + dx + w / 2, 120 - h - 8, 1.2), (255, 40, 40))
        if x + w <= W:
            wrapped(tower)
        x += w + rng.randint(0, 4)
    save(c, GROUP, "skyNight", "roadtrip/sky_night.png", dither=True, trim=False, opaque=True)


# ---- Roadside props (anchor: bottom center) --------------------------------------
def prop_palm(c, k):
    trunk = []
    x, y = 0, 0
    for i in range(7):
        nx, ny = x + 2.2 * k * (1 + i * 0.25), y - 15 * k
        trunk.append(((x, y), (nx, ny)))
        x, y = nx, ny
    for i, (a, b) in enumerate(trunk):
        c.shade(c.capsule(a, b, (4.2 - i * 0.3) * k), (150, 105, 50), k=1.2 * k)
        c.paint(c.line((a[0] - 4 * k, a[1]), (a[0] + 4 * k, a[1] - 1 * k), 0.8 * k), (100, 64, 24))
    top = (x, y)
    for ang, ln in ((-2.9, 36), (-2.3, 40), (-1.6, 30), (-0.9, 40), (-0.25, 36), (-1.25, 24)):
        pts = []
        for t in np.linspace(0, 1, 8):
            px = top[0] + math.cos(ang) * ln * k * t
            py = top[1] + math.sin(ang) * ln * k * t * 0.5 + (t ** 2) * 16 * k
            pts.append((px, py))
        leaf = []
        for i, (px, py) in enumerate(pts):
            w = 5 * k * math.sin(math.pi * (i + 0.5) / 8)
            leaf.append((px, py - w))
        for i, (px, py) in reversed(list(enumerate(pts))):
            w = 5 * k * math.sin(math.pi * (i + 0.5) / 8)
            leaf.append((px, py + w * 0.6))
        c.shade(c.poly(leaf), (40, 160, 50), hi=(120, 220, 90), lo=(20, 100, 30), k=1.0 * k)
        c.paint(c.polyline(pts, 0.7 * k), (150, 220, 120))
    for dx in (-2.5, 2.5, 0):
        c.shade(c.circle(top[0] + dx * k, top[1] + 4 * k, 2.8 * k), (110, 70, 20), k=0.6 * k)


def prop_umbrella(c, k):
    c.shade(c.ellipse(0, -1 * k, 22 * k, 4 * k), (255, 220, 150), k=0.6 * k)
    c.paint(c.rect(-14 * k, -4 * k, 20 * k, 3 * k), (70, 160, 255))  # Towel.
    c.paint(c.line((0, 0), (2 * k, -58 * k), 1.6 * k), (230, 230, 230))
    dome = c.ellipse(2 * k, -52 * k, 30 * k, 16 * k) & c.rect(-40 * k, -80 * k, 80 * k, 28 * k)
    for i in range(6):
        wedge = dome & c.poly([(2 * k, -68 * k), (-30 * k + i * 10 * k, -52 * k), (-20 * k + i * 10 * k, -52 * k)])
        c.paint(wedge, (255, 70, 150) if i % 2 == 0 else (255, 255, 255))
    c.paint(dome & c.rect(-40 * k, -55 * k, 80 * k, 3 * k), (200, 40, 110))


def prop_cactus(c, k):
    g = (70, 150, 50)
    parts = (c.rrect(-6 * k, -96 * k, 12 * k, 96 * k, 6 * k) | c.rrect(-24 * k, -66 * k, 20 * k, 9 * k, 4.5 * k) | c.rrect(-24 * k, -84 * k, 9 * k, 26 * k, 4.5 * k)
             | c.rrect(4 * k, -50 * k, 20 * k, 9 * k, 4.5 * k) | c.rrect(15 * k, -76 * k, 9 * k, 34 * k, 4.5 * k))
    c.shade(parts, g, hi=(130, 200, 90), lo=(30, 90, 30), k=2 * k)
    for x in (-2, 2):
        c.paint(parts & c.rect(x * k, -96 * k, 0.8 * k, 96 * k), (40, 110, 40))
    rng = random.Random(2)
    for _ in range(int(40 * k) + 4):
        c.paint(parts & c.circle(rng.uniform(-24, 24) * k, rng.uniform(-96, 0) * k, 0.5 * k), (240, 240, 200))
    c.shade(c.circle(0, -97 * k, 3.5 * k), (255, 100, 160), k=0.6 * k)


def prop_rock(c, k):
    c.shade(c.ellipse(-8 * k, -14 * k, 22 * k, 14 * k), (180, 80, 40), hi=(230, 130, 80), lo=(110, 40, 20), k=2.5 * k)
    c.shade(c.ellipse(14 * k, -9 * k, 14 * k, 9 * k), (160, 70, 40), hi=(220, 120, 80), lo=(100, 40, 20), k=2 * k)
    c.paint(c.line((-14 * k, -18 * k), (-4 * k, -22 * k), 0.8 * k), (110, 40, 20))


def prop_pine(c, k):
    c.shade(c.rect(-4 * k, -20 * k, 8 * k, 20 * k), (110, 70, 30), k=1 * k)
    for i in range(4):
        top, half = -112 * k + i * 22 * k, (14 + i * 9) * k
        base = top + 40 * k
        pts = [(0, top)]
        for j in range(5):
            t = (j + 1) / 5
            pts.append((half * t, top + (base - top) * t))
            pts.append((half * t - 4 * k, top + (base - top) * t - 3 * k))
        pts.append((half, base))
        pts += [(-x, y) for x, y in reversed(pts[1:])]
        tier = c.poly(pts)
        c.shade(tier, (20, 110, 50), hi=(60, 160, 80), lo=(10, 60, 30), k=2 * k)
        snow = tier & c.poly([(0, top - 2 * k), (half * 0.5, top + 14 * k), (0, top + 9 * k), (-half * 0.5, top + 14 * k)])
        c.paint(snow, (245, 250, 255))


def prop_snowman(c, k):
    for cy, r in ((-14, 14), (-38, 11), (-56, 8)):
        c.shade(c.circle(0, cy * k, r * k), (250, 250, 255), hi=(255, 255, 255), lo=(180, 200, 235), k=2 * k)
    c.shade(c.rect(-6 * k, -76 * k, 12 * k, 12 * k) | c.rect(-9 * k, -65 * k, 18 * k, 2.5 * k), (30, 30, 36), k=1 * k)
    c.paint(c.rect(-6 * k, -67 * k, 12 * k, 2 * k), (220, 30, 30))
    c.shade(c.capsule((-8 * k, -47 * k), (8 * k, -46 * k), 2.6 * k) | c.capsule((5 * k, -46 * k), (8 * k, -36 * k), 2.2 * k), (220, 30, 30), k=0.8 * k)
    for x in (-3, 3):
        c.paint(c.circle(x * k, -58 * k, 1.1 * k), (20, 20, 20))
    c.paint(c.poly([(0, -55 * k), (10 * k, -53 * k), (0, -52 * k)]), (255, 130, 20))
    for x in (-2, 0, 2):
        c.paint(c.circle(0, (-38 + x * 3) * k, 1 * k), (20, 20, 20))
    for side in (-1, 1):
        c.paint(c.polyline([(side * 10 * k, -40 * k), (side * 22 * k, -48 * k), (side * 26 * k, -54 * k)], 1.4 * k), (100, 60, 20))


def prop_barn(c, k):
    w, h = 110 * k, 62 * k
    wall = c.rect(-w / 2, -h, w, h)
    c.shade(wall, (190, 40, 40), k=2 * k)
    for x in np.arange(-w / 2, w / 2, 5 * k):
        c.paint(wall & c.rect(x, -h, 0.7 * k, h), (150, 30, 30))
    roof = c.poly([(-w / 2 - 5 * k, -h), (-w / 2 + 10 * k, -h - 26 * k), (0, -h - 38 * k), (w / 2 - 10 * k, -h - 26 * k), (w / 2 + 5 * k, -h)])
    c.shade(roof, (70, 70, 80), hi=(120, 120, 130), k=2 * k)
    c.paint(c.polyline([(-w / 2 - 5 * k, -h), (-w / 2 + 10 * k, -h - 26 * k), (0, -h - 38 * k), (w / 2 - 10 * k, -h - 26 * k), (w / 2 + 5 * k, -h)], 2 * k), (240, 240, 240))
    door = c.rect(-16 * k, -40 * k, 32 * k, 40 * k)
    c.paint(door, (170, 30, 30))
    for a, b in (((-16, -40), (16, 0)), ((16, -40), (-16, 0)), ((-16, -40), (16, -40)), ((-16, 0), (16, 0)), ((-16, -40), (-16, 0)), ((16, -40), (16, 0)), ((0, -40), (0, 0))):
        c.paint(c.line((a[0] * k, a[1] * k), (b[0] * k, b[1] * k), 2 * k), (245, 245, 245))
    loft = c.rect(-8 * k, -h - 14 * k, 16 * k, 12 * k)
    c.paint(loft, (60, 20, 20))
    c.paint(c.rect(-8 * k, -h - 14 * k, 16 * k, 12 * k) & ~c.rect(-6.5 * k, -h - 12.5 * k, 13 * k, 9 * k), (245, 245, 245))
    c.paint(c.ellipse(0, -h - 9 * k, 5 * k, 3 * k), (230, 200, 80))  # Hay peeking out.


def prop_haybale(c, k):
    side = c.rect(-14 * k, -30 * k, 28 * k, 30 * k)
    c.shade(side, (220, 180, 70), k=2 * k)
    for x in np.arange(-14, 14, 3):
        c.paint(side & c.rect(x * k, -30 * k, 0.8 * k, 30 * k), (180, 140, 40))
    face = c.ellipse(14 * k, -15 * k, 9 * k, 15 * k)
    c.shade(face, (240, 210, 110), k=1.5 * k)
    for r in (3, 6, 9):
        c.paint(face & ~c.ellipse(14 * k, -15 * k, r * 0.6 * k, r * k) & c.ellipse(14 * k, -15 * k, (r * 0.6 + 0.6) * k, (r + 1) * k), (190, 150, 50))


def prop_streetlight(c, k):
    c.shade(c.rect(-2.5 * k, -112 * k, 5 * k, 112 * k), (110, 110, 120), k=1 * k)
    c.shade(c.rect(-5 * k, -10 * k, 10 * k, 10 * k), (80, 80, 90), k=1 * k)
    c.paint(c.polyline([(0, -112 * k), (6 * k, -118 * k), (22 * k, -118 * k)], 3 * k), (110, 110, 120))
    c.shade(c.poly([(16 * k, -121 * k), (34 * k, -121 * k), (30 * k, -114 * k), (20 * k, -114 * k)]), (90, 90, 100), k=1 * k)
    c.paint(c.rect(20 * k, -115 * k, 10 * k, 3 * k), (255, 255, 200))


def prop_neon(c, k, word):
    c.shade(c.rect(-2 * k, -48 * k, 4 * k, 48 * k), (90, 90, 100), k=1 * k)
    board = c.rrect(-34 * k, -90 * k, 68 * k, 40 * k, 6 * k)
    c.paint(board, (40, 0, 76))
    tube = board & ~c.rrect(-31 * k, -87 * k, 62 * k, 34 * k, 4 * k)
    c.paint(tube, BODY_GRAYS[3], body=True)
    c.paint(c.text(word, 0, -70 * k, 16 * k), BODY_GRAYS[2], body=True)
    c.paint(c.text(word, -0.5 * k, -70.5 * k, 16 * k) & c.rect(-34 * k, -76 * k, 68 * k, 3 * k), BODY_GRAYS[3], body=True)


PROPS = [
    ("Palm", prop_palm, 140, 128), ("Umbrella", prop_umbrella, 80, 80), ("Cactus", prop_cactus, 70, 110),
    ("Rock", prop_rock, 70, 44), ("Pine", prop_pine, 90, 124), ("Snowman", prop_snowman, 70, 84),
    ("Barn", prop_barn, 130, 120), ("Haybale", prop_haybale, 50, 40), ("Streetlight", prop_streetlight, 60, 132),
]


def props():
    for size, k in (("Big", 1.0), ("Small", 0.34)):
        for name, fn, w, h in PROPS:
            c = Canvas(int(w * k) + 12, int(h * k) + 8, ax=int(w * k / 2) + 6, ay=int(h * k) + 4)
            fn(c, k)
            save(c, GROUP, f"prop{name}{size}", f"roadtrip/prop_{name.lower()}_{size.lower()}.png", outline=INK)
        for word in ("DINER", "MOTEL"):
            c = Canvas(int(80 * k) + 12, int(100 * k) + 8, ax=int(40 * k) + 6, ay=int(100 * k) + 4)
            prop_neon(c, k, word)
            save(c, GROUP, f"prop{word.capitalize()}{size}", f"roadtrip/prop_{word.lower()}_{size.lower()}.png", outline=INK)


# ---- Cars, seen from behind (anchor: bottom center) -------------------------------
STYLES = ["Roadster", "Muscle", "Offroad", "Sedan", "Van", "Truck"]
HEIGHT = {"Roadster": 0.5, "Muscle": 0.52, "Offroad": 0.74, "Sedan": 0.56, "Van": 0.82, "Truck": 1.0}


def body_paint(c, m, k=1.0):
    c.paint(m, BODY_GRAYS[1], body=True)
    d = 3
    from draw import shift
    c.paint(m & ~shift(m, -int(1.5 * k * 4), -int(1.5 * k * 4)), BODY_GRAYS[0], body=True)
    c.paint(m & ~shift(m, int(1.0 * k * 4), int(1.0 * k * 4)), BODY_GRAYS[2], body=True)


def car(c, style, w, lean):
    h = w * HEIGHT[style]
    u = w / 92  # Detail scale.
    roll = lean * w * 0.02
    tw, th = w * 0.17, h * 0.3
    for side in (-1, 1):
        tire = c.rrect(side * (w / 2 - tw / 2) - tw / 2, -th, tw, th, tw * 0.3)
        c.paint(tire, (24, 24, 28))
        for yy in np.arange(-th + 2 * u, 0, 3 * u):
            c.paint(tire & c.rect(side * (w / 2 - tw / 2) - tw / 2, yy, tw, 0.8 * u), (70, 70, 80))
    if style in ("Van", "Truck"):
        bw = w * (1.04 if style == "Truck" else 1.0)
        box = c.rrect(-bw / 2 + roll, -h, bw, h * 0.86, 3 * u)
        body_paint(c, box, u)
        c.paint(c.rect(-0.6 * u + roll, -h + 3 * u, 1.2 * u, h * 0.8), darker(BODY_GRAYS[0], 0.4))
        if style == "Van":
            for side in (-1, 1):
                win = c.rrect(roll + side * w * 0.22 - w * 0.16, -h + h * 0.1, w * 0.32, h * 0.24, 2 * u)
                c.gradient(win, (80, 120, 170), (30, 50, 90), -h, -h + h * 0.34)
            c.paint(c.rect(roll + w * 0.04, -h * 0.5, w * 0.08, 2 * u), (40, 40, 46))
        else:
            for side in (-1, 1):
                for x in (0.14, 0.32):
                    c.shade(c.rect(roll + side * w * x - u, -h + 4 * u, 2 * u, h * 0.78), (150, 150, 160), k=0.5 * u)
            c.paint(c.rect(-bw / 2 + roll, -h * 0.2, bw, 2.4 * u), (240, 240, 240))
        lights_y = -h * 0.24
    else:
        lower_top = -h * 0.62
        cab_w0, cab_w1 = w * (0.86 if style == "Offroad" else 0.78), w * (0.8 if style == "Offroad" else 0.58)
        top_shift = lean * w * 0.05
        if style == "Roadster":
            for side in (-1, 1):
                seat = c.rrect(roll + top_shift + side * w * 0.2 - w * 0.09, lower_top - h * 0.36, w * 0.18, h * 0.42, 4 * u)
                c.shade(seat, (40, 40, 46), k=1.2 * u)
            hd = (roll + top_shift - w * 0.2, lower_top - h * 0.42)
            c.shade(c.circle(*hd, w * 0.085), (110, 60, 20), hi=(160, 100, 40), k=1 * u)
            bar = c.polyline([(roll - w * 0.36, lower_top), (roll + top_shift - w * 0.3, lower_top - h * 0.5), (roll + top_shift + w * 0.3, lower_top - h * 0.5), (roll + w * 0.36, lower_top)], 2.2 * u)
            c.shade(bar, (200, 200, 215), k=0.6 * u)
        else:
            cab = c.poly([(roll - cab_w0 / 2, lower_top + 1), (roll + top_shift - cab_w1 / 2, -h), (roll + top_shift + cab_w1 / 2, -h), (roll + cab_w0 / 2, lower_top + 1)])
            if style == "Offroad":
                cab = c.rrect(roll + top_shift / 2 - cab_w0 / 2, -h, cab_w0, h - h * 0.62 + 2, 4 * u)
            body_paint(c, cab, u)
            inset = 3.5 * u
            glass = c.poly([(roll - cab_w0 / 2 + inset * 1.6, lower_top - 1), (roll + top_shift - cab_w1 / 2 + inset, -h + inset),
                            (roll + top_shift + cab_w1 / 2 - inset, -h + inset), (roll + cab_w0 / 2 - inset * 1.6, lower_top - 1)])
            c.gradient(glass, (90, 140, 190), (30, 50, 90), -h, lower_top)
            c.paint(glass & c.line((roll - w * 0.12, lower_top), (roll + top_shift - w * 0.02, -h), 3 * u), (200, 230, 255), alpha=0.6)
            if style == "Offroad":
                c.paint(c.rect(roll + top_shift / 2 - cab_w0 / 2, -h - 3 * u, cab_w0, 1.6 * u), (40, 40, 46))  # Roof rack.
                c.paint(c.rect(roll + top_shift / 2 - w * 0.2, -h - 6 * u, w * 0.4, 3 * u), (255, 190, 40))  # Light bar.
        lower = c.rrect(-w / 2 + roll, lower_top, w, h * 0.56, 5 * u)
        body_paint(c, lower, u)
        c.paint(c.rect(-w / 2 + roll + 3 * u, lower_top + 1 * u, w - 6 * u, 1.2 * u), BODY_GRAYS[3], body=True)
        if style == "Muscle":
            for dx in (-6, 3):
                c.paint(lower & c.rect(roll + dx * u, lower_top, 3.5 * u, h), (245, 245, 245))
            spoiler = c.rect(-w / 2 + roll + 2 * u, lower_top - 4 * u, w - 4 * u, 3 * u) | c.rect(-w * 0.4 + roll, lower_top - 2 * u, 2 * u, 3 * u) | c.rect(w * 0.4 - 2 * u + roll, lower_top - 2 * u, 2 * u, 3 * u)
            c.shade(spoiler, (30, 30, 36), k=0.6 * u)
        if style == "Offroad":
            c.shade(c.circle(roll, lower_top + h * 0.2, w * 0.13), (26, 26, 30), k=1.2 * u)
            c.shade(c.circle(roll, lower_top + h * 0.2, w * 0.05), (170, 170, 180), k=0.6 * u)
        lights_y = lower_top + h * 0.1
    # Tail lights, bumper, plate and exhaust.
    lw, lh = w * 0.19, max(2.5 * u, h * 0.11)
    for side in (-1, 1):
        x0 = roll + (side * (w / 2 - 2 * u - lw / 2)) - lw / 2
        c.paint(c.rrect(x0, lights_y, lw, lh, 1.5 * u), LIGHT_DIM, light=True)
        c.paint(c.rrect(x0 + lw * 0.2, lights_y + lh * 0.25, lw * 0.6, lh * 0.5, 1 * u), LIGHT_BRIGHT, light=True)
    c.shade(c.rect(-w / 2 + roll + 1 * u, -h * 0.16, w - 2 * u, h * 0.12), (40, 40, 46), k=0.6 * u)
    plate = c.rect(roll - w * 0.12, -h * 0.16 - h * 0.13, w * 0.24, h * 0.12)
    c.paint(plate, (240, 240, 240))
    c.paint(plate & c.rect(roll - w * 0.09, -h * 0.16 - h * 0.09, w * 0.18, h * 0.04), (40, 40, 120))
    c.shade(c.circle(roll + w * 0.3, -h * 0.07, 2.2 * u), (180, 180, 190), k=0.5 * u)


def cars():
    for style in STYLES:
        player = style in ("Roadster", "Muscle", "Offroad")
        sizes = [(92, 0), (46, 0), (22, 0)] + ([(92, -1), (92, 1)] if player else [])
        for w, lean in sizes:
            h = int(w * HEIGHT[style]) + 16
            c = Canvas(w + 16, h, ax=w // 2 + 8, ay=h - 2)
            car(c, style, w, lean)
            tag = {0: "", -1: "L", 1: "R"}[lean]
            save(c, GROUP, f"car{style}{w}{tag}", f"roadtrip/car_{style.lower()}_{w}{tag.lower()}.png", outline=INK)


def generate():
    sky_coast(); sky_canyon(); sky_peaks(); sky_farm(); sky_night()
    props()
    cars()
