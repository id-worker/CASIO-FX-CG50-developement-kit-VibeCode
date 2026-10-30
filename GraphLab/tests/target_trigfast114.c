#include "trigfast.h"
#include <stdint.h>
#include <math.h>
#include <string.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[65536];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return 0;void *x=arena+used;used+=n;return x;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
float debug_old_lookup(float,int), debug_new_lookup(float,int),debug_new_scale(int);
int trigfast_value_old(TrigFast const*,float,float,float,float,float*);
int trigfast_value_new(TrigFast const*,float,float,float,float,float*);
static uint32_t state=0xd1cbb85fu;
static uint32_t rng(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static float fbits(uint32_t b){float v;memcpy(&v,&b,sizeof v);return v;}
static uint32_t bits(float v){uint32_t b;memcpy(&b,&v,sizeof b);return b;}
static float pos(void){return ((float)((int32_t)(rng()%1280001)-640000))*0.0001f;}
int main(void){
 for(int i=0;i<1024;i++)target_stack[i]=0xa5;
 volatile float k=1.0f/32767.0f;
 for(int n=-32767;n<=32767;n++){
  float old=n*k,new=debug_new_scale(n);if(bits(old)!=bits(new))return 1;
 }
 for(int i=0;i<50000;i++){
  float x=pos();int c=rng()&1;float a=debug_old_lookup(x,c),b=debug_new_lookup(x,c);if(bits(a)!=bits(b))return 2;
 }
 static const uint32_t coeff[]={0,0x80000000u,0x3f800000u,0xbf800000u,0x40000000u,0xc0000000u,0x3e800000u,0x7f800000u,0x7fc00000u,0x00000001u,0x80800000u,0x7f7fffffu};
 for(int i=0;i<20000;i++){
  TrigFast p={0};p.valid=1;p.constant=pos();p.guard=(rng()%100)*.0001f;
  for(int j=0;j<3;j++){p.sine[j]=fbits(coeff[rng()%(sizeof coeff/sizeof coeff[0])]);p.cosine[j]=fbits(coeff[rng()%(sizeof coeff/sizeof coeff[0])]);}
  float x=pos(),y=pos(),z=pos(),target=pos(),a=fbits(0xdeadbeefu),b=a;
  int ra=trigfast_value_old(&p,x,y,z,target,&a),rb=trigfast_value_new(&p,x,y,z,target,&b);
  if(ra!=rb||bits(a)!=bits(b))return 3;
 }
 for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 4;
 return 0;
}
