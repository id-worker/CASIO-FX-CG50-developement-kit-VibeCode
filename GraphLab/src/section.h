#ifndef GL_SECTION_H
#define GL_SECTION_H
#include "ray.h"
#define SECTION_MAX_POINTS 32
/* XY: (u,v)=(x,y), fixed z. XZ: (x,z), fixed y. YZ: (y,z), fixed x. */
typedef struct {int plane;float fixed,radius;int divisions;} SectionView;
typedef struct {float x,y,z;} SectionPoint;
typedef void (*SectionSegmentFn)(float u0,float v0,float u1,float v1,int slot,void *ctx);
typedef struct {
    SectionPoint point[SECTION_MAX_POINTS];
    int count,overflow,segments[2],whole[2],invalid,nonisolated,outside,evaluations;
} SectionResult;
/* Samples only the fixed world cube [-radius,+radius]^3. Sections and
   isolated pair intersections are numerical approximations, not proofs.
   Returns 1 success, 0 canceled, -1 invalid input, -2 allocation failure. */
int section_compute(RaySurface const *,int,EvalVars const *,SectionView const *,
                    SectionSegmentFn,void *,ProgressFn,void *,SectionResult *);
/* Try finitely many positions; with two surfaces prioritize actual isolated
   crossings, with one surface prioritize visible trace. Returns 1 or 0/-1/-2.
   The status 'found' means a crossing (2 surfaces) or trace (1 surface). */
int section_auto(RaySurface const *,int,EvalVars const *,int plane,float radius,
                 ProgressFn,void *,float *fixed,int *found);
#endif
