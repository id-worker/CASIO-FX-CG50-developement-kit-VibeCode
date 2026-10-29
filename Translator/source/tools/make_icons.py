#!/usr/bin/env python3
"""Regenerate Translator's two text-free 92x64 menu icons. Requires Pillow."""
from pathlib import Path
from icon_renderer import render_icon

root = Path(__file__).resolve().parents[1]
for state, selected in [('unselected', False), ('selected', True)]:
    path = root / (state + '.bmp')
    render_icon('Translator', selected).save(path)
    print(path)
