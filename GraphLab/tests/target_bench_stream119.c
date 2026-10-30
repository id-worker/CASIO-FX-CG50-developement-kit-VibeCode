/* Relative SuperH/QEMU workload, not a physical fx-CG50 clock or OS. */
#include "ray.h"
#include <gint/display.h>
#include <stdint.h>
#include <stdlib.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[1024*1024];static unsigned used;
static uint16_t framebuffer[DWIDTH*DHEIGHT],layer[PLOT_W*PLOT_H];uint16_t *gint_vram=framebuffer;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program p[2],height;static RayGridCache grid;static ScreenRefine refine;
static int compile(Program *out,char const *s){return expr_parse(&parsed,s,0)&&expr_compile(&parsed,out,0,0);}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    if(!compile(&p[0],"sin(x)+sin(y)+sin(z)=0")||!compile(&p[1],"z=x-y")||!compile(&height,"x-y"))return 1;
    EvalVars vars={0};Volume v={0,0,0,10,12,0};
    Camera c={.yaw=.78539816f,.pitch=.716342f,.zoom=1,.eye_x=13,.eye_y=13,.eye_z=16,.radius=10,.perspective=1};
    RaySurface surf[2]={{&p[0],0},{&p[1],1}};RayStats st;if(!refine_begin(&refine,2))return 2;
    RayRenderOptions op={.grid=&grid,.height={NULL,&height},.screen_step=2,.layer=layer,.refine=&refine};
    if(ray3d_render_ex(surf,2,&vars,&v,&c,&op,0,0,&st)!=1)return 3;
    op.screen_step=1;
#ifdef GL_BENCH_STREAM
    op.final_stream=1;
#endif
    if(ray3d_render_ex(surf,2,&vars,&v,&c,&op,0,0,&st)!=1||st.rays!=52303||st.evaluations!=781679)return 4;
    uint64_t hash=UINT64_C(1469598103934665603);
    for(unsigned i=0;i<PLOT_W*PLOT_H;i++){hash^=layer[i];hash*=UINT64_C(1099511628211);}
    if(hash!=UINT64_C(0x0eda52ca07d3b95b))return 5;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 6;
    return 0;
}
