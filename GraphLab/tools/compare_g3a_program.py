#!/usr/bin/env python3
"""Compare program bytes of two fx-CG50 G3As, ignoring header and footer.
Run fxgxa -d separately for format/checksum verification. The 0x7000-byte
header contains the build date; the 4-byte footer is a checksum that can
also change when the date changes.
"""
from pathlib import Path
import hashlib
import sys
if len(sys.argv) != 3:
    raise SystemExit('usage: python3 compare_g3a_program.py first.g3a second.g3a')
a,b=(Path(x).read_bytes() for x in sys.argv[1:])
if len(a)<0x7004 or len(b)<0x7004:
    raise SystemExit('G3A too short')
x,y=a[0x7000:-4],b[0x7000:-4]
print('program sizes:',len(x),len(y))
print('first program SHA-256: ',hashlib.sha256(x).hexdigest())
print('second program SHA-256:',hashlib.sha256(y).hexdigest())
if x!=y: raise SystemExit('PROGRAM DIFFERS')
print('PROGRAM MATCHES (header/footer excluded; verify G3A format separately)')
