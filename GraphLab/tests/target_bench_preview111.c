/* Synthetic SH/QEMU benchmark, not a calculator OS, LCD or hardware model. */
#include "../src/mesh_preview.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#ifndef FRAMES
#define FRAMES 0
#endif
#ifndef SAMPLE_DIV
#define SAMPLE_DIV 8
#endif
#ifndef COPY_LAYER
#define COPY_LAYER 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[2*1024*1024];static unsigned used;
static uint16_t fb[DWIDTH*DHEIGHT],layer[PLOT_W*PLOT_H];uint16_t *gint_vram=fb;
static volatile unsigned observed;
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
    if(!expr_parse(&parsed,"x^2+y^2+z^2=4",0)||!expr_compile(&parsed,&pr,NULL,0))return 1;
    Volume v={0,0,0,3,SAMPLE_DIV,0};RaySurface s={&pr,0};PreviewMesh mesh={0};
    if(preview_mesh_build(&mesh,&s,1,&vars,&v,NULL,NULL)!=1)return 2;
    Camera c={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=3,
              .eye_x=4.88f,.eye_y=5.24f,.eye_z=4.39f,.perspective=1};
    for(int i=0;i<FRAMES;i++){
        c.yaw=.75f+(i%7-3)*.02f;
        for(int p=0;p<DWIDTH*DHEIGHT;p++)fb[p]=C_WHITE;
        if(!preview_mesh_draw(&mesh,&c))return 3;
#if COPY_LAYER
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)
            layer[y*PLOT_W+x]=fb[(PLOT_Y0+y)*DWIDTH+PLOT_X0+x];
        for(int p=0;p<DWIDTH*DHEIGHT;p++)fb[p]=C_WHITE;
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++){
            uint16_t col=layer[y*PLOT_W+x];
            if(col!=C_WHITE)fb[(PLOT_Y0+y)*DWIDTH+PLOT_X0+x]=col;
        }
#endif
        observed+=fb[(PLOT_Y0+PLOT_H/2)*DWIDTH+PLOT_X0+PLOT_W/2];
    }
    return 0;
}
