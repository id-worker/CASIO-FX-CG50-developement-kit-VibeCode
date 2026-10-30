/* Compare integer phase with the old binary32 multiply and truncation.
   --exhaustive checks every positive float in exponents 107..132. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int32_t debug_new_phase(float);
static uint32_t st=0x81cad3;static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float f(uint32_t x){union{uint32_t u;float f;}v={.u=x};return v.f;}
static int check(uint32_t bits){
 float x=f(bits);volatile float c=667544.2144301094f;int32_t a=(int32_t)(x*c),b=debug_new_phase(x);
 if(a!=b){printf("phase mismatch input=%08x old=%d new=%d\n",bits,a,b);return 0;}return 1;
}
int main(int argc,char **argv){
 static uint32_t edges[]={0,0x80000000u,1u,0x80000001u,0x007fffffu,0x00800000u,0x357fffffu,0x35800000u,0x3f800000u,0xbf800000u,0x427fffffu,0x42800000u,0xc2800000u};
 for(unsigned i=0;i<sizeof edges/sizeof edges[0];i++)if(!check(edges[i]))return 1;
 for(unsigned i=0;i<3000000;i++){
  uint32_t exp=107u+rng()%26u,sign=rng()&0x80000000u;
  if(!check(sign|(exp<<23)|(rng()&0x7fffffu)))return 2;
 }
 puts("3M signed binary32 samples and range edges match old phase");
 if(argc>1&&strcmp(argv[1],"--exhaustive")==0){
  for(uint32_t exp=107;exp<=132;exp++)for(uint32_t frac=0;frac<0x800000u;frac++)
   if(!check((exp<<23)|frac))return 3;
  puts("All 218103808 positive floats with relevant exponents match old phase");
 }
 return 0;
}
