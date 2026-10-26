# demo_idle.py -- Quick test for gint + time in Windows Python IDLE (press F5!)
import gint
import math
import time

VERTS = [
    (-1, -1, -1), (1, -1, -1), (1, 1, -1), (-1, 1, -1),
    (-1, -1,  1), (1, -1,  1), (1, 1,  1), (-1, 1,  1),
]
FACES = [
    (0, 3, 2, 1, gint.C_RGB(28, 8, 8)),
    (4, 5, 6, 7, gint.C_RGB(8, 26, 12)),
    (0, 1, 5, 4, gint.C_RGB(8, 16, 30)),
    (2, 3, 7, 6, gint.C_RGB(30, 24, 6)),
    (1, 2, 6, 5, gint.C_RGB(24, 10, 28)),
    (0, 4, 7, 3, gint.C_RGB(8, 26, 28)),
]

ax = 0.4
ay = 0.6
zoom = 95.0
wire = False
fps = 0
frames = 0
t_fps = time.ticks_ms()

gint.clearevents()
while not gint.keydown(gint.KEY_EXIT):
    while True:
        ev = gint.pollevent()
        if ev.type == gint.KEYEV_NONE:
            break
        if ev.type == gint.KEYEV_DOWN and ev.key == gint.KEY_F1:
            wire = not wire

    if gint.keydown(gint.KEY_LEFT):
        ay -= 0.06
    elif gint.keydown(gint.KEY_RIGHT):
        ay += 0.06
    else:
        ay += 0.02

    if gint.keydown(gint.KEY_UP):
        ax -= 0.06
    elif gint.keydown(gint.KEY_DOWN):
        ax += 0.06
    else:
        ax += 0.013

    if gint.keydown(gint.KEY_ADD):
        zoom = min(200.0, zoom * 1.04)
    if gint.keydown(gint.KEY_SUB):
        zoom = max(30.0, zoom / 1.04)

    cx, sx = math.cos(ax), math.sin(ax)
    cy, sy = math.cos(ay), math.sin(ay)

    pts3d = []
    pts2d = []
    for x, y, z in VERTS:
        x1 = x * cy + z * sy
        z1 = -x * sy + z * cy
        y2 = y * cx - z1 * sx
        z2 = y * sx + z1 * cx + 4.2
        pts3d.append((x1, y2, z2))
        s = zoom / z2
        pts2d.append((int(gint.DWIDTH // 2 + x1 * s), int(gint.DHEIGHT // 2 + 6 - y2 * s)))

    gint.dclear(gint.C_RGB(3, 4, 7))
    gint.drect(0, 0, gint.DWIDTH - 1, 16, gint.C_BLACK)
    gint.dtext(6, 3, gint.C_WHITE, f"Windows IDLE gint + time  |  FPS: {fps}")

    order = []
    for i0, i1, i2, i3, col in FACES:
        zc = (pts3d[i0][2] + pts3d[i1][2] + pts3d[i2][2] + pts3d[i3][2]) * 0.25
        order.append((zc, i0, i1, i2, i3, col))
    order.sort(reverse=True)

    for _, i0, i1, i2, i3, col in order:
        p0, p1, p2, p3 = pts2d[i0], pts2d[i1], pts2d[i2], pts2d[i3]
        # Backface check in 2D screen space
        cross = (p1[0] - p0[0]) * (p2[1] - p0[1]) - (p1[1] - p0[1]) * (p2[0] - p0[0])
        if not wire and cross <= 0:
            continue
        poly = [p0[0], p0[1], p1[0], p1[1], p2[0], p2[1], p3[0], p3[1]]
        gint.dpoly(poly, gint.C_NONE if wire else col, gint.C_WHITE)

    gint.drect(0, gint.DHEIGHT - 15, gint.DWIDTH - 1, gint.DHEIGHT - 1, gint.C_RGB(6, 8, 12))
    gint.dtext(6, gint.DHEIGHT - 13, gint.C_LIGHT, "Arrows:Spin  +-:Zoom  F1:Wire  F9:WinScale  Esc:Quit")
    gint.dupdate()

    frames += 1
    now = time.ticks_ms()
    if time.ticks_diff(now, t_fps) >= 500:
        fps = frames * 1000 // max(1, time.ticks_diff(now, t_fps))
        frames = 0
        t_fps = now
    time.sleep_ms(16)
