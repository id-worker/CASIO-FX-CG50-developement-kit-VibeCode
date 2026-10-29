#!/usr/bin/env python3
"""Check the two 1.15 syntax colors and computed RGB565/sRGB contrast.

This estimates contrast on an ideal white sRGB display; it does NOT model or
measure a Casio LCD, its viewing angle, brightness, or a photographed screen.
"""
from pathlib import Path
import re

src = (Path(__file__).resolve().parents[1] / 'src/main.c').read_text(encoding='utf-8')


def rgb(name):
    m = re.search(r'^#define\s+' + name +
                  r'\s+RGB565\(\s*(\d+)\s*,\s*(\d+)\s*,\s*(\d+)\s*\)',
                  src, re.MULTILINE)
    if not m:
        raise AssertionError(f'Cannot read {name} from source')
    return tuple(int(x) for x in m.groups())


def unpack565(c):
    r, g, b = c
    assert all(0 <= x <= 255 for x in c)
    return (r >> 3) / 31, (g >> 2) / 63, (b >> 3) / 31


def linear(v):
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def white_contrast(c):
    r, g, b = unpack565(c)
    luminance = 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b)
    return 1.05 / (luminance + 0.05)


expected = {'SYN_FUNCTION': (0, 72, 192), 'SYN_STRING': (192, 0, 0)}
previous = {'SYN_FUNCTION': (192, 0, 144), 'SYN_STRING': (0, 136, 0)}
for name, color in expected.items():
    assert rgb(name) == color, f'{name}: unexpected source value'
    new, old = white_contrast(color), white_contrast(previous[name])
    assert new > old + 1.0 and new >= 6.0, (name, new, old)
    print(f'{name}: RGB565/sRGB white contrast {old:.2f}:1 -> {new:.2f}:1')
print('Idealized sRGB calculation only: physical CG50 LCD verification is still needed.')
