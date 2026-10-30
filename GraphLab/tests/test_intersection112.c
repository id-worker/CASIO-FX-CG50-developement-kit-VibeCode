#include "../src/intersection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Program program(char const *s)
{
    Parsed *p=calloc(1,sizeof *p);assert(p);Program r;char error[120];
    if(!expr_parse(p,s,0)||!expr_compile(p,&r,error,sizeof error)){fprintf(stderr,"bad formula %s\n",s);abort();}
    free(p);return r;
}
static int cancel(int done,int total,void *ctx){(void)total;return done<*(int*)ctx;}
int main(void)
{
    EvalVars base={0};IMapView v={.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=32,.cursor_steps=128};IMapRoots r;IMapStats st;
    Program a=program("x^2+y^2+z^2=4"),b=program("x=0");RaySurface s[2]={{&a,0},{&b,1}};
    assert(imap_search(s,&base,&v,0,1.2f,NULL,NULL,&r)==1);
    fprintf(stderr,"at 0,1.2 count=%d [%.6g, %.6g]\n",r.count,(double)r.z[0],(double)r.z[1]);
    assert(r.count==2&&fabsf(r.z[0]+1.6f)<.001f&&fabsf(r.z[1]-1.6f)<.001f);
    assert(imap_search(s,&base,&v,.6f,1.2f,NULL,NULL,&r)==1&&r.count==0&&!r.interval);
    assert(imap_search(s,&base,&v,0,2,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0])<.001f);
    IMapPixel *map=malloc(IMAP_SIDE*IMAP_SIDE*sizeof *map);assert(map);
    assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1);
    fprintf(stderr,"sphere/plane: segments=%d pixels=%d ambiguous=%d eval=%d\n",st.segments,st.pixels,st.ambiguous,st.evaluations);
    IMapPoint nearest;assert(st.pixels>70&&imap_nearest(map,&v,.6f,1.2f,&nearest));
    fprintf(stderr,"nearest %.5g %.5g %.5g\n",(double)nearest.x,(double)nearest.y,(double)nearest.z);
    assert(fabsf(nearest.x)<.02f&&fabsf(nearest.y-1.2f)<.1f);
    assert(fabsf(nearest.x*nearest.x+nearest.y*nearest.y+nearest.z*nearest.z-4)<.2f);
    int lim=2;assert(imap_build(s,&base,&v,map,cancel,&lim,&st)==0);
    assert(imap_search(s,&base,&v,0,1.2f,cancel,&lim,&r)==0);
    a=program("z=5");b=program("x=0");v.zmin=-10;v.zmax=10;
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0]-5)<.002f);
    assert(imap_search(s,&base,&v,.1f,1,NULL,NULL,&r)==1&&r.count==0);
    a=program("0=0");b=program("0=0");assert(imap_search(s,&base,&v,7,2,NULL,NULL,&r)==1&&r.interval);
    a=program("1/x=0");b=program("z=0");v.zmin=-3;v.zmax=3;
    assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1&&st.pixels==0);
    a=program("1/(z-0.123)=0");b=program("x=0");
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==0);
    a=program("0.0000000001*(x-1)=0");b=program("z=0");
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==0);
    assert(imap_search(s,&base,&v,1,1,NULL,NULL,&r)==1&&r.count==1);
    /* Two user's-style implicit surfaces, including sqrt's restricted domain. */
    a=program("z+x^2=sqrt(y)");b=program("x^2+y^2+z^2=9");v.xy_div=45;v.z_div=32;v.cursor_steps=128;
    assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1);fprintf(stderr,"sqrt/sphere map pixels=%d\n",st.pixels);
    assert(st.pixels>20);
    float yy=(sqrtf(37)-1)*.5f;
    assert(imap_search(s,&base,&v,0,yy,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0]-sqrtf(yy))<.002f);
    /* The independent cursor search must work even when the coarse map misses
       a feature entirely: the XY grid deliberately excludes the origin. */
    a=program("x^2+y^2+z^2=0.0001");b=program("x=0");v.xy_div=45;v.z_div=16;
    assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1&&st.pixels==0);
    assert(imap_search(s,&base,&v,0,0,NULL,NULL,&r)==1&&r.count==2);
    assert(fabsf(r.z[0]+.01f)<.001f&&fabsf(r.z[1]-.01f)<.001f);
    a=program("sin(10*z)=0");b=program("x=0");v.cursor_steps=256;
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==19&&!r.overflow);
    a=program("sin(100*z)=0");v.cursor_steps=512;
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==IMAP_MAX_ROOTS&&r.overflow);
    a=program("z^2=1");b=program("z=1");
    assert(imap_search(s,&base,&v,5,-2,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0]-1)<.001f);
    a=program("z+x^2=sqrt(y)");b=program("x=0");
    assert(imap_search(s,&base,&v,0,0,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0])<.001f);
    a=program("(z-0.123)^2=0");b=program("x=0");
    assert(imap_search(s,&base,&v,0,1,NULL,NULL,&r)==1&&r.count==1&&fabsf(r.z[0]-.123f)<.001f);
    free(map);puts("Intersection: independent XY roots, multiple Z, range, projection, pole, cancellation passed.");
}
