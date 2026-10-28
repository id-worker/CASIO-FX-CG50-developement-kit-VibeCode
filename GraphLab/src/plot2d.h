#ifndef GL_PLOT2D_H
#define GL_PLOT2D_H
#include "plot.h"

/* Optional region filling for an implicit relation. Corner order matches
   (left/top,right/top,right/bottom,left/bottom). Screen rasterization is
   performed by the caller and is only an approximation of the relation. */
typedef void (*CellFillFn)(int ix,int iy,int cols,int rows,float const q[4],void *ctx);
typedef struct {
    float tolerance_px;
    int pixel_w,pixel_h;
    CellFillFn fill;
    void *fill_ctx;
} ContourOptions;
int plot_contour_opt(Program const *pr,EvalVars const *base,PlaneView const *view,
                     SegmentFn segment,void *segment_ctx,ContourOptions const *options,
                     ProgressFn progress,void *progress_ctx,PlotStats *stats);

/* One f(x) evaluation per screen column for safely isolated y REL f(x).
   fill receives pixel indices relative to the plot rectangle, inclusive. */
typedef void (*SpanFillFn)(int x,int first_y,int last_y,void *ctx);
int plot_explicit_y(Program const *rhs,Relation relation,EvalVars const *base,
                    PlaneView const *view,int width,int height,
                    SegmentFn segment,void *segment_ctx,SpanFillFn fill,void *fill_ctx,
                    ProgressFn progress,void *progress_ctx,PlotStats *stats);
#endif
