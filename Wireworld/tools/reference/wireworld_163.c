/*
 * wireworld.c -- see wireworld.h
 */
#include <stdlib.h>
#include <string.h>
#include "wireworld.h"

int ww_init(WWWorld *w, int width, int height)
{
    size_t ncells, nmask;

    if (!w || width <= 0 || height <= 0) return -1;
    w->w = width;
    w->h = height;
    w->gen = 0;
    w->changed = 0;
    w->cells = NULL;
    w->heads = NULL;

    ncells = (size_t)width * (size_t)height;
    nmask = (ncells + 7) / 8;

    w->cells = (unsigned char *)malloc(ncells);
    w->heads = (unsigned char *)malloc(nmask);
    if (!w->cells || !w->heads) {
        free(w->cells);
        free(w->heads);
        w->cells = w->heads = NULL;
        return -1;
    }
    memset(w->cells, WW_EMPTY, ncells);
    memset(w->heads, 0, nmask);
    return 0;
}

void ww_free(WWWorld *w)
{
    if (!w) return;
    free(w->cells);
    free(w->heads);
    w->cells = w->heads = NULL;
    w->w = w->h = 0;
}

void ww_clear(WWWorld *w)
{
    size_t ncells;
    if (!w || !w->cells) return;
    ncells = (size_t)w->w * (size_t)w->h;
    memset(w->cells, WW_EMPTY, ncells);
    memset(w->heads, 0, (ncells + 7) / 8);
    w->gen = 0;
    w->changed = 0;
}

/* ------------------------------------------------------------------ */
/* one generation                                                      */
/* ------------------------------------------------------------------ */
#define HEADBIT(mask, i) (((mask)[(i) >> 3] >> ((i) & 7)) & 1)

/* update a single cell, wrapping coordinates (used for the border cells) */
static void update_wrapped(WWWorld *w, int x, int y,
                           WWCellHook hook, void *ctx, int *changed)
{
    unsigned char *cells = w->cells;
    unsigned char *heads = w->heads;
    int W = w->w, H = w->h;
    int idx = y * W + x;
    int c = cells[idx];
    int n = 0, dx, dy;

    if (c == WW_EMPTY) return;

    if (c == WW_HEAD) {
        cells[idx] = WW_TAIL;
    } else if (c == WW_TAIL) {
        cells[idx] = WW_CONDUCTOR;
    } else {
        for (dy = -1; dy <= 1; dy++) {
            int yy = y + dy;
            int row;
            if (yy < 0) yy += H; else if (yy >= H) yy -= H;
            row = yy * W;
            for (dx = -1; dx <= 1; dx++) {
                int xx = x + dx;
                if (!dx && !dy) continue;
                if (xx < 0) xx += W; else if (xx >= W) xx -= W;
                n += HEADBIT(heads, row + xx);
            }
        }
        if (n == 1 || n == 2) cells[idx] = WW_HEAD;
        else return;                    /* conductor stays conductor */
    }

    (*changed)++;
    if (hook) hook(x, y, cells[idx], ctx);
}

int ww_step(WWWorld *w, WWCellHook hook, void *ctx)
{
    unsigned char *cells;
    unsigned char *heads;
    int W, H, x, y, i, n, changed = 0;

    if (!w || !w->cells || !w->heads) return -1;

    cells = w->cells;
    heads = w->heads;
    W = w->w;
    H = w->h;

    /* ---- pass 1: remember where the heads are ------------------------ */
    memset(heads, 0, (size_t)(((size_t)W * H + 7) / 8));
    for (i = 0; i < W * H; i++)
        if (cells[i] == WW_HEAD) heads[i >> 3] |= (unsigned char)(1 << (i & 7));

    /* ---- pass 2: interior, no wrap-around tests --------------------- */
    for (y = 1; y < H - 1; y++) {
        int idx = y * W + 1;
        for (x = 1; x < W - 1; x++, idx++) {
            int c = cells[idx];

            if (c == WW_EMPTY) continue;

            if (c == WW_HEAD) {
                cells[idx] = WW_TAIL;
            } else if (c == WW_TAIL) {
                cells[idx] = WW_CONDUCTOR;
            } else {
                n = HEADBIT(heads, idx - W - 1) + HEADBIT(heads, idx - W) +
                    HEADBIT(heads, idx - W + 1) + HEADBIT(heads, idx - 1) +
                    HEADBIT(heads, idx + 1) + HEADBIT(heads, idx + W - 1) +
                    HEADBIT(heads, idx + W) + HEADBIT(heads, idx + W + 1);
                if (n == 1 || n == 2) cells[idx] = WW_HEAD;
                else continue;
            }
            changed++;
            if (hook) hook(x, y, cells[idx], ctx);
        }
    }

    /* ---- pass 3: the four borders (coordinates wrap) ---------------- */
    if (W > 2 && H > 2) {
        for (x = 0; x < W; x++) {
            update_wrapped(w, x, 0, hook, ctx, &changed);
            update_wrapped(w, x, H - 1, hook, ctx, &changed);
        }
        for (y = 1; y < H - 1; y++) {
            update_wrapped(w, 0, y, hook, ctx, &changed);
            update_wrapped(w, W - 1, y, hook, ctx, &changed);
        }
    } else {                              /* degenerate field: slow path */
        for (y = 0; y < H; y++)
            for (x = 0; x < W; x++)
                update_wrapped(w, x, y, hook, ctx, &changed);
    }

    w->gen++;
    w->changed = changed;
    return changed;
}

int ww_load_text(WWWorld *w, const char *art, int ox, int oy)
{
    int x = 0, y = 0, written = 0;
    const char *p = art;

    if (!w || !art || !w->cells) return 0;

    for (; *p; p++) {
        int state = -1;

        switch (*p) {
        case '\n':
            x = 0;
            y++;
            continue;
        case '\r':
            continue;
        case '.':
        case ' ':
            x++;
            continue;
        case '#': state = WW_CONDUCTOR; break;
        case 'H': state = WW_HEAD;      break;
        case 'T': state = WW_TAIL;      break;
        default:
            x++;
            continue;
        }

        if (state >= 0 && x < 4096) {          /* ignore runaway rows */
            ww_set(w, ox + x, oy + y, state);
            written++;
        }
        x++;
    }
    return written;
}
