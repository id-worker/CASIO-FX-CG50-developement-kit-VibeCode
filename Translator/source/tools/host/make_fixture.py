#!/usr/bin/env python3
"""Create a tiny test-only dictionary with an entry longer than a screen."""
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from make_dict import write_dictionary

if len(sys.argv) != 2:
    raise SystemExit('usage: make_fixture.py OUTPUT.DAT')
long_key = 'a' * 63
long_body = ('x ' + 'z' * 40 + ' ') * 5 + 'end'
en = [(long_key, long_body), ('b', 'long'), ('c', 'c translation')]
ru = [('а', 'letter a')]
write_dictionary(Path(sys.argv[1]), en, {k: i for i, (k, _) in enumerate(en)},
                 ru, {k: i for i, (k, _) in enumerate(ru)})
