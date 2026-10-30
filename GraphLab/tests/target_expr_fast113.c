/* Exercise the fast/ordinary VM on SH and a 16 KiB guarded stack. */
#include "expr.h"
#include <stdint.h>
#include <math.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static Parsed parsed;static Program p;
static unsigned char arena[65536];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return 0;void *x=arena+used;used+=n;return x;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static uint32_t bits(float x){union {float f;uint32_t u;} b;b.f=x;return b.u;}
int main(void){
 static const char *f[]={"x^2+y^2+z^2=4","z+x^2=sqrt(y)",
   "sin(x)+sin(y)+sin(z)=0","tan(x)+acos(y)-atan(z)=a",
   "min(x,y)+max(y,z)+atan2(y,x)=t","x/y+z^3=0"};
 float v[]={-INFINITY,-3,-1,-0.0f,0,.25f,1,3,INFINITY,NAN};
 for(int i=0;i<1024;i++)target_stack[i]=0xa5;
 for(unsigned k=0;k<sizeof f/sizeof f[0];k++){
  if(!expr_parse(&parsed,f[k],0)||!expr_compile(&parsed,&p,0,0)||!expr_real_fast_usable(&p))return 1;
  for(unsigned i=0;i<sizeof v/sizeof v[0];i++)for(unsigned j=0;j<sizeof v/sizeof v[0];j++)for(unsigned l=0;l<sizeof v/sizeof v[0];l++){
   EvalVars vars={0};vars.v[V_X]=v[i];vars.v[V_Y]=v[j];vars.v[V_Z]=v[l];
   float a=expr_eval_real(&p,&vars),b=expr_eval_real_fast(&p,&vars);
   if(!((isnan(a)&&isnan(b))||bits(a)==bits(b)))return 2;
  }
 }
 for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 3;
 return 0;
}
