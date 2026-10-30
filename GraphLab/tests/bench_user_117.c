#define _POSIX_C_SOURCE 200809L
#include "../src/ray.h"
#include "host_gint.h"
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <time.h>
static Parsed tree;static Program p[2],height;
static void compile(Program *out,const char *s){assert(expr_parse(&tree,s,0)&&expr_compile(&tree,out,NULL,0));}
static double seconds(void){struct timespec t;clock_gettime(CLOCK_MONOTONIC,&t);return t.tv_sec+t.tv_nsec/1e9;}
int main(void)
{
    host_keys(NULL,0);compile(&p[0],"sin(x)+sin(y)+sin(z)=0");compile(&p[1],"z=x-y");compile(&height,"x-y");
    RaySurface surf[2]={{&p[0],0},{&p[1],1}};EvalVars vars={0};Volume v={0,0,0,10,8,0};
    Camera c={.yaw=.78539816f,.pitch=.716342f,.zoom=1,.eye_x=13,.eye_y=13,.eye_z=16,.radius=10,.perspective=1};
    int steps[]={1,2,4,8,12,16,24,32};RayGridCache cache={0};
    printf("Synthetic host only: Q=8 clip=10, eye (13,13,16), approximately oriented to origin.\n");
    for(unsigned i=0;i<sizeof steps/sizeof *steps;i++){
        int step=steps[i];RayRenderOptions opt={.grid=&cache,.screen_step=step,.height={NULL,&height}};RayStats st;
        dclear(C_WHITE);double t=seconds();assert(ray3d_render_ex(surf,2,&vars,&v,&c,&opt,NULL,NULL,&st)==1);t=seconds()-t;
        printf("step=%2d grid_reused=%d rays=%5d hits=%5d field_evals=%7d host_seconds=%.4f\n",step,st.grid_reused,st.rays,st.hits,st.evaluations,t);
    }
    ray_grid_cache_clear(&cache);return 0;
}
