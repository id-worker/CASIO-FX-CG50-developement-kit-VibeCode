/* Synthetic RAY scene used for the 1.14/1.15 headless comparison.
   Original GraphLab test code: MIT; see ../LICENSE.
   Uses an assumed direction; GLBench is NOT the shipped application.
   Emulator instruction-loop iterations are NOT real hardware cycles/time. */
#include "ray.h"
#include <gint/display.h>
static Parsed p0,p1;static Program a,b,height;
static RayGridCache grid;static Preview3D cloud;
RayStats gstats;int g_rc=-99;
__attribute__((noinline)) void glbench_done(void){__asm__ volatile ("" ::: "memory");}
int main(void)
{
    if(!expr_parse(&p0,"sin(x)+sin(y)+sin(z)=0",0)||!expr_compile(&p0,&a,0,0)||
       !expr_parse(&p1,"z=x-y",0)||!expr_compile(&p1,&b,0,0)||!expr_compile_explicit_z(&p1,&height)){
        g_rc=-20;glbench_done();return 0;
    }
    EvalVars vars={0};Volume v={0,0,0,10,8,0};
    Camera c={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=10,.eye_x=18.7f,.eye_y=20.1f,.eye_z=16.9f,.perspective=1};
    RaySurface s[2]={{&a,0},{&b,1}};RayRenderOptions opt={.grid=&grid,.height={NULL,&height}};
    preview3d_set_step(&cloud,4);if(preview3d_begin(&cloud,&v,&c))opt.preview=&cloud;
    dclear(C_WHITE);
    g_rc=ray3d_render_ex(s,2,&vars,&v,&c,&opt,NULL,NULL,&gstats);
    glbench_done();return 0;
}
