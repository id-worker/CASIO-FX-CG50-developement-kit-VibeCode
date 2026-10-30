/* Big-endian SuperH/QEMU: real instructions, fake display/allocator, no OS. */
#include "../src/ray.h"
#include <gint/display.h>
#include <stdint.h>
#include <string.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[2*1024*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return 0;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int n){(void)n;for(;;){}}
static uint16_t guarded[DWIDTH*DHEIGHT+2],layer[PLOT_W*PLOT_H];
uint16_t *gint_vram=guarded+1;
static Parsed parsed;static Program p;static EvalVars vars;
static RayGridCache raygrid;static ScreenRefine refine;
static void clear(void){for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;}
static int compile(char const *s){return expr_parse(&parsed,s,0)&&expr_compile(&parsed,&p,0,0);}
static int same(void){for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)
    if(layer[y*PLOT_W+x]!=gint_vram[(y+PLOT_Y0)*DWIDTH+PLOT_X0+x])return 0;return 1;}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    Volume vol={0,0,0,3,6,0};Camera cam={.yaw=.4f,.pitch=.3f,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    if(!compile("x^2+y^2+z^2=1"))return 1;
    RaySurface surface={&p,0};int seeds[]={32,31};
    for(int k=0;k<2;k++){
        if(!refine_begin(&refine,seeds[k]))return 2;
        int total=0;
        for(int step=seeds[k];step;step=refine_next(step)){
            RayStats st,ref;RayRenderOptions op={.grid=&raygrid,.screen_step=step,.layer=layer,.refine=&refine};
            if(step==1)op.final_stream=1;
            if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&op,0,0,&st)!=1)return 3;
            total+=st.rays;
            if(refine.last_step!=step)return 13;
            clear();RayRenderOptions direct={.grid=&raygrid,.screen_step=step};
            if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&direct,0,0,&ref)!=1||!same())return 4;
        }
        if(total!=PLOT_W*PLOT_H)return 5;
    }
    refine_free(&refine);ray_grid_cache_clear(&raygrid);used=0;
    if(!compile("x^2+y^2+z^2<=1"))return 6;
    for(int k=0;k<2;k++){
        if(!refine_begin(&refine,seeds[k]))return 7;
        int total=0;
        for(int step=seeds[k];step;step=refine_next(step)){
            RayStats st,ref;RayRenderOptions op={.grid=&raygrid,.screen_step=step,.layer=layer,.refine=&refine};
            if(step==1)op.final_stream=1;
            if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&op,0,0,&st)!=1)return 8;
            total+=st.rays;clear();RayRenderOptions direct={.grid=&raygrid,.screen_step=step};
            if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&direct,0,0,&ref)!=1||!same())return 9;
        }
        if(total!=PLOT_W*PLOT_H)return 10;
    }
    if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 11;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 12;
    return 0;
}
