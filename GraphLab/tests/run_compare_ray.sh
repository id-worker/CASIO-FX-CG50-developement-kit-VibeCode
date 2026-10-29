#!/bin/sh
# Compare real renderers compiled with and without the guarded LUT.
set -eu
cd "$(dirname "$0")/.."
work=$(mktemp -d "${TMPDIR:-/tmp}/gl108-compare-XXXXXX")
trap 'rm -rf "$work"' EXIT HUP INT TERM
cc=${CC:-cc}
for mode in fast reference; do
    def=''; [ "$mode" = reference ] && def='-DGL_DISABLE_TRIGFAST'
    "$cc" -std=c11 -DGL_HOST $def -O2 -Wall -Wextra -Werror -Wpedantic \
      tests/test_ray_equivalence.c tests/host_gint.c src/ray.c src/trigfast.c src/preview3d.c src/expr.c -lm -o "$work/$mode"
    "$work/$mode" "$work/$mode.rgb"
done
python3 - "$work/fast.rgb" "$work/reference.rgb" <<'PY'
import struct,sys
p=[open(x,'rb').read() for x in sys.argv[1:]]
a,b=[struct.unpack('='+str(len(z)//2)+'H',z) for z in p]
frame=396*224
assert len(a)==len(b)==10*frame
for i in range(10):
    x=a[i*frame:(i+1)*frame];y=b[i*frame:(i+1)*frame]
    mask=sum((c==65535)!=(d==65535) for c,d in zip(x,y))
    color=sum(c!=d for c,d in zip(x,y))
    print('case',i//2,'Q',12 if i%2==0 else 20,'mask differences',mask,'color differences',color)
    if i//2>=3: assert color==0 # unsafe/unsupported programs must use the exact VM
    else: assert mask<=100 # finite sampling and interpolation still differ slightly
print('Guarded RAY versus reference frame checks passed (host, not device).')
PY
