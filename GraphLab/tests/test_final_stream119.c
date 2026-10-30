#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;
static Program programs[2];
static uint16_t layer[PLOT_W*PLOT_H],full[PLOT_W*PLOT_H];
static void compile(int n,const char *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,&programs[n],NULL,0));}
static int stop_now(int done,int total,void *ctx){(void)done;(void)total;(void)ctx;return 0;}
static int check_rows(int done,int total,void *ctx){
    int *last=ctx;
    if(total!=PLOT_H||done<=*last)return 1;
    /* A callback at the start of row y means all rows <y are complete. */
    for(int y=*last;y<done;y++)for(int x=0;x<PLOT_W;x++){
        uint16_t expected=full[y*PLOT_W+x],got=layer[y*PLOT_W+x];
        if(expected!=got){fprintf(stderr,"completed row=%d x=%d got=%04x expected=%04x\n",y,x,got,expected);assert(0);}
    }
    *last=done;return 1;
}
int main(void)
{
    host_keys(NULL,0);compile(0,"x^2+y^2+z^2=1");compile(1,"z=x-y");
    RaySurface surfaces[2]={{&programs[0],0},{&programs[1],1}};EvalVars vars={0};
    Volume v={0,0,0,3,8,0};Camera c={.yaw=.7f,.pitch=.4f,.zoom=1,.radius=3,.eye_x=4,.eye_y=6,.eye_z=3,.perspective=1};
    for(int seed=1;seed<=32;seed++){
        ScreenRefine r={0};RayGridCache grid={0};assert(refine_begin(&r,seed));
        RayStats direct;RayRenderOptions base={.grid=&grid,.screen_step=1};dclear(C_WHITE);
        assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&base,NULL,NULL,&direct)==1);
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)full[y*PLOT_W+x]=gint_vram[(y+PLOT_Y0)*DWIDTH+x+PLOT_X0];
        memset(layer,0xcc,sizeof layer);int total=0;
        for(int step=seed;step;step=refine_next(step)){
            RayStats st;RayRenderOptions op={.grid=&grid,.screen_step=(uint8_t)step,.layer=layer,.refine=&r};
            if(step!=1||seed==1){
                assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&op,NULL,NULL,&st)==1);total+=st.rays;
                if(seed==1)assert(!memcmp(layer,full,sizeof full));
                continue;
            }
            assert(r.last_step==2&&r.grid_step==2);
            op.final_stream=1;
            RayRenderOptions invalid=op;invalid.screen_step=2;
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&invalid,NULL,NULL,&st)==-1);
            invalid=op;invalid.layer=NULL;
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&invalid,NULL,NULL,&st)==-1);
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&op,stop_now,NULL,&st)==0);
            assert(st.canceled&&r.last_step==2&&st.rays==0);
            int checked=0;
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&op,check_rows,&checked,&st)==1);
            int old=((PLOT_W+1)/2)*((PLOT_H+1)/2);
            assert(st.rays+st.reused_rays==PLOT_W*PLOT_H-old&&st.grid_reused);total+=st.rays;
            assert(checked==PLOT_H-1&&r.last_step==1&&!memcmp(layer,full,sizeof full));
            assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&op,NULL,NULL,&st)==-1);
        }
        assert(total==PLOT_W*PLOT_H&&host_vram_guards_ok());
        refine_free(&r);ray_grid_cache_clear(&grid);
    }
    puts("FAST 2->1: all seeds 1..32, completed rows and final pixels match independent frame; no ray recalculated.");
}
