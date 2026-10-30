#include "trigfast.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
int trigfast_prepare_new(TrigFast*,Program const*,EvalVars const*,float);
int trigfast_value_new(TrigFast const*,float,float,float,float,float*);
int trigfast_value_old(TrigFast const*,float,float,float,float,float*);
static Parsed parsed;static Program code;
static uint32_t st=0x3c51bba2u;static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float f(uint32_t x){union{uint32_t u;float f;}v={.u=x};return v.f;}
static uint32_t b(float x){union{uint32_t u;float f;}v={.f=x};return v.u;}
static float coord(void){return (int)(rng()%1280001u)*.0001f-64.0f;}
static const uint32_t edge[]={0,0x80000000u,1u,0x80000001u,0x00800000u,0x3f800000u,0xbf800000u,0x427fffffu,0x42800000u,0xc2800000u};
int main(void){
 unsigned long checked=0;
 for(unsigned mode=0;mode<64;mode++)for(int negzero=0;negzero<2;negzero++){
  /* +0 makes the accumulated constant positive zero. Negating the entire
     sum gives negative zero and the original requested coefficient signs. */
  char formula[160];unsigned inv=negzero?2u:0u;
  unsigned a=((mode>>0)&3u)^inv,c=((mode>>2)&3u)^inv,d=((mode>>4)&3u)^inv;
  snprintf(formula,sizeof formula,"%s%s%s(x)%c%s(y)%c%s(z)+0%s=0",negzero?"-(":"",a&2?"-":"",a&1?"cos":"sin",c&2?'-':'+',c&1?"cos":"sin",d&2?'-':'+',d&1?"cos":"sin",negzero?")":"");
  if(!expr_parse(&parsed,formula,0)||!expr_compile(&parsed,&code,NULL,0)){printf("parse failure: %s\n",formula);return 1;}
  TrigFast plan;EvalVars base={0};if(!trigfast_prepare_new(&plan,&code,&base,10)||!plan.xyz_mode){printf("pattern not recognized: %s mode=%u\n",formula,plan.xyz_mode);return 2;}
  unsigned expected=1u|(mode<<1)|(negzero?128u:0u);
  if(plan.xyz_mode!=expected||b(plan.constant)!=(negzero?0x80000000u:0u)){
   printf("mode/constant mismatch %s actual=%u:%08x expected=%u\n",formula,plan.xyz_mode,b(plan.constant),expected);return 3;
  }
  for(int i=0;i<4096;i++){
   float x=i%7==0?f(edge[rng()%(sizeof edge/sizeof edge[0])]):coord();
   float y=i%9==0?f(edge[rng()%(sizeof edge/sizeof edge[0])]):coord();
   float z=i%11==0?f(edge[rng()%(sizeof edge/sizeof edge[0])]):coord();
   float target=i%4==0?f(0x80000000u):i%4==1?0:coord();
   float old=f(0xdeadbeef),newval=old;
   int oa=trigfast_value_old(&plan,x,y,z,target,&old),na=trigfast_value_new(&plan,x,y,z,target,&newval);
   if(oa!=na||b(old)!=b(newval)){
    printf("Mismatch %s xyz=%08x %08x %08x target=%08x old=%d:%08x new=%d:%08x\n",formula,b(x),b(y),b(z),b(target),oa,b(old),na,b(newval));return 4;
   }
   checked++;
  }
 }
 printf("64 prepared +/-sin/cos combinations with +/-zero constants, %lu samples exact vs 1.13 fast evaluator\n",checked);return 0;
}
