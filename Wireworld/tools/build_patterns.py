#!/usr/bin/env python3
"""
build_patterns.py -- builds and verifies the demo circuits (patterns.c) for the
Wireworld add-in for the Casio fx-CG50.

The circuits are composed here from a small library of verified components
(diode, OR, XOR, ring clock).  Every composed circuit is then simulated with a
reference implementation of the Wireworld rule and checked against the expected
behaviour, so patterns.c is never generated from unverified geometry.

Usage:  python3 tools/build_patterns.py [--dump]
"""

import sys

# ---------------------------------------------------------------------------
# Reference Wireworld implementation (used for verification only)
# ---------------------------------------------------------------------------
EMPTY, WIRE, HEAD, TAIL = 0, 1, 2, 3
CH = {EMPTY: '.', WIRE: '#', HEAD: 'H', TAIL: 'T'}


class Grid(object):
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.cells = [[EMPTY] * w for _ in range(h)]

    def clone(self):
        g = Grid(self.w, self.h)
        g.cells = [row[:] for row in self.cells]
        return g

    def size(self):
        return self.w, self.h


def step(g):
    """one generation of Wireworld (Moore neighbourhood, dead borders)"""
    n = Grid(g.w, g.h)
    for y in range(g.h):
        row = g.cells[y]
        nrow = n.cells[y]
        for x in range(g.w):
            c = row[x]
            if c == EMPTY or c == HEAD or c == TAIL:
                nrow[x] = {EMPTY: EMPTY, HEAD: TAIL, TAIL: WIRE}[c]
                continue
            cnt = 0
            y0 = y - 1 if y > 0 else 0
            y1 = y + 2 if y + 2 <= g.h else g.h
            x0 = x - 1 if x > 0 else 0
            x1 = x + 2 if x + 2 <= g.w else g.w
            for yy in range(y0, y1):
                gyy = g.cells[yy]
                for xx in range(x0, x1):
                    if gyy[xx] == HEAD:
                        cnt += 1
            nrow[x] = HEAD if cnt in (1, 2) else WIRE
    return n


def to_text(g):
    return '\n'.join(''.join(CH[c] for c in row) for row in g.cells)


def from_text(text):
    rows = [r for r in text.split('\n') if r.strip('') != '']
    w = max(len(r) for r in rows)
    g = Grid(w, len(rows))
    m = {'.': EMPTY, ' ': EMPTY, '#': WIRE, 'H': HEAD, 'T': TAIL}
    for y, r in enumerate(rows):
        for x, c in enumerate(r):
            g.cells[y][x] = m[c]
    return g


# ---------------------------------------------------------------------------
# Drawing helpers
# ---------------------------------------------------------------------------
class Canvas(object):
    def __init__(self, w, h):
        self.w, self.h = w, h
        self.a = [['.'] * w for _ in range(h)]

    def put(self, x, y, c='#'):
        if 0 <= x < self.w and 0 <= y < self.h:
            self.a[y][x] = c

    def hline(self, x0, x1, y, c='#'):
        for x in range(min(x0, x1), max(x0, x1) + 1):
            self.put(x, y, c)

    def vline(self, x, y0, y1, c='#'):
        for y in range(min(y0, y1), max(y0, y1) + 1):
            self.put(x, y, c)

    def loop(self, x0, y0, w, h):
        """perimeter of a rectangle"""
        self.hline(x0, x0 + w - 1, y0)
        self.hline(x0, x0 + w - 1, y0 + h - 1)
        self.vline(x0, y0, y0 + h - 1)
        self.vline(x0 + w - 1, y0, y0 + h - 1)

    def diamond(self, cx, cy, r):
        """a "round" ring oscillator: every cell has exactly two neighbours, so
        an electron circles forever without splitting.  Period = 4*r generations."""
        for dy in range(-r, r + 1):
            dx = r - abs(dy)
            if dx == 0:
                self.put(cx, cy + dy)
            else:
                self.put(cx - dx, cy + dy)
                self.put(cx + dx, cy + dy)

    def clock(self, cx, cy, r, taplen=2):
        """ring oscillator + tap wire hanging from its bottom tip.
        Returns the (x, y) where the horizontal tap wire starts."""
        self.diamond(cx, cy, r)
        self.vline(cx, cy + r, cy + r + taplen)
        # one electron: head on the upper-right side, tail on the top tip
        self.electron(cx + 1, cy - r + 1, 1, 1)
        return cx, cy + r + taplen

    def paste(self, x0, y0, art):
        for dy, row in enumerate(art.strip('\n').split('\n')):
            for dx, ch in enumerate(row):
                if ch != '.':
                    self.put(x0 + dx, y0 + dy, ch)

    def electron(self, x, y, dx, dy=0):
        """place an electron (head + tail) that will move in direction (dx,dy)"""
        self.put(x, y, 'H')
        self.put(x - dx, y - dy, 'T')

    def text(self):
        return '\n'.join(''.join(r) for r in self.a).rstrip('.')


# ---------------------------------------------------------------------------
# Verified components
#
#   DIODE_FWD  -- conducts left -> right, blocks right -> left   (7 x 3)
#   DIODE_REV  -- blocks     left -> right, conducts right -> left (7 x 3)
#   OR_GATE    -- 7 x 5, inputs on rows 0 and 4 (left), output row 2 (right)
#   XOR_GATE   -- 9 x 7, inputs on rows 0 and 6 (left), output row 3 (right)
#
# The diode shapes are the classic Wireworld diode (a one cell gap bridged by
# two diagonal bypass wires); the XOR gate is the well known 9x7 loop gate.
# Both were verified by simulation, see verify_components() below.
# ---------------------------------------------------------------------------
DIODE_FWD = ("..##...\n"
             "###.###\n"
             "..##...")
DIODE_REV = ("...##..\n"
             "###.###\n"
             "...##..")
OR_GATE = ("###....\n"
           "...#...\n"
           "..#####\n"
           "...#...\n"
           "###....")
XOR_GATE = ("###......\n"
            "...#.....\n"
            "..####...\n"
            "..#..####\n"
            "..####...\n"
            "...#.....\n"
            "###......")


def verify_components(verbose=True):
    """simulate each component in both directions / with all input combos"""
    ok = True

    # --- diode: forward conducts, reverse blocks -------------------------
    for name, art, passes_fwd in (("DIODE_FWD", DIODE_FWD, True),
                                  ("DIODE_REV", DIODE_REV, False)):
        for direction, expect in (("fwd", passes_fwd), ("rev", not passes_fwd)):
            c = Canvas(19, 3)
            c.paste(6, 0, art)
            c.hline(0, 5, 1)        # left wire
            c.hline(13, 18, 1)      # right wire
            if direction == "fwd":
                c.electron(1, 1, 1)
                watch = 17          # far right
            else:
                c.electron(17, 1, -1)
                watch = 1           # far left
            g = from_text(c.text())
            got = False
            for _ in range(40):
                g = step(g)
                if g.cells[1][watch] == HEAD:
                    got = True
                    break
            status = "OK " if got == expect else "FAIL"
            if got != expect:
                ok = False
            if verbose:
                print("  %s %s: conducts=%s expected=%s" %
                      (status, name, got, expect))

    # --- gates: truth table ----------------------------------------------
    for name, art, inrows, outrow, table in (
            ("OR_GATE", OR_GATE, (0, 4), 2, {(0, 0): 0, (1, 0): 1, (0, 1): 1, (1, 1): 1}),
            ("XOR_GATE", XOR_GATE, (0, 6), 3, {(0, 0): 0, (1, 0): 1, (0, 1): 1, (1, 1): 0})):
        h = len(art.split('\n'))
        w = max(len(r) for r in art.split('\n'))
        for (a, b) in ((1, 0), (0, 1), (1, 1)):
            c = Canvas(w + 12, h)
            c.paste(0, 0, art)
            c.hline(w, w + 11, outrow)
            # electrons are injected so that they reach the gate together
            if a:
                c.electron(0, inrows[0], 1)
            if b:
                c.electron(0, inrows[1], 1)
            g = from_text(c.text())
            fired = False
            for _ in range(60):
                g = step(g)
                if any(g.cells[outrow][x] == HEAD for x in range(w + 1, w + 12)):
                    fired = True
                    break
            expect = bool(table[(a, b)])
            status = "OK " if fired == expect else "FAIL"
            if fired != expect:
                ok = False
            if verbose:
                print("  %s %s: A=%d B=%d -> out=%s expected=%s" %
                      (status, name, a, b, int(fired), int(expect)))
    return ok


# ---------------------------------------------------------------------------
# Demo circuits
# ---------------------------------------------------------------------------
def demo_clock():
    """three ring generators of different periods feeding three long lines"""
    c = Canvas(150, 52)
    c.clock(8, 8, 2)                 # 8 cell ring  -> a pulse every  8 gens
    c.hline(9, 148, 12)
    c.clock(8, 22, 3)                # 12 cell ring -> a pulse every 12 gens
    c.hline(9, 148, 27)
    c.clock(8, 38, 4)                # 16 cell ring -> a pulse every 16 gens
    c.hline(9, 148, 44)
    return c


def demo_diode():
    """one clock, two branches: forward-biased diode vs reverse-biased one"""
    c = Canvas(150, 52)
    c.clock(8, 8, 2)
    c.hline(9, 18, 12)               # tap wire to the split
    c.vline(18, 12, 40)              # down branch
    c.hline(19, 26, 12)              # straight branch
    c.hline(19, 26, 40)              # lower branch
    c.paste(27, 11, DIODE_FWD)       # upper branch: conducting
    c.paste(27, 39, DIODE_REV)       # lower branch: blocking
    c.hline(34, 148, 12)
    c.hline(34, 148, 40)
    return c


def demo_or():
    """two clocks of different periods merged by an OR gate"""
    c = Canvas(150, 48)
    c.clock(8, 8, 2)                 # period 8
    c.hline(9, 13, 12)
    c.paste(14, 11, DIODE_FWD)
    c.hline(21, 38, 12)
    c.vline(38, 12, 20)
    c.hline(39, 43, 20)

    c.clock(8, 34, 3)                # period 12
    c.hline(9, 13, 39)
    c.paste(14, 38, DIODE_FWD)
    c.hline(21, 36, 39)
    c.vline(36, 24, 39)
    c.hline(37, 43, 24)

    c.paste(44, 20, OR_GATE)
    c.hline(50, 148, 22)
    return c


def demo_xor():
    """two clocks of different periods into an XOR gate"""
    c = Canvas(150, 48)
    c.clock(8, 8, 2)                 # period 8
    c.hline(9, 13, 12)
    c.paste(14, 11, DIODE_FWD)
    c.hline(21, 38, 12)
    c.vline(38, 12, 18)
    c.hline(39, 43, 18)

    c.clock(8, 34, 3)                # period 12
    c.hline(9, 13, 39)
    c.paste(14, 38, DIODE_FWD)
    c.hline(21, 36, 39)
    c.vline(36, 25, 39)
    c.hline(37, 43, 24)

    c.paste(44, 18, XOR_GATE)
    c.hline(52, 148, 21)
    return c


# ---------------------------------------------------------------------------
# Verification of the composed circuits
# ---------------------------------------------------------------------------
def pulses_at(art, cells, steps):
    """run `steps` generations, return (generation, cell) of every electron head
    seen on the watch cells"""
    g = from_text(art)
    seen = []
    for t in range(1, steps + 1):
        g = step(g)
        for (x, y) in cells:
            if y < g.h and x < g.w and g.cells[y][x] == HEAD:
                seen.append((t, x, y))
    return seen


def spacings(seen):
    ts = sorted(set(t for (t, x, y) in seen))
    return [ts[i + 1] - ts[i] for i in range(len(ts) - 1)]


def verify_demos(verbose=True):
    ok = True

    # --- clock: clean periods on all three lines ------------------------
    art = demo_clock().text()
    seen = pulses_at(art, [(140, 12), (140, 27), (140, 44)], 400)
    n = [len([1 for (t, x, y) in seen if y == yy]) for yy in (12, 27, 44)]
    sp = [sorted(set(spacings([s for s in seen if s[2] == yy])))
          for yy in (12, 27, 44)]
    good = all(v >= 2 for v in n) and sp == [[8], [12], [16]]
    ok &= good
    if verbose:
        print("  %s CLOCK: %d/%d/%d pulses on the three lines, periods %s "
              "(expected 8/12/16)" % ("OK " if good else "FAIL",
                                      n[0], n[1], n[2], [s[0] for s in sp]))

    # --- diode: forward branch conducts, reverse branch silent ----------
    art = demo_diode().text()
    seen = pulses_at(art, [(140, 12), (140, 40)], 400)
    up = len([1 for (t, x, y) in seen if y == 12])
    down = len([1 for (t, x, y) in seen if y == 40])
    sp = set(spacings([s for s in seen if s[2] == 12]))
    good = up >= 2 and down == 0 and sp == {8}
    ok &= good
    if verbose:
        print("  %s DIODE: %d pulses through the forward diode (period %s), "
              "%d through the reverse one"
              % ("OK " if good else "FAIL", up, sorted(sp), down))

    # --- OR / XOR -------------------------------------------------------
    or_art = demo_or().text()
    xor_art = demo_xor().text()
    or_out = len(pulses_at(or_art, [(140, 22)], 480))
    or_in = len(pulses_at(or_art, [(42, 20), (42, 24)], 480))
    xor_out = len(pulses_at(xor_art, [(140, 21)], 480))
    xor_in = len(pulses_at(xor_art, [(42, 18), (42, 24)], 480))

    good = or_out >= 3
    ok &= good
    if verbose:
        print("  %s OR: %d pulses in, %d pulses out" %
              ("OK " if good else "FAIL", or_in, or_out))

    # XOR drops both electrons when two meet, OR only merges them, so the
    # XOR output must be strictly quieter than the OR output.
    good = xor_out >= 2 and xor_out < or_out and xor_in > 0
    ok &= good
    if verbose:
        print("  %s XOR: %d pulses in, %d pulses out (OR gives %d -> "
              "%d cancelled)" % ("OK " if good else "FAIL", xor_in, xor_out,
                                 or_out, or_out - xor_out))
    return ok


# ---------------------------------------------------------------------------
# C source generation
# ---------------------------------------------------------------------------
def c_string(art, indent='    '):
    rows = art.split('\n')
    w = max(len(r) for r in rows)
    rows = [r.ljust(w, '.') for r in rows]
    out = []
    for r in rows:
        out.append('%s"%s\\n"' % (indent, r))
    return '\n'.join(out)


DEMOS = (
    ("CLOCK", "Clock + line", demo_clock),
    ("DIODE", "Diode fwd/rev", demo_diode),
    ("OR", "OR gate", demo_or),
    ("XOR", "XOR gate", demo_xor),
)


def generate(path):
    src = []
    src.append('/*')
    src.append(' * patterns.c -- demo circuits for the Wireworld add-in.')
    src.append(' *')
    src.append(' * GENERATED FILE -- do not edit by hand.')
    src.append(' * Regenerate with:  python3 tools/build_patterns.py')
    src.append(' *')
    src.append(' * Legend:  "." empty   "#" conductor   "H" electron head   "T" electron tail')
    src.append(' *')
    src.append(' * Every circuit here was verified by simulation before being')
    src.append(' * written out (see tools/build_patterns.py).')
    src.append(' */')
    src.append('')
    src.append('#include "patterns.h"')
    src.append('')
    for i, (name, title, fn) in enumerate(DEMOS):
        src.append('static const char pat%d[] =' % i)
        src.append(c_string(fn().text()) + ';')
        src.append('')
    src.append('const Pattern patterns[] = {')
    for i, (name, title, fn) in enumerate(DEMOS):
        src.append('    { "%s", "%s", pat%d },' % (name, title, i))
    src.append('};')
    src.append('')
    src.append('const int patternCount = %d;' % len(DEMOS))
    src.append('')
    with open(path, 'w') as f:
        f.write('\n'.join(src) + '\n')
    print("wrote %s" % path)


if __name__ == '__main__':
    print("verifying components ...")
    if not verify_components():
        print("component verification FAILED")
        sys.exit(1)
    print("verifying demo circuits ...")
    if not verify_demos():
        print("demo verification FAILED")
        sys.exit(1)
    if '--dump' in sys.argv:
        for name, title, fn in DEMOS:
            print('\n===== %s (%s) =====' % (name, title))
            print(fn().text())
    else:
        import os
        out = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                           '..', 'src', 'patterns.c')
        generate(os.path.normpath(out))
    print("all checks passed")
