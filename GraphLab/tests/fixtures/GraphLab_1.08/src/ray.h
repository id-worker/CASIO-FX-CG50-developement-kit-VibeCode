#ifndef GL_RAY_H
#define GL_RAY_H
#include "plot.h"
#include "preview3d.h"
/* Direct implicit-surface ray casting: no generated triangles or polygons.
   The finite search grid is only an acceleration structure. */
#define RAY_MAX_DIV 64
#define RAY_MAX_SURFACES 2
typedef struct { Program const *program; int slot; } RaySurface;
typedef struct { int rays, hits, occupied, evaluations, canceled,grid_reused,height_fields,fast_evaluations; } RayStats;
/* Candidate bits depend on field, parameters and fixed clip cube, not camera.
   This optional cache is private to one graph view. Clear it when finished. */
typedef struct {
    unsigned char *bits;size_t bytes;int valid,count,occupied;
    Volume volume;EvalVars base;Program signature[RAY_MAX_SURFACES];
    unsigned char height[RAY_MAX_SURFACES];
} RayGridCache;
typedef struct {
    Program const *height[RAY_MAX_SURFACES]; /* equivalent f(x,y), or NULL */
    RayGridCache *grid;Preview3D *preview;
} RayRenderOptions;
void ray_grid_cache_clear(RayGridCache *cache);
/* Writes a complete image to gint_vram, but never calls dupdate(). Returns
   1 on success, 0 on cancellation, -1 for invalid inputs, -2 for memory. */
int ray3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *volume,Camera const *camera,
                 ProgressFn progress,void *progress_ctx,RayStats *stats);
int ray3d_render_ex(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *volume,Camera const *camera,RayRenderOptions const *options,
                 ProgressFn progress,void *progress_ctx,RayStats *stats);
#endif
