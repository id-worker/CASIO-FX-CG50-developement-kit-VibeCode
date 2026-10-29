#!/usr/bin/env python3
"""Finalize the two G3A checksums after mkg3a packaging.

Checksum layout cross-checked against fxSDK's fxgxa (g3a.h/util.c/edit.c),
Copyright (C) 2015-2022 gint/fxSDK contributors; see licenses/fxSDK_MIT.txt.
This does not modify the executable payload, names or icons.
"""
from pathlib import Path
import struct
import sys

if len(sys.argv) != 2:
    raise SystemExit('Usage: fix_g3a.py FILE.g3a')
path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 0x7110 or bytes(data[:14]) != bytes.fromhex('aaacbdaf90889a8dd3fffefffeff'):
    raise SystemExit('Not a supported G3A with a complete executable payload')
if struct.unpack_from('>I', data, 0x5c)[0] != len(data):
    raise SystemExit('G3A size mismatch')

# Header bytes 0x00..0x1f are stored bit-inverted. The first checksum is
# the 16-bit sum of 8 big-endian code words at 0x7100.
sum16 = sum(struct.unpack_from('>8H', data, 0x7100)) & 0xffff
struct.pack_into('>H', data, 0x16, sum16 ^ 0xffff)
# Second checksum is the unsigned byte sum of the entire file excluding
# its own 4-byte field and the 4-byte footer.
sum32 = (sum(data[:0x20]) + sum(data[0x24:-4])) & 0xffffffff
struct.pack_into('>I', data, 0x20, sum32)
struct.pack_into('>I', data, len(data) - 4, sum32)
path.write_bytes(data)
print(f'G3A checksums: {sum16:04x} / {sum32:08x}')
