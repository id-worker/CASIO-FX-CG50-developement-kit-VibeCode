/* Compare one final RAY, cached full PLOTs, and point previews on SH QEMU.
   The test allocator and simulated CPU do not model physical calculator RAM or time. */
#include "ray.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef BENCH_MODE
#define BENCH_MODE 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[512*1024];static unsigned used;
static uint16_t framebuffer[DWIDTH*DHEIGHT];uint16_t *gint_vram=framebuffer;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program program;static RayGridCache grid;static Preview3D cloud;
static void clear(void){for(int i=0;i<DWIDTH*DHEIGHT;i++)framebuffer[i]=C_WHITE;}
int main(void)
{
    if(!expr_parse(&parsed,"sin(x)+sin(y)+sin(z)=0",0)||!expr_compile(&parsed,&program,NULL,0))return 1;
    EvalVars vars={0};Volume v={0,0,0,10,12,0};
    Camera c={.yaw=.43083f,.pitch=.6083f,.zoom=1,.radius=10,.eye_x=11.4f,.eye_y=24.8f,.eye_z=19,.perspective=1};
    RaySurface surface={&program,0};RayStats stats;
    if(!preview3d_begin(&cloud,&v,&c))return 2;
    RayRenderOptions o={.grid=&grid,.preview=&cloud};clear();
    if(ray3d_render_ex(&surface,1,&vars,&v,&c,&o,NULL,NULL,&stats)!=1||stats.hits<500||cloud.count<20)return 3;
#if BENCH_MODE == 1
    o.preview=NULL;
    for(int i=0;i<3;i++){
        c.yaw+=.02f;clear();
        if(ray3d_render_ex(&surface,1,&vars,&v,&c,&o,NULL,NULL,&stats)!=1||!stats.grid_reused)return 4;
    }
#elif BENCH_MODE == 2
    for(int i=0;i<10;i++){
        c.yaw+=.02f;clear();if(preview3d_draw(&cloud,&c)!=1)return 5;
    }
#endif
    preview3d_free(&cloud);ray_grid_cache_clear(&grid);return 0;
}
