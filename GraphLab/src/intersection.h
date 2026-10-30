#ifndef GL_INTERSECTION_H
#define GL_INTERSECTION_H
#include "ray.h"
#include <stdint.h>
/* The map is a numerical projection, not a proof or a set of XY slices. */
#define IMAP_SIDE 181
#define IMAP_MAX_ROOTS 32
/* Refuse, never silently reduce pixel detail, for extreme zoom. */
#define IMAP_MAX_AUTO_SLICES 32768
typedef struct {float radius,zmin,zmax;int xy_div,z_div,cursor_steps;float center_x,center_y;} IMapView;
/* INT16_MIN means empty. Offsets locate the representative sample to 1/254
   of an XY pixel. Z is quantized over the separately configured Z interval. */
typedef struct {int16_t z;int8_t dx,dy;} IMapPixel;
typedef struct {float x,y,z;} IMapPoint;
typedef struct {int count,overflow,interval,invalid;float z[IMAP_MAX_ROOTS];} IMapRoots;
/* Distinct numerical representatives inside one whole XY pixel. There can be
   infinitely many actual points; overflow means the bounded list is partial. */
typedef struct {int count,overflow,invalid;IMapPoint point[IMAP_MAX_ROOTS];} IMapPixelHits;
typedef struct {int segments,pixels,invalid,ambiguous,low_memory,evaluations;} IMapStats;
/* Recompute clip bounds and pixel-scale sampling after a zoom. Return 0 if
   the requested number of slices is too large; never silently coarsen. */
int imap_auto_view(IMapView *,float clip_radius);
int imap_auto_view_bounds(IMapView *,float zmin,float zmax);
int imap_build(RaySurface const surface[2],EvalVars const *,IMapView const *,IMapPixel *,ProgressFn,void *,IMapStats *);
/* height[k] must be compiled from surface[k]; unknown cases use general 3-D. */
int imap_build_auto(RaySurface const surface[2],EvalVars const *,IMapView const *,
                    Program const *height[2],IMapPixel *,ProgressFn,void *,IMapStats *);
int imap_search(RaySurface const surface[2],EvalVars const *,IMapView const *,float x,float y,ProgressFn,void *,IMapRoots *);
/* Probe the footprint of a single displayed pixel in XY, not its centre.
   Coordinates are indices 0..IMAP_SIDE-1, y increasing downward. The same
   automatic z resolution as the map is used. Returned points are numerical. */
int imap_pixel_search(RaySurface const surface[2],EvalVars const *,IMapView const *,
                      int ix,int iy,IMapPixel const *map,ProgressFn,void *,IMapPixelHits *);
/* Nearest stored representative in the XY plane; not an exact global minimum. */
int imap_nearest(IMapPixel const *,IMapView const *,float x,float y,IMapPoint *);
#endif
