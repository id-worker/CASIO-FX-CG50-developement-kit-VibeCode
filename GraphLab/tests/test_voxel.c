#include "../src/voxel.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static Program program[2];static Parsed parsed;
static void compile(int slot,char const *text){assert(expr_parse(&parsed,text,0));assert(expr_compile(&parsed,&program[slot],NULL,0));}
static int calls,limit;
static int cancel(int done,int total,void *ctx){(void)done;(void)total;(void)ctx;return ++calls<limit;}
static void screenshot(char const *path){
    FILE *f=fopen(path,"wb");assert(f);fprintf(f,"P6\n%d %d\n255\n",DWIDTH,DHEIGHT);
    for(int i=0;i<DWIDTH*DHEIGHT;i++){unsigned c=gint_vram[i];unsigned char rgb[3]={((c>>11)&31)*255/31,((c>>5)&63)*255/63,(c&31)*255/31};fwrite(rgb,1,3,f);}fclose(f);
}
int main(int argc,char **argv){
    host_keys(NULL,0);EvalVars vars={0};Volume v={0,0,0,3,20,0};float yaw=.75f,pitch=.55f,distance=8.4f;
    Camera c={.yaw=yaw,.pitch=pitch,.zoom=1,.radius=3,.eye_x=distance*cosf(pitch)*sinf(yaw),
              .eye_y=distance*cosf(pitch)*cosf(yaw),.eye_z=distance*sinf(pitch),.perspective=1};
    VoxelStats st;RaySurface one[]={{&program[0],0}},both[]={{&program[0],0},{&program[1],1}};
    compile(0,"x^2+y^2+z^2=4");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1);
    printf("Sphere N20 cells=%d evals=%d occupied=%d rejected=%d faces=%d pixels=%d\n",st.cells,st.evaluations,st.occupied,st.rejected,st.faces,st.pixels);
    assert(st.cells==8000&&st.occupied>500&&st.faces>100&&st.pixels>1000&&host_vram_guards_ok());
    /* The old per-face outline used b=7. Only the three normal face shades
       remain for surface 0; no dark border pixels may be introduced. */
    int dark=0,shade[3]={0,0,0};
    int cols[3]={C_RGB(23/5,23*4/5,23),C_RGB(17/5,17*4/5,17),C_RGB(29/5,29*4/5,29)};
    for(int i=0;i<DWIDTH*DHEIGHT;i++){
        dark+=gint_vram[i]==C_RGB(7/5,7*4/5,7);
        for(int j=0;j<3;j++)shade[j]+=gint_vram[i]==cols[j];
    }
    assert(dark==0 && shade[0]>0 && shade[1]>0 && shade[2]>0);
    if(argc>1)screenshot(argv[1]);
    compile(0,"z+x^2=sqrt(y)");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1 && st.invalid>0 && st.occupied>100 && st.pixels>100);
    compile(0,"sin(x)*cos(y)");dclear(C_WHITE); /* implicit z=f(x,y) */
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1 && st.occupied>100 && st.pixels>100);
    compile(0,"x^2+y^2+z^2<=4");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1&&st.occupied>1000&&st.occupied<2500);
    if(argc>2)screenshot(argv[2]);
    compile(0,"x^2+y^2+z^2>4");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1&&st.occupied>5500);
    compile(0,"1/x=0");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1 && st.occupied==0);
    compile(0,"x^2+y^2+z^2=1");compile(1,"x^2+y^2+z^2=4");
    Camera front={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    RaySurface outer[]={{&program[1],1}},reversed[]={{&program[1],1},{&program[0],0}};
    int cx=(PLOT_X0+PLOT_X1)/2,cy=(PLOT_Y0+PLOT_Y1)/2;
    dclear(C_WHITE);assert(voxel3d_render(outer,1,&vars,&v,&front,NULL,NULL,&st)==1);
    int nearest=dgetpixel(cx,cy);assert(nearest!=C_WHITE);
    dclear(C_WHITE);assert(voxel3d_render(both,2,&vars,&v,&front,NULL,NULL,&st)==1&&st.faces>0);
    assert(dgetpixel(cx,cy)==nearest);
    dclear(C_WHITE);assert(voxel3d_render(reversed,2,&vars,&v,&front,NULL,NULL,&st)==1);
    assert(dgetpixel(cx,cy)==nearest);
    /* Inside an inequality volume, show the inside of its boundary. */
    compile(0,"x^2+y^2+z^2<4");front.eye_y=0;dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&front,NULL,NULL,&st)==1&&st.pixels>100);
    assert(dgetpixel(cx,cy)!=C_WHITE);
    /* A face only 0.01 world units in front of the eye is no longer hidden
       by the old camera near plane of 0.04*R (=0.12 for this cube). */
    compile(0,"y=0");front.eye_y=.31f;dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&front,NULL,NULL,&st)==1 && st.pixels>1000);
    assert(dgetpixel(cx,cy)!=C_WHITE);
    /* Vary R over seven orders of magnitude; the front face is about
       0.00005 world units away (subject to float rounding for large R). */
    float radii[]={.001f,.05f,3.0f,1000.0f,10000.0f};
    for(unsigned i=0;i<sizeof radii/sizeof *radii;i++){
        v.radius=radii[i];front.radius=v.radius;front.eye_y=v.radius/10+.00005f;
        for(int angle=0;angle<3;angle++){
            front.yaw=angle==0?0:angle==1?.2f:-.35f;front.pitch=angle==0?0:.1f;
            dclear(C_WHITE);
            assert(voxel3d_render(one,1,&vars,&v,&front,NULL,NULL,&st)==1);
            assert(st.pixels>1000 && dgetpixel(cx,cy)!=C_WHITE && host_vram_guards_ok());
            for(int py=PLOT_Y0;py<PLOT_Y1;py++)for(int px=PLOT_X0;px<PLOT_X1;px++)
                assert(dgetpixel(px,py)!=C_WHITE); /* no holes in the viewport */
        }
    }
    front.yaw=front.pitch=0;v.radius=front.radius=3;front.eye_y=.30005f;
    /* A very close blue face must occlude a farther orange face regardless
       of surface array order, not merely produce some non-white pixels. */
    compile(1,"y=-0.6");
    RaySurface nearonly[]={{&program[0],0}},farfirst[]={{&program[1],1},{&program[0],0}};
    dclear(C_WHITE);assert(voxel3d_render(nearonly,1,&vars,&v,&front,NULL,NULL,&st)==1);
    int near_color=dgetpixel(cx,cy);assert(near_color!=C_WHITE);
    dclear(C_WHITE);assert(voxel3d_render(farfirst,2,&vars,&v,&front,NULL,NULL,&st)==1);
    assert(dgetpixel(cx,cy)==near_color);
    dclear(C_WHITE);assert(voxel3d_render(both,2,&vars,&v,&front,NULL,NULL,&st)==1);
    assert(dgetpixel(cx,cy)==near_color);
    compile(0,"1/x=0");v.div=21;dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&front,NULL,NULL,&st)==1&&st.occupied==0);
    v.div=64;compile(0,"x^2+y^2+z^2=4");dclear(C_WHITE);
    assert(voxel3d_render(one,1,&vars,&v,&c,NULL,NULL,&st)==1 && st.occupied>1000 && st.pixels>1000);
    assert(host_vram_guards_ok());
    v.div=20;compile(0,"x^2+y^2+z^2=1");
    calls=0;limit=8;assert(voxel3d_render(one,1,&vars,&v,&c,cancel,NULL,&st)==0&&st.canceled);
    calls=0;limit=190;assert(voxel3d_render(one,1,&vars,&v,&c,cancel,NULL,&st)==0&&st.canceled&&st.faces>0);
    assert(host_vram_guards_ok());puts("Voxel shell, finite volume inequalities, pole guard, two surfaces, cancellation and VRAM guards passed.");
}
