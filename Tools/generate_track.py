"""
Generates the circuit: Source/TrackData.h (walls, centerline, surfaces). The art (Assets/track.png) is phase 3.
Both come from the same control points, so the walls always match the picture.
Run:  python Tools/generate_track.py     (needs Pillow and numpy)
"""
import math, os, random
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
W, H = 1000, 700
S = 2                      # supersampling for the art
ROAD_HALF = 55             # half width of the asphalt
CURB = 9                   # curb width
WALL_OFFSET = 100          # distance from the centerline to the walls
SPACING = 9.0              # distance between centerline points (px)

# Control points of the centerline, CLOCKWISE on screen. The start line is at the first one.
CONTROL = [(430, 150), (620, 146), (780, 190), (868, 290), (858, 410), (780, 510), (650, 552), (500, 562),
           (350, 560), (235, 538), (140, 478), (105, 380), (130, 275), (210, 200), (320, 160)]


def catmull(points, per=40):
    n = len(points)
    out = []
    for i in range(n):
        p0, p1, p2, p3 = (np.array(points[(i + k) % n], float) for k in (-1, 0, 1, 2))
        # centripetal parametrisation
        def tj(ti, a, b): return ti + max(np.linalg.norm(b - a), 1e-6) ** 0.5
        t0 = 0.0; t1 = tj(t0, p0, p1); t2 = tj(t1, p1, p2); t3 = tj(t2, p2, p3)
        for t in np.linspace(t1, t2, per, endpoint=False):
            a1 = (t1 - t) / (t1 - t0) * p0 + (t - t0) / (t1 - t0) * p1
            a2 = (t2 - t) / (t2 - t1) * p1 + (t - t1) / (t2 - t1) * p2
            a3 = (t3 - t) / (t3 - t2) * p2 + (t - t2) / (t3 - t2) * p3
            b1 = (t2 - t) / (t2 - t0) * a1 + (t - t0) / (t2 - t0) * a2
            b2 = (t3 - t) / (t3 - t1) * a2 + (t - t1) / (t3 - t1) * a3
            out.append((t2 - t) / (t2 - t1) * b1 + (t - t1) / (t2 - t1) * b2)
    return np.array(out)


def resample(poly, spacing):
    closed = np.vstack([poly, poly[:1]])
    seg = np.linalg.norm(np.diff(closed, axis=0), axis=1)
    cum = np.concatenate([[0], np.cumsum(seg)])
    n = int(round(cum[-1] / spacing))
    ts = np.linspace(0, cum[-1], n, endpoint=False)
    return np.stack([np.interp(ts, cum, closed[:, 0]), np.interp(ts, cum, closed[:, 1])], axis=1)


def smooth(poly, k=3):
    for _ in range(k):
        poly = (np.roll(poly, 1, 0) + 2 * poly + np.roll(poly, -1, 0)) / 4
    return poly


center = resample(smooth(resample(catmull(CONTROL), 4.0), 6), SPACING)
N = len(center)
tang = np.roll(center, -1, 0) - np.roll(center, 1, 0)
tang /= np.linalg.norm(tang, axis=1)[:, None]
normal_left = np.stack([tang[:, 1], -tang[:, 0]], axis=1)  # visual left of the travel direction (y down)


def area(p):  # shoelace; negative = counter-clockwise on screen
    return float(np.sum(p[:, 0] * np.roll(p[:, 1], -1) - np.roll(p[:, 0], -1) * p[:, 1]) / 2)


# Clockwise centerline: the left side is the OUTSIDE of the loop
if area(center) < 0:
    raise SystemExit("control points must be clockwise on screen")
outer = center + normal_left * WALL_OFFSET
inner = center - normal_left * WALL_OFFSET
outer = outer[::-1]          # outer wall: counter-clockwise on screen (the track is on its LEFT)
assert area(outer) < 0 and area(inner) > 0

# sanity: walls must not come closer than the road width to the opposite part of the centerline
def min_gap(poly):
    d = np.linalg.norm(poly[:, None, :] - center[None, :, :], axis=2)
    return d.min()
print("points:", N, "length px:", int(N * SPACING), "outer min dist", round(min_gap(outer)), "inner min dist", round(min_gap(inner)))
dd = np.linalg.norm(outer[:, None, :] - center[None, :, :], axis=2).min(axis=1)
print("outer tight at", [(int(outer[i][0]), int(outer[i][1]), round(dd[i])) for i in np.argsort(dd)[:4]])
dd = np.linalg.norm(inner[:, None, :] - center[None, :, :], axis=2).min(axis=1)
print("inner tight at", [(int(inner[i][0]), int(inner[i][1]), round(dd[i])) for i in np.argsort(dd)[:4]])
assert min_gap(outer) > ROAD_HALF + CURB + 20 and min_gap(inner) > ROAD_HALF + CURB + 20, "track too tight: walls overlap the road"

def where_intersects(poly):
    n = len(poly)
    def ccw(a, b, c): return (c[1] - a[1]) * (b[0] - a[0]) > (b[1] - a[1]) * (c[0] - a[0])
    for i in range(n):
        a, b = poly[i], poly[(i + 1) % n]
        for j in range(i + 2, n):
            if i == 0 and j == n - 1:
                continue
            c, e = poly[j], poly[(j + 1) % n]
            if ccw(a, c, e) != ccw(b, c, e) and ccw(a, b, c) != ccw(a, b, e):
                return (i, j, [int(v) for v in a], [int(v) for v in c])
    return None

print("outer cross:", where_intersects(outer), " inner cross:", where_intersects(inner))
assert where_intersects(outer) is None and where_intersects(inner) is None, "a wall crosses itself (corner too tight)"
for name, poly in (("outer", outer), ("inner", inner)):
    assert poly[:, 0].min() > 3 and poly[:, 0].max() < W - 3 and poly[:, 1].min() > 3 and poly[:, 1].max() < H - 3, name + " wall leaves the window"

# ---------------- special surfaces ----------------
def at(i, lateral=0.0):
    i %= N
    return center[i] + normal_left[i] * lateral

SURFACES = []  # (type, x, y, radius)  type: 2 = sand, 3 = ice
SURFACES += [(2, *at(int(N * 0.20), 82), 36), (2, *at(int(N * 0.55), -82), 36), (2, *at(int(N * 0.83), 82), 36)]
SURFACES += [(3, *at(int(N * 0.12), 10), 30), (3, *at(int(N * 0.40), -12), 30), (3, *at(int(N * 0.70), 10), 30)]

CHECKPOINTS = [0, int(N * 1 / 6), int(N * 2 / 6), int(N * 3 / 6), int(N * 4 / 6), int(N * 5 / 6)]

# ---------------- barrels (loose obstacles, friction demo) ----------------
random.seed(11)
BARRELS = []
for frac, lat in [(0.05, 78), (0.17, -80), (0.31, 80), (0.36, -78), (0.62, 78), (0.80, -80), (0.92, 80)]:
    for k in range(3):
        p = at(int(N * frac) + k * 2, lat + random.uniform(-6, 6))
        BARRELS.append((int(p[0]), int(p[1])))

# ---------------- TrackData.h ----------------
def arr(name, pts):
    flat = ", ".join("%d, %d" % (round(x), round(y)) for x, y in pts)
    return "static const int %s[%d] = { %s };\n" % (name, len(pts) * 2, flat)

with open(os.path.join(ROOT, "Source", "TrackData.h"), "w") as f:
    f.write("#pragma once\n\n// GENERATED by Tools/generate_track.py: do not edit by hand.\n\n")
    f.write("#define TRACK_ROAD_HALF_WIDTH %d\n#define TRACK_WALL_OFFSET %d\n#define TRACK_SPACING %.1ff\n" % (ROAD_HALF, WALL_OFFSET, SPACING))
    f.write("#define TRACK_POINTS %d\n#define TRACK_OUTER_POINTS %d\n#define TRACK_INNER_POINTS %d\n\n" % (N, len(outer), len(inner)))
    f.write("// Centerline, clockwise on screen (x, y). The start / finish line is at index 0\n")
    f.write(arr("TRACK_CENTER", [(round(x), round(y)) for x, y in center]))
    f.write("\n// Walls. The outer one is counter-clockwise and the inner one clockwise ON SCREEN: with a one sided chain\n// that makes both of them collide from the track side\n")
    f.write(arr("TRACK_OUTER_WALL", outer))
    f.write(arr("TRACK_INNER_WALL", inner))
    f.write("\n// Special surfaces: type (2 = sand, 3 = ice), x, y, radius\n#define TRACK_SURFACE_COUNT %d\n" % len(SURFACES))
    f.write("static const int TRACK_SURFACES[%d] = { %s };\n" % (len(SURFACES) * 4, ", ".join("%d, %d, %d, %d" % (t, round(x), round(y), r) for t, x, y, r in SURFACES)))
    f.write("\n// Centerline indices of the checkpoints. The first one is the start / finish line\n#define TRACK_CHECKPOINT_COUNT %d\n" % len(CHECKPOINTS))
    f.write("static const int TRACK_CHECKPOINTS[%d] = { %s };\n" % (len(CHECKPOINTS), ", ".join(str(c) for c in CHECKPOINTS)))
    f.write("\n// Loose barrels (x, y)\n#define TRACK_BARREL_COUNT %d\n" % len(BARRELS))
    f.write("static const int TRACK_BARRELS[%d] = { %s };\n" % (len(BARRELS) * 2, ", ".join("%d, %d" % b for b in BARRELS)))
print("TrackData.h written")

print("done (the art comes in phase 3)")
