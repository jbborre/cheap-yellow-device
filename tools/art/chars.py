"""Black Knight fighters: heroes, pirates, thugs and the two bosses.

Every figure is built on one jointed skeleton (side view, facing right, feet
on y=0) so each pose is a real frame rather than a squashed shape.
"""
import math

import numpy as np

from draw import Canvas, ang, darker, lighter, mix, save

INK = (20, 10, 26)
GROUP = "BlackKnightArt"

# lean: torso tilt forward (deg). Legs: (hip angle, knee bend). Arms: (shoulder angle, elbow bend).
# Angles are measured from straight down; positive points forward.
POSES = {
    "idle":   dict(lean=3,   fl=(12, 14),  bl=(-10, 6),  fa=(25, 70),  ba=(-5, 60),  weapon=62),
    "walk1":  dict(lean=5,   fl=(28, 18),  bl=(-24, 28), fa=(-18, 35), ba=(28, 45),  weapon=40),
    "walk2":  dict(lean=5,   fl=(-22, 28), bl=(26, 16),  fa=(28, 45),  ba=(-18, 35), weapon=75),
    "punch":  dict(lean=10,  fl=(24, 10),  bl=(-28, 6),  fa=(88, 4),   ba=(15, 115), weapon=92, grip=0),
    "kick":   dict(lean=-12, fl=(100, 8),  bl=(-4, 6),   fa=(50, 60),  ba=(-35, 50), weapon=125),
    "windup": dict(lean=-6,  fl=(16, 12),  bl=(-16, 8),  fa=(165, 25), ba=(20, 70),  weapon=205),
    "raise":  dict(lean=-4,  fl=(14, 10),  bl=(-14, 8),  fa=(176, 6),  ba=(150, 30), weapon=182, grip=0),
    "hurt":   dict(lean=-16, fl=(20, 22),  bl=(-6, 14),  fa=(-40, 30), ba=(-70, 20), weapon=15, head=(-2, 1)),
    "dash":   dict(lean=24,  fl=(40, 25),  bl=(-42, 35), fa=(85, 10),  ba=(-35, 70), weapon=90, head=(1, 2)),
}


class Look:
    def __init__(self, leg, boot, torso, arm, hand, head, fist=1.0, sleeve=None):
        self.leg, self.boot, self.torso, self.arm, self.hand, self.head = leg, boot, torso, arm, hand, head
        self.fist = fist
        self.sleeve = sleeve  # Upper-arm color when it differs from the forearm.


def skeleton(pose, s):
    P = POSES[pose]
    lean = math.radians(P["lean"])
    up = np.array([math.sin(lean), -math.cos(lean)])
    right = np.array([math.cos(lean), math.sin(lean)])
    hip = np.array([0.0, -24 * s])
    J = {"up": up, "right": right, "hip": hip}
    J["shoulder"] = hip + up * 18 * s
    hx, hy = P.get("head", (0, 0))
    J["head"] = J["shoulder"] + up * 9 * s + right * 1 * s + np.array([hx, hy]) * s
    for side, key, off in (("f", "fl", 1), ("b", "bl", -1)):
        a, k = P[key]
        h = hip + np.array([off * 1.0 * s, 0])
        knee = h + 12 * s * ang(a)
        foot = knee + 12 * s * ang(a - k)
        J[side + "hip"], J[side + "knee"], J[side + "foot"], J[side + "shin"] = h, knee, foot, ang(a - k)
    for side, key, off in (("f", "fa", 2), ("b", "ba", -3)):
        a, e = P[key]
        sh = J["shoulder"] + right * off * s - up * 3 * s
        elbow = sh + 9.5 * s * ang(a)
        hand = elbow + 9 * s * ang(a + e)
        J[side + "sh"], J[side + "elbow"], J[side + "hand"], J[side + "fore"] = sh, elbow, hand, ang(a + e)
    # Plant the lowest foot on the floor.
    drop = -max(J["ffoot"][1], J["bfoot"][1])
    for k, v in list(J.items()):
        if k in ("up", "right", "fshin", "bshin", "ffore", "bfore"):
            continue
        J[k] = v + np.array([0, drop])
    # A held weapon sits at a fixed angle to the forearm, like a real grip:
    # about 70 degrees further forward/up, or straight along it for a thrust.
    a, e = P["fa"]
    J["weapon"] = ang(a + e + P.get("grip", 70))
    return J


def perp(v):
    return np.array([v[1], -v[0]])  # Rotates a "down" vector to "forward".


def leg(c, J, side, s, look, dim):
    legc = darker(look.leg, 0.3) if dim else look.leg
    boot = darker(look.boot, 0.3) if dim else look.boot
    c.shade(c.capsule(J[side + "hip"], J[side + "knee"], 3.3 * s) | c.capsule(J[side + "knee"], J[side + "foot"], 2.9 * s), legc)
    foot, toe = J[side + "foot"], perp(J[side + "shin"])
    c.shade(c.capsule(foot - toe * 1.0 * s, foot + toe * 4.2 * s, 2.5 * s), boot)


def arm(c, J, side, s, look, dim):
    armc = darker(look.arm, 0.3) if dim else look.arm
    upper = look.sleeve if look.sleeve else look.arm
    upper = darker(upper, 0.3) if dim else upper
    c.shade(c.capsule(J[side + "sh"], J[side + "elbow"], 2.8 * s), upper)
    c.shade(c.capsule(J[side + "elbow"], J[side + "hand"], 2.5 * s), armc)


def fist(c, J, side, s, look, dim):
    hand = darker(look.hand, 0.3) if dim else look.hand
    c.shade(c.circle(*J[side + "hand"], 2.9 * s * look.fist), hand)


def torso_mask(c, J, s):
    up, right, hip, sh = J["up"], J["right"], J["hip"], J["shoulder"]
    pts = [sh - right * 7 * s, sh + right * 7 * s, hip + right * 6 * s - up * 2 * s, hip - right * 6 * s - up * 2 * s]
    return c.poly([tuple(p) for p in pts]) | c.capsule(sh - right * 5 * s, sh + right * 5 * s, 2.6 * s)


def figure(c, pose, s, look, back=None, mid=None, weapon=None, front=None):
    J = skeleton(pose, s)
    if back:
        back(c, J, s)
    arm(c, J, "b", s, look, True)
    fist(c, J, "b", s, look, True)
    leg(c, J, "b", s, look, True)
    leg(c, J, "f", s, look, False)
    t = torso_mask(c, J, s)
    c.shade(t, look.torso)
    J["torso"] = t
    J["headmask"] = c.circle(*J["head"], 7 * s)
    c.shade(J["headmask"], look.head)
    if mid:
        mid(c, J, s)
    arm(c, J, "f", s, look, False)
    if weapon:
        weapon(c, J, s)
    fist(c, J, "f", s, look, False)
    if front:
        front(c, J, s)
    return J


def eye(c, J, s, color=INK, dx=3.2, dy=-0.6, r=1.0):
    c.paint(c.circle(J["head"][0] + dx * s, J["head"][1] + dy * s, r * s), color)


def blade(c, J, s, length, width, steel=(220, 220, 240), curve=0.0, guard=(219, 182, 0)):
    """A sword gripped in the front fist: pommel behind, guard and blade in front."""
    h, d = J["fhand"], J["weapon"]
    n = perp(d)
    c.shade(c.capsule(h - d * 3.4 * s, h + d * 2.4 * s, 1.1 * s), (90, 50, 20), k=0.5)  # Leather grip.
    c.shade(c.circle(*(h - d * 3.8 * s), 1.4 * s), guard or (150, 150, 160), k=0.5)    # Pommel.
    base = h + d * 2.8 * s
    pts_a, pts_b = [], []
    for i in range(9):
        t = i / 8
        bend = curve * math.sin(t * math.pi * 0.9) * length
        w = width * (1 - 0.75 * t ** 2)
        p = base + d * length * t + n * bend
        pts_a.append(tuple(p + n * w / 2)); pts_b.append(tuple(p - n * w / 2))
    m = c.poly(pts_a + pts_b[::-1])
    c.shade(m, steel, hi=(255, 255, 255), lo=darker(steel, 0.35), k=0.6)
    c.paint(c.line(tuple(base + d * length * 0.12), tuple(base + d * length * 0.8 + n * curve * length * 0.6), 0.5 * s), lighter(steel, 0.6))
    if guard:
        c.shade(c.capsule(base + n * 3.6 * s, base - n * 3.6 * s, 1.0 * s), guard)


# ---- Heroes ---------------------------------------------------------------
def knight(c, pose, s=1.0):
    look = Look((44, 44, 52), (20, 20, 24), (48, 48, 58), (74, 74, 86), (120, 120, 132), (52, 52, 62))

    def mid(c, J, s):
        up, right, hip, sh = J["up"], J["right"], J["hip"], J["shoulder"]
        # Breastplate shine and a gold belt.
        c.paint(c.poly([tuple(sh + right * 4 * s + up * -2 * s), tuple(sh + right * 7 * s - up * 3 * s),
                        tuple(sh + right * 5 * s - up * 12 * s), tuple(sh + right * 2 * s - up * 9 * s)]), (96, 96, 112))
        c.shade(c.capsule(hip + up * 1.5 * s - right * 6 * s, hip + up * 1.5 * s + right * 6 * s, 1.3 * s), (219, 182, 0))
        # Helmet: crest ridge, glowing visor slit and a flowing plume.
        hd = J["head"]
        c.paint(c.rect(hd[0] + 0.5 * s, hd[1] - 1.4 * s, 6.5 * s, 2.0 * s), (255, 60, 0))
        c.paint(c.rect(hd[0] + 1.5 * s, hd[1] - 0.9 * s, 4.5 * s, 0.9 * s), (255, 200, 80))
        c.paint(c.capsule(hd + np.array([-3 * s, -6.5 * s]), hd + np.array([3 * s, -6.5 * s]), 1.0 * s), (100, 100, 116))
        wave = 1.5 if pose.startswith("walk") else 0.5
        plume = [(hd[0] - 1 * s, hd[1] - 7.5 * s), (hd[0] - 9 * s, hd[1] - 6 * s), (hd[0] - 15 * s, hd[1] - 1 * s + wave * s),
                 (hd[0] - 11 * s, hd[1] - 2 * s), (hd[0] - 4 * s, hd[1] - 3 * s)]
        c.shade(c.poly(plume), (160, 0, 0), hi=(230, 40, 40))

    def weapon(c, J, s):
        blade(c, J, s, 24 * s, 3.0 * s)
        if pose == "punch":
            for i in range(3):
                pts = [tuple(J["fhand"] + np.array([6 + i * 3, -14 + i * 5]) * s), tuple(J["fhand"] + np.array([20 + i * 3, -8 + i * 5]) * s),
                       tuple(J["fhand"] + np.array([30, -2 + i * 4]) * s)]
                c.paint(c.polyline(pts, 0.7 * s), (180, 200, 255), alpha=0.9)

    def front(c, J, s):
        c.shade(c.circle(*(J["fsh"] + J["up"] * 1 * s), 4.8 * s), (82, 82, 96))  # Pauldron.

    return figure(c, pose, s, look, mid=mid, weapon=weapon, front=front)


def eclipse(c, pose, s=1.0):
    purple, deep = (120, 40, 180), (76, 10, 116)
    look = Look(deep, (40, 0, 76), purple, purple, (150, 80, 220), purple)
    gold = (255, 219, 60)

    def star(c, p, r, color=(255, 255, 170)):
        x, y = p
        c.paint(c.poly([(x, y - r), (x + r * 0.3, y - r * 0.3), (x + r, y), (x + r * 0.3, y + r * 0.3),
                        (x, y + r), (x - r * 0.3, y + r * 0.3), (x - r, y), (x - r * 0.3, y - r * 0.3)]), color)

    def back(c, J, s):
        sh, right = J["shoulder"], J["right"]
        flow = 3 if pose.startswith("walk") or pose == "kick" else 0
        pts = [tuple(sh + right * 2 * s), tuple(sh - right * 5 * s), (sh[0] - 16 * s - flow * s, -10 * s),
               (sh[0] - 19 * s - flow * s, -2 * s), (sh[0] - 12 * s, -5 * s), (sh[0] - 7 * s, -1 * s), (sh[0] + 1 * s, -6 * s)]
        cape = c.poly(pts)
        c.shade(cape, (40, 0, 80), hi=(90, 30, 140), lo=(20, 0, 40))
        for i in range(4):
            star(c, (sh[0] - (6 + i * 3) * s, sh[1] + (8 + i * 8) * s), 1.6 * s)

    def mid(c, J, s):
        up, right, sh = J["up"], J["right"], J["shoulder"]
        for dx, dy in ((-3, 8), (4, 14), (-2, 20), (3, 3)):
            star(c, sh + right * dx * s - up * dy * s, 1.3 * s)
        # Crescent moon on the chest.
        cc = sh + right * 1 * s - up * 9 * s
        moon = c.circle(*cc, 3.2 * s) & ~c.circle(*(cc + right * 1.6 * s - up * 0.6 * s), 2.8 * s)
        c.paint(moon, gold)
        hd = J["head"]
        c.paint(c.rect(hd[0] + 1 * s, hd[1] - 2 * s, 5 * s, 1.8 * s), (255, 255, 255))  # Mask slit.
        star(c, (hd[0] - 3 * s, hd[1] - 4 * s), 1.4 * s)

    return figure(c, pose, s, look, back=back, mid=mid)


def spade(c, pose, s=1.0):
    gi, skin, red = (40, 40, 96), (255, 210, 165), (220, 20, 20)
    look = Look(gi, (36, 36, 40), gi, gi, skin, skin)

    def back(c, J, s):
        hd = J["head"]
        c.shade(c.capsule(hd + np.array([-5, -3]) * s, hd + np.array([-12, 5]) * s, 2.0 * s), (40, 10, 10))  # Ponytail.

    def mid(c, J, s):
        up, right, hip, sh, hd = J["up"], J["right"], J["hip"], J["shoulder"], J["head"]
        hair = J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 8 * s, 16 * s, 5.5 * s)
        c.shade(hair, (40, 10, 10))
        c.paint(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 3.2 * s, 16 * s, 2.0 * s), red)  # Headband.
        flap = 2 if pose.startswith("walk") else 0.8
        for dy, spread in ((-2.6, -2), (-1.6, 2)):
            c.paint(c.polyline([(hd[0] - 6.5 * s, hd[1] + dy * s), (hd[0] - 11 * s, hd[1] + (dy + spread * 0.5 - flap * 0.3) * s),
                                (hd[0] - 15 * s, hd[1] + (dy + spread + flap) * s)], 1.2 * s), red)
        c.shade(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] + 1 * s, 16 * s, 8 * s), gi)  # Face mask.
        c.paint(c.rect(hd[0] + 2 * s, hd[1] - 1 * s, 2.4 * s, 1.4 * s), (10, 10, 10))
        c.shade(c.capsule(hip + up * 1.5 * s - right * 6 * s, hip + up * 1.5 * s + right * 6 * s, 1.2 * s), red)
        c.paint(c.capsule(hip + up * 1 * s + right * 2 * s, hip - up * 4 * s + right * 4 * s, 0.8 * s), red)  # Belt tail.
        cc = sh - up * 9 * s
        x, y = cc
        spade_m = (c.poly([(x, y - 4 * s), (x - 4 * s, y + 1 * s), (x + 4 * s, y + 1 * s)]) | c.circle(x - 2 * s, y + 1 * s, 2 * s)
                   | c.circle(x + 2 * s, y + 1 * s, 2 * s) | c.poly([(x, y + 1 * s), (x - 1.5 * s, y + 5 * s), (x + 1.5 * s, y + 5 * s)]))
        c.paint(spade_m, (255, 255, 255))

    return figure(c, pose, s, look, back=back, mid=mid)


# ---- Shipwreck crew ---------------------------------------------------------
def pirate(c, pose, s=1.0):
    skin = (219, 150, 90)
    look = Look((40, 40, 44), (90, 50, 20), (240, 240, 240), (240, 240, 240), skin, skin)

    def mid(c, J, s):
        up, t, hd = J["up"], J["torso"], J["head"]
        for i in range(6):
            y = J["shoulder"][1] + (3 + i * 3.2) * s
            c.paint(t & c.rect(-20 * s, y, 40 * s, 1.4 * s), (210, 20, 20))
        c.shade(c.capsule(J["hip"] + up * 1.5 * s - J["right"] * 6 * s, J["hip"] + up * 1.5 * s + J["right"] * 6 * s, 1.2 * s), (60, 30, 10))
        c.shade(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 8 * s, 16 * s, 5 * s), (210, 20, 20))
        c.paint(c.poly([(hd[0] - 6 * s, hd[1] - 5 * s), (hd[0] - 12 * s, hd[1]), (hd[0] - 8 * s, hd[1] + 2 * s)]), (190, 10, 10))
        c.paint(c.line((hd[0] - 6 * s, hd[1] - 3 * s), (hd[0] + 6 * s, hd[1] + 1 * s), 0.7 * s), INK)  # Patch strap.
        c.paint(c.circle(hd[0] + 3.2 * s, hd[1] - 0.2 * s, 1.8 * s), INK)
        c.paint(J["headmask"] & c.rect(hd[0] - 3 * s, hd[1] + 3 * s, 10 * s, 4 * s), darker(skin, 0.25))  # Stubble.
        c.paint(c.circle(hd[0] - 1 * s, hd[1] + 2.5 * s, 0.9 * s), (255, 210, 0))  # Earring.

    def weapon(c, J, s):
        blade(c, J, s, 18 * s, 3.2 * s, curve=-0.12)

    return figure(c, pose, s, look, mid=mid, weapon=weapon)


def brute(c, pose, s=1.25):
    skin = (219, 150, 90)
    look = Look((80, 46, 10), (36, 36, 36), skin, skin, skin, skin, fist=1.25)

    def mid(c, J, s):
        up, right, t, hd = J["up"], J["right"], J["torso"], J["head"]
        for side in (-1, 1):
            panel = t & c.poly([tuple(J["shoulder"] + right * side * 2.5 * s + up * 2 * s), tuple(J["shoulder"] + right * side * 9 * s + up * 2 * s),
                                tuple(J["hip"] + right * side * 8 * s), tuple(J["hip"] + right * side * 3 * s)])
            c.shade(panel, (110, 70, 20))
        c.shade(c.capsule(J["hip"] + up * 1.5 * s - right * 6 * s, J["hip"] + up * 1.5 * s + right * 6 * s, 1.4 * s), (50, 30, 10))
        c.paint(c.circle(hd[0] - 2 * s, hd[1] - 4 * s, 1.6 * s), lighter(skin, 0.5))  # Shiny head.
        beard = c.circle(hd[0] + 2 * s, hd[1] + 5 * s, 5 * s) | c.circle(hd[0] - 1 * s, hd[1] + 4 * s, 4 * s)
        c.shade(beard, (50, 36, 10))
        eye(c, J, s, dy=-1.5)
        c.paint(c.line((hd[0] + 1 * s, hd[1] - 4 * s), (hd[0] + 6 * s, hd[1] - 2.5 * s), 0.9 * s), INK)

    return figure(c, pose, s, look, mid=mid)


def captain(c, pose, s=1.55):
    """Davy Jones, the ghost captain of the shipwreck."""
    skin, coat = (150, 215, 185), (36, 80, 80)
    look = Look((36, 36, 40), (20, 20, 20), coat, coat, skin, skin, fist=1.15)
    weed = (40, 140, 60)

    def back(c, J, s):
        hip = J["hip"]
        # Long, ragged coat tails.
        pts = [(hip[0] + 3 * s, hip[1] - 2 * s), (hip[0] - 12 * s, hip[1] + 2 * s), (hip[0] - 16 * s, -2 * s), (hip[0] - 12 * s, -6 * s),
               (hip[0] - 10 * s, -1 * s), (hip[0] - 6 * s, -5 * s), (hip[0] - 3 * s, -2 * s), (hip[0] + 3 * s, -8 * s)]
        c.shade(c.poly(pts), darker(coat, 0.3))
        # Anchor chain swinging from the back hand.
        h = J["bhand"]
        for i in range(5):
            p = h + np.array([-0.6 * i, 2.4 * i]) * s
            c.paint(c.ellipse(p[0], p[1], 1.0 * s, 1.5 * s) & ~c.ellipse(p[0], p[1], 0.4 * s, 0.8 * s), (120, 110, 100))
        a = h + np.array([-3 * s, 13 * s])
        anchor = (c.capsule(a, a + np.array([0, 7 * s]), 0.9 * s) | c.capsule(a + np.array([-3 * s, 1.5 * s]), a + np.array([3 * s, 1.5 * s]), 0.8 * s)
                  | (c.circle(a[0], a[1] + 5 * s, 4 * s) & ~c.circle(a[0], a[1] + 4 * s, 3 * s) & c.rect(a[0] - 5 * s, a[1] + 5 * s, 10 * s, 5 * s)))
        c.shade(anchor, (110, 100, 90))

    def mid(c, J, s):
        up, right, sh, hd, t = J["up"], J["right"], J["shoulder"], J["head"], J["torso"]
        for i in range(3):
            c.shade(c.circle(*(sh + right * 3 * s - up * (5 + i * 6) * s), 1.0 * s), (219, 182, 0))
        for p in ((-4, 10), (-2, 16), (2, 19)):
            c.shade(c.circle(*(sh + right * p[0] * s - up * p[1] * s), 1.3 * s), (170, 170, 160))  # Barnacles.
        for dx in (-5, 5):  # Seaweed draped over the shoulders.
            base = sh + right * dx * s
            c.paint(c.polyline([tuple(base), tuple(base + np.array([1.5, 5]) * s), tuple(base + np.array([-1, 10]) * s),
                                tuple(base + np.array([0.8, 14]) * s)], 1.1 * s), weed)
        # Hollow ghostly face with glowing eyes and a scraggly beard.
        c.paint(c.circle(hd[0] + 3.2 * s, hd[1] - 0.8 * s, 1.9 * s), (20, 50, 50))
        c.paint(c.circle(hd[0] + 3.4 * s, hd[1] - 0.8 * s, 0.9 * s), (160, 255, 255))
        c.paint(c.line((hd[0] + 1 * s, hd[1] + 3.5 * s), (hd[0] + 5 * s, hd[1] + 3 * s), 0.6 * s), (20, 50, 50))
        x, y = hd[0], hd[1]
        beard = c.poly([(x - 3 * s, y + 2 * s), (x + 7 * s, y + 2.5 * s), (x + 6 * s, y + 6 * s), (x + 4 * s, y + 5.5 * s),
                        (x + 3 * s, y + 9 * s), (x + 1 * s, y + 6.5 * s), (x - 1 * s, y + 8 * s), (x - 2 * s, y + 5 * s)])
        c.shade(beard, (200, 210, 200), hi=(240, 250, 240), lo=(140, 150, 140), k=0.8)
        # Battered tricorn hat with a starfish pinned to it.
        hat = c.poly([(hd[0] - 12 * s, hd[1] - 5 * s), (hd[0] + 12 * s, hd[1] - 5 * s), (hd[0] + 2 * s, hd[1] - 16 * s)])
        hat |= c.rrect(hd[0] - 13 * s, hd[1] - 7 * s, 26 * s, 3 * s, 1 * s)
        c.shade(hat, (36, 36, 36), hi=(80, 80, 80))
        c.paint(c.rect(hd[0] - 13 * s, hd[1] - 7 * s, 26 * s, 0.7 * s), (219, 182, 0))
        cx, cy = hd[0] + 1 * s, hd[1] - 10 * s
        pts = []
        for k in range(10):
            r = (3 if k % 2 == 0 else 1.3) * s
            a = -math.pi / 2 + k * math.pi / 5
            pts.append((cx + math.cos(a) * r, cy + math.sin(a) * r))
        c.shade(c.poly(pts), (255, 140, 40))

    def weapon(c, J, s):
        blade(c, J, s, 26 * s, 4.2 * s, steel=(190, 200, 190), curve=-0.1)

    return figure(c, pose, s, look, back=back, mid=mid, weapon=weapon)


# ---- City street gang -------------------------------------------------------
def thug(c, pose, s=1.0):
    skin, hoodie = (219, 150, 110), (76, 112, 76)
    look = Look((40, 76, 150), (240, 240, 240), hoodie, hoodie, skin, skin)

    def mid(c, J, s):
        up, right, hd, t = J["up"], J["right"], J["head"], J["torso"]
        c.shade(c.ellipse(hd[0] - 1 * s, hd[1] + 6.5 * s, 6.5 * s, 2.6 * s), darker(hoodie, 0.25))  # Hood at the neck.
        pocket = t & c.rrect(J["hip"][0] - 4 * s, J["hip"][1] - 8 * s, 9 * s, 5 * s, 1.5 * s)
        c.paint(pocket, darker(hoodie, 0.2))
        for dx in (1.5, 3.5):
            c.paint(c.line((hd[0] + dx * s, hd[1] + 7 * s), (hd[0] + dx * s, hd[1] + 12 * s), 0.5 * s), (235, 235, 235))
        c.shade(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 8 * s, 16 * s, 5.5 * s), (30, 30, 34))  # Beanie.
        c.paint(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 3.4 * s, 16 * s, 1.4 * s), (90, 90, 96))
        eye(c, J, s, dy=-0.4)
        c.paint(c.line((hd[0] + 1.5 * s, hd[1] - 2 * s), (hd[0] + 5 * s, hd[1] - 1.2 * s), 0.7 * s), INK)
        c.paint(J["headmask"] & c.rect(hd[0] - 1 * s, hd[1] + 2.5 * s, 9 * s, 4 * s), darker(skin, 0.2))

    def weapon(c, J, s):
        h, d = J["fhand"], J["weapon"]
        n = perp(d)
        end = h + d * 20 * s
        bar = c.capsule(h - d * 4 * s, end, 1.4 * s)
        hook = c.polyline([tuple(end), tuple(end + d * 2.5 * s - n * 2 * s), tuple(end + d * 1.5 * s - n * 4.5 * s), tuple(end - n * 5 * s)], 2.4 * s)
        c.shade(bar | hook, (150, 150, 170), hi=(230, 230, 250), lo=(80, 80, 96), k=0.6)
        c.paint(c.capsule(h - d * 4 * s, h - d * 1 * s, 1.5 * s), (200, 20, 20))  # Taped grip.

    return figure(c, pose, s, look, mid=mid, weapon=weapon)


def big_thug(c, pose, s=1.3):
    skin = (219, 150, 110)
    look = Look((80, 80, 44), (36, 36, 36), (240, 240, 240), skin, skin, skin, fist=1.3)

    def mid(c, J, s):
        up, right, hd, t = J["up"], J["right"], J["head"], J["torso"]
        for side in (-1, 1):  # Bare shoulders around the muscle shirt.
            c.shade(c.circle(*(J["shoulder"] + right * side * 6 * s), 3 * s) & t, skin)
        c.paint(c.capsule(J["hip"] + up * 1.5 * s - right * 6 * s, J["hip"] + up * 1.5 * s + right * 6 * s, 1.3 * s), (30, 30, 30))
        chain = c.ellipse(hd[0] + 0.5 * s, hd[1] + 9 * s, 4 * s, 2.5 * s) & ~c.ellipse(hd[0] + 0.5 * s, hd[1] + 8.4 * s, 3.2 * s, 1.8 * s)
        c.paint(chain, (240, 200, 40))
        c.paint(J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 8 * s, 16 * s, 3.5 * s), (70, 40, 10))  # Buzz cut.
        eye(c, J, s, dy=-1.2)
        c.paint(c.line((hd[0] + 1 * s, hd[1] - 3.8 * s), (hd[0] + 6 * s, hd[1] - 2.2 * s), 1.0 * s), INK)
        c.paint(J["headmask"] & c.rect(hd[0] - 1 * s, hd[1] + 3 * s, 9 * s, 4 * s), (150, 80, 40))

    def front(c, J, s):
        mid_arm = (J["fsh"] + J["felbow"]) / 2
        c.paint(c.circle(*mid_arm, 1.6 * s) | c.capsule(mid_arm - np.array([1.5, 0]) * s, mid_arm + np.array([1.5, 1]) * s, 0.6 * s), (40, 80, 160))

    return figure(c, pose, s, look, mid=mid, front=front)


def brock(c, pose, s=1.55):
    skin, red = (219, 150, 110), (220, 10, 10)
    look = Look((40, 76, 150), (90, 50, 20), (240, 240, 240), skin, skin, skin, fist=1.3)

    def back(c, J, s):
        hd = J["head"]
        stream = pose == "dash"
        for dy, drop in ((-5, -1), (-4, 3)):
            tip = (hd[0] - 15 * s, hd[1] + (dy - 1) * s) if stream else (hd[0] - 12 * s, hd[1] + (dy + drop + 3) * s)
            c.shade(c.poly([(hd[0] - 5 * s, hd[1] + dy * s - 1 * s), tip, (hd[0] - 5 * s, hd[1] + dy * s + 1.5 * s)]), red)

    def mid(c, J, s):
        up, right, t, hd = J["up"], J["right"], J["torso"], J["head"]
        for side in (-1, 1):  # Black leather vest over the white tank.
            panel = t & c.poly([tuple(J["shoulder"] + right * side * 2 * s + up * 3 * s), tuple(J["shoulder"] + right * side * 9 * s + up * 3 * s),
                                tuple(J["hip"] + right * side * 8 * s), tuple(J["hip"] + right * side * 2.5 * s)])
            c.shade(panel, (36, 36, 40), hi=(90, 90, 100))
        for i in range(3):
            c.paint(c.circle(*(J["shoulder"] + right * 2.6 * s - up * (4 + i * 4) * s), 0.6 * s), (200, 200, 210))  # Studs.
        c.shade(c.capsule(J["hip"] + up * 1.5 * s - right * 6 * s, J["hip"] + up * 1.5 * s + right * 6 * s, 1.3 * s), (80, 46, 10))
        c.shade(c.rrect(J["hip"][0] - 2 * s, J["hip"][1] - 3.5 * s, 4.5 * s, 3.2 * s, 0.8 * s), (230, 190, 30))
        beard = J["headmask"] & c.rect(hd[0] - 4 * s, hd[1] + 1.5 * s, 12 * s, 8 * s)
        c.shade(beard | c.ellipse(hd[0] + 2 * s, hd[1] + 6 * s, 4 * s, 3 * s), (80, 40, 10))
        eye(c, J, s, dy=-1)
        c.paint(c.line((hd[0] + 0.5 * s, hd[1] - 3.4 * s), (hd[0] + 6 * s, hd[1] - 1.6 * s), 1.0 * s), INK)
        # Bandana with white polka dots.
        band = J["headmask"] & c.rect(hd[0] - 8 * s, hd[1] - 8 * s, 16 * s, 5.8 * s)
        c.shade(band, red, hi=(255, 80, 80))
        for dx, dy in ((-4, -5), (0, -6), (4, -5), (-1, -3.5), (3, -3.3)):
            c.paint(c.circle(hd[0] + dx * s, hd[1] + dy * s, 0.6 * s), (255, 255, 255))

    def front(c, J, s):
        h = J["fhand"]
        c.paint(c.capsule(h - J["ffore"] * 2.5 * s - np.array([1.8, 0]) * s, h - J["ffore"] * 2.5 * s + np.array([1.8, 0]) * s, 0.8 * s), (240, 240, 240))

    return figure(c, pose, s, look, back=back, mid=mid, front=front)


ROSTER = [
    # (name, draw function, scale, poses)
    ("Knight", knight, 1.0, ["idle", "walk1", "walk2", "punch", "kick"]),
    ("Eclipse", eclipse, 1.0, ["idle", "walk1", "walk2", "punch", "kick"]),
    ("Spade", spade, 1.0, ["idle", "walk1", "walk2", "punch", "kick"]),
    ("Pirate", pirate, 1.0, ["idle", "walk1", "walk2", "windup", "hurt"]),
    ("Brute", brute, 1.25, ["idle", "walk1", "walk2", "windup", "hurt"]),
    ("Captain", captain, 1.55, ["idle", "walk1", "walk2", "windup", "raise", "hurt"]),
    ("Thug", thug, 1.0, ["idle", "walk1", "walk2", "windup", "hurt"]),
    ("BigThug", big_thug, 1.3, ["idle", "walk1", "walk2", "windup", "hurt"]),
    ("Brock", brock, 1.55, ["idle", "walk1", "walk2", "windup", "dash", "hurt"]),
]


def generate():
    for name, fn, s, poses in ROSTER:
        for pose in poses:
            w, h = int(120 * s), int(110 * s)
            c = Canvas(w, h, ax=int(w * 0.42), ay=int(h * 0.94))
            fn(c, pose, s)
            save(c, GROUP, f"{name[0].lower()}{name[1:]}{pose.capitalize()}", f"blackknight/{name.lower()}_{pose}.png", outline=INK)
