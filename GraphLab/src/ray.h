#ifndef GL_RAY_H
#define GL_RAY_H
#include "plot.h"
/* Direct implicit-surface ray casting: no generated triangles or polygons.
   The finite search grid is only an acceleration structure. */
#define RAY_MAX_DIV 64
#define RAY_MAX_SURFACES 2
typedef struct { Program const *program; int slot; } RaySurface;
typedef struct { int rays, hits, occupied, evaluations, canceled; } RayStats;
/* Writes a complete image to gint_vram, but never calls dupdate(). Returns
   1 on success, 0 on cancellation, -1 for invalid inputs, -2 for memory. */
int ray3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *volume,Camera const *camera,
                 ProgressFn progress,void *progress_ctx,RayStats *stats);
#endif
