/* Host CPU comparison, not real fx-CG50 timing. Compile with -DGL_BENCH_STREAM
   for 1.19; without it this also builds against the 1.18 source. */
#define _POSIX_C_SOURCE 200809L
#include "ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
static Parsed parsed;static Program p[2],height;
static uint16_t layer[PLOT_W*PLOT_H],coarse[PLOT_W*PLOT_H];
static void compile(Program *out,char const *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,out,NULL,0));}
static double now(void){struct timespec t;assert(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t)==0);return t.tv_sec+t.tv_nsec*1e-9;}
int main(void)
{
    host_keys(NULL,0);compile(&p[0],"sin(x)+sin(y)+sin(z)=0");compile(&p[1],"z=x-y");compile(&height,"x-y");
    RaySurface surf[2]={{&p[0],0},{&p[1],1}};EvalVars vars={0};Volume v={0,0,0,10,12,0};
    Camera c={.yaw=.78539816f,.pitch=.716342f,.zoom=1,.eye_x=13,.eye_y=13,.eye_z=16,.radius=10,.perspective=1};
    ScreenRefine refine={0};RayGridCache grid={0};assert(refine_begin(&refine,2));RayStats st;
    RayRenderOptions op={.grid=&grid,.height={NULL,&height},.screen_step=2,.layer=layer,.refine=&refine};
    assert(ray3d_render_ex(surf,2,&vars,&v,&c,&op,NULL,NULL,&st)==1);memcpy(coarse,layer,sizeof coarse);
    op.screen_step=1;
#ifdef GL_BENCH_STREAM
    op.final_stream=1;
#endif
    double elapsed=0;
    for(int i=0;i<5;i++){
        memcpy(layer,coarse,sizeof layer);refine.last_step=2;
        double begin=now();assert(ray3d_render_ex(surf,2,&vars,&v,&c,&op,NULL,NULL,&st)==1);elapsed+=now()-begin;
        assert(st.rays==52303&&st.evaluations==781679&&st.grid_reused);
    }
    uint64_t hash=UINT64_C(1469598103934665603);
    for(size_t i=0;i<(size_t)PLOT_W*PLOT_H;i++){hash^=layer[i];hash*=UINT64_C(1099511628211);}
    printf("mean_host_cpu_seconds=%.6f new_rays=%d reused_lookups=%d field_evals=%d frame_fnv=%016llx\n",
           elapsed/5,st.rays,st.reused_rays,st.evaluations,(unsigned long long)hash);
    refine_free(&refine);ray_grid_cache_clear(&grid);
}
