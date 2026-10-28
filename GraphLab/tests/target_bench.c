/* GraphLab 1.02 actual voxel vs ray work, under SH QEMU only. No OS/hardware. */
#include "../src/voxel.h"
#include "../src/ray.h"
#include <gint/display.h>
#include <stdlib.h>
#include <stdint.h>
#ifndef BENCH_N
#define BENCH_N 20
#endif
#ifndef BENCH_TYPE
#define BENCH_TYPE 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[512*1024];static unsigned used;
static uint16_t guarded[DWIDTH*DHEIGHT+2];uint16_t *gint_vram=guarded+1;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program program;
int main(void){
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    if(!expr_parse(&parsed,"x^2+y^2+z^2=4",0)||!expr_compile(&parsed,&program,NULL,0))return 1;
    EvalVars vars={0};Volume v={0,0,0,3,BENCH_N,0};
    Camera cam={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=3,.eye_x=4.88f,.eye_y=5.24f,.eye_z=4.39f,.perspective=1};
    RaySurface rs={&program,0};
    for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;
#if BENCH_TYPE == 0
    VoxelStats stats;if(voxel3d_render(&rs,1,&vars,&v,&cam,NULL,NULL,&stats)!=1||stats.occupied<10||stats.pixels<1000)return 2;
#else
    RayStats stats;if(ray3d_render(&rs,1,&vars,&v,&cam,NULL,NULL,&stats)!=1||stats.hits<1000)return 3;
#endif
    if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 4;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 5;
    return 0;
}
