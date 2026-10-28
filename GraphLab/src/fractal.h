#ifndef GL_FRACTAL_H
#define GL_FRACTAL_H
#include "plot.h"
typedef struct {
    float cx,cy,units_per_pixel,julia_re,julia_im,escape_radius;
    int iterations,block,julia;
} FractalView;
typedef void (*FractalBlockFn)(int x,int y,int w,int h,int iteration,void *ctx);
/* Returns 1..iterations on escape, iterations+1 if not escaped yet, or -1 on invalid input. */
int fractal_point(Program const *pr,EvalVars const *base,float re,float im,
                  FractalView const *view);
int fractal_generate(Program const *pr,EvalVars const *base,FractalView const *view,
                     FractalBlockFn paint,void *paint_ctx,ProgressFn progress,void *progress_ctx);
#endif
