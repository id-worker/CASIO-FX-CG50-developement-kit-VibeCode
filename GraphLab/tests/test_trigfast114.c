#include "trigfast.h"
#include "trig_table.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
float debug_old_lookup(float,int), debug_new_lookup(float,int), debug_new_scale(int);
int trigfast_value_old(TrigFast const*,float,float,float,float,float*);
int trigfast_value_new(TrigFast const*,float,float,float,float,float*);
static uint32_t state=0xd1cbb85fu;
static uint32_t rng(void){state^=state<<13;state^=state>>17;state^=state<<5;return state;}
static float fbits(uint32_t b){float v;memcpy(&v,&b,sizeof v);return v;}
static uint32_t bits(float v){uint32_t b;memcpy(&b,&v,sizeof b);return b;}
static float pos(void){return ((float)((int32_t)(rng()%1280001)-640000))*0.0001f;}
int main(void){
 volatile float scale=1.0f/32767.0f;
 for(int n=-32767;n<=32767;n++){
  float expected=n*scale, actual=debug_new_scale(n);
  if(bits(expected)!=bits(actual)){printf("Q15=%d expected=%08x actual=%08x\n",n,bits(expected),bits(actual));return 1;}
 }
 /* Prove the interpolation output stays in the conversion's tested range. */
 for(int k=0;k<1024;k++)for(int fraction=0;fraction<4096;fraction++){
  int a=gl_sin_table[k],b=gl_sin_table[(k+1)&1023],product=(b-a)*fraction;
  int delta=product>=0?(product+2048)>>12:-((-product+2048)>>12);
  int n=a+delta;if(n< -32767||n>32767){printf("out-of-range Q15 %d\n",n);return 1;}
 }
 for(int i=0;i<2000000;i++){
  float x=pos(); int c=rng()&1;float a=debug_old_lookup(x,c), b=debug_new_lookup(x,c);
  if(bits(a)!=bits(b)){printf("lookup i=%d x=%g c=%d old=%08x new=%08x\n",i,x,c,bits(a),bits(b));return 1;}
 }
 static const uint32_t coeff[]={0,0x80000000u,0x3f800000u,0xbf800000u,0x40000000u,0xc0000000u,0x3e800000u,0x7f800000u,0x7fc00000u,0x00000001u,0x80800000u,0x7f7fffffu};
 for(int i=0;i<1000000;i++){
  TrigFast p={0};p.valid=1;p.constant=pos();p.guard=(rng()%100)*.0001f;
  for(int j=0;j<3;j++){p.sine[j]=fbits(coeff[rng()%(sizeof coeff/sizeof coeff[0])]);p.cosine[j]=fbits(coeff[rng()%(sizeof coeff/sizeof coeff[0])]);}
  float x=pos(),y=pos(),z=pos(),target=pos(),a=fbits(0xdeadbeefu),b=a;
  int ra=trigfast_value_old(&p,x,y,z,target,&a),rb=trigfast_value_new(&p,x,y,z,target,&b);
  if(ra!=rb || bits(a)!=bits(b)){printf("value i=%d old=%d:%08x new=%d:%08x x=%g y=%g z=%g\n",i,ra,bits(a),rb,bits(b),x,y,z);return 1;}
 }
 puts("65535 exact Q15 conversions, 4194304 interpolation bounds, 2M lookups and 1M trigfast calls bit-identical");return 0;
}
