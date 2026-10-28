#include "../src/fractal.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static Parsed parsed; static Program program;
static void compile(char const *s){if(!expr_parse(&parsed,s,0)||!expr_compile(&parsed,&program,NULL,0)){fprintf(stderr,"%s: %s\n",s,parsed.message);abort();}}
static void block(int x,int y,int w,int h,int i,void *v){(void)x;(void)y;(void)w;(void)h;assert(i>=1);(*(int*)v)++;}
static int cancel(int done,int total,void *v){(void)total;(void)v;return done<3;}
int main(void){
    EvalVars vars={0};FractalView v={-.5f,0,.01f,-.8f,.156f,4,40,4,0};
    compile("z^2+c");assert(fractal_point(&program,&vars,0,0,&v)==41);assert(fractal_point(&program,&vars,-1,0,&v)==41);assert(fractal_point(&program,&vars,2,0,&v)==2);
    compile("z*z+c");assert(fractal_point(&program,&vars,2,0,&v)==2);
    compile("z^3+c");assert(fractal_point(&program,&vars,0,0,&v)==41);assert(fractal_point(&program,&vars,2,0,&v)==2);
    compile("(abs(re(z))+i*abs(im(z)))^2+c");assert(fractal_point(&program,&vars,0,0,&v)==41);
    v.iterations=2;compile("z^2+c");assert(fractal_point(&program,&vars,2,0,&v)==2);assert(fractal_point(&program,&vars,0,0,&v)==3);v.iterations=40;
    compile("(abs(re(z))+i*abs(im(z)))^2+c");
    int n=0;assert(fractal_generate(&program,&vars,&v,block,&n,cancel,NULL)==0);assert(n>0);
    compile("z^2+c");n=0;assert(fractal_generate(&program,&vars,&v,block,&n,NULL,NULL)==1);
    assert(n==((PLOT_W+3)/4)*((PLOT_H+3)/4));
    puts("Custom fractal recurrence, fast polynomial paths, escape and cancellation passed.");
}
