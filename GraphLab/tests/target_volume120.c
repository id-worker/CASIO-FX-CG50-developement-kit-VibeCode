/* Analytic direct-ray checks using actual big-endian SuperH instructions.
   Fake display/allocator; QEMU is not the Casio OS or physical hardware. */
#include "../src/ray.c"
#include <gint/display.h>
#include <stdint.h>
#include <string.h>
#include <math.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[2*1024*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return 0;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int n){(void)n;for(;;){}}
static uint16_t guarded[DWIDTH*DHEIGHT+2];uint16_t *gint_vram=guarded+1;
static Parsed parsed;static Program programs[2];static RayField fields[2];static RayStats stats;
static unsigned char cells[2][(48*48*48+7)/8];static float memory[2*49*49];
static Volume v={0,0,0,3,8,0};
static int prepare(char const *a,char const *b)
{
    char const *formula[2]={a,b};memset(fields,0,sizeof fields);memset(cells,0,sizeof cells);memset(&stats,0,sizeof stats);
    for(int i=0;i<(b?2:1);i++){
        if(!expr_parse(&parsed,formula[i],0)||!expr_compile(&parsed,&programs[i],0,0))return 0;
        fields[i].program=&programs[i];fields[i].cells=cells[i];fields[i].slot=i;fields[i].implicit=1;
        fields[i].continuous=expr_program_continuous(&programs[i]);fields[i].real_fast=expr_real_fast_usable(&programs[i]);
        small_prepare(&fields[i]);if(build_cells(&fields[i],&v,memory,0,0,0,&stats)!=1)return 0;
    }
    return 1;
}
static int shoot(float y,float dy,int count,float expected,int axis,int slot)
{
    Ray r={{0,y/3,0},{0,dy,0}};RayRowZ row;RayHit h;
    ray_row_z_init(&row,0,0,2.0f/v.div,v.div);
    if(!first_hit_mixed(fields,count,&v,&r,.00001f,&h,&row))return 0;
    return fabsf(h.point.y-expected)<.015f&&h.clip_axis==axis&&h.field==slot;
}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    if(!prepare("x^2+y^2+z^2<=4",0)||!shoot(6,-1,1,2,-1,0)||!shoot(0,-1,1,-2,-1,0))return 1;
    if(!prepare("x^2+y^2+z^2<100",0)||!shoot(6,-1,1,3,1,0)||!shoot(0,-1,1,-3,1,0))return 2;
    if(!prepare("y<0",0)||!shoot(6,-1,1,0,-1,0))return 3;
    if(!prepare("1/y>0",0)||!shoot(-6,1,1,0,-1,0))return 4;
    if(!prepare("sqrt(y)>-1",0)||!shoot(-6,1,1,0,-1,0))return 5;
    if(!prepare("y<0","y=1")||!shoot(6,-1,2,1,-1,1))return 6;
    if(!prepare("y=1","y>0")||!shoot(6,-1,2,3,1,1))return 7;
    if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 8;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 9;
    return 0;
}
