#include "refine.h"
#include <stdlib.h>
#include <string.h>
#define EVEN_W ((PLOT_W+1)/2)
#define EVEN_H ((PLOT_H+1)/2)
static int power2(int n){return n>0&&(n&(n-1))==0;}
int refine_begin(ScreenRefine *p,int step)
{
    if(!p||step<1||step>32)return 0;
    /* Reuse the even table between views; discard a non-power-of-two seed. */
    free(p->seed);p->seed=NULL;
    p->seed_step=p->seed_done=p->grid_step=p->last_step=0;
    if(!p->even)p->even=malloc((size_t)EVEN_W*EVEN_H*sizeof *p->even);
    if(!p->even)return 0;
    if((step&1)&&step>1){
        size_t n=(size_t)((PLOT_W+step-1)/step)*((PLOT_H+step-1)/step);
        p->seed=malloc(n*sizeof *p->seed);
        if(!p->seed)return 0;
    }
    p->seed_step=step;p->seed_done=p->grid_step=p->last_step=0;return 1;
}
void refine_free(ScreenRefine *p)
{
    if(p){free(p->even);free(p->seed);memset(p,0,sizeof *p);}
}
int refine_next(int step)
{
    if(step<=1)return 0;
    int v=1;while(v*2<step)v*=2;return v;
}
int refine_stage_allowed(ScreenRefine const *p,int step)
{
    if(!p||!p->even||p->seed_step<1||step<1||step>32)return 0;
    return !p->seed_done?step==p->seed_step:step==refine_next(p->last_step);
}
int refine_get(ScreenRefine const *p,int x,int y,uint16_t *color)
{
    if(!p||!p->seed_done||x<0||y<0||x>=PLOT_W||y>=PLOT_H)return 0;
    if(p->grid_step>0 && x%p->grid_step==0 && y%p->grid_step==0){
        if(color)*color=p->even[(size_t)(y/2)*EVEN_W+x/2];
        return 1;
    }
    int step=p->seed_step;
    if(x%step||y%step)return 0;
    if((step&1)&&step>1){
        int w=(PLOT_W+step-1)/step;
        if(color)*color=p->seed[(size_t)(y/step)*w+x/step];
    }else if(step>1){
        if(color)*color=p->even[(size_t)(y/2)*EVEN_W+x/2];
    }else return 0; /* A full seed has no subsequent step. */
    return 1;
}
int refine_keep(ScreenRefine const *p,int step,int x,int y)
{
    /* A previous lattice anchor lies in the retained top-left child only
       when the new tiles subdivide the previous lattice exactly. */
    return p&&p->last_step>0&&p->last_step%step==0&&
           x%p->last_step==0&&y%p->last_step==0;
}
void refine_store(ScreenRefine *p,int x,int y,uint16_t color)
{
    if(!p||!p->even||x<0||y<0||x>=PLOT_W||y>=PLOT_H)return;
    if(!(x&1)&&!(y&1))p->even[(size_t)(y/2)*EVEN_W+x/2]=color;
    if(!p->seed_done&&(p->seed_step&1)&&p->seed_step>1){
        int s=p->seed_step,w=(PLOT_W+s-1)/s;
        if(x%s==0&&y%s==0)p->seed[(size_t)(y/s)*w+x/s]=color;
    }
}
void refine_commit(ScreenRefine *p,int step)
{
    if(!p)return;
    if(!p->seed_done)p->seed_done=1;
    if(step>1&&power2(step))p->grid_step=step;
    p->last_step=step;
}
void refine_tile(uint16_t *layer,int x,int y,int step,uint16_t color)
{
    if(!layer)return;
    int endx=x+step<PLOT_W?x+step:PLOT_W,endy=y+step<PLOT_H?y+step:PLOT_H;
    for(int yy=y;yy<endy;yy++)for(int xx=x;xx<endx;xx++)layer[(size_t)yy*PLOT_W+xx]=color;
}
