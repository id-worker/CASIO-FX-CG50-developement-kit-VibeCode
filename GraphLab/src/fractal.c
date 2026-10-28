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
int fractal_generate(Program const *pr,EvalVars const *base,FractalView const *v,
                     FractalBlockFn paint,void *paint_ctx,ProgressFn progress,void *progress_ctx)
{
    if(!pr||!base||!v||!paint||v->iterations<1||v->iterations>512||v->block<1||v->block>12 ||
       !isfinite(v->cx)||!isfinite(v->cy)||!isfinite(v->units_per_pixel)||v->units_per_pixel<=0 ||
       !isfinite(v->escape_radius)||v->escape_radius<=0)return -1;
    int block=v->block,rows=(PLOT_H+block-1)/block,fast=polynomial_kind(pr);
    for(int j=0;j<rows;j++) {
        if(progress && !progress(j,rows,progress_ctx))return 0;
        int py=PLOT_Y0+j*block,h=PLOT_Y1-py+1;if(h>block)h=block;
        for(int px=PLOT_X0;px<=PLOT_X1;px+=block) {
            if(((px-PLOT_X0)/block)%32==0 && progress && !progress(j,rows,progress_ctx))return 0;
            int w=PLOT_X1-px+1;if(w>block)w=block;
            float x=v->cx+((float)px+w*0.5f-(PLOT_X0+PLOT_X1)*0.5f)*v->units_per_pixel;
            float y=v->cy-((float)py+h*0.5f-(PLOT_Y0+PLOT_Y1)*0.5f)*v->units_per_pixel;
            int iteration=point(pr,base,x,y,v,fast,progress,progress_ctx,j,rows);
            if(iteration<0)return 0;
            paint(px,py,w,h,iteration,paint_ctx);
        }
    }
    if(progress && !progress(rows,rows,progress_ctx))return 0;
    return 1;
}
