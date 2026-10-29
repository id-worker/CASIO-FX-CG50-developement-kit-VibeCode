#!/usr/bin/env python3
"""Check catalog coverage and last-page labels, no network required."""
from pathlib import Path
import re
root = Path(__file__).resolve().parent.parent
src = (root / 'src/main.c').read_text(encoding='utf-8')
table = src.split('static const include_entry_t include_catalog[] = {', 1)[1].split('};', 1)[0]
items = re.findall(r'\{\s*"([^"\n]+)"\s*,\s*([01])\s*\}', table)
expected = [line for line in (root / 'tools/picoc_registered_headers.txt').read_text().splitlines()
            if line and not line.startswith('#')]
assert len(expected) == 43 and len(set(expected)) == 43
assert len(items) == 62 and len({name for name, flag in items}) == 62
assert {name for name, flag in items if flag == '0'} == set(expected)
assert len([name for name, flag in items if flag == '1']) == 19
for name, flag in items:
    # draw_str_n has room for 50 chars and the item scratch has 100 bytes.
    label = '#include <' + name + '>' + (' [SDK]' if flag == '1' else '')
    assert len(label) <= 50 and len(name) < 60
print('Header catalog: 43 registered PicoC + 19 native SDK, no duplicates/clipping.')

mod_table = src.split('static const pymodule_entry_t pymodule_catalog[] = {', 1)[1].split('};', 1)[0]
modules = re.findall(r'\{\s*"([^"\n]+)"\s*,\s*PYMOD_(COMMON|EXTRA|FILE)\s*\}', mod_table)
module_reference = [tuple(line.split()) for line in (root / 'tools/python_modules_reference.txt').read_text().splitlines()
                    if line and not line.startswith('#')]
assert modules == module_reference
assert len(modules) == 18 and len({name for name, group in modules}) == 18
assert {name for name, group in modules if group == 'FILE'} == {'turtle', 'matplotl'}
assert {name for name, group in modules if group == 'COMMON'} == {'math', 'random', 'casioplot'}
for name, group in modules:
    assert re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*', name)
    label = 'import ' + name + (' [PE]' if group == 'EXTRA' else ' [FILE]' if group == 'FILE' else '')
    assert len(label) <= 50 and len(name) < 60
print('Python catalog: 18 unique import templates, optional files marked, no clipped labels.')
