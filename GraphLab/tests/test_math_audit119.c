/* Independent arithmetic checks on calculator, 2D and 3D-style formulas.
   This does not establish mathematical completeness of sampled plots. */
#include "../src/expr.h"
#include "../src/plot2d.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static Parsed parsed;
static Program programs[8],height;
static Relation relation;
static void compile(int i,char const *s)
{
    if(!expr_parse(&parsed,s,0)||!expr_compile(&parsed,&programs[i],NULL,0)){
        fprintf(stderr,"Cannot compile %s: %s\n",s,parsed.message);abort();
    }
}
static void near(float actual,float expected,float eps,char const *label)
{
    if(!isfinite(actual)||!isfinite(expected)||fabsf(actual-expected)>eps*(1+fabsf(expected))){
        fprintf(stderr,"%s got %.9g expected %.9g\n",label,actual,expected);abort();
    }
}
typedef struct {int segments;} Lines;
static void saddle_segment(float x0,float y0,float x1,float y1,void *ctx)
{
    Lines *c=ctx;c->segments++;
    assert(x0>=-1 && x0<=1 && x1>=-1 && x1<=1);
    assert(y0>=-1 && y0<=1 && y1>=-1 && y1<=1);
}
typedef struct {int start[3],end[3];} Spans;
static void span(int x,int a,int b,void *ctx)
{
    Spans *s=ctx;s->start[x]=a;s->end[x]=b;
}
static void segment(float x0,float y0,float x1,float y1,void *ctx)
{
    Lines *c=ctx;c->segments++;
    near(y0,(x0+1)/(1+x0*x0),2e-5f,"2D segment start");
    near(y1,(x1+1)/(1+x1*x1),2e-5f,"2D segment end");
}
int main(void)
{
    compile(0,"(sin(x)+cos(y))/(1+z^2)");
    compile(1,"summ(0,6,n,x^n/n!)");
    compile(2,"prod(1,4,k,k+x)");
    compile(3,"summ(1,3,n,summ(1,2,k,n*k))+x");
    compile(4,"z=(x+1)/(1+y^2)");
    assert(expr_compile_explicit_z(&parsed,&height));
    compile(5,"z^2+x^2+y^2=4");
    compile(6,"z^2+c");
    compile(7,"z^3+c");
    EvalVars v={0};
    for(int i=0;i<1001;i++){
        float x=(i-500)*.004f,y=((i*37)%401-200)*.01f,z=((i*17)%301-150)*.01f;
        v.v[V_X]=x;v.v[V_Y]=y;v.v[V_Z]=z;
        near(expr_eval_real(&programs[0],&v),(sinf(x)+cosf(y))/(1+z*z),2e-5f,"trig fraction");
        float term=1,sum=1;for(int n=1;n<=6;n++){term*=x/n;sum+=term;}
        near(expr_eval_real(&programs[1],&v),sum,2e-5f,"finite series");
        float product=1;for(int k=1;k<=4;k++)product*=k+x;
        near(expr_eval_real(&programs[2],&v),product,2e-5f,"finite product");
        near(expr_eval_real(&programs[3],&v),18+x,2e-5f,"nested sums");
        float h=(x+1)/(1+y*y);
        near(expr_eval_real(&height,&v),h,2e-5f,"explicit z");
        near(expr_eval_real(&programs[4],&v),z-h,2e-5f,"3D residual");
        near(expr_eval_real(&programs[5],&v),z*z+x*x+y*y-4,2e-5f,"sphere residual");
        v.cz=(Complex){x,y};v.cc=(Complex){z,.25f};
        Complex a=expr_eval_complex(&programs[6],&v);
        near(a.re,x*x-y*y+z,2e-5f,"complex square real");
        near(a.im,2*x*y+.25f,2e-5f,"complex square imaginary");
        a=expr_eval_complex(&programs[7],&v);
        near(a.re,x*x*x-3*x*y*y+z,3e-5f,"complex cube real");
        near(a.im,3*x*x*y-y*y*y+.25f,3e-5f,"complex cube imaginary");
    }
    compile(0,"1/0");assert(isnan(expr_eval_real(&programs[0],&v)));
    compile(0,"sqrt(-1)");assert(isnan(expr_eval_real(&programs[0],&v)));
    compile(0,"summ(0,inf,n,1)");assert(isnan(expr_eval_real(&programs[0],&v)));
    compile(0,"summ(0,3,n,1/(2))+1");near(expr_eval_real(&programs[0],&v),3,1e-6f,"finite sum entered via editor");
    compile(0,"y=(x+1)/(1+x^2)");
    assert(expr_compile_explicit_y(&parsed,&height,&relation)&&relation==REL_EQ);
    PlaneView view={-4,4,-4,4,64,64,0};Lines lines={0};PlotStats st;
    assert(plot_explicit_y(&height,relation,&v,&view,386,181,segment,&lines,NULL,NULL,NULL,NULL,&st)==1);
    assert(lines.segments>380 && st.evaluations>=386);
    PlaneView tiny={-1,1,-1,1,1,1,0};
    for(int sign=-1;sign<=1;sign+=2){
        compile(0,sign<0?"x*y=-0.1":"x*y=0.1");lines.segments=0;
        assert(plot_contour(&programs[0],&v,&tiny,saddle_segment,&lines,NULL,NULL,&st)==1);
        assert(lines.segments==2 && st.segments==2);
    }
    struct {char const *text;int a,b;} fills[]={
        {"y<x",2,2},{"y<=x",1,2},{"y>x",0,0},{"y>=x",0,1},
        {"x>y",2,2},{"x>=y",1,2},{"x<y",0,0},{"x<=y",0,1}
    };
    for(unsigned i=0;i<sizeof fills/sizeof *fills;i++){
        compile(0,fills[i].text);assert(expr_compile_explicit_y(&parsed,&height,&relation));
        Spans spans={{-1,-1,-1},{-1,-1,-1}};
        assert(plot_explicit_y(&height,relation,&v,&tiny,3,3,saddle_segment,&lines,span,&spans,NULL,NULL,&st)==1);
        assert(spans.start[1]==fills[i].a&&spans.end[1]==fills[i].b);
    }
    puts("Arithmetic audit: 1001 independent values each for trig, fractions, SUM/PROD, nested sums, 3D residual/height and complex square/cube; 2D rational segments, saddle cells and 8 inequality directions passed.");
}
