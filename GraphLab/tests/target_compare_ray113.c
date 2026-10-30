/* Compare real compiled SH rendering images/stats vs 1.12.1 in QEMU-user.
   This is not Casio OS or real-time hardware emulation. */
#include "ray.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef FAIL_COLUMNS
#define FAIL_COLUMNS 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[600*1024];static unsigned used;
#if FAIL_COLUMNS
static int forced_failures;
#endif
static uint16_t guarded[DWIDTH*DHEIGHT+2];uint16_t *gint_vram=guarded+1;
void *fxlibc_hal_malloc(unsigned n){
#if FAIL_COLUMNS
 if(n==PLOT_W*2*sizeof(float)){forced_failures++;return NULL;}
#endif
 n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;
}
void fxlibc_hal_free(void *p){(void)p;}
extern int host_write(int fd,const void *p,unsigned n);
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program program,height;
typedef struct {char const *formula;int q,explicit_z;float yaw,pitch,ex,ey,ez,radius;} Scene;
static const Scene scenes[]={
 {"z+x^2=sqrt(y)",20,0,.75f,.55f,4.88f,5.24f,4.39f,3},
 {"x^2+y^2+z^2=4",20,0,.75f,.55f,4.88f,5.24f,4.39f,3},
 {"sin(x)+sin(y)+sin(z)=0",12,0,.43f,.61f,11.4f,24.8f,19,10},
 {"z=sin(3*x)*cos(2*y)",20,1,.75f,.55f,3.4f,3.6f,3,3},
 {"1/x=0",20,0,0,0,0,6,0,3},
};
int main(void){
 for(int i=0;i<1024;i++)target_stack[i]=0xa5;
 guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
 for(unsigned i=0;i<sizeof scenes/sizeof scenes[0];i++){
  Scene const *s=&scenes[i];used=0;
  if(!expr_parse(&parsed,s->formula,0)||!expr_compile(&parsed,&program,NULL,0))return 1;
  RaySurface rs={&program,0};RayRenderOptions opt={0};
  if(s->explicit_z){if(!expr_compile_explicit_z(&parsed,&height))return 2;opt.height[0]=&height;}
  EvalVars vars={0};Volume v={0,0,0,s->radius,s->q,0};
  Camera c={.yaw=s->yaw,.pitch=s->pitch,.zoom=1,.radius=s->radius,
   .eye_x=s->ex,.eye_y=s->ey,.eye_z=s->ez,.perspective=1};
  for(int k=0;k<DWIDTH*DHEIGHT;k++)gint_vram[k]=C_WHITE;
  RayStats stats;
  if(ray3d_render_ex(&rs,1,&vars,&v,&c,&opt,NULL,NULL,&stats)!=1)return 3;
  if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 4;
  for(int j=0;j<1024;j++)if(target_stack[j]!=0xa5)return 5;
  if(host_write(1,&stats,sizeof stats)!=(int)sizeof stats)return 6;
  if(host_write(1,gint_vram,DWIDTH*DHEIGHT*sizeof(uint16_t))!=(int)(DWIDTH*DHEIGHT*sizeof(uint16_t)))return 7;
 }
#if FAIL_COLUMNS
 if(forced_failures!=5)return 8;
#endif
 return 0;
}
