#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
static Program program[2];static Parsed parsed;
static void compile(int slot,char const *text){assert(expr_parse(&parsed,text,0));assert(expr_compile(&parsed,&program[slot],NULL,0));}
static int cancel_calls,cancel_limit;
static int cancel(int done,int total,void *ctx){(void)done;(void)total;(void)ctx;return ++cancel_calls<cancel_limit;}
int main(void){
    host_keys(NULL,0);
    EvalVars vars={0};Volume v={0,0,0,3,20,0};
    Camera c={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    RayStats stats;compile(0,"x^2+y^2+z^2=1");
    RaySurface one[]={{&program[0],0}};
    dclear(C_WHITE);assert(ray3d_render(one,1,&vars,&v,&c,NULL,NULL,&stats)==1);
    assert(stats.hits>2000 && stats.rays==PLOT_W*PLOT_H && stats.occupied>0 && stats.evaluations>0);
    /* Compare to the analytic projected sphere silhouette, ignoring a 2px edge. */
    int expected=0,correct=0,false_inside=0;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++){
        float dx=(x-(PLOT_X0+PLOT_X1)*.5f)/(72*2.8f),dz=((PLOT_Y0+PLOT_Y1)*.5f-y)/(72*2.8f);
        /* Eye is (0,2,0), direction is (dx,-1,dz), sphere radius 1/3. */
        float discriminant=4-((dx*dx+1+dz*dz)*(4-1.0f/9));
        int drawn=dgetpixel(x,y)!=C_WHITE;
        if(discriminant>.003f){expected++;if(drawn)correct++;}
        if(discriminant<-.003f && drawn)false_inside++;
    }
    printf("Sphere: %d/%d interior pixels, %d far false positives; hits=%d evals=%d\n",correct,expected,false_inside,stats.hits,stats.evaluations);
    assert(expected>1000 && correct>expected*98/100 && false_inside==0);
    int colors[65536]={0},unique=0;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++){
        int col=dgetpixel(x,y);if(col!=C_WHITE && !colors[col]){colors[col]=1;unique++;}
    }
    assert(unique>10); /* normals change smoothly across the actual surface */
    int centerx=(PLOT_X0+PLOT_X1)/2,centery=(PLOT_Y0+PLOT_Y1)/2;
    compile(1,"x^2+y^2+z^2=4");
    RaySurface outer[]={{&program[1],1}},both[]={{&program[0],0},{&program[1],1}};
    dclear(C_WHITE);assert(ray3d_render(outer,1,&vars,&v,&c,NULL,NULL,&stats)==1);
    int outer_col=dgetpixel(centerx,centery);assert(outer_col!=C_WHITE);
    dclear(C_WHITE);assert(ray3d_render(both,2,&vars,&v,&c,NULL,NULL,&stats)==1);
    assert(dgetpixel(centerx,centery)==outer_col); /* nearest surface, irrespective of array order */
    compile(0,"z+x^2=sqrt(y)");dclear(C_WHITE);
    assert(ray3d_render(one,1,&vars,&v,&c,NULL,NULL,&stats)==1 && stats.hits>500);
    cancel_calls=0;cancel_limit=8;assert(ray3d_render(one,1,&vars,&v,&c,cancel,NULL,&stats)==0 && stats.canceled);
    cancel_calls=0;cancel_limit=160;assert(ray3d_render(one,1,&vars,&v,&c,cancel,NULL,&stats)==0 && stats.canceled && stats.rays>0);
    /* Surface 0.05 units ahead: the old near plane at 0.12 hid it. */
    compile(0,"y=0");c.eye_y=.05f;dclear(C_WHITE);
    assert(ray3d_render(one,1,&vars,&v,&c,NULL,NULL,&stats)==1 && stats.hits>1000);
    assert(dgetpixel(centerx,centery)!=C_WHITE);
    /* Also check the same 0.00005-world-unit distance for several clip sizes,
       including a large R where near used to grow to hundreds of units. */
    float radii[]={.001f,.05f,3.0f,1000.0f,10000.0f};
    c.eye_y=.00005f;
    for(unsigned i=0;i<sizeof radii/sizeof *radii;i++){
        v.radius=c.radius=radii[i];dclear(C_WHITE);
        assert(ray3d_render(one,1,&vars,&v,&c,NULL,NULL,&stats)==1);
        assert(stats.hits>1000 && dgetpixel(centerx,centery)!=C_WHITE && host_vram_guards_ok());
    }
    v.radius=c.radius=3;c.eye_y=6;compile(0,"1/x=0");dclear(C_WHITE);
    assert(ray3d_render(one,1,&vars,&v,&c,NULL,NULL,&stats)==1 && stats.hits==0); /* pole is not a surface */
    assert(host_vram_guards_ok());
    puts("Polygon-free ray casting: analytic sphere, smooth normals, visibility, domain and cancellation passed.");
}
