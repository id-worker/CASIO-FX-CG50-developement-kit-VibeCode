/* New coarse frames on real SuperH instructions under QEMU, not Casio OS. */
#include "../src/ray.h"
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
static uint16_t guarded[DWIDTH*DHEIGHT+2],full[PLOT_W*PLOT_H];
uint16_t *gint_vram=guarded+1;
static Parsed parsed;static Program p;static EvalVars vars;
static RayGridCache raygrid;
static void clear(void){for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;}
static int compile(char const *s){return expr_parse(&parsed,s,0)&&expr_compile(&parsed,&p,0,0);}
static int stop(int done,int total,void *p0){(void)total;(void)p0;return done<35;}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    Volume vol={0,0,0,3,12,0};Camera cam={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    if(!compile("x^2+y^2+z^2=1"))return 1;
    RaySurface surface={&p,0};RayStats rs;RayRenderOptions ro={.grid=&raygrid};
    int steps[]={1,3,7,16,31,32};
    for(int k=0;k<6;k++){
        int step=steps[k];ro.screen_step=step;clear();
        if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&ro,0,0,&rs)!=1||
           (k&&!rs.grid_reused)||rs.rays!=((PLOT_W+step-1)/step)*((PLOT_H+step-1)/step))return 2;
        for(int y=PLOT_Y0;y<=PLOT_Y1;y+=step)for(int x=PLOT_X0;x<=PLOT_X1;x+=step){
            uint16_t c=gint_vram[y*DWIDTH+x];
            for(int dy=0;dy<step&&y+dy<=PLOT_Y1;dy++)for(int dx=0;dx<step&&x+dx<=PLOT_X1;dx++)
                if(gint_vram[(y+dy)*DWIDTH+x+dx]!=c)return 3;
        }
    }
    if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&ro,stop,0,&rs)!=0||!rs.canceled)return 4;
    ro.screen_step=33;if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&ro,0,0,&rs)!=-1)return 5;
    ray_grid_cache_clear(&raygrid);used=0;
    if(!compile("x^2+y^2+z^2<=1"))return 6;
    RayRenderOptions vo={.grid=&raygrid,.screen_step=1};RayStats vs;clear();
    if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&vo,0,0,&vs)!=1||!vs.occupied)return 7;
    for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)full[y*PLOT_W+x]=gint_vram[(PLOT_Y0+y)*DWIDTH+PLOT_X0+x];
    for(int k=1;k<6;k++){
        int step=steps[k];vo.screen_step=step;clear();
        if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&vo,0,0,&vs)!=1||!vs.grid_reused)return 8;
        for(int y=0;y<PLOT_H;y+=step)for(int x=0;x<PLOT_W;x+=step){
            uint16_t c=full[y*PLOT_W+x];
            for(int dy=0;dy<step&&y+dy<PLOT_H;dy++)for(int dx=0;dx<step&&x+dx<PLOT_W;dx++)
                if(gint_vram[(PLOT_Y0+y+dy)*DWIDTH+PLOT_X0+x+dx]!=c)return 9;
        }
    }
    if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 10;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 11;
    return 0;
}
