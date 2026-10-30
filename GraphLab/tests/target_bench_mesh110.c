/* A synthetic 1.10 mesh build followed by N cached redraws on SH QEMU.
   The arena and host timing do not model the fx-CG50 OS, RAM or LCD. */
#include "../src/mesh_preview.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef FRAMES
#define FRAMES 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[2*1024*1024];static unsigned used;
static uint16_t fb[DWIDTH*DHEIGHT];uint16_t *gint_vram=fb;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void *fxlibc_hal_realloc(void *p,unsigned n){(void)n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;} int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program pr;static EvalVars vars;
int main(void)
{
    if(!expr_parse(&parsed,"sin(x)+sin(y)+sin(z)=0",0)||!expr_compile(&parsed,&pr,NULL,0))return 1;
    Volume v={0,0,0,10,12,0};RaySurface s={&pr,0};PreviewMesh mesh={0};
    if(preview_mesh_build(&mesh,&s,1,&vars,&v,NULL,NULL)!=1)return 2;
    Camera c={.yaw=.43083f,.pitch=.6083f,.zoom=1,.radius=10,
              .eye_x=11.4f,.eye_y=24.8f,.eye_z=19,.perspective=1};
    for(int i=0;i<FRAMES;i++){
        c.yaw=.43083f+(i%7-3)*.02f;
        for(int p=0;p<DWIDTH*DHEIGHT;p++)fb[p]=C_WHITE;
        if(!preview_mesh_draw(&mesh,&c))return 3;
    }
    return 0;
}
