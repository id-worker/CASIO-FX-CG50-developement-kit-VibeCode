#include "../src/section.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static size_t deny_size;
static int denied;
void *__real_malloc(size_t);
void *__wrap_malloc(size_t n){if(deny_size&&n==deny_size){denied++;return NULL;}return __real_malloc(n);}
#include <string.h>
static Parsed parsed;static Program programs[2];static EvalVars base;
static void compile(int i,char const *s){assert(expr_parse(&parsed,s,0));assert(expr_compile(&parsed,&programs[i],NULL,0));}
static SectionResult section(char const *a,char const *b,int plane,float fixed,float r,int n)
{
    compile(0,a);if(b)compile(1,b);
    RaySurface s[2]={{&programs[0],0},{&programs[1],1}};
    SectionView v={plane,fixed,r,n};SectionResult result;
    assert(section_compute(s,b?2:1,&base,&v,NULL,NULL,NULL,NULL,&result)==1);return result;
}
static int cancel(int done,int total,void *ctx){(void)done;(void)total;int *n=ctx;return --*n>0;}
int main(void)
{
    SectionResult r=section("x^2+y^2+z^2=4","x=0",0,0,3,64);
    assert(r.count==2&&r.segments[0]>50&&r.segments[1]>50);
    assert(fabsf(r.point[0].x)<1e-4f&&fabsf(fabsf(r.point[0].y)-2)<.001f&&r.point[0].z==0);
    r=section("x^2+y^2+z^2=4","x=0",1,0,3,64);
    assert(r.count==2&&fabsf(fabsf(r.point[1].z)-2)<.001f&&r.point[1].y==0);
    r=section("x^2+y^2+z^2=4","y=0",2,0,3,64);
    assert(r.count==2&&fabsf(fabsf(r.point[1].z)-2)<.001f&&r.point[1].x==0);
    r=section("x=3","y=0",0,0,3,64);assert(r.count==1&&fabsf(r.point[0].x-3)<.001f);
    r=section("x=0","y=0",0,0,3,64);assert(r.count==1&&fabsf(r.point[0].x)<.001f&&fabsf(r.point[0].y)<.001f);
    r=section("1/(x-.03125)=0","y=0",0,0,3,64);assert(r.count==0&&r.segments[0]==0);
    r=section("1/x=0","y=0",0,0,3,64);assert(r.count==0&&r.invalid>0);
    r=section("x=0","x=0",0,0,3,64);assert(r.count==0&&r.nonisolated);
    r=section("z=0","x^2+y^2+z^2=4",0,0,3,64);
    assert(r.count==0&&r.nonisolated&&r.whole[0]&&!r.whole[1]);
    r=section("sqrt(x)=y","y=1",0,0,3,64);
    assert(r.count==1&&r.invalid>0&&fabsf(r.point[0].x-1)<.001f&&fabsf(r.point[0].y-1)<.001f);
    r=section("x^2+y^2+z^2<=4","x=0",0,0,3,64);assert(r.count==2); /* boundary only */
    r=section("sin(x)","y=1",0,0,3,64);assert(r.count==1&&fabsf(r.point[0].x)<.001f); /* z=sin(x) */
    r=section("x=0","y=0",0,4,3,64);assert(r.outside&&r.count==0&&r.segments[0]==0);
    r=section("x=0","y=0",0,0,.001f,64);assert(r.count==1);
    r=section("x=0","y=0",0,0,10000,64);assert(r.count==1);
    /* Many genuine isolated roots: keep a bounded list and advertise more. */
    r=section("sin(12*x)=0","sin(12*y)=0",0,0,3,128);
    assert(r.count==SECTION_MAX_POINTS&&r.overflow);
    for(int i=0;i<r.count;i++){
        assert(fabsf(sinf(12*r.point[i].x))<.004f);
        assert(fabsf(sinf(12*r.point[i].y))<.004f);
    }
    /* Distinct linear systems with roots away from lattice corners. */
    for(int i=0;i<60;i++){
        float x=-2.5f+(i%10)*.53f,y=-2.2f+(i/10)*.79f;
        char a[96],b[96];snprintf(a,sizeof a,"x+.35*y=%.8g",(double)(x+.35f*y));
        snprintf(b,sizeof b,"y-.4*x=%.8g",(double)(y-.4f*x));
        r=section(a,b,0,0,3,48);
        assert(r.count==1&&fabsf(r.point[0].x-x)<.002f&&fabsf(r.point[0].y-y)<.002f);
    }
    compile(0,"x^2+y^2+z^2=4");compile(1,"x=0");
    RaySurface pair[2]={{&programs[0],0},{&programs[1],1}};
    float selected=99;int found=0;assert(section_auto(pair,2,&base,0,3,NULL,NULL,&selected,&found)==1);
    assert(found&&fabsf(selected)<=3);
    compile(0,"x=4");compile(1,"y=0");selected=99;found=1;
    assert(section_auto(pair,2,&base,0,3,NULL,NULL,&selected,&found)==1&&!found);
    compile(0,"z=.75");compile(1,"x^2+y^2+z^2=4");selected=99;found=0;
    assert(section_auto(pair,2,&base,0,3,NULL,NULL,&selected,&found)==1&&found&&fabsf(selected-.75f)<1e-6f);
    SectionView view={0,0,3,64};int calls=3;
    assert(section_compute(pair,2,&base,&view,NULL,NULL,cancel,&calls,&r)==0);
    view.divisions=0;assert(section_compute(pair,2,&base,&view,NULL,NULL,NULL,NULL,&r)==-1);
    view.divisions=64;deny_size=(size_t)2*2*(view.divisions+1)*sizeof(float);denied=0;
    assert(section_compute(pair,2,&base,&view,NULL,NULL,NULL,NULL,&r)==-2&&denied==1);
    deny_size=(size_t)2*2*21*sizeof(float);denied=0;
    assert(section_auto(pair,2,&base,0,3,NULL,NULL,&selected,&found)==-2&&denied==1);
    deny_size=0;
    puts("Sections: 3 planes, finite crossings, boundaries, coincident curves/whole planes, pole guard, auto selection, 60 linear systems, 32-point cap, cancellation and allocation failures passed.");
}
