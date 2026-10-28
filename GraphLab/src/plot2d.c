/* Fast explicit y=f(x) plotting. General implicit contours remain in plot.c. */
#include "plot2d.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>

static int aligned_period(Program const *pr,PlaneView const *v,int width)
{
    if(!expr_program_uses(pr,V_X))return 1;
    float dx=(v->xmax-v->xmin)/(width-1);
    if(!isfinite(v->period_x)||v->period_x<=0||!isfinite(dx)||dx<=0)return 0;
    float steps=v->period_x/dx;
    if(!isfinite(steps)||steps<1||steps>width-1)return 0;
    int count=(int)lroundf(steps);
    return fabsf(steps-count)<=.000001f?count:0;
}
/* Shade the pixel centers satisfying y REL f(x). This is raster
   quantization, not a global epsilon applied to the expression value. */
static void shade_column(int x,float y,Relation rel,PlaneView const *v,int h,
                         SpanFillFn fill,void *ctx)
{
    if(!fill||rel<REL_LT||!isfinite(y))return;
    float t=(v->ymax-y)/(v->ymax-v->ymin)*(h-1);
    if(!isfinite(t))t=y>v->ymax?-1.0f:(float)h;
    int a=0,b=h-1;
    if(rel==REL_LE){
        if(t>h-1)return;
        if(t>0)a=(int)ceilf(t);
    }else if(rel==REL_LT){
        if(t>=h-1)return;
        if(t>=0)a=(int)floorf(t)+1;
    }else if(rel==REL_GE){
        if(t<0)return;
        if(t<h-1)b=(int)floorf(t);
    }else if(rel==REL_GT){
        if(t<=0)return;
        if(t<=h-1)b=(int)ceilf(t)-1;
    }
    if(a<0)a=0;
    if(b>=h)b=h-1;
    if(a<=b)fill(x,a,b,ctx);
}
static int clip_y(float x0,float y0,float x1,float y1,PlaneView const *v,
                  float *ax,float *ay,float *bx,float *by)
{
    if((y0<v->ymin&&y1<v->ymin)||(y0>v->ymax&&y1>v->ymax))return 0;
    float d=y1-y0;
    if(!isfinite(d))return 0;
    float lo=0,hi=1;
    if(d!=0){
        float a=(v->ymin-y0)/d,b=(v->ymax-y0)/d;
        if(a>b){float t=a;a=b;b=t;}
        if(a>lo)lo=a;
        if(b<hi)hi=b;
    } else if(y0<v->ymin||y0>v->ymax)return 0;
    if(lo>hi||lo>1||hi<0)return 0;
    if(lo<0)lo=0;
    if(hi>1)hi=1;
    *ax=x0+(x1-x0)*lo;*bx=x0+(x1-x0)*hi;
    *ay=y0+d*lo;*by=y0+d*hi;
    if(*ay<v->ymin)*ay=v->ymin;
    if(*ay>v->ymax)*ay=v->ymax;
    if(*by<v->ymin)*by=v->ymin;
    if(*by>v->ymax)*by=v->ymax;
    return isfinite(*ax)&&isfinite(*ay)&&isfinite(*bx)&&isfinite(*by);
}
int plot_explicit_y(Program const *rhs,Relation relation,EvalVars const *base,
                    PlaneView const *view,int width,int height,
                    SegmentFn segment,void *segment_ctx,SpanFillFn fill,void *fill_ctx,
                    ProgressFn progress,void *progress_ctx,PlotStats *stats)
{
    if(!rhs||!base||!view||!segment||!stats||width<2||width>400||height<2||height>250||
       !isfinite(view->xmin)||!isfinite(view->xmax)||!isfinite(view->ymin)||!isfinite(view->ymax)||
       view->xmin>=view->xmax||view->ymin>=view->ymax||expr_program_uses(rhs,V_Y)||
       relation<REL_EQ||relation>REL_GE)return -1;
    memset(stats,0,sizeof *stats);stats->all_zero=1;
    int period=aligned_period(rhs,view,width);
    float *cache=period?malloc((size_t)width*sizeof(float)):NULL;
    if(period&&!cache)period=0; /* Optional optimization only. */
    EvalVars vars=*base;float prev=0,prev_x=0;int prev_valid=0;
    float dx=(view->xmax-view->xmin)/(width-1),range=view->ymax-view->ymin;
    for(int x=0;x<width;x++){
        if((x%32)==0 && progress && !progress(x,width,progress_ctx)){
            stats->canceled=1;free(cache);return 0;
        }
        float wx=view->xmin+dx*x,y;
        if(period && x>=period)y=cache[x-period];
        else {vars.v[V_X]=wx;y=expr_eval_real(rhs,&vars);stats->evaluations++;}
        if(period)cache[x]=y;
        stats->samples++;
        if(!isfinite(y)){stats->invalid++;prev_valid=0;continue;}
        if(y!=0)stats->all_zero=0;
        shade_column(x,y,relation,view,height,fill,fill_ctx);
        if(prev_valid){
            float delta=y-prev;int join=isfinite(delta);
            /* A steep jump can hide a pole. A midpoint test rejects many
               (not all) discontinuities without connecting across them. */
            if(join && fabsf(delta)>range*.5f){
                vars.v[V_X]=(prev_x+wx)*.5f;
                float mid=expr_eval_real(rhs,&vars);stats->evaluations++;
                float linear=prev*.5f+y*.5f;
                if(!isfinite(mid)||!isfinite(linear)||
                   fabsf(mid-linear)>fabsf(delta)*.75f+range*.05f){join=0;stats->rejected++;}
            }
            float ax,ay,bx,by;
            if(join && clip_y(prev_x,prev,wx,y,view,&ax,&ay,&bx,&by)){
                segment(ax,ay,bx,by,segment_ctx);stats->segments++;
            }
        }
        prev_x=wx;prev=y;prev_valid=1;
    }
    if(progress && !progress(width,width,progress_ctx)){stats->canceled=1;free(cache);return 0;}
    free(cache);return 1;
}
