/* GraphLab 1.08 regressions: conservative period/continuity decisions,
   real 2D/3D render paths and guarded trigonometric acceleration. */
#include "../src/expr.h"
#include "../src/plot2d.h"
#include "../src/ray.h"
#include "../src/trigfast.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed parsed;static Program p;static EvalVars vars;
static void compile(char const *s){assert(expr_parse(&parsed,s,0)&&expr_compile(&parsed,&p,NULL,0));}
static void proof(char const *s,float period,int expected)
{compile(s);assert(expr_proves_x_period(&p,&vars,period)==expected);}
static void continuous(char const *s,int expected)
{compile(s);assert(expr_program_continuous(&p)==expected);}
static unsigned random32=123456789u;
static float random_axis(void){random32^=random32<<13;random32^=random32>>17;random32^=random32<<5;return (random32/4294967295.0f)*128-64;}
static void trig_test(void)
{
    static const char *supported[]={"sin(x)+sin(y)+sin(z)=0","2*sin(x)-3*cos(y)+sin(z)+.5=0","sin(-x)-cos(-z)=0","sin(x)-sin(x)+sin(y)=0"};
    static const char *unsupported[]={"sin(2*x)=0","sin(x*2/2)=0","sin(x)*cos(y)=0","1e30+sin(x)-1e30+sin(y)=0","200*(sin(x)-sin(x))+sin(y)=0","sin(x)/0=0"};
    for(size_t i=0;i<sizeof supported/sizeof *supported;i++){
        compile(supported[i]);TrigFast fast;assert(trigfast_prepare(&fast,&p,&vars,10));
        int used=0,near=0;
        for(int j=0;j<150000;j++){
            vars.v[V_X]=random_axis();vars.v[V_Y]=random_axis();vars.v[V_Z]=random_axis();
            float exact=expr_eval_real(&p,&vars),value;
            if(trigfast_value(&fast,vars.v[V_X],vars.v[V_Y],vars.v[V_Z],0,&value)){
                assert(isfinite(exact)&&fabsf(exact-value)<=fast.guard&&(exact<0)==(value<0));used++;
            }else near++;
        }
        assert(used>100000);(void)near;
    }
    for(size_t i=0;i<sizeof unsupported/sizeof *unsupported;i++){
        compile(unsupported[i]);TrigFast fast;assert(!trigfast_prepare(&fast,&p,&vars,10));
    }
    compile(supported[0]);TrigFast fast;assert(!trigfast_prepare(&fast,&p,&vars,64));
    assert(trigfast_prepare(&fast,&p,&vars,10));float value;
    assert(!trigfast_value(&fast,0,0,0,0,&value)); /* exact VM near root */
    assert(!trigfast_value(&fast,65,0,0,0,&value));
    memset(&vars,0,sizeof vars);
}
typedef struct {int count;double sum;} Segments;
static void segment(float a,float b,float c,float d,void *ctx){Segments *s=ctx;s->count++;s->sum+=a+b+c+d;}
static void periods_2d(void)
{
    compile("y=sin(x)");PlaneView v={-10,10,-2,2,20,16,0};PlotStats a,b;Segments sa={0},sb={0};
    assert(plot_contour(&p,&vars,&v,segment,&sa,NULL,NULL,&a)==1);
    v.period_x=10;assert(plot_contour(&p,&vars,&v,segment,&sb,NULL,NULL,&b)==1);
    assert(a.evaluations==b.evaluations&&sa.count==sb.count&&fabs(sa.sum-sb.sum)<1e-8);
    Program rhs;Relation rel;assert(expr_compile_explicit_y(&parsed,&rhs,&rel));
    v.period_x=0;sa=(Segments){0};sb=(Segments){0};
    assert(plot_explicit_y(&rhs,rel,&vars,&v,81,81,segment,&sa,NULL,NULL,NULL,NULL,&a)==1);
    v.period_x=10;assert(plot_explicit_y(&rhs,rel,&vars,&v,81,81,segment,&sb,NULL,NULL,NULL,NULL,&b)==1);
    assert(a.evaluations==b.evaluations&&sa.count==sb.count&&fabs(sa.sum-sb.sum)<1e-8);
    v.xmin=0;v.xmax=4*3.14159265358979323846f;v.cols=80;v.period_x=0;
    sa=(Segments){0};sb=(Segments){0};
    assert(plot_explicit_y(&rhs,rel,&vars,&v,81,81,segment,&sa,NULL,NULL,NULL,NULL,&a)==1);
    v.period_x=2*3.14159265358979323846f;
    assert(plot_explicit_y(&rhs,rel,&vars,&v,81,81,segment,&sb,NULL,NULL,NULL,NULL,&b)==1);
    assert(b.evaluations<a.evaluations&&abs(sa.count-sb.count)<=2);
}
int main(void)
{
    float tau=6.2831853071795864769f;
    proof("sin(x)+sin(y)+sin(z)=0",10,0);proof("sin(x)+sin(y)+sin(z)=0",tau,1);
    proof("y=sin(2*pi*x)",1,1);proof("y=sin(2*pi*x)",.5f,0);
    proof("sin(0.00000001*x)=0",10,0);proof("sin(x)+.01*x=0",tau,0);
    proof("sin(x*y)=0",tau,0);proof("1/sin(x)=0",tau,0);proof("y^2+z=1",10,1);
    continuous("sin(x)+sin(y)+sin(z)=0",1);continuous("x^2+y^2+z^2=1",1);
    continuous("1/x=0",0);continuous("tan(x)=0",0);continuous("sqrt(x)=0",0);continuous("floor(x)=0",0);
    trig_test();periods_2d();
    puts("1.08: guarded trig, true/false X periods, both 2D paths, continuous crossings and poles passed.");
}
