#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int debug_ray_isfinite(float),debug_ray_zero(float),debug_ray_negative_finite(float);
float debug_ray_fabs(float),debug_ray_add_offset(float,float);
int debug_ray_opposite(float,float),debug_ray_same(float,float);
static uint32_t st=0x5110daccu;static uint32_t rng(void){st^=st<<13;st^=st>>17;st^=st<<5;return st;}
static float f(uint32_t x){union{uint32_t u;float f;}v={.u=x};return v.f;}
static uint32_t b(float x){union{uint32_t u;float f;}v={.f=x};return v.u;}
static const uint32_t edges[]={0,0x80000000u,1u,0x80000001u,0x007fffffu,0x807fffffu,0x00800000u,0x80800000u,0x3f800000u,0xbf800000u,0x7f7fffffu,0xff7fffffu,0x7f800000u,0xff800000u,0x7fc00001u,0xffc00001u,0x7f800001u};
int main(void){
 for(int i=0;i<3000000;i++){
  float a=f(i%4?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  float z=f(i%3==0?0:i%3==1?0x80000000u:rng());
  float c=f(i%7?rng():edges[rng()%(sizeof edges/sizeof edges[0])]);
  int finite=isfinite(a)!=0,zero=finite&&a==0,opposite=isfinite(a)&&isfinite(c)&&((a<0&&c>=0)||(c<0&&a>=0));
  volatile float vz=z,va=a;float ref=vz+va,got=debug_ray_add_offset(z,a);
  if(debug_ray_isfinite(a)!=finite||b(debug_ray_fabs(a))!=b(fabsf(a))||debug_ray_zero(a)!=zero||
     (finite&&debug_ray_negative_finite(a)!=(a<0))||debug_ray_opposite(a,c)!=opposite||
     debug_ray_same(a,c)!=(memcmp(&a,&c,sizeof a)==0)||b(ref)!=b(got)){
   printf("mismatch i=%d a=%08x z=%08x c=%08x plus %08x vs %08x\n",i,b(a),b(z),b(c),b(ref),b(got));return 1;
  }
  int n=4<<(i%5);volatile float denom=2.0f/n,inv=n*.5f,p=a+1.0f;
  float old=p/denom,newval=p*inv;
  if(!isnan(old) && b(old)!=b(newval)){printf("coord mismatch n=%d p=%08x old=%08x new=%08x\n",n,b(p),b(old),b(newval));return 2;}
 }
 puts("3M binary32 classification, signs, +/-0 addition and Q4..64 coordinate-scale checks identical");return 0;
}
