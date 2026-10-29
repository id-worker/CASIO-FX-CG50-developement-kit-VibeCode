/*
 * wireworld.c -- see wireworld.h
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
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

static int ww_step_generic(WWWorld *w, WWCellHook hook, void *ctx)
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

/* Read two aligned 32-bit words without aliasing violations. A set bit 1 in
   any byte means that group may contain a head or a tail.  The mask is
   endian-independent, and this fast path is used only on aligned buffers. */
#if defined(__GNUC__)
typedef uint32_t ww_alias_u32 __attribute__((__may_alias__));
static __inline__ int group_has_activity(const unsigned char *p)
{
    const ww_alias_u32 *a = (const ww_alias_u32 *)(const void *)p;
    return ((a[0] | a[1]) & 0x02020202u) != 0;
}

/* Standard calculator world widths are all multiples of eight. Snapshot the
   heads eight cells at a time; inactive groups need just two aligned word
   reads. Only an active bounding region, plus toroidal seams, can change.
   Within that region, skip groups without nearby heads or tails. The mask,
   cell encoding and file format remain identical to the generic engine. */
static int ww_step_aligned(WWWorld *w, WWCellHook hook, void *ctx)
{
    unsigned char *cells = w->cells, *heads = w->heads;
    int W = w->w, H = w->h, B = W / 8;
    int min_b = B, max_b = -1, min_y = H, max_y = -1;
    int b, y, changed = 0;

    /* For a very busy field, activity skipping cannot pay for itself. Sample
       up to 128 groups across the world and use the original dense loop when
       most groups have heads or tails. This choice never affects the result. */
    {
        int groups = B * H, samples, stride, busy = 0, i;
        if (groups < 128) { samples = groups; stride = 1; }
        else { samples = 128; stride = groups >> 7; }
        for (i = 0; i < samples; i++) {
            int group = i * stride + (i & (stride - 1));
            if (group_has_activity(cells + (size_t)group * 8)) busy++;
        }
        if (samples >= 16 && busy > samples / 2)
            return ww_step_generic(w, hook, ctx);
    }

    /* Pass 1: exact head snapshot and a conservative activity rectangle. */
    for (y = 0; y < H; y++) {
        unsigned char *p = cells + (size_t)y * W;
        unsigned char *h = heads + (size_t)y * B;
        for (b = 0; b < B; b++, p += 8) {
            unsigned char bits = 0;
            if (group_has_activity(p)) {
                int bit;
                for (bit = 0; bit < 8; bit++)
                    if (p[bit] == WW_HEAD) bits |= (unsigned char)(1u << bit);
                if (b < min_b) min_b = b;
                if (b > max_b) max_b = b;
                if (y < min_y) min_y = y;
                if (y > max_y) max_y = y;
            }
            h[b] = bits;
        }
    }

    if (max_b >= 0) {
        int xl = min_b * 8 - 1, xr = max_b * 8 + 8;
        int yl = min_y - 1, yr = max_y + 1;
        int top_needed = min_y <= 1 || max_y == H - 1;
        int bot_needed = max_y >= H - 2 || min_y == 0;
        int side_needed = min_b == 0 || max_b == B - 1;
        int x;
        if (xl < 1) xl = 1;
        if (xr > W - 2) xr = W - 2;
        if (yl < 1) yl = 1;
        if (yr > H - 2) yr = H - 2;

        /* Pass 2: interior, still in the original row-major hook order. */
        for (y = yl; y <= yr; y++) {
            int above = (y - 1) * B, here = y * B, below = (y + 1) * B;
            int first = xl / 8, last = xr / 8;
            int pb = first ? first - 1 : B - 1;
            unsigned prev = heads[above + pb] | heads[here + pb] | heads[below + pb];
            unsigned cur = heads[above + first] | heads[here + first] | heads[below + first];
            for (b = first; b <= last; b++) {
                int nb = (b + 1 < B) ? b + 1 : 0;
                unsigned next = heads[above + nb] | heads[here + nb] | heads[below + nb];
                int from = b * 8, to = from + 7;
                if (from < xl) from = xl;
                if (to > xr) to = xr;
                if (!(prev | cur | next)) {
                    /* With no head in these nine blocks, conductors stay
                       unchanged. Only tails in the group can change. */
                    if (group_has_activity(cells + (size_t)y * W + b * 8)) {
                        for (x = from; x <= to; x++) {
                            int idx = y * W + x;
                            if (cells[idx] == WW_TAIL) {
                                cells[idx] = WW_CONDUCTOR;
                                changed++;
                                if (hook) hook(x, y, WW_CONDUCTOR, ctx);
                            }
                        }
                    }
                } else {
                    int idx = y * W + from;
                    for (x = from; x <= to; x++, idx++) {
                        int c = cells[idx], n;
                        if (c == WW_EMPTY) continue;
                        if (c == WW_HEAD) cells[idx] = WW_TAIL;
                        else if (c == WW_TAIL) cells[idx] = WW_CONDUCTOR;
                        else {
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
                prev = cur;
                cur = next;
            }
        }

        /* Pass 3: borders in the same order as the generic version. */
        if (top_needed || bot_needed) {
            for (x = 0; x < W; x++) {
                if (top_needed) update_wrapped(w, x, 0, hook, ctx, &changed);
                if (bot_needed) update_wrapped(w, x, H - 1, hook, ctx, &changed);
            }
        }
        if (side_needed) {
            for (y = 1; y < H - 1; y++) {
                update_wrapped(w, 0, y, hook, ctx, &changed);
                update_wrapped(w, W - 1, y, hook, ctx, &changed);
            }
        }
    }
    w->gen++;
    w->changed = changed;
    return changed;
}
#endif

int ww_step_readonly_hook(WWWorld *w, WWCellHook hook, void *ctx)
{
    if (!w || !w->cells || !w->heads) return -1;
#if defined(__GNUC__)
    if (w->w >= 8 && !(w->w & 7) && w->h > 2 &&
        !((uintptr_t)w->cells & 3u))
        return ww_step_aligned(w, hook, ctx);
#endif
    return ww_step_generic(w, hook, ctx);
}

int ww_step(WWWorld *w, WWCellHook hook, void *ctx)
{
    /* A general hook may edit a cell outside the precomputed active region.
       Preserve 1.63 semantics in that case. The app uses the explicit
       read-only-hook entry point for its dirty rectangle notifications. */
    if (hook) return ww_step_generic(w, hook, ctx);
    return ww_step_readonly_hook(w, NULL, ctx);
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
