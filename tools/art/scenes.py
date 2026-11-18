"""Black Knight backgrounds: the shipwreck deck and the run-down city street."""
import math
import random

import numpy as np

from draw import Canvas, darker, lighter, mix, save

GROUP = "BlackKnightArt"
INK = (20, 10, 26)


def noise_streaks(c, m, color, count, rng, w, h, length=(6, 20), alpha=0.5, width=0.6, wrap=None):
    for _ in range(count):
        x, y = rng.uniform(0, w), rng.uniform(0, h)
        ln = rng.uniform(*length)
        for dx in ([0] if wrap is None else [0, -wrap, wrap]):
            c.paint(m & c.line((x + dx, y), (x + dx + ln, y + rng.uniform(-0.6, 0.6)), width), color, alpha=alpha)


# ---- Shipwreck ----------------------------------------------------------------
def ship_sky():
    c = Canvas(400, 124)
    rng = random.Random(3)
    c.gradient(c.everything(), (0, 0, 44), (44, 80, 120), 0, 98)
    for _ in range(40):  # Stars.
        x, y = rng.uniform(0, 400), rng.uniform(0, 70)
        c.paint(c.circle(x, y, rng.uniform(0.35, 0.7)), (255, 255, 230), alpha=rng.uniform(0.5, 1))
    c.glow(300, 34, 40, (150, 160, 200), 0.6)
    moon = c.circle(300, 34, 13)
    c.paint(moon, (255, 255, 225))
    for dx, dy, r in ((-4, -3, 2.5), (5, 4, 3), (2, -6, 1.6)):
        c.paint(c.circle(300 + dx, 34 + dy, r), (220, 220, 190))
    for i, (x, y, s) in enumerate(((40, 30, 30), (150, 52, 40), (250, 64, 34), (360, 22, 28))):
        cl = c.ellipse(x, y, s * 1.4, s * 0.25) | c.ellipse(x - s * 0.4, y - 3, s * 0.6, s * 0.22) | c.ellipse(x + s * 0.5, y - 2, s * 0.5, s * 0.2)
        c.paint(cl, (30, 34, 70), alpha=0.9)
        c.paint(cl & ~c.ellipse(x + 2, y + 2, s * 1.4, s * 0.25), (110, 110, 150), alpha=0.8)
    # Distant wreck on the horizon.
    hull = c.poly([(60, 98), (130, 98), (124, 92), (70, 93)])
    c.paint(hull | c.rect(84, 70, 2, 24) | c.rect(108, 76, 2, 18) | c.poly([(86, 72), (98, 80), (86, 84)]), (20, 30, 60))
    sea = c.rect(0, 98, 400, 26)
    c.gradient(sea, (10, 40, 110), (0, 30, 80), 98, 124)
    for _ in range(70):
        x, y = rng.uniform(0, 400), rng.uniform(100, 123)
        c.paint(c.line((x, y), (x + rng.uniform(4, 12), y), 0.7), (80, 120, 190), alpha=0.8)
    for y in range(100, 124, 3):  # Moonlight on the water.
        w = rng.uniform(6, 16)
        c.paint(c.line((300 - w / 2 + rng.uniform(-3, 3), y), (300 + w / 2, y), 1.0), (230, 230, 200), alpha=0.9)
    save(c, GROUP, "shipSky", "blackknight/ship_sky.png", dither=True, trim=False, opaque=True)


def wood(c, m, base, rng, w, h, wrap=None):
    c.paint(m, base)
    noise_streaks(c, m, darker(base, 0.3), int(w * h / 40), rng, w, h, alpha=0.6, wrap=wrap)
    noise_streaks(c, m, lighter(base, 0.15), int(w * h / 90), rng, w, h, alpha=0.5, wrap=wrap)


def ship_mast():
    c = Canvas(140, 140, ax=92, ay=136)
    rng = random.Random(5)
    c.paint(c.line((-36, -97), (-72, 0), 1.0), (60, 40, 20))  # Rigging.
    c.paint(c.line((30, -97), (44, 0), 0.8), (60, 40, 20))
    mast = c.poly([(-5, -122), (-3, -132), (0, -126), (2, -134), (5, -124), (5, 0), (-5, 0)])
    wood(c, mast, (120, 80, 40), rng, 140, 140)
    c.paint(mast & c.rect(-6, -140, 4, 140), (150, 110, 60), alpha=0.6)
    for y in (-80, -40):
        c.shade(c.rect(-6, y, 12, 3), (70, 70, 80))
    yard = c.rrect(-42, -104, 84, 5, 2)
    wood(c, yard, (90, 60, 30), rng, 140, 140)
    sail = c.poly([(-38, -99), (38, -99), (32, -60), (28, -38), (12, -42), (-4, -36), (-20, -40), (-34, -44)])
    holes = c.poly([(-14, -80), (2, -74), (-8, -60)]) | c.poly([(14, -88), (24, -82), (18, -70)]) | c.ellipse(-24, -62, 4, 3)
    sail &= ~holes
    c.gradient(sail, (235, 230, 200), (190, 180, 150), -99, -36)
    for x in (-24, -8, 8, 24):
        c.paint(sail & c.line((x, -99), (x * 0.9, -38), 0.6), (170, 160, 130))
    c.paint(sail & ~c.poly([(-38, -98), (38, -98), (34, -70), (-36, -72)]), (160, 150, 120), alpha=0.22)
    save(c, GROUP, "shipMast", "blackknight/ship_mast.png", outline=INK)


def ship_railing():
    c = Canvas(168, 20)
    rng = random.Random(7)
    beam = c.rect(0, 2, 168, 4)
    wood(c, beam, (110, 74, 36), rng, 168, 20, wrap=168)
    c.paint(c.rect(0, 2, 168, 1), (160, 120, 70))
    for k in range(7):
        x = k * 24
        if k == 3:
            stub = c.poly([(x, 20), (x, 13), (x + 1.5, 11), (x + 2.5, 14), (x + 4, 12), (x + 4, 20)])
            c.shade(stub, (80, 40, 10))
            continue
        post = c.rect(x, 2, 4, 18)
        c.shade(post, (90, 50, 16))
    save(c, GROUP, "shipRailing", "blackknight/ship_railing.png", trim=False, outline=None)


def ship_deck():
    c = Canvas(120, 74)
    rng = random.Random(11)
    for row in range(7):
        y0 = row * 11
        plank = c.rect(0, y0, 120, 11)
        base = mix((150, 110, 70), (80, 40, 10), row / 6)
        c.gradient(plank, lighter(base, 0.15), darker(base, 0.15), y0, y0 + 11)
        noise_streaks(c, plank, darker(base, 0.3), 40, rng, 120, 74, alpha=0.5, wrap=120)
        c.paint(c.rect(0, y0 + 10, 120, 1), darker(base, 0.55))
        off = (row % 2) * 30
        for x in range(off, 120 + 60, 60):
            xx = x % 120
            c.paint(c.rect(xx, y0, 1, 10), darker(base, 0.55))
            for ny in (3, 7):
                c.paint(c.circle(xx + 2.5, y0 + ny, 0.6), (40, 40, 40))
    save(c, GROUP, "shipDeck", "blackknight/ship_deck.png", dither=True, trim=False, opaque=True)


def ship_props():
    rng = random.Random(13)
    # Splintered hole in the deck.
    c = Canvas(70, 24, ax=35, ay=12)
    hole = c.poly([(-28, -2), (-12, -6), (6, -5), (26, -1), (10, 4), (-4, 8), (-18, 5)])
    c.paint(hole, (30, 6, 6))
    c.paint(hole & c.rect(-30, -8, 60, 4), (60, 20, 10))
    for a, b in (((-28, -2), (-33, -7)), ((26, -1), (31, 4)), ((6, -5), (9, -10)), ((-4, 8), (-6, 12))):
        c.paint(c.line(a, b, 1.6), (190, 150, 80))
    save(c, GROUP, "shipHole", "blackknight/ship_hole.png", outline=None)

    c = Canvas(30, 34, ax=15, ay=32)
    barrel = c.rrect(-11, -28, 22, 28, 6)
    c.shade(barrel, (150, 80, 20), k=2)
    for x in (-6, 0, 6):
        c.paint(barrel & c.line((x, -28), (x, 0), 0.5), (100, 50, 10))
    for y in (-22, -7):
        c.shade(barrel & c.rect(-12, y, 24, 2.5), (70, 60, 60))
    save(c, GROUP, "shipBarrel", "blackknight/ship_barrel.png", outline=INK)

    c = Canvas(52, 30, ax=26, ay=28)
    c.shade(c.rrect(-16, -12, 32, 10, 3), (110, 70, 30))
    c.shade(c.capsule((-18, -16), (16, -14), 5), (40, 40, 46), hi=(110, 110, 120), k=1.5)
    c.paint(c.circle(20, -14, 4.5), (10, 10, 10))
    c.shade(c.circle(20, -14, 6) & ~c.circle(20, -14, 4.5), (60, 60, 70))
    for x in (-9, 9):
        c.shade(c.circle(x, -5, 5.5), (90, 56, 20))
        c.paint(c.circle(x, -5, 1.5), (40, 30, 20))
    save(c, GROUP, "shipCannon", "blackknight/ship_cannon.png", outline=INK)

    c = Canvas(34, 32, ax=17, ay=30)
    crate = c.rect(-13, -24, 26, 24)
    wood(c, crate, (190, 150, 80), rng, 34, 32)
    for m in (c.rect(-13, -24, 26, 3), c.rect(-13, -3, 26, 3), c.rect(-13, -24, 3, 24), c.rect(10, -24, 3, 24), c.line((-11, -22), (11, -2), 2.5)):
        c.shade(m, (120, 80, 36), k=0.6)
    save(c, GROUP, "shipCrate", "blackknight/ship_crate.png", outline=INK)

    c = Canvas(54, 74, ax=27, ay=72)
    c.shade(c.rect(-3, -30, 6, 30), (80, 40, 10))
    ring = c.circle(0, -36, 16) & ~c.circle(0, -36, 13)
    spokes = c.circle(0, -36, 3.5)
    for i in range(8):
        a = i * math.pi / 4
        spokes |= c.capsule((0, -36), (math.cos(a) * 21, -36 + math.sin(a) * 21), 1.2)
    c.shade(spokes | ring, (150, 80, 20), k=0.8)
    c.paint(c.circle(0, -36, 1.5), (219, 182, 0))
    save(c, GROUP, "shipWheel", "blackknight/ship_wheel.png", outline=INK)

    c = Canvas(20, 70, ax=10, ay=66)
    c.shade(c.rect(-1.5, -56, 3, 56), (50, 50, 56))
    c.shade(c.rrect(-4.5, -64, 9, 13, 2), (255, 220, 100), hi=(255, 255, 200), lo=(200, 140, 30))
    c.paint(c.rect(-5, -65, 10, 2) | c.rect(-5, -52, 10, 2), (40, 40, 40))
    c.paint(c.line((-4.5, -58), (4.5, -58), 0.6), (120, 80, 20))
    save(c, GROUP, "shipLantern", "blackknight/ship_lantern.png", outline=INK)


# ---- City street ----------------------------------------------------------------
def city_sky():
    c = Canvas(400, 134)
    rng = random.Random(17)
    c.gradient(c.everything(), (36, 0, 76), (230, 120, 76), 0, 134)
    c.glow(60, 26, 22, (240, 200, 160), 0.5)
    c.paint(c.circle(60, 26, 9), (230, 200, 160))
    for y, a in ((60, 0.25), (84, 0.3)):  # Smog bands.
        c.paint(c.rect(0, y, 400, 6), (200, 120, 110), alpha=a)
    x = -10
    while x < 400:
        w, h = rng.randint(26, 46), rng.randint(36, 86)
        tower = c.rect(x, 134 - 30 - h, w, h + 30)
        c.paint(tower, (76, 36, 76))
        for _ in range(int(w * h / 90)):
            wx, wy = rng.uniform(x + 3, x + w - 4), rng.uniform(134 - 26 - h, 120)
            c.paint(c.rect(wx, wy, 1.6, 1.6), (255, 220, 110) if rng.random() < 0.6 else (150, 80, 110))
        if rng.random() < 0.25:  # Water tower on the roof.
            tx, ty = x + w / 2, 134 - 30 - h
            c.paint(c.rect(tx - 5, ty - 12, 10, 8) | c.poly([(tx - 6, ty - 12), (tx + 6, ty - 12), (tx, ty - 17)]) | c.rect(tx - 4, ty - 4, 1, 4) | c.rect(tx + 3, ty - 4, 1, 4), (60, 26, 60))
        x += w + rng.randint(0, 6)
    save(c, GROUP, "citySky", "blackknight/city_sky.png", dither=True, trim=False, opaque=True)


def brick_wall(c, m, base, rng, x0, y0, w, h, brick=True):
    if not brick:
        c.paint(m, base)
        for _ in range(8):  # Water stains.
            sx = rng.uniform(x0, x0 + w)
            c.paint(m & c.rect(sx, y0, rng.uniform(2, 5), rng.uniform(15, h)), darker(base, 0.25), alpha=0.6)
        for yy in range(int(y0) + 18, int(y0 + h), 18):
            c.paint(m & c.rect(x0, yy, w, 0.6), darker(base, 0.3))
        return
    c.paint(m, darker(base, 0.35))  # Mortar.
    for row, y in enumerate(np.arange(y0, y0 + h, 4)):
        off = 4 if row % 2 else 0
        for x in np.arange(x0 - off, x0 + w, 8):
            shade = mix(base, (0, 0, 0) if rng.random() < 0.5 else (255, 255, 255), rng.uniform(0, 0.14))
            c.paint(m & c.rect(x + 0.5, y + 0.5, 7, 3), shade)
    for _ in range(6):
        sx = rng.uniform(x0, x0 + w)
        c.paint(m & c.rect(sx, y0 + rng.uniform(0, h / 2), rng.uniform(2, 6), rng.uniform(10, h / 2)), (20, 10, 10), alpha=0.25)


def window(c, x, y, w, h, kind, rng):
    c.paint(c.rect(x - 1.5, y - 1.5, w + 3, h + 3), (40, 36, 36))
    glass = (60, 90, 130)
    if kind in (0, 1):  # Dark, whole.
        c.gradient(c.rect(x, y, w, h), (40, 50, 90), (20, 20, 50), y, y + h)
        c.paint(c.line((x + 2, y + h - 3), (x + w - 3, y + 2), 1.0), (120, 150, 200), alpha=0.8)
        c.paint(c.rect(x, y + h / 2, w, 0.8), (40, 36, 36))
    elif kind in (2, 3):  # Smashed, shards left in the frame.
        c.paint(c.rect(x, y, w, h), (6, 6, 12))
        c.paint(c.poly([(x, y), (x + w * 0.65, y), (x + w * 0.2, y + h * 0.25), (x, y + h * 0.55)]), glass)
        c.paint(c.poly([(x + w, y + h), (x + w * 0.35, y + h), (x + w * 0.7, y + h * 0.7), (x + w, y + h * 0.3)]), glass)
        c.paint(c.line((x + w * 0.65, y), (x + w * 0.2, y + h * 0.25), 0.7), (200, 230, 255))
    elif kind == 4:  # Boarded up.
        c.paint(c.rect(x, y, w, h), (10, 10, 10))
        for a, b in (((x - 1, y + 3), (x + w + 1, y + h - 4)), ((x - 1, y + h - 4), (x + w + 1, y + 3)), ((x - 1, y + h / 2), (x + w + 1, y + h / 2))):
            c.shade(c.line(a, b, 3.2), (160, 120, 70), k=0.6)
        for px, py in ((x + 1, y + 3), (x + w - 2, y + 3), (x + 1, y + h - 4)):
            c.paint(c.circle(px, py, 0.5), (40, 40, 40))
    elif kind == 5:  # Cracked like a spider web.
        c.gradient(c.rect(x, y, w, h), (50, 60, 100), (24, 24, 56), y, y + h)
        cx, cy = x + w * 0.35, y + h * 0.35
        for tx, ty in ((x, y + h), (x + w, y + h * 0.5), (x + w * 0.5, y), (x, y + 2), (x + w, y + h)):
            c.paint(c.line((cx, cy), (tx, ty), 0.6), (200, 200, 230))
        c.paint(c.circle(cx, cy, 2.5) & ~c.circle(cx, cy, 1.8), (200, 200, 230))
    elif kind == 6:  # Someone's still home.
        c.gradient(c.rect(x, y, w, h), (255, 230, 130), (220, 150, 70), y, y + h)
        c.paint(c.poly([(x, y), (x + w * 0.45, y), (x + w * 0.3, y + h), (x, y + h)]), (190, 70, 60))
    else:  # Empty frame, the glass long gone.
        c.paint(c.rect(x, y, w, h), (8, 8, 14))
        c.paint(c.rect(x, y + h / 2, w, 0.8), (70, 66, 66))
    c.shade(c.rect(x - 2.5, y + h + 1, w + 5, 2), (150, 150, 150), k=0.5)


def building(name, w, h, wall, brick, seed, tag=None, brock=False):
    rng = random.Random(seed)
    c = Canvas(w + 6, h + 8, ax=3, ay=h + 8)
    top = -h
    m = c.rect(0, top, w, h)
    brick_wall(c, m, wall, rng, 0, top, w, h, brick)
    # Crumbling ledge with a chunk missing.
    gap = rng.uniform(20, w - 40)
    ledge = (c.rect(-2.5, top - 4, gap + 2.5, 5) | c.rect(gap + 14, top - 4, w - gap - 11.5, 5))
    c.shade(ledge, darker(wall, 0.35), k=0.7)
    c.paint(c.poly([(gap, top), (gap + 14, top), (gap + 7, top + 9)]), darker(wall, 0.5))
    # Windows above the ground floor.
    cols = int((w - 12) // 26)
    x0 = (w - (cols * 26 - 10)) / 2
    r = 0
    while top + 12 + r * 30 + 20 < -34:
        for col in range(cols):
            kind = rng.randrange(8)
            window(c, x0 + col * 26, top + 12 + r * 30, 16, 20, kind, rng)
            if kind in (0, 1, 7) and rng.random() < 0.3:  # Rusty AC unit.
                c.shade(c.rect(x0 + col * 26 + 2, top + 12 + r * 30 + 16, 12, 7), (150, 150, 140), k=0.6)
                c.paint(c.rect(x0 + col * 26 + 3, top + 12 + r * 30 + 22, 1, 4), (110, 60, 20))
        r += 1
    # A long crack.
    cx, cy = rng.uniform(w * 0.2, w * 0.8), top + 8
    pts = [(cx, cy)]
    for i in range(6):
        cx += rng.uniform(-4, 4); cy += rng.uniform(6, 11)
        pts.append((cx, cy))
    c.paint(c.polyline(pts, 0.9), (24, 20, 20))
    # Rusty fire escape.
    if seed % 2:
        fx = w - 24
        y = top + 34
        while y < -40:
            c.shade(c.rect(fx - 4, y, 26, 2), (110, 50, 40), k=0.4)
            for xx in range(int(fx - 4), int(fx + 22), 3):
                c.paint(c.rect(xx, y - 8, 0.8, 8), (90, 40, 30))
            c.paint(c.line((fx, y + 2), (fx + 16, y + 28), 1.0), (110, 50, 40))
            y += 30
    # Torn posters.
    for _ in range(2):
        px, py = rng.uniform(4, w - 20), rng.uniform(-30, -24)
        c.paint(c.poly([(px, py - 10), (px + 12, py - 11), (px + 13, py), (px + 6, py + 2), (px, py)]), (230, 220, 180))
        c.paint(c.rect(px + 2, py - 8, 8, 2), (200, 60, 60))
    # Ground floor: a dented roll-down shutter with graffiti, and a door.
    sx = w / 2 - 22
    c.paint(c.rect(sx - 2, -32, 48, 32), (70, 70, 74))
    shutter = c.rect(sx, -30, 44, 30)
    c.gradient(shutter, (170, 170, 176), (120, 120, 126), -30, 0)
    for y in range(-28, 0, 3):
        c.paint(c.rect(sx, y, 44, 0.7), (90, 90, 96))
    c.paint(c.circle(sx + 30, -14, 3.5), (100, 100, 106), alpha=0.7)  # Dent.
    paint_c = [(255, 70, 180), (70, 220, 255), (150, 255, 70), (255, 220, 40)]
    tagc = paint_c[seed % 4]
    pts = [(sx + 5, -10)]
    for i in range(6):
        pts.append((sx + 9 + i * 6, -18 + (i % 2) * 10 + rng.uniform(-2, 2)))
    c.paint(c.polyline(pts, 2.2), darker(tagc, 0.5))
    c.paint(c.polyline(pts, 1.4), tagc)
    if tag:
        tx = w * 0.79
        c.paint(c.text(tag, tx + 1, -39, 17), (40, 0, 0))
        c.paint(c.text(tag, tx, -40, 17), (240, 30, 30))
        for dx in (-12, -1, 9):
            c.paint(c.rect(tx + dx, -34, 1.2, rng.uniform(3, 7)), (220, 20, 20))  # Drips.
    dx = 8 if sx > 30 else w - 22
    c.shade(c.rect(dx, -26, 14, 26), (80, 50, 30), k=0.8)
    c.paint(c.circle(dx + 11, -13, 0.8), (219, 182, 0))
    save(c, GROUP, name, f"blackknight/{name}.png", outline=None, trim=False)


CITY_BUILDINGS = [
    # name, width, height, wall color, brick?
    ("cityBuilding0", 140, 106, (150, 74, 40), True),
    ("cityBuilding1", 150, 86, (110, 70, 70), True),
    ("cityBuilding2", 130, 116, (112, 112, 112), False),
    ("cityBuilding3", 140, 94, (125, 60, 50), True),
    ("cityBuilding4", 150, 108, (170, 140, 90), True),
    ("cityBuilding5", 140, 76, (100, 112, 100), False),
]


def city_street():
    c = Canvas(300, 74)
    rng = random.Random(23)
    walk = c.rect(0, 0, 300, 12)
    c.gradient(walk, (165, 165, 165), (140, 140, 140), 0, 12)
    for x in range(0, 300, 50):
        c.paint(c.rect(x, 0, 1, 12), (100, 100, 100))
    for _ in range(6):  # Cracks and weeds in the sidewalk.
        x = rng.uniform(5, 290)
        c.paint(c.polyline([(x, 0), (x + 3, 4), (x + 1, 8), (x + 5, 12)], 0.6), (90, 90, 90))
    for _ in range(4):
        x = rng.uniform(5, 290)
        for k in range(3):
            c.paint(c.line((x, 12), (x + (k - 1) * 2, 8), 0.8), (60, 140, 40))
    c.shade(c.rect(0, 12, 300, 3), (190, 190, 190), k=0.5)
    road = c.rect(0, 15, 300, 59)
    c.gradient(road, (80, 80, 84), (36, 36, 40), 15, 74)
    for _ in range(500):
        x, y = rng.uniform(0, 300), rng.uniform(16, 74)
        c.paint(c.rect(x, y, 0.6, 0.6), (110, 110, 110) if rng.random() < 0.5 else (30, 30, 30), alpha=0.8)
    for _ in range(4):  # Patched asphalt.
        x, y = rng.uniform(0, 280), rng.uniform(20, 66)
        c.paint(c.rect(x, y, rng.uniform(12, 26), rng.uniform(4, 8)), (44, 44, 48), alpha=0.8)
    for x in (0, 50, 150, 200):  # Faded center line: two dashes, then a gap.
        c.paint(c.rect(x + 4, 60, 22, 3), (170, 160, 80), alpha=0.85)
    for x in (40, 180):
        pts = [(x, 18)]
        for i in range(5):
            pts.append((pts[-1][0] + 8, pts[-1][1] + (5 if i % 2 == 0 else -3)))
        c.paint(c.polyline(pts, 0.7), (10, 10, 10))
    man = c.ellipse(240, 42, 12, 4)
    c.shade(man, (70, 70, 74), k=0.6)
    for x in range(232, 250, 3):
        c.paint(man & c.rect(x, 38, 0.6, 8), (40, 40, 44))
    save(c, GROUP, "cityStreet", "blackknight/city_street.png", dither=True, trim=False, opaque=True)


def city_props():
    rng = random.Random(29)
    c = Canvas(48, 14, ax=24, ay=7)
    c.shade(c.ellipse(0, 0, 19, 6), (110, 110, 110), k=0.6)
    c.paint(c.ellipse(0, 0, 17, 5), (10, 10, 16))
    c.paint(c.ellipse(-4, -1, 7, 1.4), (60, 60, 110))
    save(c, GROUP, "cityPothole", "blackknight/city_pothole.png", outline=None)

    c = Canvas(44, 40, ax=20, ay=36)
    c.shade(c.ellipse(15, -4, 7, 5) | c.ellipse(19, -7, 4, 3), (36, 36, 40), hi=(90, 90, 100))  # Trash bag.
    c.paint(c.rect(-17, -3, 6, 3), (230, 230, 190))
    can = c.rrect(-10, -24, 20, 24, 3)
    c.shade(can, (130, 130, 136), k=1.6)
    for x in (-5, 0, 5):
        c.paint(can & c.rect(x, -22, 1, 20), (90, 90, 96))
    c.paint(c.circle(4, -12, 3), (95, 95, 100), alpha=0.8)  # Dent.
    c.shade(c.capsule((-13, -25), (10, -31), 1.8), (150, 150, 156), k=0.6)
    c.shade(c.capsule((-2, -30), (1, -31), 1.2), (90, 90, 96), k=0.4)
    save(c, GROUP, "cityTrashCan", "blackknight/city_trashcan.png", outline=INK)

    for frame in range(3):
        c = Canvas(30, 52, ax=15, ay=50)
        r2 = random.Random(31 + frame)
        for i in (-1, 0, 1):
            h = 12 + r2.uniform(0, 8)
            sway = r2.uniform(-2, 2)
            fl = c.poly([(i * 5 - 5, -22), (i * 5 + 5, -22), (i * 5 + sway, -22 - h)])
            c.paint(fl, (255, 120, 0))
            c.paint(c.poly([(i * 5 - 2.5, -22), (i * 5 + 2.5, -22), (i * 5 + sway * 0.6, -22 - h * 0.6)]), (255, 230, 60))
        barrel = c.rrect(-10, -24, 20, 24, 3)
        c.shade(barrel, (150, 76, 36), k=1.6)
        for y in (-18, -7):
            c.paint(barrel & c.rect(-11, y, 22, 1.4), (80, 36, 10))
        c.paint(c.rect(-3, -14, 4, 4), (36, 6, 0))
        c.paint(c.rect(-10, -24, 20, 2), (255, 150, 40))
        save(c, GROUP, f"cityFireBarrel{frame}", f"blackknight/city_firebarrel_{frame}.png", outline=INK)

    for lit in (True, False):
        c = Canvas(36, 90, ax=6, ay=86)
        c.shade(c.rect(-1.5, -82, 3, 82), (40, 40, 46), k=0.5)
        c.shade(c.rect(-3, -6, 6, 6), (50, 50, 56), k=0.5)
        c.paint(c.polyline([(0, -82), (6, -86), (14, -85)], 2.4), (40, 40, 46))
        head = c.poly([(9, -86), (21, -86), (19, -81), (11, -81)])
        c.shade(head, (60, 60, 66), k=0.5)
        c.paint(c.rect(11, -81.5, 8, 2), (255, 255, 190) if lit else (110, 110, 80))
        save(c, GROUP, "cityLamp" + ("On" if lit else "Off"), f"blackknight/city_lamp_{'on' if lit else 'off'}.png", outline=INK)
    c = Canvas(40, 90, ax=26, ay=86)
    c.shade(c.rect(-1.5, -82, 3, 82), (40, 40, 46), k=0.5)
    c.shade(c.rect(-3, -6, 6, 6), (50, 50, 56), k=0.5)
    c.paint(c.polyline([(0, -82), (-8, -76), (-14, -68)], 2.4), (40, 40, 46))
    c.shade(c.poly([(-18, -70), (-10, -66), (-12, -61), (-20, -64)]), (70, 70, 76), k=0.5)
    save(c, GROUP, "cityLampBroken", "blackknight/city_lamp_broken.png", outline=INK)


def generate():
    ship_sky(); ship_mast(); ship_railing(); ship_deck(); ship_props()
    city_sky(); city_street(); city_props()
    for i, (name, w, h, wall, brick) in enumerate(CITY_BUILDINGS):
        building(name, w, h, wall, brick, seed=40 + i)
    building("cityBuildingBrock", 180, 110, (140, 70, 50), True, seed=47, tag="BROCK")
