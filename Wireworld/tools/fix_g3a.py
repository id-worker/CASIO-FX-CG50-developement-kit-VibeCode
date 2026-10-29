#!/usr/bin/env python3
"""Finalize G3A checksums and, optionally, install exact RGB565 BMP icons.

Checksum layout cross-checked against fxSDK's fxgxa (g3a.h/util.c/edit.c),
Copyright (C) 2015-2022 gint/fxSDK contributors; see licenses/fxSDK_MIT.txt.
With no extra arguments this remains a checksum-only tool. With --icons it
replaces only the two menu-icon fields, before recalculating both checksums;
the executable payload and program names are never modified. No Pillow needed.
"""
from pathlib import Path
import argparse
import struct


def icon_bmp_to_rgb565(filename):
    """Read 92x64 uncompressed, 24-bit BMP and encode G3A big-endian RGB565."""
    raw = Path(filename).read_bytes()
    if len(raw) < 54 or raw[:2] != b'BM':
        raise ValueError(f'{filename}: not a supported BMP')
    pixel_offset = struct.unpack_from('<I', raw, 10)[0]
    dib_size = struct.unpack_from('<I', raw, 14)[0]
    width, height, planes, bits, compression = struct.unpack_from('<iiHHI', raw, 18)
    if (dib_size < 40 or width != 92 or abs(height) != 64 or planes != 1 or
            bits != 24 or compression != 0 or pixel_offset < 14 + dib_size):
        raise ValueError(f'{filename}: expected uncompressed 92x64 24-bit BMP')
    stride = (width * 3 + 3) & ~3
    if len(raw) < pixel_offset + stride * 64:
        raise ValueError(f'{filename}: truncated BMP pixels')
    result = bytearray(92 * 64 * 2)
    for y in range(64):
        source_y = 63 - y if height > 0 else y
        row = pixel_offset + source_y * stride
        for x in range(92):
            b, g, r = raw[row + 3*x:row + 3*x + 3]
            color = ((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3)
            struct.pack_into('>H', result, (y*92+x)*2, color)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('g3a')
    parser.add_argument('--icons', nargs=2, metavar=('UNSELECTED.bmp','SELECTED.bmp'))
    args = parser.parse_args()
    path = Path(args.g3a)
    data = bytearray(path.read_bytes())
    if len(data) < 0x7110 or bytes(data[:14]) != bytes.fromhex('aaacbdaf90889a8dd3fffefffeff'):
        raise SystemExit('Not a supported G3A with a complete executable payload')
    if struct.unpack_from('>I', data, 0x5c)[0] != len(data):
        raise SystemExit('G3A size mismatch')

    if args.icons:
        # Fully validate both images before altering the output file.
        try:
            uns, sel = (icon_bmp_to_rgb565(p) for p in args.icons)
        except (ValueError, OSError) as exc:
            raise SystemExit(str(exc))
        data[0x1000:0x3e00] = uns
        data[0x4000:0x6e00] = sel

    # Header bytes 0x00..0x1f are stored bit-inverted. The first checksum is
    # the 16-bit sum of 8 big-endian code words at 0x7100.
    sum16 = sum(struct.unpack_from('>8H', data, 0x7100)) & 0xffff
    struct.pack_into('>H', data, 0x16, sum16 ^ 0xffff)
    # Second checksum is the unsigned byte sum excluding its own 4-byte field
    # and the 4-byte footer.
    sum32 = (sum(data[:0x20]) + sum(data[0x24:-4])) & 0xffffffff
    struct.pack_into('>I', data, 0x20, sum32)
    struct.pack_into('>I', data, len(data) - 4, sum32)
    path.write_bytes(data)
    print(f'G3A checksums: {sum16:04x} / {sum32:08x}' +
          ('; menu icons updated' if args.icons else ''))


if __name__ == '__main__':
    main()
