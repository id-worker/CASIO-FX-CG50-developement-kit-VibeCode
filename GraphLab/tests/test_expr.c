#include "../src/expr.h"
#include <assert.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
static Parsed p; static Program prog;
static void good(const char *s) {
    if(!expr_parse(&p,s,0) || !expr_compile(&p,&prog,NULL,0)) {
        fprintf(stderr,"parse failed: %s at %d (%s)\n",s,p.error_pos,p.message); abort();
    }
}
static void bad(const char *s) {
    if(expr_parse(&p,s,0) && expr_compile(&p,&prog,NULL,0)) {
        fprintf(stderr,"unexpected acceptance: '%s'\n",s); abort();
    }
}
static void nearf_(float got,float want,float eps) {
    if(!isfinite(got) || fabsf(got-want)>eps*(1+fabsf(want))) {
        fprintf(stderr,"got %.9g want %.9g\n",got,want); abort();
    }
}
int main(void) {
    EvalVars v={0};v.v[V_X]=2;v.v[V_Y]=9;v.v[V_Z]=5;
    struct {const char *s;float want;} cases[]={
        {"1+2*3",7},{"(1+2)*3",9},{"-2^2",-4},{"(-2)^2",4},
        {"2^3^2",512},{"2^-2",.25f},{"+-2",-2},{"+2",2},
        {"1/2+1/4",.75f},{"2x+3(y-1)",28},{"x(y+1)",20},
        {"x^2+y^2=85",0},{"z+x^2=sqrt(y)",6},
        {"y=sin(x)",9-sinf(2)}, {"cos(pi)+exp(0)",0},
        {"min(5,2)+max(3,4)",6},{"re(-2)+im(9)+conj(2)",0},
        {"abs(-2)+sqrt(9)+log(100)+ln(e)",8},
        {"atan2(1,1)",0.785398163f},{".5+1e-2",.51f},
        {"z=x+y",-6},{"x^2+y^2+z^2=110",0},
    };
    int n=sizeof cases/sizeof *cases;
    for(int i=0;i<n;i++){good(cases[i].s);nearf_(expr_eval_real(&prog,&v),cases[i].want,1e-5f);}
    const char *invalid[]={"", "x=", "=2", "a=1=2", "sqrt(-1", "unknown(x)", "1//2", "min(x)","1+*2", "1.2.3"};
    for(unsigned i=0;i<sizeof invalid/sizeof *invalid;i++)bad(invalid[i]);
    struct {char const *text;Relation rel;int yes;} comparisons[]={
        {"x<y",REL_LT,1},{"x<=y",REL_LE,1},{"x>y",REL_GT,0},{"x>=y",REL_GE,0},
        {"x<2",REL_LT,0},{"x<=2",REL_LE,1},{"x>2",REL_GT,0},{"x>=2",REL_GE,1},
        {"2=x",REL_EQ,1},{"2<y",REL_LT,1},{"3>=y",REL_GE,0}
    };
    for(unsigned i=0;i<sizeof comparisons/sizeof *comparisons;i++){
        good(comparisons[i].text);assert(prog.has_equal && prog.relation==comparisons[i].rel);
        assert(expr_relation_true((Relation)prog.relation,expr_eval_real(&prog,&v))==comparisons[i].yes);
    }
    assert(!expr_relation_true(REL_LE,NAN) && !expr_relation_true(REL_GE,INFINITY));
    bad("x<y<2");bad("x==2");bad("x=<y");bad("x<=");bad("x<>y");
    Program explicit_program;Relation normalized;
    good("y=sin(x)");assert(expr_compile_explicit_y(&p,&explicit_program,&normalized));
    assert(normalized==REL_EQ && !expr_program_uses(&explicit_program,V_Y));
    nearf_(expr_eval_real(&explicit_program,&v),sinf(2),1e-5f);
    good("y<=x^2");assert(expr_compile_explicit_y(&p,&explicit_program,&normalized) && normalized==REL_LE);
    good("x^2<y");assert(expr_compile_explicit_y(&p,&explicit_program,&normalized) && normalized==REL_GT);
    good("x+1");assert(expr_compile_explicit_y(&p,&explicit_program,&normalized) && normalized==REL_EQ);
    good("x^2+y^2=1");assert(!expr_compile_explicit_y(&p,&explicit_program,&normalized));
    good("y=x+y");assert(!expr_compile_explicit_y(&p,&explicit_program,&normalized));
    good("z=sin(x)+y");assert(expr_compile_explicit_z(&p,&explicit_program));
    nearf_(expr_eval_real(&explicit_program,&v),sinf(2)+9,1e-5f);
    good("sin(x)+y=z");assert(expr_compile_explicit_z(&p,&explicit_program));
    good("sin(x)+y");assert(expr_compile_explicit_z(&p,&explicit_program));
    good("z=z+x");assert(!expr_compile_explicit_z(&p,&explicit_program));
    good("z<=x");assert(!expr_compile_explicit_z(&p,&explicit_program));
    good("x^2+y^2+z^2=1");assert(!expr_compile_explicit_z(&p,&explicit_program));
    good("z^2=0");assert(expr_compile_zero_surface(&p,&explicit_program));
    assert(explicit_program.has_equal&&explicit_program.relation==REL_EQ);
    nearf_(expr_eval_real(&explicit_program,&v),5,1e-6f);
    good("0=abs((x^2+y^2+z^2-1)^2)");assert(expr_compile_zero_surface(&p,&explicit_program));
    nearf_(expr_eval_real(&explicit_program,&v),109,1e-6f);
    good("z^2=1");assert(!expr_compile_zero_surface(&p,&explicit_program));
    good("z^0=0");assert(!expr_compile_zero_surface(&p,&explicit_program));
    good("z^2<=0");assert(!expr_compile_zero_surface(&p,&explicit_program));
    good("1/0");assert(isnan(expr_eval_real(&prog,&v)));
    good("sqrt(-1)");assert(isnan(expr_eval_real(&prog,&v)));
    good("ln(0)");assert(isnan(expr_eval_real(&prog,&v)));
    good("(-2)^.5");assert(isnan(expr_eval_real(&prog,&v)));
    v.cz=(Complex){1,2};v.cc=(Complex){-.5f,.25f};
    good("z^2+c"); Complex a=expr_eval_complex(&prog,&v);nearf_(a.re,-3.5f,1e-5);nearf_(a.im,4.25f,1e-5);
    good("z^3+c");a=expr_eval_complex(&prog,&v);nearf_(a.re,-11.5f,1e-5);nearf_(a.im,-1.75f,1e-5);
    good("conj(z)+i*im(z)");a=expr_eval_complex(&prog,&v);nearf_(a.re,1,1e-5);nearf_(a.im,0,1e-5);
    good("sqrt(-1)");a=expr_eval_complex(&prog,&v);nearf_(a.re,0,1e-5);nearf_(a.im,1,1e-5);
    good("sin(i)");a=expr_eval_complex(&prog,&v);nearf_(a.re,0,1e-5);nearf_(a.im,sinhf(1),1e-5);
    good("z");assert(expr_program_uses(&prog,V_Z));assert(!expr_program_uses(&prog,V_X));
    struct {const char *s;float want;} sums[]={
        {"0!+1!+5!",122},{"2^3!",64},{"summ(0,4,n,n)",10},
        {"summ(1,3,k,k^2)",14},{"summ(0,3,i,i)+i*0",6},
        {"summ(0,3,e,e)+ln(e)",7},{"summ(0,3,x,x)+x",8},
        {"summ(1,3,n,summ(1,2,k,n*k))",18},
        {"summ(1,3,n,summ(1,2,n,n)+n)",15},
        {"summ(3,1,n,n)",0},{"summ(0,inf,n,x^n/n!)",7.389056f}
    };
    for(unsigned i=0;i<sizeof sums/sizeof *sums;i++){
        good(sums[i].s);
        /* i outside a complex expression cannot be interpreted as real. */
        if(i!=4)nearf_(expr_eval_real(&prog,&v),sums[i].want,3e-5f);
    }
    good("summ(0,3,x,x)+z");assert(!expr_program_uses(&prog,V_X));assert(expr_program_uses(&prog,V_Z));
    good("summ(0,inf,n,x^n/n!)");v.v[V_X]=10;nearf_(expr_eval_real(&prog,&v),expf(10),3e-5f);
    v.v[V_X]=20;nearf_(expr_eval_real(&prog,&v),expf(20),3e-5f);v.v[V_X]=2;
    good("summ(0,inf,n,x^(n)/(n!))");nearf_(expr_eval_real(&prog,&v),expf(2),3e-5f);
    good("summ(0,inf,n,1)");assert(isnan(expr_eval_real(&prog,&v)));
    good("summ(0,200,n,n)");assert(isnan(expr_eval_real(&prog,&v)));
    good("summ(0,3,n,-1!)");nearf_(expr_eval_real(&prog,&v),-4,1e-5);
    good("(-1)!");assert(isnan(expr_eval_real(&prog,&v)));
    good("summ(0,3,z,z)+z");a=expr_eval_complex(&prog,&v);nearf_(a.re,7,1e-5);nearf_(a.im,2,1e-5);
    good("summ(0,3,c,c)+c");a=expr_eval_complex(&prog,&v);nearf_(a.re,5.5f,1e-5);nearf_(a.im,.25f,1e-5);
    good("summ(0,3,i,i)+i");a=expr_eval_complex(&prog,&v);nearf_(a.re,6,1e-5);nearf_(a.im,1,1e-5);
    good("summ(0,inf,n,z^n/n!)");v.cz=(Complex){0,0};a=expr_eval_complex(&prog,&v);nearf_(a.re,1,1e-5);nearf_(a.im,0,1e-5);
    good("summ(0,inf,n,z^n/n!)");v.cz=(Complex){0,1};a=expr_eval_complex(&prog,&v);nearf_(a.re,cosf(1),3e-5);nearf_(a.im,sinf(1),3e-5);
    bad("summ(0,2,xx,xx)");bad("summ(0,2,n)");bad("summ(0,2,3,n)");
    /* Finite products, empty product, mixed nesting and local index scope. */
    good("prod(1,5,n,n)");nearf_(expr_eval_real(&prog,&v),120,1e-5f);
    good("prod(1,3,k,k+x)");nearf_(expr_eval_real(&prog,&v),60,1e-5f);
    good("prod(3,1,n,n)");nearf_(expr_eval_real(&prog,&v),1,1e-5f);
    good("summ(1,3,n,prod(1,2,k,n+k))");nearf_(expr_eval_real(&prog,&v),38,1e-5f);
    good("prod(1,3,n,summ(1,2,n,n)+n)");nearf_(expr_eval_real(&prog,&v),120,1e-5f);
    good("prod(1,3,x,x)+x");nearf_(expr_eval_real(&prog,&v),8,1e-5f);
    good("prod(1,3,z,z)+z");v.cz=(Complex){1,2};a=expr_eval_complex(&prog,&v);
    nearf_(a.re,7,1e-5f);nearf_(a.im,2,1e-5f);
    good("prod(1,2,n,z+n)");v.cz=(Complex){0,1};a=expr_eval_complex(&prog,&v);
    nearf_(a.re,1,1e-5f);nearf_(a.im,3,1e-5f);
    good("prod(1,inf,n,1)");nearf_(expr_eval_real(&prog,&v),1,1e-5f);
    good("prod(1,inf,n,2)");assert(isnan(expr_eval_real(&prog,&v)));
    good("prod(1,200,n,n)");assert(isnan(expr_eval_real(&prog,&v)));
    bad("prod(0,2,nn,nn)");bad("prod(0,2,n)");bad("prod(0,2,3,n)");
    const char *partial[]={"", "1/", "(x+", "sqrt(", "1/(x+1", "x=", "z^", "-", "sin", "."};
    for(unsigned i=0;i<sizeof partial/sizeof *partial;i++) {
        if(!expr_parse(&p,partial[i],1)){fprintf(stderr,"tolerant failed: %s (%s)\n",partial[i],p.message);abort();}
    }
    char long_nested[100];int j=0;for(;j<35;j++)long_nested[j]='(';long_nested[j++]='x';while(j<71)long_nested[j++]=')';long_nested[j]=0;
    bad(long_nested);
    char facts[100];facts[0]='1';memset(facts+1,'!',63);facts[64]=0;good(facts);
    facts[64]='!';facts[65]=0;bad(facts);
    printf("Expression parser and VM: %d arithmetic/equation cases plus syntax/domain/complex/partial checks passed.\n",n);
}
