#include "trigfast.h"
#include <stdint.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
int trigfast_value_old(TrigFast const*,float,float,float,float,float*);
int trigfast_value_new(TrigFast const*,float,float,float,float,float*);
static uint32_t st=0x3182510fu;static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float f(uint32_t x){union{uint32_t u;float f;}v={.u=x};return v.f;}
static uint32_t b(float x){union{uint32_t u;float f;}v={.f=x};return v.u;}
static float coord(void){return ((int)(rng()%1280001u)-640000)*.0001f;}
static const uint32_t edges[]={0u,0x80000000u,1u,0x80000001u,0x3f800000u,0xbf800000u,0x427fffffu,0x42800000u,0xc2800000u};
int main(void){
 for(int i=0;i<1024;i++)target_stack[i]=0xa5;
 for(unsigned m=0;m<64;m++)for(int negzero=0;negzero<2;negzero++){
  TrigFast p={0};p.valid=1;p.constant=negzero?f(0x80000000u):0;p.guard=.0008f;p.xyz_mode=1u|(m<<1)|(negzero?128u:0u);
  for(int i=0;i<3;i++){
   unsigned c=(m>>(2*i))&3u;float v=c&2?-1.0f:1.0f;
   if(c&1)p.cosine[i]=v;else p.sine[i]=v;
  }
  for(int j=0;j<128;j++){
   float x=j%7==0?f(edges[rng()%(sizeof edges/sizeof edges[0])]):coord();
   float y=j%9==0?f(edges[rng()%(sizeof edges/sizeof edges[0])]):coord();
   float z=j%11==0?f(edges[rng()%(sizeof edges/sizeof edges[0])]):coord();
   float target=j%4==0?f(0x80000000u):j%4==1?0:coord();
   float old=f(0xdeadbeefu),newval=old;
   int oa=trigfast_value_old(&p,x,y,z,target,&old),na=trigfast_value_new(&p,x,y,z,target,&newval);
   if(oa!=na||b(old)!=b(newval))return 1;
  }
 }
 for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 2;
 return 0;
}
