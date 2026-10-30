#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed parsed;static Program programs[2];
static uint16_t layer[PLOT_W*PLOT_H];
static void compile(int n,const char *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,&programs[n],NULL,0));}
int main(void)
{
    host_keys(NULL,0);compile(0,"x^2+y^2+z^2=1");compile(1,"z=x-y");
    RaySurface surfaces[2]={{&programs[0],0},{&programs[1],1}};EvalVars vars={0};
    Volume v={0,0,0,3,8,0};Camera c={.yaw=.7f,.pitch=.4f,.zoom=1,.radius=3,.eye_x=4,.eye_y=6,.eye_z=3,.perspective=1};
    for(int seed=1;seed<=32;seed++){
        ScreenRefine r={0};RayGridCache grid={0};assert(refine_begin(&r,seed));
        for(int i=0;i<PLOT_W*PLOT_H;i++)layer[i]=C_WHITE;
        int step=seed,total=0,stages=0;
        do{
            RayStats st;RayRenderOptions op={.grid=&grid,.screen_step=(uint8_t)step,.layer=layer,.refine=&r};
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&op,NULL,NULL,&st)==1);
            int all=((PLOT_W+step-1)/step)*((PLOT_H+step-1)/step);
            assert(st.rays+st.reused_rays==all);if(stages)assert(st.reused_rays>0&&st.grid_reused);
            total+=st.rays;stages++;
            RayStats ref;RayRenderOptions opt={.grid=&grid,.screen_step=(uint8_t)step};dclear(C_WHITE);
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&opt,NULL,NULL,&ref)==1);
            for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++){
                uint16_t a=layer[y*PLOT_W+x],b=gint_vram[(y+PLOT_Y0)*DWIDTH+x+PLOT_X0];
                if(a!=b){fprintf(stderr,"seed=%d step=%d xy=%d,%d got=%x wanted=%x\n",seed,step,x,y,a,b);assert(0);}
            }
            step=refine_next(step);
        }while(step);
        assert(total==PLOT_W*PLOT_H); /* no ray is evaluated twice */
        if(seed==32)assert(stages==6);
        refine_free(&r);ray_grid_cache_clear(&grid);assert(host_vram_guards_ok());
    }
    puts("Incremental RAY: all 32 starting steps match independent full-stage frames, no ray repeated.");
}
