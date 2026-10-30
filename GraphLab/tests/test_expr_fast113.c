/* Real evaluator preserves exact values for prevalidated arithmetic; all
   advanced or malformed bytecode is rejected by the validator. */
#include "../src/expr.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
static Parsed parsed;static Program program;
static uint32_t bits(float a){uint32_t v;memcpy(&v,&a,sizeof v);return v;}
static const char *formulas[]={
 "x^2+y^2+z^2=4","z+x^2=sqrt(y)","sin(x)+sin(y)+sin(z)=0",
 "tan(x)+acos(y)-atan(z)=a", "abs(x)+ln(y)+log(z)=b", "exp(x)-cos(y)=z",
 "floor(x)+ceil(y)-sqrt(z)=c", "min(x,y)+max(y,z)+atan2(y,x)=t",
 "asin(x)+conj(y)+re(z)+im(t)=0", "x/y+z^3-(-a)=0", "x^y+z^2=4"
};
int main(void){
 float values[]={-INFINITY,-100,-3,-1,-0.0f,0.0f,1e-20f,.25f,.99f,1,2,10,INFINITY,NAN};
 int tests=0;
 for(unsigned f=0;f<sizeof formulas/sizeof formulas[0];f++){
  assert(expr_parse(&parsed,formulas[f],0)&&expr_compile(&parsed,&program,NULL,0));
  assert(expr_real_fast_usable(&program));
  for(unsigned i=0;i<sizeof values/sizeof values[0];i++)
   for(unsigned j=0;j<sizeof values/sizeof values[0];j++)
    for(unsigned k=0;k<sizeof values/sizeof values[0];k++){
     EvalVars v={0};v.v[V_X]=values[i];v.v[V_Y]=values[j];v.v[V_Z]=values[k];
     v.v[V_A]=.125f;v.v[V_B]=-3;v.v[V_C]=0;v.v[V_T]=1;
     float a=expr_eval_real(&program,&v),b=expr_eval_real_fast(&program,&v);
     assert((isnan(a)&&isnan(b))||bits(a)==bits(b));tests++;
    }
 }
 /* Adversarial programs must fall back; no out-of-bounds unchecked path. */
 Program p={0};assert(!expr_real_fast_usable(&p));
 p.size=1;p.code[0].op=I_ADD;assert(!expr_real_fast_usable(&p));
 p.code[0].op=I_VAR;p.code[0].aux=255;assert(!expr_real_fast_usable(&p));
 p.code[0].op=I_SUM;assert(!expr_real_fast_usable(&p));
 p.size=EXPR_CODE+1;assert(!expr_real_fast_usable(&p));
 p.size=1;p.code[0].op=I_FUNC;p.code[0].aux=255;assert(!expr_real_fast_usable(&p));
 printf("Real fast VM: %d exact comparisons, domain/NaN/signed-zero cases and invalid bytecode passed.\n",tests);
}
