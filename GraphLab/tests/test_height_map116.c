#include "../src/intersection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
static Parsed parsed;static Program p[2],h[2];static EvalVars vars;static int have[2];
static IMapPixel map[IMAP_SIDE*IMAP_SIDE],ref[IMAP_SIDE*IMAP_SIDE];
static void compile(int k,char const *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,&p[k],NULL,0));have[k]=expr_compile_explicit_z(&parsed,&h[k]);}
static int cancel(int done,int total,void *ctx){(void)total;return done<*(int *)ctx;}
static int occupied(int x,int y){return map[y*IMAP_SIDE+x].z!=INT16_MIN;}
static int overlap(double a,double b,double c,double d){return fmax(a,c)<fmin(b,d)-1e-10;}
/* sin(x)+sin(y)+sin(x-y)=4*cos(y/2)*sin(x/2)*cos((x-y)/2).
   Independently test every map pixel against the three analytic line families,
   also clipping x-y to [-10,10]. This checks one scene only. */
static int analytic_user_pixel(int ix,int iy)
{
    const double pi=3.14159265358979323846,w=20.0/180.0;
    double x=-10+ix*w,y=10-iy*w,x0=fmax(-10,x-w/2),x1=fmin(10,x+w/2);
    double y0=fmax(-10,y-w/2),y1=fmin(10,y+w/2);
    for(int n=-3;n<=3;n++){
        double xx=2*pi*n,yy=pi*(2*n+1),d=yy;
        if(x0<xx&&xx<x1&&overlap(y0,y1,xx-10,xx+10))return 1;
        if(y0<yy&&yy<y1&&overlap(x0,x1,yy-10,yy+10))return 1;
        if(fabs(d)<=10&&overlap(x0,x1,y0+d,y1+d))return 1;
    }
    return 0;
}
int main(void)
{
    RaySurface s[2]={{&p[0],0},{&p[1],1}};
    IMapView v={.radius=10};assert(imap_auto_view(&v,10));IMapStats st,slow;
    compile(0,"sin(x)+sin(y)+sin(z)=0");compile(1,"z=x-y");
    assert(!have[0]&&have[1]);Program const *height[2]={NULL,&h[1]};
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1);
    int fast_eval=st.evaluations;assert(st.pixels>1800&&st.pixels<2000);
    for(int y=0;y<IMAP_SIDE;y++)for(int x=0;x<IMAP_SIDE;x++)
        assert(occupied(x,y)==analytic_user_pixel(x,y));
    assert(imap_build(s,&vars,&v,ref,NULL,NULL,&slow)==1);
    assert(slow.evaluations>fast_eval*15); /* calls, not calculator time */
    int different=0;for(int i=0;i<IMAP_SIDE*IMAP_SIDE;i++)different+=((map[i].z!=INT16_MIN)!=(ref[i].z!=INT16_MIN));
    printf("affine example: fast=%d evals, general=%d evals, occupancy differences=%d\n",fast_eval,slow.evaluations,different);
    assert(different==0);
    int lim=2;assert(imap_build_auto(s,&vars,&v,height,map,cancel,&lim,&st)==0);
    /* z=0.123 is between pixel-spaced layers; x=0 intersects all Y. */
    compile(0,"x=0");compile(1,"z=0.123");height[1]=&h[1];assert(have[1]);
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1&&st.pixels>=IMAP_SIDE-2);
    for(int y=1;y<IMAP_SIDE-1;y++)assert(occupied(90,y));
    /* Parallel surfaces and roots outside clip produce no claimed points. */
    compile(0,"z=1");compile(1,"z=-1");height[0]=&h[0];height[1]=&h[1];
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1&&st.pixels==0);
    compile(0,"z=11");compile(1,"x=0");height[0]=&h[0];height[1]=NULL;
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1&&st.pixels==0);
    /* Identical affine planes coincide everywhere; each marked centre is
       individually checked, not a fictitious line through an ambiguous cell. */
    compile(0,"z=0");compile(1,"z=0");height[0]=&h[0];height[1]=&h[1];
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1);
    assert(st.pixels==IMAP_SIDE*IMAP_SIDE);
    /* Restricted domain: sqrt and 1/x explicitly fall back to the general path. */
    v=(IMapView){.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=32,.cursor_steps=64};
    compile(0,"z=sqrt(y)");compile(1,"x=0");height[0]=&h[0];height[1]=NULL;
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1);
    assert(imap_build(s,&vars,&v,ref,NULL,NULL,&slow)==1);
    assert(!memcmp(map,ref,sizeof map)&&st.evaluations==slow.evaluations);
    compile(0,"1/x=0");compile(1,"z=0");height[0]=NULL;height[1]=&h[1];
    assert(imap_build_auto(s,&vars,&v,height,map,NULL,NULL,&st)==1&&st.pixels==0);
    assert(imap_build(s,&vars,&v,ref,NULL,NULL,&slow)==1&&!memcmp(map,ref,sizeof map));
    puts("Affine-height shortcut, clipping, domain fallback and cancellation passed on host.");
}
