#include "../src/ray.h"
#include "../src/voxel.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;static Program pr;static EvalVars vars;
static void compile(char const *s){assert(expr_parse(&parsed,s,0)&&expr_compile(&parsed,&pr,NULL,0));}
static int colored(void){int n=0;for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++)n+=dgetpixel(x,y)!=C_WHITE;return n;}
int main(void)
{
    host_keys(NULL,0);Volume v={0,0,0,3,20,0};
    Camera c={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    Preview3D p={0};assert(preview3d_begin(&p,&v,&c));
    /* Two cached points on the same ray: the nearer one must win regardless
       of insertion order. This depth buffer is a reduced-resolution preview. */
    preview3d_add(&p,PLOT_X0,PLOT_Y0,0,1,0,C_BLUE);
    preview3d_add(&p,PLOT_X0+4,PLOT_Y0,0,-1,0,C_RED);
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1);
    assert(dgetpixel(198,109)==C_BLUE);preview3d_free(&p);
    /* Coarse points depth-test against one another, but cannot overwrite a
       previously visible (more reliable) sample at the same pixel. */
    assert(preview3d_begin(&p,&v,&c)&&preview3d_aux_begin(&p));
    preview3d_aux_add(&p,0,1.0f/3,0,C_BLUE);
    preview3d_aux_add(&p,0,-1.0f/3,0,C_RED);
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&dgetpixel(198,109)==C_BLUE);
    preview3d_add(&p,PLOT_X0,PLOT_Y0,0,-1,0,C_GREEN);
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&dgetpixel(198,109)==C_GREEN);
    preview3d_free(&p);
    compile("x^2+y^2+z^2=1");RaySurface s={&pr,0};RayStats rs;
    assert(preview3d_begin(&p,&v,&c));RayRenderOptions opt={.preview=&p};
    dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&rs)==1);
    assert(p.count>150&&p.count<=p.capacity&&rs.hits>2000);
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&colored()>1000);
    c.yaw=.3f;dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&colored()>500);
    preview3d_free(&p);
    c.yaw=0;VoxelStats vs;assert(preview3d_begin(&p,&v,&c));dclear(C_WHITE);
    assert(voxel3d_render_ex(&s,1,&vars,&v,&c,&p,NULL,NULL,&vs)==1);
    assert(p.count>100&&p.count<=p.capacity);
    dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&colored()>1000);
    c.pitch=.2f;dclear(C_WHITE);assert(preview3d_draw(&p,&c)==1&&colored()>500);
    assert(host_vram_guards_ok());preview3d_free(&p);
    /* User's equation: retain coarse sampled surface points behind the
       initially visible pixels, then look from the opposite side. */
    compile("sin(x)+sin(y)+sin(z)=0");v=(Volume){0,0,0,10,12,0};
    c=(Camera){.yaw=.43083f,.pitch=.6083f,.zoom=1,.radius=10,
               .eye_x=11.4f,.eye_y=24.8f,.eye_z=19,.perspective=1};
    RayGridCache grid={0};assert(preview3d_begin(&p,&v,&c));
    opt=(RayRenderOptions){.preview=&p,.grid=&grid};dclear(C_WHITE);
    assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&rs)==1);
    assert(p.count>1000&&p.aux_count>1000&&p.aux_count<=PREVIEW3D_AUX_CAPACITY);
    Camera opposite=c;opposite.eye_x=-11.4f;opposite.eye_y=-24.8f;opposite.eye_z=-19;
    opposite.yaw=-2.71076f;opposite.pitch=-.6083f;
    int auxiliary=p.aux_count;p.aux_count=0;dclear(C_WHITE);
    assert(preview3d_draw(&p,&opposite)==1);int old_coverage=colored();
    p.aux_count=auxiliary;dclear(C_WHITE);assert(preview3d_draw(&p,&opposite)==1);
    assert(colored()>old_coverage+2000);
    /* A second final PLOT reuses the exact candidate grid and keeps the
       coarse points without performing another grid evaluation. */
    assert(preview3d_recapture(&p,&v,&opposite,1)&&p.count==0&&p.aux_count==auxiliary);
    dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&opposite,&opt,NULL,NULL,&rs)==1&&rs.grid_reused);
    assert(p.count>0&&p.aux_count==auxiliary&&host_vram_guards_ok());
    preview3d_free(&p);ray_grid_cache_clear(&grid);
    puts("Point-cloud preview: RAY/VOXEL capture, added coverage, cache reuse, occlusion and VRAM guards passed.");
}
