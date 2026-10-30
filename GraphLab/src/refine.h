#ifndef GL_REFINE_H
#define GL_REFINE_H
#include "plot.h"
#include <stdint.h>
/* Per-view saved screen samples. Camera/field changes require begin() again.
   Power-of-two stages use a dense even/even colour table (~35 KiB); an odd
   seed uses a small additional table. No final full-resolution colour copy. */
typedef struct {
    uint16_t *even,*seed;
    int seed_step,seed_done,grid_step,last_step;
} ScreenRefine;
int refine_begin(ScreenRefine *p,int seed_step);
void refine_free(ScreenRefine *p);
int refine_next(int step);
int refine_stage_allowed(ScreenRefine const *p,int step);
/* Coordinates are relative to the plot, not to the VRAM origin. */
int refine_get(ScreenRefine const *p,int x,int y,uint16_t *color);
int refine_keep(ScreenRefine const *p,int step,int x,int y);
void refine_store(ScreenRefine *p,int x,int y,uint16_t color);
void refine_commit(ScreenRefine *p,int step);
/* Paint a clipped tile into a PLOT_W*PLOT_H layer. */
void refine_tile(uint16_t *layer,int x,int y,int step,uint16_t color);
#endif
