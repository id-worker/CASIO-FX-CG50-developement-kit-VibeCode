/* Exact short-operation evaluator on big-endian SH/QEMU; no hardware timing. */
#include "../src/ray.h"
#include <stdint.h>
#include <string.h>
uint8_t target_stack[16384] __attribute__((aligned(16)));
void *fxlibc_hal_malloc(unsigned n){(void)n;return 0;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int n){(void)n;for(;;){}}
int ray_small_host_test(Program const *,EvalVars const *,float,float,float,float *);
static uint32_t rng=0x91a2b3c4u;
static uint32_t random_bits(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static float from_bits(uint32_t u){union{uint32_t u;float f;}x;x.u=u;return x.f;}
static uint32_t bits(float f){union{uint32_t u;float f;}x;x.f=f;return x.u;}
static int finite_bits(uint32_t u){return (u&0x7f800000u)!=0x7f800000u;}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    static Program p;static EvalVars v;float out;
    for(int kind=0;kind<11;kind++){
        memset(&p,0,sizeof p);p.max_stack=2;p.code[0].op=I_VAR;p.code[0].aux=V_X;
        if(kind==0)p.size=1;
        else if(kind<=3){p.size=2;p.code[1].op=kind==1?I_NEG:kind==2?I_SQUARE:I_CUBE;}
        else {
            p.size=3;p.code[1].op=kind>=8?I_NUM:I_VAR;p.code[1].aux=V_Y;
            p.code[1].number=kind==8?0.0f:kind==9?-0.0f:3.25f;
            p.code[2].op=kind==4||kind==8?I_ADD:kind==5||kind==9?I_SUB:kind==6||kind==10?I_MUL:I_DIV;
        }
        if(!expr_real_fast_usable(&p))return 1;
        for(int j=0;j<8000;j++){
            v.v[V_X]=from_bits(random_bits());v.v[V_Y]=from_bits(random_bits());
            if(j==0)v.v[V_X]=0.0f;if(j==1)v.v[V_X]=-0.0f;if(j==2)v.v[V_Y]=-0.0f;
            float a=expr_eval_real_fast(&p,&v);
            if(!ray_small_host_test(&p,&v,v.v[V_X],v.v[V_Y],v.v[V_Z],&out))return 2;
            uint32_t aa=bits(a),bb=bits(out);
            if(finite_bits(aa)&&finite_bits(bb)){if(aa!=bb)return 3;}
            else if(finite_bits(aa)!=finite_bits(bb))return 4;
        }
    }
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 5;
    return 0;
}
