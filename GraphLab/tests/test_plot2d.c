#include "../src/plot2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;static Program expr,rhs;static Relation relation;
static void compile(char const *s){assert(expr_parse(&parsed,s,0));assert(expr_compile(&parsed,&expr,NULL,0));}
static void explicit_(char const *s){compile(s);assert(expr_compile_explicit_y(&parsed,&rhs,&relation));}
typedef struct {int count,cross_pole;float max_error;} Collector;
static void line(float x0,float y0,float x1,float y1,void *ctx){
    Collector *c=ctx;c->count++;
    if(x0<0&&x1>0)c->cross_pole++;
    float a=fabsf(x0-y0),b=fabsf(x1-y1);
    if(a>c->max_error)c->max_error=a;
    if(b>c->max_error)c->max_error=b;
}
typedef struct {int a[400],b[400],count;} Spans;
static void span(int x,int a,int b,void *ctx){Spans *s=ctx;s->a[x]=a;s->b[x]=b;s->count++;}
static int cancel(int done,int total,void *ctx){(void)total;(void)ctx;return done<32;}
int main(void){
    EvalVars vars={0};PlaneView v={-2,2,-2,2,64,64,0};PlotStats stats;Collector c={0};
    explicit_("y=x");assert(plot_explicit_y(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats)==1);
    assert(c.count>380&&c.max_error<.0001f&&stats.evaluations==386);
    explicit_("y=1/x");c=(Collector){0};assert(plot_explicit_y(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats)==1);
    assert(c.count>100&&c.cross_pole==0&&stats.rejected>0);
    explicit_("y=1000*x");c=(Collector){0};assert(plot_explicit_y(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats)==1);
    assert(c.count>0); /* steep but continuous: not rejected as a pole */
    explicit_("y=sqrt(x)");c=(Collector){0};assert(plot_explicit_y(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats)==1);
    assert(stats.invalid>100&&c.count>100);
    v=(PlaneView){-1,1,-1,1,2,2,0};Spans s;memset(&s,0xff,sizeof s);s.count=0;
    explicit_("y<x");c=(Collector){0};assert(plot_explicit_y(&rhs,relation,&vars,&v,3,3,line,&c,span,&s,NULL,NULL,&stats)==1);
    assert(s.a[1]==2&&s.b[1]==2); /* strict: y=0 excluded */
    memset(&s,0xff,sizeof s);s.count=0;explicit_("y<=x");
    assert(plot_explicit_y(&rhs,relation,&vars,&v,3,3,line,&c,span,&s,NULL,NULL,&stats)==1);
    assert(s.a[1]==1&&s.b[1]==2); /* non-strict boundary included */
    memset(&s,0xff,sizeof s);s.count=0;explicit_("x<y");
    assert(relation==REL_GT && plot_explicit_y(&rhs,relation,&vars,&v,3,3,line,&c,span,&s,NULL,NULL,&stats)==1);
    assert(s.a[1]==0&&s.b[1]==0);
    explicit_("y=sin(2*pi*x)");v=(PlaneView){0,4,-2,2,64,64,1};c=(Collector){0};
    assert(plot_explicit_y(&rhs,relation,&vars,&v,385,181,line,&c,NULL,NULL,NULL,NULL,&stats)==1);
    assert(stats.evaluations<=98); /* 1 unit = 96 pixels, plus occasional midpoints */
    /* Cache stores finite and NaN column values; a vertical pan requires no
       new f(x) evaluations, a 45px horizontal pan only the exposed columns. */
    explicit_("y=sin(x)");v=(PlaneView){-4,4,-2,2,64,64,0};c=(Collector){0};
    float cached[386]={0};unsigned char known[386]={0};
    assert(plot_explicit_y_cached(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats,cached,known,0,386)==1);
    assert(stats.evaluations==386);
    v.ymin+=.5f;v.ymax+=.5f;c=(Collector){0};
    assert(plot_explicit_y_cached(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats,cached,known,0,386)==1);
    assert(stats.evaluations==0);
    memmove(cached,cached+45,(386-45)*sizeof *cached);memmove(known,known+45,386-45);memset(known+386-45,0,45);
    float delta=45*(v.xmax-v.xmin)/385.0f;v.xmin+=delta;v.xmax+=delta;c=(Collector){0};
    assert(plot_explicit_y_cached(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,NULL,NULL,&stats,cached,known,0,386)==1);
    assert(stats.evaluations==45);
    v=(PlaneView){-2,2,-2,2,64,64,0};c=(Collector){0};
    assert(plot_explicit_y(&rhs,relation,&vars,&v,386,181,line,&c,NULL,NULL,cancel,NULL,&stats)==0&&stats.canceled);
    puts("Explicit 2D column path: isolated inequalities, strict fill, clipping, pole guard, period reuse and cancellation passed.");
}
