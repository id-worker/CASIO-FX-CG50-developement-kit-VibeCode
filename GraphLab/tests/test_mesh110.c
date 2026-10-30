#include "../src/mesh_preview.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
static Parsed parsed;static Program program;static EvalVars vars;
static unsigned evals;
float __real_expr_eval_real(Program const *,EvalVars const *);
float __wrap_expr_eval_real(Program const *p,EvalVars const *v){evals++;return __real_expr_eval_real(p,v);}
void *__real_malloc(size_t);
static size_t fail_size;static int failures;
void *__wrap_malloc(size_t n){if(fail_size==n){failures++;return NULL;}return __real_malloc(n);}
static void compile(char const *s){assert(expr_parse(&parsed,s,0)&&expr_compile(&parsed,&program,NULL,0));}
static int cancel(int done,int total,void *p){(void)done;(void)total;return --*(int *)p>0;}
static void clear(void){dclear(C_WHITE);}
static int pixels(void){int n=0;for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++)n+=dgetpixel(x,y)!=C_WHITE;return n;}
static Camera look(float yaw,float pitch,float distance)
{
    float r=3;
    return (Camera){.yaw=yaw,.pitch=pitch,.zoom=1,.radius=r,
        .eye_x=r*distance*sinf(yaw)*cosf(pitch),.eye_y=r*distance*cosf(yaw)*cosf(pitch),
        .eye_z=r*distance*sinf(pitch),.perspective=1};
}
int main(void)
{
    host_keys(NULL,0);compile("x^2+y^2+z^2=4");
    RaySurface surface={&program,0};Volume vol={0,0,0,3,20,0};PreviewMesh m={0};
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==1);
    assert(m.vertex_count>100&&m.triangle_count>200&&m.div==20);
    unsigned after_build=evals;int comparisons=0;
    unsigned char *mask=malloc((size_t)PLOT_W*PLOT_H);assert(mask);
    for(int n=0;n<24;n++){
        float yaw=-3.1f+n*6.2f/24,pitch=-.7f+(n%9)*.175f;
        Camera c=look(yaw,pitch,2.6f);clear();assert(preview_mesh_draw(&m,&c));assert(pixels()>100);
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)
            mask[(size_t)y*PLOT_W+x]=dgetpixel(PLOT_X0+x,PLOT_Y0+y)!=C_WHITE;
        clear();Render3D *reference=render3_begin(&c);assert(reference);
        for(int i=0;i<m.triangle_count;i++){
            PMTriangle t=m.triangles[i];Triangle3 tri={{m.vertices[t.a],m.vertices[t.b],m.vertices[t.c]},0,0};
            assert(render3_triangle(&tri,reference));
        }
        render3_end(reference);
        int both=0,either=0;
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++){
            int a=mask[(size_t)y*PLOT_W+x],b=dgetpixel(PLOT_X0+x,PLOT_Y0+y)!=C_WHITE;
            both+=a&&b;either+=a||b;
        }
        if(either){float ratio=(float)both/either;if(ratio<.91f){fprintf(stderr,"camera %d mask IoU %.4f\n",n,ratio);abort();}comparisons++;}
    }
    assert(comparisons==24&&evals==after_build);
    /* Inside, behind, close to near plane, and at unusual camera angles.
       This exercises clipping and guards, without asserting analytic accuracy. */
    srand(12345);
    for(int i=0;i<500;i++){
        Camera c=look((float)(rand()%6280-3140)/1000,(float)(rand()%2900-1450)/1000,
                      (float)(rand()%6000)/1000);
        c.zoom=.2f+(rand()%3800)/1000.0f;
        clear();int rc=preview_mesh_draw(&m,&c);assert(rc==0||rc==1);
        assert(host_vram_guards_ok());
    }
    assert(evals==after_build);free(mask);preview_mesh_free(&m);
    compile("sin(9*x)+sin(9*y)+sin(9*z)=0");
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==1);
    assert(m.div<20&&m.triangle_count<=PM_MAX_TRIANGLES&&!m.overflow);
    int low=0,high=0;for(int i=0;i<m.vertex_count;i++){if(m.vertices[i].z< -12000)low=1;if(m.vertices[i].z>12000)high=1;}
    assert(low&&high);preview_mesh_free(&m);
    /* A coarse preview is deliberately approximate: an off-grid tiny sphere
       can be absent at M8 but present at M20. The final RAY is separate. */
    compile("(x-0.3)^2+(y-0.3)^2+(z-0.3)^2=0.04");vol.div=8;
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)<0&&!m.valid);
    preview_mesh_free(&m);vol.div=20;
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==1&&m.triangle_count>0);
    preview_mesh_free(&m);
    compile("1/x=0");assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)<0&&!m.valid);
    compile("x^2+y^2+z^2<=4");assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)<0&&!m.valid);
    compile("z+x^2=sqrt(y)");assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==1&&m.triangle_count>50);
    preview_mesh_free(&m);
    compile("x^2+y^2+z^2=4");int calls=2;
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,cancel,&calls)==0&&!m.valid&&!m.vertices);
    size_t sizes[]={PM_MAX_VERTICES*sizeof(Point3),PM_MAX_TRIANGLES*sizeof(PMTriangle),16384*sizeof(uint16_t)};
    for(unsigned i=0;i<sizeof sizes/sizeof *sizes;i++){
        fail_size=sizes[i];failures=0;assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==-2);
        assert(failures&&m.vertices==NULL&&m.triangles==NULL);fail_size=0;
    }
    assert(preview_mesh_build(&m,&surface,1,&vars,&vol,NULL,NULL)==1);
    Camera c=look(.75f,.55f,2.8f);fail_size=(size_t)PLOT_W*PLOT_H*sizeof(uint16_t);failures=0;
    clear();assert(!preview_mesh_draw(&m,&c)&&failures);fail_size=0;
    clear();assert(preview_mesh_draw(&m,&c)&&pixels()>100);preview_mesh_free(&m);
    assert(host_vram_guards_ok());
    puts("Mesh preview: 24 reference masks, 500 camera/clip guards, no VM in redraw, full-cube retry, M8 detail loss, poles, inequality fallback and low-memory passed.");
}
