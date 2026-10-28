#!/usr/bin/env python3
"""
make_icons.py -- draws the two add-in icons (92x64, 24 bit BMP) that mkg3a
needs: unselected.bmp and selected.bmp.

The picture is a tiny Wireworld circuit: a ring generator with one electron in
it and a tap wire carrying the pulses away.  It is drawn as a 23x16 grid of
4x4 pixel "cells" so it stays recognisable at icon size.
"""

import os
import struct

W, H, CELL = 92, 64, 4
COLS, ROWS = W // CELL, H // CELL

# the circuit, in cells
grid = [['.'] * COLS for _ in range(ROWS)]


def put(x, y, ch):
    if 0 <= x < COLS and 0 <= y < ROWS:
        grid[y][x] = ch


def diamond(cx, cy, r):
    for dy in range(-r, r + 1):
        dx = r - abs(dy)
        put(cx - dx, cy + dy, '#')
        put(cx + dx, cy + dy, '#')


diamond(11, 7, 5)                       # ring oscillator
put(12, 3, 'H')                         # an electron inside the ring
put(11, 2, 'T')
for y in (13, 14):                      # tap wire
    put(11, y, '#')
for x in range(12, COLS):               # ... carrying pulses to the right
    put(x, 14, '#')
put(17, 14, 'H')
put(16, 14, 'T')


def render(path, palette):
    rows = []
    for y in range(ROWS - 1, -1, -1):          # BMP rows are bottom up
        row = bytearray()
        for x in range(COLS):
            r, g, b = palette[grid[y][x]]
            row += bytes((b, g, r)) * CELL
        rows.append(bytes(row) * CELL)         # CELL identical pixel rows
    pixels = b''.join(rows)

    hdr = struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54)
    dib = struct.pack('<IiiHHIIiiII', 40, W, H, 1, 24, 0, len(pixels),
                      0, 0, 0, 0)
    with open(path, 'wb') as f:
        f.write(hdr + dib + pixels)
    print('wrote %s (%d bytes)' % (path, 54 + len(pixels)))


BLACK = (16, 16, 16)
render(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    '..', 'unselected.bmp'),
       {'.': BLACK,
        '#': (110, 100, 30),      # dim, "off" look
        'H': (60, 80, 90),
        'T': (90, 40, 40)})

render(os.path.join(os.path.dirname(os.path.abspath(__file__)),
                    '..', 'selected.bmp'),
       {'.': BLACK,
        '#': (255, 214, 0),       # conductor: yellow
        'H': (0, 226, 255),       # electron head: cyan
        'T': (240, 40, 40)})      # electron tail: red
