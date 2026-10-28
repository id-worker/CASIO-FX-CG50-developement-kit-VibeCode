/*
 * test_native.c -- host side test of the Wireworld engine and of the demo
 * circuits.  It exercises exactly the same wireworld.c / patterns.c that get
 * compiled into the add-in.
 *
 *   gcc -std=c99 -O2 -Wall -Wextra -I../src -o /tmp/wwtest \
 *       tools/test_native.c src/wireworld.c src/patterns.c
 *   /tmp/wwtest
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wireworld.h"
#include "patterns.h"

/* any size will do, the engine does not care; the add-in uses a bigger one */
#define FIELD_W 192
#define FIELD_H 101
#define PAT_X  2     /* circuits are loaded at (2,2) */
#define PAT_Y  2

static int failures = 0;

static void check(const char *what, int got, int expected)
{
    printf("  %-46s %4d   (expected %d)  %s\n", what, got, expected,
           got == expected ? "ok" : "FAIL");
    if (got != expected) failures++;
}

static void check_true(const char *what, int cond)
{
    check(what, cond ? 1 : 0, 1);
}

/* ------------------------------------------------------------------ */
/* a deliberately naive reference implementation (own copy buffer,     */
/* straightforward wrapping) to compare the optimised one against      */
/* ------------------------------------------------------------------ */
static void ref_step(WWWorld *w)
{
    int W = w->w, H = w->h, x, y, dx, dy;
    unsigned char *old = (unsigned char *)malloc((size_t)W * H);

    memcpy(old, w->cells, (size_t)W * H);
    for (y = 0; y < H; y++) {
        for (x = 0; x < W; x++) {
            int c = old[y * W + x], n = 0;
            if (c == WW_EMPTY) { w->cells[y * W + x] = WW_EMPTY; continue; }
            if (c == WW_HEAD)  { w->cells[y * W + x] = WW_TAIL;  continue; }
            if (c == WW_TAIL)  { w->cells[y * W + x] = WW_CONDUCTOR; continue; }
            for (dy = -1; dy <= 1; dy++)
                for (dx = -1; dx <= 1; dx++) {
                    int xx = x + dx, yy = y + dy;
                    if (!dx && !dy) continue;
                    if (xx < 0) xx += W; else if (xx >= W) xx -= W;
                    if (yy < 0) yy += H; else if (yy >= H) yy -= H;
                    if (old[yy * W + xx] == WW_HEAD) n++;
                }
            w->cells[y * W + x] = (n == 1 || n == 2) ? WW_HEAD : WW_CONDUCTOR;
        }
    }
    w->gen++;
    free(old);
}

/* ------------------------------------------------------------------ */
static int watch(WWWorld *w, int x, int y, int steps, int *spacing)
{
    int i, n = 0, last = -1, sp = 0, uniform = 1;

    for (i = 0; i < steps; i++) {
        ww_step(w, NULL, NULL);
        if (ww_get(w, x, y) == WW_HEAD) {
            n++;
            if (last >= 0) {
                int d = i - last;
                if (sp && d != sp) uniform = 0;
                sp = d;
            }
            last = i;
        }
    }
    *spacing = uniform ? sp : -1;
    return n;
}

static void dump_ascii(WWWorld *w, int rows)
{
    int x, y;
    static const char ch[4] = { '.', '#', 'H', 'T' };
    for (y = 0; y < rows && y < w->h; y++) {
        for (x = 0; x < w->w && x < 150; x++) putchar(ch[ww_get(w, x, y)]);
        putchar('\n');
    }
}

static int load(WWWorld *w, int idx)
{
    if (ww_init(w, FIELD_W, FIELD_H) != 0) return 0;
    ww_load_text(w, patterns[idx].art, PAT_X, PAT_Y);
    return 1;
}

static int find_pattern(const char *name)
{
    int i;
    for (i = 0; i < patternCount; i++)
        if (!strncmp(patterns[i].name, name, strlen(name))) return i;
    return 0;
}

int main(void)
{
    WWWorld w;
    int sp, n, idx;

    printf("rule sanity checks\n");
    ww_init(&w, 16, 16);

    /* an electron walking along a wire */
    ww_set(&w, 1, 1, WW_CONDUCTOR);
    ww_set(&w, 2, 1, WW_CONDUCTOR);
    ww_set(&w, 3, 1, WW_CONDUCTOR);
    ww_set(&w, 4, 1, WW_CONDUCTOR);
    ww_set(&w, 2, 1, WW_HEAD);
    ww_set(&w, 1, 1, WW_TAIL);

    ww_step(&w, NULL, NULL);
    check("head becomes tail",            ww_get(&w, 2, 1) == WW_TAIL, 1);
    check("tail becomes conductor",       ww_get(&w, 1, 1) == WW_CONDUCTOR, 1);
    check("next conductor becomes head",  ww_get(&w, 3, 1) == WW_HEAD, 1);
    check("nothing teleports two cells",  ww_get(&w, 4, 1) == WW_CONDUCTOR, 1);

    ww_step(&w, NULL, NULL);
    check("electron moved one cell",      ww_get(&w, 4, 1) == WW_HEAD, 1);
    check("... leaving a tail behind",    ww_get(&w, 3, 1) == WW_TAIL, 1);

    /* three heads around a conductor must NOT excite it */
    ww_clear(&w);
    ww_set(&w, 3, 3, WW_CONDUCTOR);
    ww_set(&w, 2, 2, WW_HEAD);
    ww_set(&w, 3, 2, WW_HEAD);
    ww_set(&w, 4, 2, WW_HEAD);
    ww_step(&w, NULL, NULL);
    check("3 neighbours block the conductor", ww_get(&w, 3, 3) == WW_CONDUCTOR, 1);

    /* two heads do excite it */
    ww_clear(&w);
    ww_set(&w, 3, 3, WW_CONDUCTOR);
    ww_set(&w, 2, 2, WW_HEAD);
    ww_set(&w, 3, 2, WW_HEAD);
    ww_step(&w, NULL, NULL);
    check("2 neighbours excite the conductor", ww_get(&w, 3, 3) == WW_HEAD, 1);

    /* the world is a torus: an electron leaving on the right comes back left */
    ww_clear(&w);
    {
        int x;
        for (x = 0; x < 16; x++) ww_set(&w, x, 0, WW_CONDUCTOR);
        ww_set(&w, 15, 0, WW_HEAD);
        ww_set(&w, 14, 0, WW_TAIL);
        ww_step(&w, NULL, NULL);
        check("electron wraps around the right edge", ww_get(&w, 0, 0) == WW_HEAD, 1);
        /* and vertically */
        for (x = 0; x < 16; x++) ww_set(&w, 0, x, WW_CONDUCTOR);
        ww_set(&w, 0, 15, WW_HEAD);
        ww_set(&w, 0, 14, WW_TAIL);
        ww_clear(&w);
        for (x = 0; x < 16; x++) ww_set(&w, 0, x, WW_CONDUCTOR);
        ww_set(&w, 0, 15, WW_HEAD);
        ww_set(&w, 0, 14, WW_TAIL);
        ww_step(&w, NULL, NULL);
        check("electron wraps around the bottom edge", ww_get(&w, 0, 0) == WW_HEAD, 1);
    }
    ww_free(&w);

    /* ---------------------------------------------------------------- */
    printf("\noptimised step vs. naive reference (XOR circuit, 300 generations)\n");
    {
        WWWorld a, b;
        int i, bad = -1;
        load(&a, find_pattern("XOR"));
        ww_init(&b, FIELD_W, FIELD_H);
        memcpy(b.cells, a.cells, (size_t)FIELD_W * FIELD_H);

        for (i = 0; i < 300 && bad < 0; i++) {
            ww_step(&a, NULL, NULL);
            ref_step(&b);
            if (memcmp(a.cells, b.cells, (size_t)FIELD_W * FIELD_H) != 0)
                bad = i;
        }
        check_true("both implementations agree", bad < 0);
        if (bad >= 0) printf("    first difference in generation %d\n", bad);
        ww_free(&a);
        ww_free(&b);
    }

    /* ---------------------------------------------------------------- */
    printf("\noptimised step vs. naive reference (circuit placed on the border)\n");
    {
        WWWorld a, b;
        int i, bad = -1;
        ww_init(&a, FIELD_W, FIELD_H);
        ww_load_text(&a, patterns[find_pattern("DIODE")].art, 0, 0);
        ww_init(&b, FIELD_W, FIELD_H);
        memcpy(b.cells, a.cells, (size_t)FIELD_W * FIELD_H);

        for (i = 0; i < 200 && bad < 0; i++) {
            ww_step(&a, NULL, NULL);
            ref_step(&b);
            if (memcmp(a.cells, b.cells, (size_t)FIELD_W * FIELD_H) != 0)
                bad = i;
        }
        check_true("border handling agrees too", bad < 0);
        if (bad >= 0) printf("    first difference in generation %d\n", bad);
        ww_free(&a);
        ww_free(&b);
    }

    /* ---------------------------------------------------------------- */
    idx = find_pattern("CLOCK");
    printf("\ndemo circuit: %s\n", patterns[idx].name);
    load(&w, idx);
    n = watch(&w, 142, 14, 400, &sp);
    check_true("pulses on line 1", n > 0);
    check("period of clock 1 (generations)", sp, 8);
    ww_free(&w);

    load(&w, idx);
    n = watch(&w, 142, 29, 400, &sp);
    check_true("pulses on line 2", n > 0);
    check("period of clock 2 (generations)", sp, 12);
    ww_free(&w);

    load(&w, idx);
    n = watch(&w, 142, 46, 400, &sp);
    check_true("pulses on line 3", n > 0);
    check("period of clock 3 (generations)", sp, 16);
    ww_free(&w);

    idx = find_pattern("DIODE");
    printf("\ndemo circuit: %s\n", patterns[idx].name);
    load(&w, idx);
    n = watch(&w, 142, 14, 400, &sp);
    printf("  %-46s %4d\n", "pulses through the forward diode", n);
    check_true("forward diode conducts", n > 2);
    check("... with the clock period", sp, 8);
    ww_free(&w);

    load(&w, idx);
    n = watch(&w, 142, 42, 400, &sp);
    check("pulses through the reverse diode", n, 0);
    ww_free(&w);

    idx = find_pattern("OR");
    printf("\ndemo circuit: %s\n", patterns[idx].name);
    load(&w, idx);
    n = watch(&w, 142, 24, 480, &sp);
    printf("  %-46s %4d\n", "pulses on the OR output", n);
    check_true("OR output fires", n > 2);
    {
        int or_out = n;
        ww_free(&w);

        idx = find_pattern("XOR");
        printf("\ndemo circuit: %s\n", patterns[idx].name);
        load(&w, idx);
        n = watch(&w, 142, 23, 480, &sp);
        printf("  %-46s %4d\n", "pulses on the XOR output", n);
        check_true("XOR output fires", n > 2);
        check("XOR is quieter than OR (pulses cancel)", n < or_out ? 1 : 0, 1);
        ww_free(&w);
    }

    /* ---------------------------------------------------------------- */
    printf("\nfirst 24 rows of the XOR circuit after 60 generations:\n");
    load(&w, find_pattern("XOR"));
    {
        int i;
        for (i = 0; i < 60; i++) ww_step(&w, NULL, NULL);
        dump_ascii(&w, 24);
    }
    ww_free(&w);

    printf("\nmemory for a %dx%d field: %lu bytes\n",
           FIELD_W, FIELD_H, (unsigned long)ww_memory(FIELD_W, FIELD_H));

    printf("\n%s\n", failures ? "*** TESTS FAILED ***" : "all tests passed");
    return failures ? 1 : 0;
}
