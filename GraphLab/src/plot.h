#ifndef GL_PLOT_H
#define GL_PLOT_H
#include "expr.h"
#include <stdint.h>
#include <stddef.h>
#define PLOT_X0 5
#define PLOT_X1 390
#define PLOT_Y0 19
#define PLOT_Y1 199
#define PLOT_W (PLOT_X1-PLOT_X0+1)
#define PLOT_H (PLOT_Y1-PLOT_Y0+1)
#define PLOT_MAX_DIV 48
/* Limit for the retained legacy in-memory Mesh API only. The GraphLab 1.02
   UI uses ray3d_render() or voxel3d_render(), not a polygonal surface mesh. */
#define PLOT_MAX_TRIANGLES 6000

typedef struct { float xmin,xmax,ymin,ymax; int cols,rows; float period_x; } PlaneView;
typedef struct { int samples, cells, segments, invalid, rejected, all_zero, canceled, evaluations; } PlotStats;
typedef void (*SegmentFn)(float x0,float y0,float x1,float y1,void *ctx);
/* Return nonzero to continue, 0 to cancel. Called periodically. */
typedef int (*ProgressFn)(int done,int total,void *ctx);
/* With no '=', an expression using y is interpreted as F(x,y)=0; one without
 * y is interpreted as y=expression. An explicit '=' always means lhs=rhs. */
int plot_contour(Program const *pr, EvalVars const *base, PlaneView const *view,
                 SegmentFn segment, void *segment_ctx,
                 ProgressFn progress, void *progress_ctx, PlotStats *stats);

typedef struct { int16_t x,y,z; } Point3;
typedef struct { Point3 p[3]; uint8_t slot; uint8_t pad; } Triangle3;
typedef int (*TriangleFn)(Triangle3 const *triangle,void *ctx);
typedef struct { Triangle3 *tri; int count, capacity, truncated; TriangleFn emit; void *emit_ctx; } Mesh;
typedef struct { float x,y,z,radius; int div; float period_x; } Volume;
typedef struct {
    float yaw,pitch,zoom; int wire,axes;
    /* Domain cube, for locating the actual mathematical coordinate axes. */
    float center_x,center_y,center_z,radius;
    /* Actual observer coordinates; used when perspective is nonzero. */
    float eye_x,eye_y,eye_z;
    int perspective;
} Camera;
int mesh_init(Mesh *m,int capacity);
void mesh_free(Mesh *m);
/* If no '=' and z is unused, interpret expression as z=expression;
 * otherwise use F(x,y,z)=0. Append triangles with the given color slot. */
int plot_surface(Mesh *mesh, Program const *pr, EvalVars const *base,
                 Volume const *vol,int slot,ProgressFn progress,void *progress_ctx,
                 PlotStats *stats);
/* Streams triangles directly to a consumer, using two sampling planes.
   No triangle count cap; callback returning 0 cancels the operation. */
int plot_surface_stream(Program const *pr,EvalVars const *base,Volume const *vol,
                        int slot,TriangleFn emit,void *emit_ctx,
                        ProgressFn progress,void *progress_ctx,PlotStats *stats);
/* Uses gint's DWIDTH stride and a dynamically allocated Z buffer. */
int mesh_render(Mesh const *mesh,Camera const *cam,ProgressFn progress,void *progress_ctx);
/* Streaming perspective rasterizer. A completed frame is kept in gint_vram. */
typedef struct Render3D Render3D;
Render3D *render3_begin(Camera const *cam);
int render3_triangle(Triangle3 const *triangle,void *ctx);
void render3_end(Render3D *renderer);
/* Screen-clipped perspective axis geometry, also exposed for regression tests.
   Tick values are world coordinates and may use adaptive spacing in 3D. */
#define AXIS3_MAX_TICKS 16
typedef struct {float value;int x,y;} Axis3Tick;
typedef struct {
    int x0,y0,x1,y1,count;
    float world_start,world_end,reference_step;
    Axis3Tick tick[AXIS3_MAX_TICKS];
} Axis3Geometry;
int render3_axis_geometry(Camera const *cam,int axis,Axis3Geometry *geometry);
int render3_project_world(Camera const *cam,float x,float y,float z,float *sx,float *sy);
/* Draw labeled mathematical axes into the current VRAM (without depth writes). */
void render3_axes(Camera const *cam);
#endif
