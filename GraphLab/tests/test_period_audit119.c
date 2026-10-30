/* Exercise a true, sample-aligned period in Ray. */
#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;
static Program pr;
static uint16_t before[DWIDTH*DHEIGHT];
static void compile(char const *s)
{assert(expr_parse(&parsed,s,0));assert(expr_compile(&parsed,&pr,NULL,0));}
static int changed(void)
{int n=0;for(int i=0;i<DWIDTH*DHEIGHT;i++)n+=before[i]!=gint_vram[i];return n;}
int main(void)
{
    host_keys(NULL,0);EvalVars vars={0};float p=6.283185307179586f;
    Volume v={0,0,0,p,20,0};Camera c={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=p,
        .eye_x=p*2.8f*cosf(.55f)*sinf(.75f),.eye_y=p*2.8f*cosf(.55f)*cosf(.75f),
        .eye_z=p*2.8f*sinf(.55f),.perspective=1};
    RaySurface surface={&pr,0};RayStats ra,rb;
    /* A bare f(x,y) uses Ray's height-lattice preparation. */
    compile("sin(x)+cos(y)");RayRenderOptions opt={.screen_step=4};v.period_x=0;
    dclear(C_WHITE);assert(ray3d_render_ex(&surface,1,&vars,&v,&c,&opt,NULL,NULL,&ra)==1);
    memcpy(before,gint_vram,sizeof before);v.period_x=p;dclear(C_WHITE);
    assert(ray3d_render_ex(&surface,1,&vars,&v,&c,&opt,NULL,NULL,&rb)==1);
    assert(rb.evaluations<ra.evaluations && rb.occupied==ra.occupied && changed()==0);
    printf("Ray height aligned period: evals %d -> %d, occupied %d, changed pixels %d\n",ra.evaluations,rb.evaluations,rb.occupied,changed());
    assert(host_vram_guards_ok());puts("Aligned true 3D period passed for the Ray height lattice.");
}
