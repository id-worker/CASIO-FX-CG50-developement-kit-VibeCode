#include "fractal.h"
#include <math.h>
static int polynomial_kind(Program const *p)
{
    if(p->size!=4)return 0;
    if(p->code[0].op!=I_VAR || p->code[0].aux!=V_Z || p->code[2].op!=I_VAR || p->code[2].aux!=V_C || p->code[3].op!=I_ADD)return 0;
    return p->code[1].op==I_SQUARE?2:p->code[1].op==I_CUBE?3:0;
}
static int point(Program const *p,EvalVars const *base,float re,float im,FractalView const *v,int fast,
                 ProgressFn progress,void *progress_ctx,int done,int total)
{
    EvalVars vars=*base;
    vars.v[V_X]=re;vars.v[V_Y]=im;
    Complex z=v->julia?(Complex){re,im}:(Complex){0,0};
    Complex c=v->julia?(Complex){v->julia_re,v->julia_im}:(Complex){re,im};
    float limit=v->escape_radius*v->escape_radius;
    for(int n=0;n<v->iterations;n++) {
        if(n>0 && (n%32)==0 && progress && !progress(done,total,progress_ctx))return -2;
        if(fast==2) {
            float x=z.re*z.re-z.im*z.im+c.re,y=2*z.re*z.im+c.im;z=(Complex){x,y};
        } else if(fast==3) {
            float x=z.re*z.re-z.im*z.im,y=2*z.re*z.im;
            z=(Complex){x*z.re-y*z.im+c.re,x*z.im+y*z.re+c.im};
        } else {
            vars.cz=z;vars.cc=c;vars.v[V_N]=(float)n;z=expr_eval_complex(p,&vars);
        }
        if(!isfinite(z.re)||!isfinite(z.im)||z.re*z.re+z.im*z.im>limit)return n+1;
    }
    /* Keep a distinct sentinel for points that never escaped: an escape on
       the final allowed iteration must not be painted as an interior point. */
    return v->iterations+1;
}
int fractal_point(Program const *pr,EvalVars const *base,float re,float im,FractalView const *view)
{
    if(!pr||!base||!view || view->iterations<1 || view->iterations>512 || !isfinite(view->escape_radius) || view->escape_radius<=0)return -1;
    return point(pr,base,re,im,view,polynomial_kind(pr),NULL,NULL,0,0);
}
static int floor_div(int a,int b)
{
    int q=a/b,r=a%b;
    return r<0?q-1:q;
}
int fractal_generate_region(Program const *pr,EvalVars const *base,FractalView const *v,
                            int x0,int y0,int x1,int y1,int phase_x,int phase_y,
                            FractalBlockFn paint,void *paint_ctx,
                            ProgressFn progress,void *progress_ctx)
{
    if(!pr||!base||!v||!paint||v->iterations<1||v->iterations>512||v->block<1||v->block>12 ||
       !isfinite(v->cx)||!isfinite(v->cy)||!isfinite(v->units_per_pixel)||v->units_per_pixel<=0 ||
       !isfinite(v->escape_radius)||v->escape_radius<=0)return -1;
    if(x0<PLOT_X0)x0=PLOT_X0;
    if(y0<PLOT_Y0)y0=PLOT_Y0;
    if(x1>PLOT_X1)x1=PLOT_X1;
    if(y1>PLOT_Y1)y1=PLOT_Y1;
    if(x0>x1||y0>y1)return 1;
    int block=v->block,fast=polynomial_kind(pr);
    /* Normalize before adding coordinates, even if the caller passed a large
       accumulated pan. The same lattice is reused after every integer shift. */
    phase_x%=block;if(phase_x<0)phase_x+=block;
    phase_y%=block;if(phase_y<0)phase_y+=block;
    int anchor_x=PLOT_X0+phase_x,anchor_y=PLOT_Y0+phase_y;
    int first_x=anchor_x+floor_div(x0-anchor_x,block)*block;
    int first_y=anchor_y+floor_div(y0-anchor_y,block)*block;
    int rows=(y1-first_y)/block+1,row=0;
    for(int gy=first_y;gy<=y1;gy+=block,row++){
        if(progress&&!progress(row,rows,progress_ctx))return 0;
        int painted=0;
        for(int gx=first_x;gx<=x1;gx+=block,painted++){
            if((painted%32)==0 && progress&&!progress(row,rows,progress_ctx))return 0;
            /* Sample the center of the full conceptual block, not the center
               of its clipped fragment at a screen or exposed-strip edge. */
            float x=v->cx+((float)gx+block*.5f-(PLOT_X0+PLOT_X1)*.5f)*v->units_per_pixel;
            float y=v->cy-((float)gy+block*.5f-(PLOT_Y0+PLOT_Y1)*.5f)*v->units_per_pixel;
            int iteration=point(pr,base,x,y,v,fast,progress,progress_ctx,row,rows);
            if(iteration<0)return 0;
            int ax=gx<x0?x0:gx,ay=gy<y0?y0:gy;
            int bx=gx+block-1>x1?x1:gx+block-1;
            int by=gy+block-1>y1?y1:gy+block-1;
            paint(ax,ay,bx-ax+1,by-ay+1,iteration,paint_ctx);
        }
    }
    if(progress&&!progress(rows,rows,progress_ctx))return 0;
    return 1;
}
int fractal_generate(Program const *pr,EvalVars const *base,FractalView const *v,
                     FractalBlockFn paint,void *paint_ctx,ProgressFn progress,void *progress_ctx)
{
    return fractal_generate_region(pr,base,v,PLOT_X0,PLOT_Y0,PLOT_X1,PLOT_Y1,0,0,
                                   paint,paint_ctx,progress,progress_ctx);
}
