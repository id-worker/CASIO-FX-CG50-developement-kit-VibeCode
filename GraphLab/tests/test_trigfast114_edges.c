#include "trigfast.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int trigfast_value_old(TrigFast const*,float,float,float,float,float*);
int trigfast_value_new(TrigFast const*,float,float,float,float,float*);
static uint32_t st=0x69caf123;
static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float fbits(uint32_t b){float f;memcpy(&f,&b,4);return f;}
static uint32_t bits(float f){uint32_t b;memcpy(&b,&f,4);return b;}
static const uint32_t edges[]={0u,0x80000000u,1u,0x80000001u,0x007fffffu,0x807fffffu,0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x427fffffu,0x42800000u,0x42800001u,0xc27fffffu,0xc2800000u,0xc2800001u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00001u,0xffc00001u,0x7f800001u};
int main(void){
 for(int i=0;i<2000000;i++){
  TrigFast p={.constant=fbits(rng()),.guard=fbits(rng()),.valid=(int)(rng()&1)};
  for(int j=0;j<3;j++){p.sine[j]=fbits(rng());p.cosine[j]=fbits(rng());}
  float x=fbits((i&1)?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  float y=fbits((i&2)?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  float z=fbits((i&4)?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  float target=fbits((i&8)?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  float a=fbits(0xdeadbeef),b=a;int ra=trigfast_value_old(&p,x,y,z,target,&a),rb=trigfast_value_new(&p,x,y,z,target,&b);
  if(ra!=rb||bits(a)!=bits(b)){printf("MISMATCH %d old %d %08x new %d %08x xyz %08x %08x %08x\n",i,ra,bits(a),rb,bits(b),bits(x),bits(y),bits(z));return 1;}
 }
 puts("2M raw float-bit edge cases (including ±64/NaN/infinity/subnormals) bit-identical");return 0;
}
