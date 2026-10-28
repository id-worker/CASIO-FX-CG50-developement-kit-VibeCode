#ifndef GL_VOXEL_H
#define GL_VOXEL_H
#include "ray.h"
#define VOXEL_MIN_DIV 6
#define VOXEL_MAX_DIV 64
/* 2D slices become voxel occupancy. A per-pixel depth buffer draws exposed
   voxel faces directly, without constructing a polygon/triangle mesh. */
typedef struct {
    int samples,evaluations,invalid,cells,occupied,rejected,faces,pixels,canceled;
} VoxelStats;
int voxel3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                   Volume const *volume,Camera const *camera,
                   ProgressFn progress,void *progress_ctx,VoxelStats *stats);
#endif
