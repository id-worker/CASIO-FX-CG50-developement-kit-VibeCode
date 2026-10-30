/* Counts, not calculator seconds. Equal scene/camera/Q/cold grid for both. */
#define _POSIX_C_SOURCE 200809L
#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
static Parsed parsed;static Program p[2],height;static uint16_t layer[PLOT_W*PLOT_H];
static void compile(Program *out,char const *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,out,NULL,0));}
int main(void)
{
    host_keys(NULL,0);compile(&p[0],"sin(x)+sin(y)+sin(z)=0");compile(&p[1],"z=x-y");compile(&height,"x-y");
    RaySurface surf[2]={{&p[0],0},{&p[1],1}};EvalVars vars={0};Volume v={0,0,0,10,12,0};
    Camera c={.yaw=.78539816f,.pitch=.716342f,.zoom=1,.eye_x=13,.eye_y=13,.eye_z=16,.radius=10,.perspective=1};
    ScreenRefine refine={0};RayGridCache grid={0};assert(refine_begin(&refine,32));
    int total=0,ev=0;
    for(int step=32;step;step=refine_next(step)){
        RayStats st;RayRenderOptions op={.grid=&grid,.height={NULL,&height},.screen_step=step,.layer=layer,.refine=&refine};
        assert(ray3d_render_ex(surf,2,&vars,&v,&c,&op,NULL,NULL,&st)==1);
        printf("step=%2d new_rays=%5d reused=%5d grid_reused=%d field_evals=%7d\n",step,st.rays,st.reused_rays,st.grid_reused,st.evaluations);
        total+=st.rays;ev+=st.evaluations;
    }
    RayGridCache direct_grid={0};RayStats direct;RayRenderOptions opt={.grid=&direct_grid,.height={NULL,&height},.screen_step=1};
    dclear(C_WHITE);assert(ray3d_render_ex(surf,2,&vars,&v,&c,&opt,NULL,NULL,&direct)==1);
    for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)assert(layer[y*PLOT_W+x]==gint_vram[(y+PLOT_Y0)*DWIDTH+PLOT_X0+x]);
    printf("incremental_total: rays=%d field_evals=%d\ndirect_step_1:    rays=%d field_evals=%d\n",total,ev,direct.rays,direct.evaluations);
    assert(total==direct.rays);refine_free(&refine);ray_grid_cache_clear(&grid);ray_grid_cache_clear(&direct_grid);
}
