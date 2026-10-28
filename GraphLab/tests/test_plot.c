#include "../src/plot.h"
#include "../src/plot2d.h"
#include "host_gint.h"
#include <math.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed p; static Program pr;
static void compile(const char *s) { if(!expr_parse(&p,s,0)||!expr_compile(&p,&pr,NULL,0)){fprintf(stderr,"parse %s: %s\n",s,p.message);abort();} }
typedef struct {int count;float max_error;} Collector;
static void circle(float x0,float y0,float x1,float y1,void *context) {
    Collector *c=context;float e0=fabsf(x0*x0+y0*y0-1),e1=fabsf(x1*x1+y1*y1-1);
    if(e0>c->max_error)c->max_error=e0;
    if(e1>c->max_error)c->max_error=e1;
    c->count++;
}
static void diagonal(float x0,float y0,float x1,float y1,void *context) {
    Collector *c=context;float e0=fabsf(x0-y0),e1=fabsf(x1-y1);
    if(e0>c->max_error)c->max_error=e0;
    if(e1>c->max_error)c->max_error=e1;
    c->count++;
}
static void any(float x0,float y0,float x1,float y1,void *context) {(void)x0;(void)y0;(void)x1;(void)y1;(*(int*)context)++;}
static int cancel(int d,int t,void *ctx) {(void)t;(void)ctx;return d<2;}
static int triangles(Triangle3 const *t,void *ctx){(void)t;(*(int*)ctx)++;return 1;}
static int cancel_triangles(Triangle3 const *t,void *ctx){(void)t;return ++(*(int*)ctx)<20;}
int main(void) {
    host_keys(NULL,0);
    EvalVars vars={0};PlaneView plane={-2,2,-2,2,64,64,0};PlotStats stats;Collector c={0};
    compile("x^2+y^2=1");assert(plot_contour(&pr,&vars,&plane,circle,&c,NULL,NULL,&stats)==1);
    assert(c.count>30 && c.max_error<0.01f && !stats.canceled);
    c=(Collector){0};compile("y=x");assert(plot_contour(&pr,&vars,&plane,diagonal,&c,NULL,NULL,&stats)==1);assert(c.count>20 && c.max_error<0.001f);
    c=(Collector){0};compile("x");assert(plot_contour(&pr,&vars,&plane,diagonal,&c,NULL,NULL,&stats)==1);assert(c.count>20 && c.max_error<0.001f);
    int segments=0;compile("1/x=0");plane=(PlaneView){-2.03f,2,-2,2,64,64,0};assert(plot_contour(&pr,&vars,&plane,any,&segments,NULL,NULL,&stats)==1);assert(segments==0);
    compile("sqrt(x)=y");segments=0;assert(plot_contour(&pr,&vars,&plane,any,&segments,NULL,NULL,&stats)==1);assert(segments>0 && stats.invalid>0);
    compile("x^2+y^2=1");segments=0;assert(plot_contour(&pr,&vars,&plane,any,&segments,cancel,NULL,&stats)==0);assert(stats.canceled);
    Mesh mesh;assert(mesh_init(&mesh,6000));Volume v={0,0,0,2,12,0};compile("x^2+y^2+z^2=1");
    assert(plot_surface(&mesh,&pr,&vars,&v,0,NULL,NULL,&stats)==1);assert(mesh.count>100 && !mesh.truncated);
    float err=0;for(int i=0;i<mesh.count;i++)for(int j=0;j<3;j++) {
        Point3 q=mesh.tri[i].p[j];float x=q.x/16000.0f*v.radius,y=q.y/16000.0f*v.radius,z=q.z/16000.0f*v.radius;
        float e=fabsf(x*x+y*y+z*z-1);if(e>err)err=e;
    }assert(err<0.04f);mesh_free(&mesh);
    assert(mesh_init(&mesh,6000));v=(Volume){0,0,0,3,12,0};compile("z+x^2=sqrt(y)");
    assert(plot_surface(&mesh,&pr,&vars,&v,0,NULL,NULL,&stats)==1);assert(mesh.count>10 && stats.invalid>0 && !mesh.truncated);mesh_free(&mesh);
    assert(mesh_init(&mesh,5));compile("x^2+y^2+z^2=1");v=(Volume){0,0,0,2,12,0};
    assert(plot_surface(&mesh,&pr,&vars,&v,0,NULL,NULL,&stats)==1);assert(mesh.count==5 && mesh.truncated);mesh_free(&mesh);
    assert(mesh_init(&mesh,6000));assert(plot_surface(&mesh,&pr,&vars,&v,0,cancel,NULL,&stats)==0 && stats.canceled);mesh_free(&mesh);
    assert(mesh_init(&mesh,2));
    Triangle3 a={.p={{-8000,16000,-8000},{8000,16000,-8000},{0,16000,8000}},.slot=0};
    Triangle3 b={.p={{-8000,-16000,-8000},{8000,-16000,-8000},{0,-16000,8000}},.slot=1};
    Camera cam={.yaw=0,.pitch=0,.zoom=1};mesh.tri[0]=a;mesh.tri[1]=b;mesh.count=2;dclear(C_WHITE);assert(mesh_render(&mesh,&cam,NULL,NULL)==1);
    int x=(PLOT_X0+PLOT_X1)/2,y=(PLOT_Y0+PLOT_Y1)/2;int near=dgetpixel(x,y);assert(near!=C_WHITE);
    mesh.tri[0]=b;mesh.tri[1]=a;dclear(C_WHITE);assert(mesh_render(&mesh,&cam,NULL,NULL)==1);assert(dgetpixel(x,y)==near);
    mesh.count=0;cam.axes=1;cam.center_x=cam.center_y=cam.center_z=10;cam.radius=1;
    dclear(C_WHITE);assert(mesh_render(&mesh,&cam,NULL,NULL)==1);
    for(int yy=PLOT_Y0;yy<=PLOT_Y1;yy++)for(int xx=PLOT_X0;xx<=PLOT_X1;xx++)assert(dgetpixel(xx,yy)==C_WHITE);
    cam.center_x=cam.center_y=cam.center_z=0;
    dclear(C_WHITE);assert(mesh_render(&mesh,&cam,NULL,NULL)==1);assert(dgetpixel(x,y)!=C_WHITE);
    mesh_free(&mesh);
    /* Streaming must not stop at the old 6000-triangle ceiling. */
    v=(Volume){.x=0,.y=0,.z=0,.radius=3,.div=32};
    compile("sin(7*x)+sin(7*y)+sin(7*z)=0");int streamed=0;
    assert(plot_surface_stream(&pr,&vars,&v,0,triangles,&streamed,NULL,NULL,&stats)==1);
    assert(streamed>6000 && !stats.canceled && stats.cells==32*32*32);
    streamed=0;assert(plot_surface_stream(&pr,&vars,&v,0,cancel_triangles,&streamed,NULL,NULL,&stats)==0);
    assert(stats.canceled && streamed==20);
    /* Configurable pixel tolerance reduces refinement work without the pole
       guard discarding valid branches just because a bracket became narrow. */
    compile("sin(x*y)=0");plane=(PlaneView){-9,9,-5,5,96,45,0};int full=0,fast=0;
    assert(plot_contour(&pr,&vars,&plane,any,&full,NULL,NULL,&stats)==1);int full_evals=stats.evaluations;
    ContourOptions opt={2.0f,PLOT_W,PLOT_H,NULL,NULL};
    assert(plot_contour_opt(&pr,&vars,&plane,any,&fast,&opt,NULL,NULL,&stats)==1);
    assert(fast==full && stats.evaluations<full_evals && stats.rejected==0);
    compile("1/x=0");plane=(PlaneView){-2.03f,2,-2,2,64,64,0};fast=0;
    assert(plot_contour_opt(&pr,&vars,&plane,any,&fast,&opt,NULL,NULL,&stats)==1 && fast==0);
    /* Explicitly declared, sample-aligned periods save work; off by default. */
    compile("y=sin(2*pi*x)");
    plane=(PlaneView){.xmin=0,.xmax=4,.ymin=-2,.ymax=2,.cols=64,.rows=32};
    int seg0=0,seg1=0;assert(plot_contour(&pr,&vars,&plane,any,&seg0,NULL,NULL,&stats)==1);
    int eval0=stats.evaluations;plane.period_x=1;
    assert(plot_contour(&pr,&vars,&plane,any,&seg1,NULL,NULL,&stats)==1);
    assert(stats.evaluations<eval0 && abs(seg0-seg1)<10);
    /* Perspective renderer, near clipping, labeled world axes and z-buffer. */
    Camera perspective={.yaw=.75f,.pitch=.55f,.zoom=1,.wire=1,.radius=3,.eye_x=5,.eye_y=5,.eye_z=4,.perspective=1};
    dclear(C_WHITE);Render3D *renderer=render3_begin(&perspective);assert(renderer);
    v=(Volume){.radius=3,.div=12};compile("x^2+y^2+z^2=4");
    assert(plot_surface_stream(&pr,&vars,&v,0,render3_triangle,renderer,NULL,NULL,&stats)==1);
    render3_end(renderer);int colored=0;
    for(int yy=PLOT_Y0;yy<=PLOT_Y1;yy++)for(int xx=PLOT_X0;xx<=PLOT_X1;xx++)colored+=dgetpixel(xx,yy)!=C_WHITE;
    assert(colored>300);render3_axes(&perspective);assert(host_vram_guards_ok());
    /* Perspective z is reciprocal-depth interpolated. The slanted surface
       is closer at the center, although linear camera-depth interpolation
       would choose the flat one. Result must also be drawing-order invariant. */
    Camera depthcam={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    Triangle3 slanted={.p={{-8000,8000,-8000},{8000,8000,-8000},{0,-8000,12800}},.slot=0};
    Triangle3 flat={.p={{-12000,0,-12000},{12000,0,-12000},{0,0,12000}},.slot=1};
    dclear(C_WHITE);renderer=render3_begin(&depthcam);assert(renderer);assert(render3_triangle(&slanted,renderer));render3_end(renderer);
    int slanted_color=dgetpixel(x,y);assert(slanted_color!=C_WHITE);
    dclear(C_WHITE);renderer=render3_begin(&depthcam);assert(renderer);assert(render3_triangle(&flat,renderer));render3_end(renderer);
    int flat_color=dgetpixel(x,y);assert(flat_color!=C_WHITE && flat_color!=slanted_color);
    dclear(C_WHITE);renderer=render3_begin(&depthcam);assert(renderer);assert(render3_triangle(&slanted,renderer));assert(render3_triangle(&flat,renderer));render3_end(renderer);
    assert(dgetpixel(x,y)==slanted_color);
    dclear(C_WHITE);renderer=render3_begin(&depthcam);assert(renderer);assert(render3_triangle(&flat,renderer));assert(render3_triangle(&slanted,renderer));render3_end(renderer);
    assert(dgetpixel(x,y)==slanted_color);
    perspective.eye_x=perspective.eye_y=perspective.eye_z=0;
    renderer=render3_begin(&perspective);assert(renderer);
    Triangle3 crossing={.p={{-8000,-8000,0},{8000,8000,0},{0,-8000,8000}}};
    assert(render3_triangle(&crossing,renderer));render3_end(renderer);
    printf("2D contours/period reuse, 3D streamed >6000 triangles, clipping, z-buffer, axes and cancellation passed.\n");
}
