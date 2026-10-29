#!/usr/bin/env python3
"""Regenerate Wireworld's neutral, text-free 92x64 menu icons.

Requires Pillow. This changes only unselected.bmp and selected.bmp, never the
colors of the simulation or the application UI.
"""
from pathlib import Path
from icon_renderer import render_icon

root = Path(__file__).resolve().parents[1]
for state, selected in [('unselected', False), ('selected', True)]:
    image = render_icon('Wireworld', selected)
    image.save(root / (state + '.bmp'))
    print(root / (state + '.bmp'))
