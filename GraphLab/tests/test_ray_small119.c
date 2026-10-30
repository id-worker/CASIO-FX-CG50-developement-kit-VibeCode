#include "../src/ray.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int ray_small_host_test(Program const *,EvalVars const *,float,float,float,float *);
static Parsed parsed;static Program p;
static uint32_t rng=0x82ab73d1u;
static uint32_t random_bits(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static float from_bits(uint32_t u){float x;memcpy(&x,&u,sizeof x);return x;}
static uint32_t bits(float x){uint32_t u;memcpy(&u,&x,sizeof u);return u;}
int main(void)
{
    char const *formula[]={"0","x","a","-x","x^2","x^3","x+y","x-y","x*y","x/y","a/b","x+7","3-y","x=z"};
    for(unsigned f=0;f<sizeof formula/sizeof *formula;f++){
        assert(expr_parse(&parsed,formula[f],0)&&expr_compile(&parsed,&p,0,0)&&expr_real_fast_usable(&p));
        for(int i=0;i<50000;i++){
            EvalVars v={0};v.v[V_X]=from_bits(random_bits());v.v[V_Y]=from_bits(random_bits());
            v.v[V_Z]=from_bits(random_bits());v.v[V_A]=from_bits(random_bits());v.v[V_B]=from_bits(random_bits());
            if(i==0)v.v[V_X]=0.0f;
            if(i==1)v.v[V_X]=-0.0f;
            if(i==2)v.v[V_Y]=-0.0f;
            float a=expr_eval_real_fast(&p,&v),b;
            assert(ray_small_host_test(&p,&v,v.v[V_X],v.v[V_Y],v.v[V_Z],&b));
            if(isfinite(a)&&isfinite(b)){
                if(bits(a)!=bits(b)){fprintf(stderr,"formula %s sample %d got %08x expected %08x\n",formula[f],i,bits(b),bits(a));assert(0);}
            }else assert(!isfinite(a)&&!isfinite(b)); /* field_at normalizes nonfinite results */
        }
    }
    assert(expr_parse(&parsed,"x+y+z",0)&&expr_compile(&parsed,&p,0,0));
    EvalVars v={0};float result;assert(!ray_small_host_test(&p,&v,0,0,0,&result));
    puts("RAY short-op plan: 700000 raw binary32 cases including signed zeros, infinities, NaNs and VM fallback passed.");
}
