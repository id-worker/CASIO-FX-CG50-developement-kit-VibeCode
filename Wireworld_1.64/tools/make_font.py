#!/usr/bin/env python3
"""Create Wireworld's compact ASCII 6x11 bitmap from DejaVu Sans Mono.

Requires Pillow and DejaVuSansMono.ttf. No TTF or Pillow is needed to build the
add-in: the resulting src/font_ww.h is checked in. Set WW_FONT to use a custom
location for the original font. The original DejaVu/Bitstream license is in
licenses/DejaVu_fonts.txt.
"""
import os
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont

root = Path(__file__).resolve().parents[1]
ttf = os.environ.get('WW_FONT', '/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf')
font = ImageFont.truetype(ttf, 9)
width, height, baseline = 6, 11, 9
lines = []
for code in range(32, 127):
    c = chr(code)
    # Draw in a padded canvas and reject any clipping before generating rows.
    canvas = Image.new('1', (width + 20, height + 20), 0)
    ImageDraw.Draw(canvas).text((10, 10 + baseline), c, font=font,
                                fill=1, anchor='ls')
    box = canvas.getbbox()
    if box and (box[0] < 10 or box[1] < 10 or
                box[2] > 10 + width or box[3] > 10 + height):
        raise SystemExit(f'Character U+{code:04X} is clipped: {box}')
    glyph = canvas.crop((10, 10, 10 + width, 10 + height))
    rows = []
    for y in range(height):
        bits = 0
        for x in range(width):
            if glyph.getpixel((x, y)):
                bits |= 0x80 >> x
        rows.append(bits)
    label = repr(c).replace('*/', '* /')
    lines.append('    { ' + ', '.join(f'0x{v:02X}' for v in rows) +
                 f' }}, /* {code:3d} {label} */')
header = '''/* font_ww.h -- generated from DejaVu Sans Mono by tools/make_font.py.
 * 9 px source font, 6x11 cell, ASCII 32..126. See licenses/DejaVu_fonts.txt.
 */
#ifndef FONT_WW_H
#define FONT_WW_H
#define WW_FONT_W 6
#define WW_FONT_H 11
static const unsigned char ww_glyphs[95][WW_FONT_H] = {
''' + '\n'.join(lines) + '\n};\n#endif\n'
(root / 'src/font_ww.h').write_text(header, encoding='ascii')
print('Generated src/font_ww.h (95 glyphs, 6x11), no clipped glyphs')
