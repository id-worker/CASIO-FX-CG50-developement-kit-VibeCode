#!/usr/bin/env python3
"""Extract gint's default font solely for the host screenshot test.
The input remains a separate upstream gint file; it is not relicensed.
Usage: python3 export_host_font.py path/to/gint/src/font8x9.png > host_font.h
"""
from PIL import Image
import sys

im = Image.open(sys.argv[1]).convert('RGB')
if im.size != (160, 78):
    raise SystemExit('Expected gint font8x9.png (160x78)')
widths, rows = [], []
for n in range(95):  # printable ASCII
    x, y = 1 + (n % 16) * 10, 1 + (n // 16) * 13
    glyph = im.crop((x, y, x + 8, y + 11))
    def blank(col):
        return all(glyph.getpixel((col, row)) == (255, 255, 255) for row in range(11))
    left, right = 0, 8
    while left + 1 < right and blank(left): left += 1
    while right - 1 > left and blank(right - 1): right -= 1
    widths.append(right - left)
    rows.append([sum(1 << (7 - col) for col in range(right - left)
                     if glyph.getpixel((left + col, row)) == (0, 0, 0)) for row in range(11)])
print('/* Generated from the upstream gint default font for host tests. */')
print('static unsigned char const host_font_width[95]={' + ','.join(map(str, widths)) + '};')
print('static unsigned char const host_font_rows[95][11]={')
for row in rows: print('{' + ','.join(map(str, row)) + '},')
print('};')
