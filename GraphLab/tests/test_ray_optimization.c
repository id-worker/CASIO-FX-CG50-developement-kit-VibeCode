#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;static Program original,height,normal;
static uint16_t reference[DWIDTH*DHEIGHT];
static void compile(char const *s){assert(expr_parse(&parsed,s,0)&&expr_compile(&parsed,&original,NULL,0));}
static void copy_image(void){memcpy(reference,gint_vram,sizeof reference);}
static int same_image(void){return !memcmp(reference,gint_vram,sizeof reference);}
static int stop(int done,int total,void *ctx){(void)total;(void)ctx;return done>4?0:1;}
/* A dense independent screen-ray sign-search reference for one oscillating
   height field. Like any finite sampler it is not an interval proof. */
static int height_reference(float ox,float oy,float oz,float dx,float dy,float dz)
{
    float lo=0,hi=1000,o[]={ox,oy,oz},d[]={dx,dy,dz};
    for(int j=0;j<3;j++){
        if(fabsf(d[j])<1e-9f){if(fabsf(o[j])>3)return 0;continue;}
        float a=(-3-o[j])/d[j],b=(3-o[j])/d[j];
        if(a>b){float z=a;a=b;b=z;}if(a>lo)lo=a;if(b<hi)hi=b;if(lo>hi)return 0;
    }
    float prev=0;
    for(int k=0;k<=512;k++){
        float t=lo+(hi-lo)*k/512.0f,x=ox+t*dx,y=oy+t*dy,z=oz+t*dz;
        float q=z-sinf(15*x)*cosf(15*y);
        if(k&&((prev<0&&q>=0)||(prev>0&&q<=0)))return 1;
        prev=q;
    }
    return 0;
}
static int missing(Camera const *c)
{
    int miss=0;float cy=cosf(c->yaw),sy=sinf(c->yaw),cp=cosf(c->pitch),sp=sinf(c->pitch),f=201.6f*c->zoom;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y+=2)for(int x=PLOT_X0;x<=PLOT_X1;x+=2){
        float vv=((PLOT_Y0+PLOT_Y1)*.5f-y)/f,hh=(x-(PLOT_X0+PLOT_X1)*.5f)/f,ry=-sp*vv-cp;
        float dx=cy*hh+sy*ry,dy=-sy*hh+cy*ry,dz=cp*vv-sp;
        if(height_reference(c->eye_x,c->eye_y,c->eye_z,dx,dy,dz)&&dgetpixel(x,y)==C_WHITE)miss++;
    }
    return miss;
}
int main(void)
{
    host_keys(NULL,0);EvalVars vars={0};Volume v={0,0,0,3,32,0};
    Camera c={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=3,.eye_x=3.4f,.eye_y=3.6f,.eye_z=3,.perspective=1};
    RaySurface s={&original,0};RayStats st;
    compile("z=0");dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1);copy_image();
    compile("z^2=0");assert(expr_compile_zero_surface(&parsed,&normal));s.program=&normal;
    dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&same_image());
    compile("abs(z)=0");assert(expr_compile_zero_surface(&parsed,&normal));
    dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&same_image());
    s.program=&original;compile("x^2+y^2+z^2=1");dclear(C_WHITE);
    assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&st.hits>3000);copy_image();
    compile("(x^2+y^2+z^2-1)^2=0");assert(expr_compile_zero_surface(&parsed,&normal));s.program=&normal;
    dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&same_image());
    RayGridCache grid={0};RayRenderOptions opt={.grid=&grid};s.program=&original;
    compile("x^2+y^2+z^2=a");vars.v[V_A]=1;dclear(C_WHITE);
    assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&st)==1&&!st.grid_reused&&grid.valid);
    c.yaw+=.15f;dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&st)==1&&st.grid_reused);
    int reuse_evals=st.evaluations;copy_image();dclear(C_WHITE);
    assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&same_image()&&st.evaluations>reuse_evals);
    vars.v[V_A]=4;dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&st)==1&&!st.grid_reused);
    copy_image();dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1&&same_image());
    v.div=20;dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,NULL,NULL,&st)==1&&!st.grid_reused);
    ray_grid_cache_clear(&grid);dclear(C_WHITE);
    assert(ray3d_render_ex(&s,1,&vars,&v,&c,&opt,stop,NULL,&st)==0&&st.canceled&&!grid.valid);
    ray_grid_cache_clear(&grid);
    compile("z=sin(15*x)*cos(15*y)");assert(expr_compile_explicit_z(&parsed,&height));
    s.program=&original;v.div=48;c.yaw=.75f;vars=(EvalVars){0};
    dclear(C_WHITE);assert(ray3d_render(&s,1,&vars,&v,&c,NULL,NULL,&st)==1);int old=missing(&c);
    RayRenderOptions explicit_options={.height={&height,NULL}};
    dclear(C_WHITE);assert(ray3d_render_ex(&s,1,&vars,&v,&c,&explicit_options,NULL,NULL,&st)==1);
    int improved=missing(&c);assert(old>200&&improved<50&&improved<old/3&&host_vram_guards_ok());
    printf("RAY: equivalent zero surfaces, parameter-safe candidate cache and sampled oscillating surface: missing %d -> %d (Q48).\n",old,improved);
}
