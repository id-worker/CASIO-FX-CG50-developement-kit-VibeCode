/* CPU-only synthetic test; no calculator LCD/OS/timer. */
#include "../src/preview3d.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#ifndef STEP
#define STEP 4
#endif
#ifndef FRAMES
#define FRAMES 0
#endif
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[1024*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static uint16_t fb[DWIDTH*DHEIGHT],layer[PLOT_W*PLOT_H];uint16_t *gint_vram=fb;static volatile unsigned observed;
int main(void)
{
    Volume v={.radius=3,.div=20};Camera c={.zoom=1,.radius=3,.eye_y=6,.perspective=1};Preview3D p={0};
    preview3d_set_step(&p,STEP);if(!preview3d_begin(&p,&v,&c)||!preview3d_aux_begin(&p))return 1;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y+=STEP)for(int x=PLOT_X0;x<=PLOT_X1;x+=STEP){
        float wx=((float)(x-PLOT_X0)/(PLOT_W-1)*2-1)*3;
        float wz=((float)(y-PLOT_Y0)/(PLOT_H-1)*2-1)*3;
        preview3d_add(&p,x,y,wx,0,wz,C_BLUE);
    }
    for(int i=0;i<2048;i++)preview3d_aux_add(&p,(i%43)/21.0f-1,0,(i%37)/18.0f-1,C_RED);
    for(int i=0;i<FRAMES;i++){
        c.yaw=(i%7-3)*.02f;for(int k=0;k<DWIDTH*DHEIGHT;k++)fb[k]=C_WHITE;
        if(preview3d_draw(&p,&c)!=1)return 2;
        for(int y=0;y<PLOT_H;y++)memcpy(layer+y*PLOT_W,fb+(PLOT_Y0+y)*DWIDTH+PLOT_X0,PLOT_W*sizeof *layer);
        for(int k=0;k<DWIDTH*DHEIGHT;k++)fb[k]=C_WHITE;
        for(int y=0;y<PLOT_H;y++)memcpy(fb+(PLOT_Y0+y)*DWIDTH+PLOT_X0,layer+y*PLOT_W,PLOT_W*sizeof *layer);
        observed+=fb[(PLOT_Y0+PLOT_H/2)*DWIDTH+PLOT_X0+PLOT_W/2];
    }
    return 0;
}
