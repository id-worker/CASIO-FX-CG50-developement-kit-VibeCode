#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
void *__real_calloc(size_t,size_t);
static size_t target;static int armed,failures;
void *__wrap_calloc(size_t n,size_t s){
    if(armed&&n*s==target){armed=0;failures++;return NULL;}
    return __real_calloc(n,s);
}
static Parsed parsed;static Program prog;static uint16_t layer[PLOT_W*PLOT_H];
int main(void)
{host_keys(NULL,0);
    assert(expr_parse(&parsed,"x^2+y^2+z^2<=1",0)&&expr_compile(&parsed,&prog,NULL,0));
    RaySurface surf={&prog,0};EvalVars vars={0};Volume vol={0,0,0,3,8,0};
    Camera cam={.yaw=.7f,.pitch=.4f,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    ScreenRefine r={0};RayGridCache grid={0};assert(refine_begin(&r,32));
    RayStats st;RayRenderOptions op={.grid=&grid,.screen_step=32,.layer=layer,.refine=&r};
    assert(ray3d_render_ex(&surf,1,&vars,&vol,&cam,&op,NULL,NULL,&st)==1);
    /* The old skip bitmap belonged to the removed renderer. Ray does not
       allocate one; fail its required candidate grid and retry the same
       stage, keeping the last finished screen lattice. */
    op.screen_step=16;ray_grid_cache_clear(&grid);
    target=(size_t)(vol.div*vol.div*vol.div+7)/8;armed=1;
    assert(ray3d_render_ex(&surf,1,&vars,&vol,&cam,&op,NULL,NULL,&st)==-2);
    assert(failures==1&&r.last_step==32&&r.seed_done);
    assert(ray3d_render_ex(&surf,1,&vars,&vol,&cam,&op,NULL,NULL,&st)==1);
    assert(st.reused_rays>0&&!st.grid_reused);
    RayRenderOptions direct={.grid=&grid,.screen_step=16};dclear(C_WHITE);
    assert(ray3d_render_ex(&surf,1,&vars,&vol,&cam,&direct,NULL,NULL,&st)==1);
    for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)
        assert(layer[y*PLOT_W+x]==gint_vram[(y+PLOT_Y0)*DWIDTH+x+PLOT_X0]);
    refine_free(&r);ray_grid_cache_clear(&grid);assert(host_vram_guards_ok());
    puts("Incremental Ray reports candidate-grid allocation failure, retries safely and reuses the completed screen lattice.");
}
