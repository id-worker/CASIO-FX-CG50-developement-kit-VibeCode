#!/usr/bin/env python3
"""Regenerate ObjView's two text-free 92x64 menu icons. Requires Pillow."""
from pathlib import Path
from icon_renderer import render_icon

out = Path(__file__).resolve().parents[1] / 'assets-cg'
out.mkdir(exist_ok=True)
for suffix, selected in [('uns', False), ('sel', True)]:
    path = out / ('icon-' + suffix + '.png')
    render_icon('ObjView', selected).save(path, optimize=True)
    print(path)
