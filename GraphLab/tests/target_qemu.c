/* Bare SH/QEMU checks: no Casio operating system or display is emulated. */
#include "../src/expr.h"
#include "../src/layout.h"
#include "../src/plot.h"
#include "../src/ray.h"
#include "../src/voxel.h"
#include "../src/plot2d.h"
#include "../src/fractal.h"
#include "../src/navigation.h"
#include <gint/display.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <fxlibc/printf.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[1024*1024]; /* monotonic test allocator; frees are not reused */
static uint16_t guarded_vram[DWIDTH*DHEIGHT+2];
uint16_t *gint_vram=guarded_vram+1;
static unsigned used;
void *fxlibc_hal_malloc(unsigned n) {n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
void dsize(char const *s,font_t const *font,int *w,int *h){(void)font;if(w)*w=(int)strlen(s)*6;if(h)*h=10;}
void dtext(int x,int y,int col,char const *s){(void)x;(void)y;(void)col;(void)s;}
static int lines;
void dline(int x,int y,int xx,int yy,int c){(void)x;(void)y;(void)xx;(void)yy;(void)c;lines++;}
void drect_border(int x,int y,int xx,int yy,int c,int w,int bc){(void)x;(void)y;(void)xx;(void)yy;(void)c;(void)w;(void)bc;}
static Parsed p;static Program pr;static EvalVars v;static char b[EXPR_MAX];
static int segments,triangles;
static int collect_triangle(Triangle3 const *t,void *ctx){(void)t;(void)ctx;triangles++;return 1;}
static void segment(float x,float y,float xx,float yy,void *ctx){(void)x;(void)y;(void)xx;(void)yy;(void)ctx;segments++;}
static int compile(char const *s){return expr_parse(&p,s,0)&&expr_compile(&p,&pr,NULL,0);}
int main(void){
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    __printf_enable_fp();
    snprintf(b,sizeof b,"%.6g %.3g",1.25,0.05);if(strcmp(b,"1.25 0.05"))return 10;
    if(!compile("z+x^2=sqrt(y)"))return 11;
    v.v[V_Z]=5;v.v[V_X]=2;v.v[V_Y]=9;if(fabsf(expr_eval_real(&pr,&v)-6)>0.0001f)return 12;
    if(!compile("1e-2+.5") || fabsf(expr_eval_real(&pr,&v)-0.51f)>0.0001f)return 13;
    if(!compile("z^2+c"))return 14;
    v.cz=(Complex){1,2};v.cc=(Complex){-.5f,.25f};Complex z=expr_eval_complex(&pr,&v);
    if(fabsf(z.re+3.5f)>.001f || fabsf(z.im-4.25f)>.001f)return 15;
    FractalView fv={-.5f,0,.012f,-.8f,.156f,4,40,4,0};
    if(fractal_point(&pr,&v,0,0,&fv)!=41 || fractal_point(&pr,&v,2,0,&fv)!=2)return 16;
    if(!compile("x^2+y^2=1"))return 17;
    PlaneView plane={-2,2,-2,2,64,64,0};PlotStats stats;
    if(plot_contour(&pr,&v,&plane,segment,NULL,NULL,NULL,&stats)!=1 || segments<20)return 18;
    if(!compile("z+x^2=sqrt(y)"))return 19;
    Volume vol={0,0,0,3,10,0};Mesh mesh;if(!mesh_init(&mesh,6000))return 20;
    if(plot_surface(&mesh,&pr,&v,&vol,0,NULL,NULL,&stats)!=1 || mesh.count<10)return 21;
    mesh_free(&mesh);
    int n=0;for(int i=0;i<15;i++)b[n++]='(';
    for(int i=0;i<49;i++){if(i)b[n++]='+';b[n++]='x';}
    for(int i=0;i<15;i++)b[n++]=')';b[n]=0;
    if(!expr_parse(&p,b,1) || !layout_measure(&p))return 22;
    FormulaCaret caret=layout_draw(&p,n/2,0,0,C_BLACK,C_BLUE,1);
    if(!caret.found)return 23;
    layout_draw(&p,n/2,0,0,C_BLACK,C_BLUE,0);
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 24;
    if(!expr_parse(&p,"1/2",1)||!layout_measure(&p))return 25;
    lines=0;layout_draw(&p,-1,0,0,C_BLACK,C_BLUE,0);if(!lines)return 26;
    if(!compile("summ(0,inf,n,x^n/n!)"))return 27;
    v.v[V_X]=10;float sum=expr_eval_real(&pr,&v);
    if(!isfinite(sum)||fabsf(sum-expf(10))>1.0f)return 28;
    if(!compile("summ(1,3,n,summ(1,2,n,n)+n)"))return 29;
    if(fabsf(expr_eval_real(&pr,&v)-15)>.0001f)return 30;
    if(!compile("summ(0,inf,n,z^n/n!)"))return 31;
    v.cz=(Complex){0,1};z=expr_eval_complex(&pr,&v);
    if(fabsf(z.re-cosf(1))>.001f||fabsf(z.im-sinf(1))>.001f)return 32;
    if(!expr_parse(&p,"summ(0,inf,n,x^n/n!)",1)||!layout_measure(&p))return 33;
    if(layout_cursor_step(&p,0,1)!=13)return 34;
    for(int k=0;k<=p.len;k++)if(!layout_draw(&p,k,0,0,C_BLACK,C_BLUE,1).found)return 35;
    b[0]='1';for(int k=1;k<64;k++)b[k]='!';b[64]=0;
    if(!expr_parse(&p,b,1)||!layout_measure(&p))return 36;
    layout_draw(&p,32,0,0,C_BLACK,C_BLUE,0);
    if(!compile("sin(7*x)+sin(7*y)+sin(7*z)=0"))return 37;
    vol=(Volume){0,0,0,3,32,0};triangles=0;
    if(plot_surface_stream(&pr,&v,&vol,0,collect_triangle,NULL,NULL,NULL,&stats)!=1||triangles<=6000)return 38;
    Camera cam={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    Triangle3 slanted={.p={{-8000,8000,-8000},{8000,8000,-8000},{0,-8000,12800}},.slot=0};
    Triangle3 flat={.p={{-12000,0,-12000},{12000,0,-12000},{0,0,12000}},.slot=1};
    guarded_vram[0]=guarded_vram[DWIDTH*DHEIGHT+1]=0x5aa5;
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
    Render3D *rr=render3_begin(&cam);if(!rr)return 39;
    render3_triangle(&slanted,rr);render3_triangle(&flat,rr);render3_end(rr);
    int center=((PLOT_Y0+PLOT_Y1)/2)*DWIDTH+(PLOT_X0+PLOT_X1)/2;
    uint16_t pixel=gint_vram[center];if(pixel==C_WHITE)return 40;
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
    rr=render3_begin(&cam);if(!rr)return 41;
    render3_triangle(&flat,rr);render3_triangle(&slanted,rr);render3_end(rr);
    if(gint_vram[center]!=pixel || guarded_vram[0]!=0x5aa5 || guarded_vram[DWIDTH*DHEIGHT+1]!=0x5aa5)return 42;
    if(!compile("x^2+y^2+z^2=1"))return 43;
    v=(EvalVars){0};vol=(Volume){0,0,0,3,12,0};
    RaySurface rs={&pr,0};RayStats rstats;
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
    if(ray3d_render(&rs,1,&v,&vol,&cam,NULL,NULL,&rstats)!=1 || rstats.hits<2000 || gint_vram[center]==C_WHITE)return 44;
    if(guarded_vram[0]!=0x5aa5 || guarded_vram[DWIDTH*DHEIGHT+1]!=0x5aa5)return 45;
    if(!compile("y<x"))return 49;
    Program rhs;Relation rel;if(!expr_compile_explicit_y(&p,&rhs,&rel)||rel!=REL_LT)return 50;
    if(!expr_relation_true(REL_LT,-1)||expr_relation_true(REL_LT,0)||expr_relation_true(REL_LE,NAN))return 51;
    plane=(PlaneView){-2,2,-2,2,64,64,0};segments=0;
    if(plot_explicit_y(&rhs,rel,&v,&plane,386,181,segment,NULL,NULL,NULL,NULL,NULL,&stats)!=1 || segments<300)return 52;
    if(!compile("x^2+y^2+z^2=4"))return 53;
    vol=(Volume){0,0,0,3,12,0};VoxelStats vox;rs=(RaySurface){&pr,0};
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
    int vrc=voxel3d_render(&rs,1,&v,&vol,&cam,NULL,NULL,&vox);
    if(vrc!=1)return 60;
    if(vox.occupied<100)return 61;
    if(vox.pixels<500)return 62;
    if(gint_vram[center]==C_WHITE)return 63;
    if(guarded_vram[0]!=0x5aa5||guarded_vram[DWIDTH*DHEIGHT+1]!=0x5aa5)return 55;
    if(!compile("x^2+y^2+z^2<=4"))return 56;
    if(voxel3d_render(&rs,1,&v,&vol,&cam,NULL,NULL,&vox)!=1||vox.occupied<100||vox.occupied>=1728)return 57;
    if(!expr_parse(&p,"summ(0,inf,n,)",1)||!layout_measure(&p))return 46;
    int cursor=6;if(layout_sum_field(&p,cursor,NULL)!=0 || layout_sum_step(&p,cursor,-1)!=10 || layout_delete_start(&p,10)!=7)return 47;
    if(layout_cursor_step(&p,6,1)!=13 || layout_delete_start(&p,13)!=13)return 68;
    if(!compile("prod(1,5,n,n)") || fabsf(expr_eval_real(&pr,&v)-120)>.001f)return 69;
    if(!expr_parse(&p,"prod(1,5,n,)",1) || !layout_measure(&p) || layout_sum_field(&p,11,NULL)!=3)return 70;
    GraphState nstate={0};
    nstate.volume_radius=3;nstate.camera_yaw=.75f;nstate.camera_pitch=.55f;nstate.camera_zoom=1;
    nstate.eye_x=1;nstate.eye_y=2;nstate.eye_z=3;nstate.follow_eye=1;
    if(!navigation_step(&nstate,NAV_FORWARD|NAV_UP|NAV_RISE,1,0))return 64;
    if(fabsf(nstate.camera_pitch-.485f)>.0001f)return 65;
    if(nstate.volume_x!=0 || nstate.volume_y!=0 || nstate.volume_z!=0)return 66;
    Camera nearcam={.yaw=0.75f,.pitch=0.55f,.zoom=1,.radius=3,.perspective=1};
    nearcam.eye_x=.2f*cosf(nearcam.pitch)*sinf(nearcam.yaw);
    nearcam.eye_y=.2f*cosf(nearcam.pitch)*cosf(nearcam.yaw);
    nearcam.eye_z=.2f*sinf(nearcam.pitch);
    Axis3Geometry axes;if(!render3_axis_geometry(&nearcam,0,&axes) || axes.count<2 || axes.reference_step>=.5f)return 71;
    float ox,oy;if(!render3_project_world(&nearcam,0,0,0,&ox,&oy))return 72;
    float dx=axes.x1-axes.x0,dy=axes.y1-axes.y0;
    if(fabsf(dx*(oy-axes.y0)-dy*(ox-axes.x0))>2*sqrtf(dx*dx+dy*dy))return 73;
    /* Exercise precise face clipping with real SH soft-double arithmetic:
       a voxel face is only ~0.00006 world units away in an R=10000 cube. */
    if(!compile("y=0"))return 74;
    vol=(Volume){0,0,0,10000,20,0};
    cam=(Camera){.yaw=.2f,.pitch=.1f,.zoom=1,.radius=10000,.eye_y=1000.00006f,.perspective=1};
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
    if(voxel3d_render(&rs,1,&v,&vol,&cam,NULL,NULL,&vox)!=1 || gint_vram[center]==C_WHITE)return 75;
    for(int py=PLOT_Y0;py<PLOT_Y1;py++)for(int px=PLOT_X0;px<PLOT_X1;px++)
        if(gint_vram[py*DWIDTH+px]==C_WHITE)return 76;
    if(guarded_vram[0]!=0x5aa5||guarded_vram[DWIDTH*DHEIGHT+1]!=0x5aa5)return 77;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 48;
    return 0;
}
