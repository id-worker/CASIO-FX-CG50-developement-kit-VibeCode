#include <stdint.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
int32_t debug_new_phase(float x);
static uint32_t st=0xa1cbb04f;static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float frombits(uint32_t x){union{uint32_t u;float f;}v={.u=x};return v.f;}
int main(void){
 for(int i=0;i<1024;i++)target_stack[i]=0xa5;
 static const uint32_t edges[]={0,0x80000000u,1,0x80000001u,0x007fffffu,0x00800000u,0x357fffffu,0x35800000u,0x3f800000u,0xbf800000u,0x427fffffu,0x42800000u,0xc2800000u};
 volatile float constant=667544.2144301094f;
 for(unsigned j=0;j<sizeof edges/sizeof edges[0];j++){
   float x=frombits(edges[j]);int32_t old=(int32_t)(x*constant);if(old!=debug_new_phase(x))return 1;
 }
 for(unsigned i=0;i<100000;i++){
  uint32_t exp=107u+rng()%26u,sign=rng()&0x80000000u;
  float x=frombits(sign|(exp<<23)|(rng()&0x7fffffu));
  int32_t old=(int32_t)(x*constant);if(old!=debug_new_phase(x))return 2;
 }
 for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 3;
 return 0;
}
