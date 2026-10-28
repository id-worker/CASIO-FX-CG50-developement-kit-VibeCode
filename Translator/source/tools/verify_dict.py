#!/usr/bin/env python3
"""Check every TDIC.DAT key, record and compressed block independently."""
import argparse
from bisect import bisect_left
from pathlib import Path
import re
import struct
import zlib

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('path', nargs='?', type=Path, default=ROOT/'dicts/TDIC.DAT')
args = parser.parse_args()
data = args.path.read_bytes()
assert len(data) >= 72
magic, version, flags, pad = struct.unpack_from('<4sBBH', data, 0)
assert (magic, version, flags, pad) == (b'TDIC', 2, 0, 0)
(en_n, ru_n, en_keys, en_recs, en_text, en_blocks,
 ru_keys, ru_recs, ru_text, ru_blocks, file_size,
 en_rec_n, ru_rec_n, en_bn, ru_bn, _) = struct.unpack_from('<16I', data, 8)
assert len(data) == file_size
assert en_keys == 72
assert en_keys + 10*en_n <= en_recs
assert en_recs + 6*en_rec_n <= en_text <= en_blocks
assert en_blocks + 8*en_bn <= ru_keys
assert ru_keys + 10*ru_n <= ru_recs
assert ru_recs + 6*ru_rec_n <= ru_text <= ru_blocks
assert ru_blocks + 8*ru_bn <= file_size


def section(koff, roff, toff, blockoff, section_end, kn, rn, bn):
    blocks = []
    for i in range(bn):
        co, cl, rl = struct.unpack_from('<IHH', data, blockoff + 8*i)
        assert blockoff + 8*bn <= co < co+cl <= section_end
        assert 0 < cl <= 8900 and 0 < rl <= 8192
        dec = zlib.decompressobj(-15)
        raw = dec.decompress(data[co:co+cl]) + dec.flush()
        assert dec.eof and not dec.unused_data and not dec.unconsumed_tail
        assert len(raw) == rl
        blocks.append(raw)
    records = []
    for i in range(rn):
        block, pos, size, reserved = struct.unpack_from('<HHBB', data, roff + 6*i)
        assert block < len(blocks) and reserved == 0
        assert 0 < size <= 240 and pos+size <= len(blocks[block])
        body = blocks[block][pos:pos+size].decode('utf-8', errors='strict')
        assert body and '\x00' not in body
        records.append(body)
    keys = []
    ids = []
    prev = b''
    for i in range(kn):
        ko, rid, kl = struct.unpack_from('<IIH', data, koff + 10*i)
        assert toff <= ko < ko+kl <= blockoff
        assert 0 < kl <= 63 and rid < rn
        key = data[ko:ko+kl]
        key.decode('utf-8', errors='strict')
        assert b'\x00' not in key
        assert i == 0 or prev < key, 'keys must be strictly sorted'
        keys.append(key)
        ids.append(rid)
        prev = key
    assert set(ids) == set(range(rn)), 'every record should be searchable'
    return keys, ids, records


ek, ei, er = section(en_keys, en_recs, en_text, en_blocks, ru_keys,
                     en_n, en_rec_n, en_bn)
rk, ri, rr = section(ru_keys, ru_recs, ru_text, ru_blocks, file_size,
                     ru_n, ru_rec_n, ru_bn)
print(f'{args.path}: {file_size} bytes; EN={en_n} keys/{en_rec_n} records; '
      f'RU={ru_n} keys/{ru_rec_n} records')
print(f'All {en_bn + ru_bn} blocks and {len(er) + len(rr)} records decode as valid UTF-8.')


def check(keys, ids, records, query, expected):
    query = query.encode('utf-8')
    i = bisect_left(keys, query)
    assert i < len(keys) and keys[i] == query, query
    body = records[ids[i]]
    assert all(term in body for term in expected), (query.decode(), body, expected)
    print(f'  {query.decode()} -> {body}')

for query, terms in [
    ('cat', ['кошка', 'n.']), ('book', ['книга', 'v.', 'бронировать']),
    ('run', ['бежать', 'n.', 'v.']), ('computer', ['компьютер']),
    ('hello', ['привет']), ('apple', ['яблоко'])]:
    check(ek, ei, er, query, terms)
for query, terms in [
    ('кот', ['cat']), ('бежать', ['run']), ('самолёт', ['plane']),
    ('самолет', ['plane']), ('жук', ['beetle']), ('хлеб', ['bread']),
    ('говорить', ['speak'])]:
    check(rk, ri, rr, query, terms)

# Parts-of-speech availability is a property of upstream data, not every entry.
pos = re.compile(r'(^|; )[A-Za-z]{1,8}\. ')
print('POS-tagged: EN', sum(bool(pos.search(x)) for x in er), '/', len(er),
      '; RU', sum(bool(pos.search(x)) for x in rr), '/', len(rr))
print('ALL DICTIONARY CHECKS PASSED')
