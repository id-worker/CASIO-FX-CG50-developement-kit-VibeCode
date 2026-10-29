#!/usr/bin/env python3
"""Verify the five local G3As without calculator hardware or external packages.

Checks exact current checksums, names, BMP pixels and embedded RGB565 icons.
Four icon-only updates preserve executable data against recorded baseline
hashes. PyCEdit 1.15 changes code and, as requested, the menu/filename/ID;
the new fields and unchanged icon blocks are checked explicitly. This does
not emulate Casio OS, the calculator's Main Menu or display.
"""
from pathlib import Path
import hashlib
import importlib.util
import json
import struct

root = Path(__file__).resolve().parents[1]
manifest = json.loads((root / 'release_tools/icon_baseline.json').read_text(encoding='utf-8'))
spec = importlib.util.spec_from_file_location('fix_g3a', root / 'PyCEdit/tools/fix_g3a.py')
fix = importlib.util.module_from_spec(spec)
spec.loader.exec_module(fix)


def sha(data):
    return hashlib.sha256(data).hexdigest()


def baseline_masked(data):
    b = bytearray(data)
    for lo, hi in [(0x20,0x24),(0x1000,0x3e00),(0x4000,0x6e00),(len(b)-4,len(b))]:
        b[lo:hi] = bytes(hi-lo)
    return sha(b)


def validate_bmp_greyscale(path):
    raw = path.read_bytes()
    # The independent BMP parser also checks dimensions, format and truncation.
    encoded = fix.icon_bmp_to_rgb565(path)
    off = struct.unpack_from('<I',raw,10)[0]
    width, height = struct.unpack_from('<ii',raw,18)
    stride = (width*3+3)&~3
    for row in range(abs(height)):
        p=off+stride*row
        for x in range(width):
            b,g,r=raw[p+3*x:p+3*x+3]
            assert r==g==b, f'{path}: color pixel at ({x},{row})'
    return encoded


for name, info in manifest.items():
    path = root/'releases'/info['filename']
    data = path.read_bytes()
    assert (root/name/info['filename']).read_bytes() == data, name
    assert len(data) == info['length'] and sha(data)==info['new_file_sha256'], name
    assert data[:14] == bytes.fromhex('aaacbdaf90889a8dd3fffefffeff'), name
    assert struct.unpack_from('>I',data,0x5c)[0] == len(data), name
    assert data[0x60:0x6b].split(b'\0')[0].decode('ascii') == info['internal_id'], name
    assert data[0x130:0x13c].split(b'\0')[0].decode('ascii') == info['version'], name
    c16 = sum(struct.unpack_from('>8H',data,0x7100)) & 0xffff
    assert struct.unpack_from('>H',data,0x16)[0] == (c16^0xffff), name
    c32 = (sum(data[:0x20])+sum(data[0x24:-4])) & 0xffffffff
    assert struct.unpack_from('>I',data,0x20)[0]==c32==struct.unpack_from('>I',data,len(data)-4)[0], name
    assert sha(data[0x7000:-4]) == info['payload_sha256'], name
    name_fields = data[0x40:0x5c] + data[0x60:0x130] + data[0x170:0x290] + data[0xebc:0x1000]
    if info['code_update']:
        assert sha(name_fields) == info['current_name_fields_sha256'], name
        assert sha(name_fields) != info['previous_name_fields_sha256'], name
        display = info['display_name'].encode('ascii')
        assert data[0x40:0x5c].split(b'\0')[0] == display, name
        for i in range(8):
            # mkg3a stores eight short/localized menu-name fields, including
            # unused/reserved slots. Ensure none fall back to PyC-Lite.
            field = data[0x6b + i*0x18:0x6b + (i+1)*0x18]
            assert field.split(b'\0')[0] == display, (name, i)
        assert data[0xebc:0x1000].split(b'\0')[0] == ('/' + info['filename']).encode('ascii'), name
        assert info['payload_sha256'] != info['previous_payload_sha256'], name
    else:
        assert sha(name_fields) == info['preserved_name_fields_sha256'], name
        assert baseline_masked(data) == info['unchanged_masked_file_sha256'], name
    icons=[]
    for state, start, end in [('unselected',0x1000,0x3e00),('selected',0x4000,0x6e00)]:
        bmp=root/'Icons'/name/(state+'.bmp')
        expected=validate_bmp_greyscale(bmp)
        assert data[start:end]==expected, f'{name} {state}: embedded icon mismatch'
        if info['code_update']:
            assert sha(data[start:end]) == info['previous_1_14_icon_sha256'][state], (name, state)
        icons.append(expected)
        if name in ('GraphLab','ObjView'):
            project_asset=root/name/'assets-cg'/('icon-sel.png' if state=='selected' else 'icon-uns.png')
            assert project_asset.read_bytes()==(root/'Icons'/name/(state+'.png')).read_bytes(), name
        else:
            sub='Translator/source' if name=='Translator' else name
            assert (root/sub/(state+'.bmp')).read_bytes()==bmp.read_bytes(), name
    assert icons[0]!=icons[1], name
    code_status = 'new payload and PyCEdit names verified; 1.14 icons unchanged' if info['code_update'] else 'payload and metadata preserved'
    print(f"{name}: {info['version']}, {info['internal_id']}, {code_status}; two grey icons + checksums OK")

assert sha((root/'Translator/TDIC.DAT').read_bytes()) == 'e7f6ab4a090c4f4998cbc5641a7cb06b44c5ef38a5c4c1c9d33f2e303a98cef2'
print('Translator dictionary is unchanged. No Casio hardware was tested.')
