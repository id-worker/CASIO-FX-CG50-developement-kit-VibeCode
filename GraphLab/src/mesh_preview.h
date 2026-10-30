#ifndef GL_MESH_PREVIEW_H
#define GL_MESH_PREVIEW_H
#include "ray.h"
/* Optional temporary indexed mesh, only for navigating a 3D preview.
   It is never used by the final RAY renderer or written to storage. */
#define PM_MAX_VERTICES 6500
#define PM_MAX_TRIANGLES 9000

typedef struct { uint16_t a,b,c,color; } PMTriangle;
typedef struct {
    Point3 *vertices; PMTriangle *triangles;
    void *projected; uint16_t *depth;
    int vertex_count,triangle_count,div,valid,overflow;
    Volume volume;
} PreviewMesh;
/* 1 built; 0 canceled; -1 unsupported/complex or -2 allocation failure.
   Surface sampling is an EXTRA, one-time approximation, not the RAY result. */
int preview_mesh_build(PreviewMesh *,RaySurface const *,int,EvalVars const *,
                       Volume const *,ProgressFn,void *);
/* 1 success, 0 invalid/unavailable camera or memory. No expression evaluation. */
int preview_mesh_draw(PreviewMesh *,Camera const *);
void preview_mesh_free(PreviewMesh *);
#endif
