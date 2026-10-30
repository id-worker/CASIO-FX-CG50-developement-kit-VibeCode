#ifndef GL_PREVIEW3D_H
#define GL_PREVIEW3D_H
#include "plot.h"
#include <stdint.h>
/* Previously visible ray hits plus optional coarse surface samples. No mesh
   or polygons: neither point set is used for the final PLOT. Positions are
   relative to the fixed clip cube, quantized to signed 16-bit coordinates. */
#define PREVIEW3D_STEP 4
#define PREVIEW3D_AUX_CAPACITY 2048
typedef struct {int16_t x,y,z;uint16_t color;} Preview3DPoint;
typedef struct {
    Preview3DPoint *point;int count,capacity,step;
    Preview3DPoint *aux;int aux_count,aux_seen;
    uint16_t *depth; /* allocated lazily, reused between preview frames */
    Volume volume;Camera captured;
} Preview3D;
/* 4 is the original 1.09 sampling; 8 is faster/coarser, 2 denser.
   Set before recapture; a changed sampling requires a new final PLOT. */
void preview3d_set_step(Preview3D *preview,int step);
/* Initialize a zeroed object; failure merely disables the optional preview. */
int preview3d_begin(Preview3D *preview,Volume const *volume,Camera const *captured);
/* Reset the visible hits while optionally keeping the same-volume coarse
   cloud when the caller is reusing its RAY candidate grid. */
int preview3d_recapture(Preview3D *preview,Volume const *volume,Camera const *captured,int retain_aux);
void preview3d_free(Preview3D *preview);
/* Optional, bounded reservoir of approximate points already sampled during
   grid construction; no extra equation evaluations. */
int preview3d_aux_begin(Preview3D *preview);
void preview3d_aux_add(Preview3D *preview,float nx,float ny,float nz,uint16_t color);
void preview3d_add(Preview3D *preview,int pixel_x,int pixel_y,
                   float world_x,float world_y,float world_z,uint16_t color);
/* Reproject cached points into gint_vram using a compact approximate depth
   buffer. Caller clears VRAM first and does not display partial frames.
   Returns 1, or -1 on invalid input, or -2 if memory is unavailable. */
int preview3d_draw(Preview3D *preview,Camera const *camera);
#endif
