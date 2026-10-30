#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
static Parsed parsed;static Program program;
static int stop(int done,int total,void *ctx){(void)total;(void)ctx;return done<40;}
int main(void)
{
    host_keys(NULL,0);
    assert(expr_parse(&parsed,"x^2+y^2+z^2=1",0)&&expr_compile(&parsed,&program,NULL,0));
    RaySurface one={&program,0};EvalVars vars={0};Volume v={0,0,0,3,12,0};
    Camera c={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    for(int step=1;step<=32;step++){RayRenderOptions opts={.screen_step=(uint8_t)step};RayStats st;
        dclear(C_WHITE);assert(ray3d_render_ex(&one,1,&vars,&v,&c,&opts,NULL,NULL,&st)==1);
        assert(st.rays==((PLOT_W+step-1)/step)*((PLOT_H+step-1)/step));
        assert(st.hits>0&&host_vram_guards_ok());
        int drawn=0;
        for(int y=PLOT_Y0;y<=PLOT_Y1;y+=step)for(int x=PLOT_X0;x<=PLOT_X1;x+=step){
            int col=dgetpixel(x,y);
            for(int dy=0;dy<step&&y+dy<=PLOT_Y1;dy++)for(int dx=0;dx<step&&x+dx<=PLOT_X1;dx++)
                assert(dgetpixel(x+dx,y+dy)==col); /* filled tile, including clipped edges */
            if(col!=C_WHITE)drawn++;
        }
        assert(drawn==st.hits);printf("step=%d rays=%d hit_tiles=%d field_evals=%d\n",step,st.rays,st.hits,st.evaluations);
        dclear(C_WHITE);assert(ray3d_render_ex(&one,1,&vars,&v,&c,&opts,stop,NULL,&st)==0&&st.canceled);
    }
    RayRenderOptions bad={.screen_step=33};RayStats stats;
    assert(ray3d_render_ex(&one,1,&vars,&v,&c,&bad,NULL,NULL,&stats)==-1);
    puts("FAST RAY: all integer spacings 1..32, filled tiles, cancellation and bounds passed on host.");
}
