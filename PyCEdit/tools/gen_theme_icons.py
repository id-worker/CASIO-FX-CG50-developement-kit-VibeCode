#!/usr/bin/env python3
"""Regenerate the two text-free menu icons for the light-only PyCEdit.

Requires Pillow. The BMP files are the ones consumed by the Makefile. This
script never generates a second theme or modifies editor syntax colors.
"""
from pathlib import Path
from icon_renderer import render_icon

root = Path(__file__).resolve().parents[1]
for state, selected in [('unselected', False), ('selected', True)]:
    image = render_icon('PyCEdit', selected)
    image.save(root / (state + '.bmp'))
    print(root / (state + '.bmp'))
