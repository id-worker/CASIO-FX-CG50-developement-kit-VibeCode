#!/usr/bin/env python3
"""Build TDIC.DAT from the two unmodified FreeDict+WikDict TEI sources.

The source dictionary headers state CC BY-SA 3.0; our adaptation is offered
under CC BY-SA 4.0. See ../dicts/LICENSE_data.txt for attribution and caveats.
The program code is separately licensed; see ../LICENSE.

Example:
  python3 tools/make_dict.py --en eng-rus.tei --ru rus-eng.tei

Format version 2, integers little-endian, offsets absolute:
  72-byte header: magic TDIC, version byte 2, flags + padding, then 16 u32:
   en_key_count, ru_key_count,
   en_key_offset, en_record_offset, en_text_offset, en_block_offset,
   ru_key_offset, ru_record_offset, ru_text_offset, ru_block_offset,
   file_size, en_record_count, ru_record_count, en_block_count,
   ru_block_count, reserved.
  Key entry:    u32 text_offset, u32 record_id, u16 text_length (10 bytes).
  Record entry: u16 block_id, u16 offset_within_block, u8 length, u8 reserved.
  Block table:  u32 compressed_offset, u16 compressed_len, u16 raw_len.
  Key strings are sorted by UTF-8 bytes. Translation bodies are stored in raw
  deflate blocks of at most 8192 bytes; an individual body is never split.
"""
from __future__ import annotations

import argparse
from collections import OrderedDict
from pathlib import Path
import re
import struct
import unicodedata
import xml.etree.ElementTree as ET
import zlib

ROOT = Path(__file__).resolve().parents[1]
NS = '{http://www.tei-c.org/ns/1.0}'
KEY_MAX_BYTES = 63
REVERSE_KEY_MAX_BYTES = 32
BODY_MAX_BYTES = 240             # must fit the file format's u8 and C buffer
QUOTE_MAX_BYTES = 100
BLOCK_RAW = 8192
MAX_QUOTES_PER_PART = 5
MAX_REVERSE_TRANSLATIONS = 3

POS_MAP = {
    'suffix': 'suff', 'prefix': 'pref', 'interjection': 'int',
    'numeral': 'num', 'participle': 'ptcp', 'participleadjective': 'adj',
    'conjunction': 'conj', 'preposition': 'prep', 'pronoun': 'pron',
    'adjective': 'adj', 'adverb': 'adv', 'verb': 'v', 'noun': 'n',
    'particle': 'ptcl', 'determiner': 'det', 'phrase': 'phr',
    'phraseologicalunit': 'phr', 'abbreviation': 'abbr', 'article': 'art',
    'proverb': 'prov', 'postposition': 'post',
}


def normalize(text: str | None) -> str:
    if not text:
        return ''
    text = unicodedata.normalize('NFC', text)
    # Some WikDict quotes contain escaped HTML underline tags or wiki links.
    text = re.sub(r'</?[a-zA-Z][^>]*>', '', text)
    text = re.sub(r'\[\[([^\[\]]+)\]\]',
                  lambda m: m.group(1).split('|')[-1], text)
    # Accent marks in the sources describe stress, not spelling. Keep ё/й.
    text = ''.join(c for c in text if c not in ('\u0301', '\u0300'))
    return ' '.join(text.split())


def text_of(node: ET.Element) -> str:
    return normalize(''.join(node.itertext()))


def pos_of(element: ET.Element) -> str | None:
    node = element.find(f'{NS}gramGrp/{NS}pos')
    if node is None or not node.text:
        return None
    pos = normalize(node.text).lower()
    return POS_MAP.get(pos, pos[:8]) or None


def parse_tei(path: Path):
    """Yield (headwords, [(part_of_speech, translation_list), ...])."""
    for _, entry in ET.iterparse(path, events=('end',)):
        if entry.tag != NS + 'entry':
            continue
        heads = [text_of(x) for x in entry.findall(f'{NS}form/{NS}orth')]
        heads = [h for h in heads if h]
        entry_pos = pos_of(entry)
        senses = []
        for sense in entry.findall(f'{NS}sense'):
            words = []
            for citation in sense.iter(NS + 'cit'):
                if citation.get('type') == 'trans':
                    words.extend(text_of(q) for q in citation.findall(f'{NS}quote'))
            words = [w for w in words if w]
            if words:
                senses.append((pos_of(sense) or entry_pos, words))
        if not senses:
            words = []
            for citation in entry.findall(f'{NS}cit'):
                if citation.get('type') == 'trans':
                    words.extend(text_of(q) for q in citation.findall(f'{NS}quote'))
            words = [w for w in words if w]
            if words:
                senses.append((entry_pos, words))
        entry.clear()
        if heads and senses:
            yield heads, senses


def clip_bytes(text: str, max_bytes: int) -> str:
    """Truncate only on a UTF-8 character boundary, preferably a word break."""
    raw = text.encode('utf-8')
    if len(raw) <= max_bytes:
        return text
    shortened = raw[:max_bytes].decode('utf-8', errors='ignore').rstrip(' ,;')
    if ' ' in shortened:
        # Only prefer the space if doing so preserves most of the phrase.
        candidate = shortened.rsplit(' ', 1)[0].rstrip(' ,;')
        if len(candidate.encode('utf-8')) >= max_bytes // 2:
            shortened = candidate
    return shortened


def compose_body(senses) -> str:
    """Merge equal headwords and prefer showing every available POS first.

    Only a limited number of translations and bytes fit the target device;
    the result is a compact bilingual dictionary, not a sentence translator.
    """
    # Preserve distinct senses before taking extra synonyms from any one
    # sense. E.g. computer should retain both "вычислитель" and "компьютер".
    sense_groups = OrderedDict()
    for pos, quotes in senses:
        cleaned = []
        for word in quotes:
            word = clip_bytes(normalize(word), QUOTE_MAX_BYTES)
            if word and word.casefold() not in {x.casefold() for x in cleaned}:
                cleaned.append(word)
        if cleaned:
            sense_groups.setdefault(pos, []).append(cleaned)
    groups = OrderedDict()
    for pos, lists in sense_groups.items():
        out = []
        seen = set()
        for column in range(MAX_QUOTES_PER_PART):
            for words in lists:
                if column < len(words) and words[column].casefold() not in seen:
                    out.append(words[column])
                    seen.add(words[column].casefold())
                    if len(out) >= MAX_QUOTES_PER_PART:
                        break
            if len(out) >= MAX_QUOTES_PER_PART:
                break
        if out:
            groups[pos] = out
    if not groups:
        return ''

    chosen = OrderedDict()

    def render():
        return '; '.join((f'{p}. ' if p else '') + ', '.join(words)
                         for p, words in chosen.items())

    # Reserve at least one sense from each part of speech before adding more.
    for pos, words in groups.items():
        chosen[pos] = [words[0]]
        if len(render().encode('utf-8')) > BODY_MAX_BYTES:
            del chosen[pos]
    # Very long one-sense entries are still useful: truncate the first quote.
    if not chosen:
        pos, words = next(iter(groups.items()))
        label = (f'{pos}. ' if pos else '')
        return label + clip_bytes(words[0], BODY_MAX_BYTES - len(label.encode()))

    for offset in range(1, MAX_QUOTES_PER_PART):
        for pos, words in groups.items():
            if pos not in chosen or offset >= len(words):
                continue
            chosen[pos].append(words[offset])
            if len(render().encode('utf-8')) > BODY_MAX_BYTES:
                chosen[pos].pop()
    body = render()
    assert len(body.encode('utf-8')) <= BODY_MAX_BYTES
    return body


def has_cyrillic(text: str) -> bool:
    return any('\u0400' <= c <= '\u04ff' for c in text)


def valid_key(key: str, direction: str) -> bool:
    return (bool(key) and len(key.encode('utf-8')) <= KEY_MAX_BYTES and
            (has_cyrillic(key) if direction == 'ru' else
             any(ch.isalnum() for ch in key)))


def collect(path: Path, direction: str):
    grouped = OrderedDict()
    uppercase_forms = {}
    entries = 0
    for heads, senses in parse_tei(path):
        entries += 1
        # The upstream currently has one orth per TEI entry. An exact
        # lowercase spelling is preferred to case-folded proper names:
        # "apple" should show яблоко before "Apple" / Эпл.
        for head in heads:
            original = normalize(head)
            key = original.lower()
            if valid_key(key, direction):
                grouped.setdefault(key, [])
                if original == key:
                    grouped[key].extend(senses)
                else:
                    uppercase_forms.setdefault(key, []).extend(senses)
    for key, senses in uppercase_forms.items():
        grouped[key].extend(senses)
    return grouped, entries


def make_records(groups):
    records = []
    key_map = {}
    for key, senses in groups.items():
        body = compose_body(senses)
        if body:
            key_map[key] = len(records)
            records.append((key, body))
    return records, key_map


def pack_section(records, keys):
    raw_blocks = []
    recs = []
    raw = bytearray()
    for _, body in records:
        encoded = body.encode('utf-8')
        if not 0 < len(encoded) <= BODY_MAX_BYTES:
            raise ValueError(f'body length out of range: {len(encoded)}')
        if raw and len(raw) + len(encoded) > BLOCK_RAW:
            raw_blocks.append(bytes(raw))
            raw.clear()
        recs.append((len(raw_blocks), len(raw), len(encoded)))
        raw.extend(encoded)
    if raw:
        raw_blocks.append(bytes(raw))
    compressed = bytearray()
    table = []
    for raw in raw_blocks:
        z = zlib.compressobj(9, zlib.DEFLATED, -15)
        packed = z.compress(raw) + z.flush()
        if len(packed) > 8900 or len(raw) > BLOCK_RAW:
            raise ValueError('unexpectedly large dictionary block')
        table.append((len(compressed), len(packed), len(raw)))
        compressed.extend(packed)

    text = bytearray()
    entries = []
    for key, rid in sorted(keys.items(), key=lambda pair: pair[0].encode('utf-8')):
        data = key.encode('utf-8')
        assert len(data) <= KEY_MAX_BYTES
        entries.append((len(text), rid, len(data)))
        text.extend(data)
    return entries, recs, bytes(text), table, bytes(compressed)


def write_dictionary(path: Path, en_records, en_keys, ru_records, ru_keys):
    en_k, en_r, en_t, en_table, en_data = pack_section(en_records, en_keys)
    ru_k, ru_r, ru_t, ru_table, ru_data = pack_section(ru_records, ru_keys)
    en_nblocks = len(en_table)
    ru_nblocks = len(ru_table)
    align4 = lambda n: (n + 3) & ~3
    ek = 72
    er = align4(ek + 10 * len(en_k))
    et = align4(er + 6 * len(en_r))
    eb = align4(et + len(en_t))
    rk = align4(eb + 8 * en_nblocks + len(en_data))
    rr = align4(rk + 10 * len(ru_k))
    rt = align4(rr + 6 * len(ru_r))
    rb = align4(rt + len(ru_t))
    size = align4(rb + 8 * ru_nblocks + len(ru_data))
    buf = bytearray(size)
    struct.pack_into('<4sBBH', buf, 0, b'TDIC', 2, 0, 0)
    struct.pack_into('<16I', buf, 8, len(en_k), len(ru_k),
                     ek, er, et, eb, rk, rr, rt, rb, size,
                     len(en_r), len(ru_r), en_nblocks, ru_nblocks, 0)

    def place(koff, roff, toff, boff, keys, recs, text, table, data):
        for i, (text_rel, rid, length) in enumerate(keys):
            struct.pack_into('<IIH', buf, koff + 10*i, toff + text_rel, rid, length)
        for i, (block, block_off, length) in enumerate(recs):
            struct.pack_into('<HHBB', buf, roff + 6*i, block, block_off, length, 0)
        buf[toff:toff+len(text)] = text
        data_offset = boff + 8 * len(table)
        for i, (relative, comp_len, raw_len) in enumerate(table):
            struct.pack_into('<IHH', buf, boff + 8*i,
                             data_offset + relative, comp_len, raw_len)
        buf[data_offset:data_offset+len(data)] = data

    place(ek, er, et, eb, en_k, en_r, en_t, en_table, en_data)
    place(rk, rr, rt, rb, ru_k, ru_r, ru_t, ru_table, ru_data)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(buf)
    return len(en_k), len(ru_k), len(en_r), len(ru_r), size


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--en', type=Path, default=Path('/tmp/fd/eng-rus/eng-rus.tei'))
    parser.add_argument('--ru', type=Path, default=Path('/tmp/fd/rus-eng/rus-eng.tei'))
    parser.add_argument('--output', type=Path, default=ROOT/'dicts/TDIC.DAT')
    args = parser.parse_args()

    print('Reading Russian-English TEI...')
    ru_groups, ru_entries = collect(args.ru, 'ru')
    print('Reading English-Russian TEI...')
    en_groups, en_entries = collect(args.en, 'en')
    ru_records, ru_keys = make_records(ru_groups)
    en_records, en_keys = make_records(en_groups)

    # Extra reverse entries are explicitly derived from EN->RU translations,
    # only when the official RU->EN dictionary lacks that headword.
    reverse = OrderedDict()
    for head, senses in en_groups.items():
        for pos, words in senses:
            for word in words:
                key = normalize(word).lower()
                if (not valid_key(key, 'ru') or len(key.encode('utf-8')) > REVERSE_KEY_MAX_BYTES
                        or key in ru_keys):
                    continue
                items = reverse.setdefault(key, [])
                item = (pos, head)
                if item not in items and len(items) < MAX_REVERSE_TRANSLATIONS:
                    items.append(item)
    for key, items in reverse.items():
        senses = [(pos, [head]) for pos, head in items]
        body = compose_body(senses)
        if body:
            ru_keys[key] = len(ru_records)
            ru_records.append((key, body))

    # Common spelling variant: allow е when ё was omitted, except where the
    # existing е word already has its own entry (do not override its meaning).
    for key, rid in list(ru_keys.items()):
        if 'ё' in key:
            alias = key.replace('ё', 'е')
            ru_keys.setdefault(alias, rid)

    en_count, ru_count, en_rec, ru_rec, size = write_dictionary(
        args.output, en_records, en_keys, ru_records, ru_keys)
    # Human-readable derivative aids inspection and future source releases.
    for name, records in [('en_ru.tsv', en_records), ('ru_en.tsv', ru_records)]:
        path = args.output.parent / name
        with path.open('w', encoding='utf-8', newline='\n') as f:
            for key, body in records:
                f.write(f'{key}\t{body}\n')
    print(f'Source entries: EN {en_entries}, RU {ru_entries}')
    print(f'Search keys: EN->RU {en_count}, RU->EN {ru_count}')
    print(f'Records: EN {en_rec}, RU {ru_rec} (including generated reverse entries)')
    print(f'{args.output}: {size} bytes ({size/1048576:.2f} MiB)')


if __name__ == '__main__':
    main()
