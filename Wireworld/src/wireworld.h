/*
 * wireworld.h -- the Wireworld cellular automaton, platform independent part.
 *
 * Rules (Moore neighbourhood, the world is a torus -- electrons that leave one
 * edge come back in on the opposite one, so the field never ends):
 *
 *   empty      -> empty
 *   head       -> tail
 *   tail       -> conductor
 *   conductor  -> head if exactly 1 or 2 of the 8 neighbours are heads,
 *                 otherwise stays conductor
 *
 * Memory: one byte per cell plus a one-bit-per-cell mask of the electron
 * heads.  The head mask is a snapshot of the current generation, which makes
 * it possible to update the grid in place -- that is what lets the field be
 * much bigger than the screen (a second full buffer would not fit in the
 * calculator's heap).
 */
#ifndef WIREWORLD_H
#define WIREWORLD_H

#ifdef __cplusplus
extern "C" {
#endif

/* cell states */
#define WW_EMPTY     0
#define WW_CONDUCTOR 1
#define WW_HEAD      2
#define WW_TAIL      3

typedef struct WWWorld {
    int w;                /* width  in cells                        */
    int h;                /* height in cells                        */
    unsigned char *cells; /* w * h bytes                            */
    unsigned char *heads; /* w * h bits, snapshot of the heads      */
    unsigned long gen;    /* generation counter                     */
    int changed;          /* cells changed by the last step         */
} WWWorld;

/* called for every cell the last step modified */
typedef void (*WWCellHook)(int x, int y, int state, void *ctx);

/* 0 on success, -1 when there is not enough memory. */
int ww_init(WWWorld *w, int width, int height);
void ww_free(WWWorld *w);

/* wipes the field (all cells empty, generation back to 0) */
void ww_clear(WWWorld *w);

/* Advances the world by one generation, in place.  Returns the number of
   changed cells (0 means nothing will ever move again without editing).
   `hook` may be NULL. */
int ww_step(WWWorld *w, WWCellHook hook, void *ctx);

static __inline__ int ww_get(const WWWorld *w, int x, int y)
{
    if (!w || !w->cells || x < 0 || y < 0 || x >= w->w || y >= w->h)
        return WW_EMPTY;
    return w->cells[y * w->w + x];
}

static __inline__ void ww_set(WWWorld *w, int x, int y, int state)
{
    if (!w || !w->cells || x < 0 || y < 0 || x >= w->w || y >= w->h) return;
    w->cells[y * w->w + x] = (unsigned char)state;
}

/* Loads ASCII art (rows separated by '\n') with its top left corner at
   (ox, oy).  '.' and ' ' are empty, '#' conductor, 'H' electron head,
   'T' electron tail.  Returns the number of non empty cells written. */
int ww_load_text(WWWorld *w, const char *art, int ox, int oy);

/* bytes taken by one field of that size */
#define ww_memory(width, height) \
    ((size_t)(width) * (size_t)(height) + \
     (size_t)(((width) * (height) + 7) / 8))

/* wrap a coordinate into [0, n) -- only valid for values in (-n, 2n) */
static __inline__ int ww_wrap(int v, int n)
{
    if (v < 0) v += n;
    else if (v >= n) v -= n;
    return v;
}

#ifdef __cplusplus
}
#endif

#endif /* WIREWORLD_H */
