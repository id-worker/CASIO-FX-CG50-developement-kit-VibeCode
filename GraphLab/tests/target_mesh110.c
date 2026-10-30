/* SuperH instruction tests only, not hardware or Casio OS. */
#include "../src/mesh_preview.h"
#include "../src/section.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[2*1024*1024];static unsigned used;
static uint16_t fb[DWIDTH*DHEIGHT+2];uint16_t *gint_vram=fb+1;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
void *fxlibc_hal_realloc(void *p,unsigned n){(void)n;return p;} /* only shrinking in this test */
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;} int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program pr[2];static EvalVars vars;
static int compile(int i,char const *s){return expr_parse(&parsed,s,0)&&expr_compile(&parsed,&pr[i],NULL,0);}
static void clear(void){for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    fb[0]=fb[DWIDTH*DHEIGHT+1]=0x5aa5;
    if(!compile(0,"x^2+y^2+z^2=4"))return 1;
    Volume vol={0,0,0,3,16,0};RaySurface s[2]={{&pr[0],0},{&pr[1],1}};
    PreviewMesh m={0};
    if(preview_mesh_build(&m,s,1,&vars,&vol,NULL,NULL)!=1||m.vertex_count<100||m.triangle_count<200)return 2;
    Camera c={.yaw=.75f,.pitch=.55f,.zoom=1,.radius=3,.eye_x=4.29f,.eye_y=9.32f,.eye_z=7.1f,.perspective=1};
    for(int k=0;k<8;k++){
        c.yaw+=.025f;clear();if(!preview_mesh_draw(&m,&c))return 3;
        int pixels=0;for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++)pixels+=gint_vram[y*DWIDTH+x]!=C_WHITE;
        if(pixels<100)return 4;
    }
    if(fb[0]!=0x5aa5||fb[DWIDTH*DHEIGHT+1]!=0x5aa5)return 5;
    if(!compile(1,"x=0"))return 6;
    SectionResult r;SectionView v={0,0,3,40};
    if(section_compute(s,2,&vars,&v,NULL,NULL,NULL,NULL,&r)!=1||r.count!=2)return 7;
    if(fabsf(fabsf(r.point[0].y)-2)>.02f||fabsf(r.point[0].z)>.001f)return 8;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 9;
    return 0;
}
