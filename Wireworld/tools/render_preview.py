#!/usr/bin/env python3
"""
render_preview.py -- draws PNG previews of what the add-in shows on the
calculator screen (384x216 visible pixels).  Handy for the README and for
checking the layout without transferring anything to the calculator.

    python3 tools/render_preview.py --gen 120 --out preview.png
    python3 tools/render_preview.py --pattern XOR --gen 300 --scale 3
    python3 tools/render_preview.py --pattern XOR --full --zoom 1 --grid dots

The text is set with a plain bitmap font, so it looks slightly different from
the calculator's own mini font.
"""
import argparse
import os
import sys

from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from build_patterns import DEMOS, from_text, step   # noqa: E402

VIS_W, VIS_H = 384, 216
HEADER_H = 18

# the add-in palette (RGB565 -> 24 bit)
COL = {
    '.': (0x00, 0x00, 0x00),
    'g': (0x31, 0x86, 0x31),     # ruler mark every 10 cells
    '#': (0xFF, 0xE0, 0x00),
    'H': (0x07, 0xFF, 0xFF),     # COL_HEAD 0x03FF -> bright cyan
    'T': (0xF8, 0x00, 0x00),
}
COL_BG = (0, 0, 0)
COL_GRID2 = (0x18, 0xC3, 0x18)   # thin grid lines
COL_HDRBG = (0x00, 0x18, 0x60)
COL_CURSOR = (255, 255, 255)
COL_EDGE = (0xA0, 0x00, 0xF8)    # seam where the world wraps


def simulate(art, gens):
    g = from_text(art)
    for _ in range(gens):
        g = step(g)
    return g


PAT_OX, PAT_OY = 2, 2     # the add-in loads circuits at cell (2,2)


def render(g, gen, scale, cell=2, header=True, grid='dots', cursor=True):
    header_h = HEADER_H if header else 0
    gw, gh = VIS_W // cell, (VIS_H - header_h) // cell

    img = Image.new('RGB', (VIS_W * scale, VIS_H * scale), COL_BG)
    d = ImageDraw.Draw(img)

    if header:
        d.rectangle([0, 0, VIS_W * scale - 1, header_h * scale - 1],
                    fill=COL_HDRBG)
        line = 'G%06d S3 %s PEN WIRE X%03d Y%03d Z%d' % (
            gen, 'RUN' if gen else 'STOP', gw // 2, gh // 2, cell)
        d.text((3 * scale, 1 * scale), line, fill=(255, 255, 255))

    for y in range(gh):
        for x in range(gw):
            gx, gy = x - PAT_OX, y - PAT_OY
            if 0 <= gy < g.h and 0 <= gx < g.w:
                ch = {0: '.', 1: '#', 2: 'H', 3: 'T'}[g.cells[gy][gx]]
                c = COL[ch]
                if ch == '.' and grid == 'dots' and x % 10 == 0 and y % 10 == 0:
                    c = COL['g']
            else:
                c = COL_BG

            x0 = x * cell * scale
            y0 = (header_h + y * cell) * scale
            x1 = x0 + cell * scale - 1
            y1 = y0 + cell * scale - 1
            d.rectangle([x0, y0, x1, y1], fill=c)

            if grid == 'lines' and cell >= 3:
                gc = COL['g'] if (x + 1) % 10 == 0 else COL_GRID2
                d.rectangle([x0, y1 - scale + 1, x1, y1], fill=gc)
                d.rectangle([x1 - scale + 1, y0, x1, y1], fill=gc)

            # the seam of the world
            if cell == 1 and c == COL_BG and (x == 0 or y == 0):
                d.rectangle([x0, y0, x1, y1], fill=COL_EDGE)
            if cell >= 2 and (x == 0 or y == 0):
                if y == 0:
                    d.rectangle([x0, y0, x1, y0 + scale - 1], fill=COL_EDGE)
                if x == 0:
                    d.rectangle([x0, y0, x0 + scale - 1, y1], fill=COL_EDGE)

    if cursor:
        cx, cy = gw // 2, gh // 2
        x0, y0 = cx * cell * scale, (header_h + cy * cell) * scale
        x1, y1 = x0 + cell * scale - 1, y0 + cell * scale - 1
        t = 2 if cell >= 6 else 1
        rx0, ry0 = x0 - scale, y0 - scale
        rx1, ry1 = x1 + scale, y1 + scale
        d.rectangle([rx0, ry0, rx1, ry0 + t * scale - 1], fill=COL_CURSOR)
        d.rectangle([rx0, ry1 - t * scale + 1, rx1, ry1], fill=COL_CURSOR)
        d.rectangle([rx0, ry0 + t * scale, rx0 + t * scale - 1,
                     ry1 - t * scale], fill=COL_CURSOR)
        d.rectangle([rx1 - t * scale + 1, ry0 + t * scale, rx1,
                     ry1 - t * scale], fill=COL_CURSOR)
    return img


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--pattern', default=None,
                    help='name of the demo circuit (default: all)')
    ap.add_argument('--gen', type=int, default=150)
    ap.add_argument('--scale', type=int, default=2)
    ap.add_argument('--zoom', type=int, default=2, choices=(1, 2, 3, 4, 6, 8),
                    help='pixels per cell')
    ap.add_argument('--grid', default='dots', choices=('off', 'dots', 'lines'))
    ap.add_argument('--full', action='store_true',
                    help='full screen: no status line')
    ap.add_argument('--suffix', default='')
    ap.add_argument('--out', default=None)
    ap.add_argument('--outdir', default=os.path.join(
        os.path.dirname(os.path.abspath(__file__)), '..', 'preview'))
    args = ap.parse_args()

    chosen = [d for d in DEMOS if not args.pattern
              or d[0].strip() == args.pattern.upper()]
    if not chosen:
        sys.exit('no such pattern: %s (have: %s)'
                 % (args.pattern, ', '.join(d[0].strip() for d in DEMOS)))

    os.makedirs(args.outdir, exist_ok=True)
    for name, title, fn in chosen:
        art = fn().text()
        g = simulate(art, args.gen)
        img = render(g, args.gen, args.scale, cell=args.zoom,
                     header=not args.full, grid=args.grid)
        out = args.out if (args.out and len(chosen) == 1) else os.path.join(
            args.outdir, 'screen_%s%s_%04d.png'
            % (name.strip().lower(), args.suffix, args.gen))
        img.save(out)
        print('wrote %s  (%s, generation %d)' % (out, title, args.gen))


if __name__ == '__main__':
    main()
