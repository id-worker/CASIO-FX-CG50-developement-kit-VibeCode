/* New 1.07 paths on real SuperH instructions under QEMU; no Casio OS/hardware.
   The arena is a test allocator, not a measurement of free calculator RAM. */
#include "../src/expr.h"
#include "../src/ray.h"
#include "../src/voxel.h"
#include "../src/trigfast.h"
#include "../src/fractal.h"
#include "../src/scroll.h"
#include <gint/display.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[1024*1024];static unsigned used;
static uint16_t guarded[DWIDTH*DHEIGHT+2];uint16_t *gint_vram=guarded+1;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int n){(void)n;for(;;){}}
static Parsed parsed;static Program pr,rhs;static EvalVars vars;
static RayGridCache grid;static Preview3D cloud;
static int compile(char const *text){return expr_parse(&parsed,text,0)&&expr_compile(&parsed,&pr,NULL,0);}
static void clear(void){for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=C_WHITE;}
static int painted;
static void block(int x,int y,int w,int h,int iter,void *ctx){
    (void)iter;(void)ctx;painted++;
    for(int j=y;j<y+h;j++)for(int i=x;i<x+w;i++){
        if(i<PLOT_X0||i>PLOT_X1||j<PLOT_Y0||j>PLOT_Y1){painted=-10000;return;}
        gint_vram[j*DWIDTH+i]=C_BLUE;
    }
}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    uint16_t pixels[15]={1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    scroll_pixels(pixels,5,3,5,2,-1,0);
    if(pixels[0]!=0||pixels[2]!=6||pixels[6]!=0||pixels[7]!=11||pixels[12]!=0)return 1;
    unsigned char bits[2]={0x15,0};scroll_bits(bits,5,2,1,0);
    if(bits[0]!=0x0a)return 2;
    ScrollRect rectangles[2];if(scroll_exposed(5,3,2,-1,rectangles)!=2)return 3;
    if(!compile("z^2=0")||!expr_compile_zero_surface(&parsed,&rhs)||!rhs.has_equal)return 4;
    vars.v[V_Z]=2;if(fabsf(expr_eval_real(&rhs,&vars)-2)>.0001f)return 5;
    if(!compile("z=sin(x)+y")||!expr_compile_explicit_z(&parsed,&rhs)||expr_program_uses(&rhs,V_Z))return 6;
    Volume vol={0,0,0,3,12,0};Camera cam={.yaw=0,.pitch=0,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    if(!compile("x^2+y^2+z^2=1"))return 7;
    RaySurface surface={&pr,0};RayStats st;memset(&grid,0,sizeof grid);memset(&cloud,0,sizeof cloud);
    if(!preview3d_begin(&cloud,&vol,&cam))return 8;
    RayRenderOptions opt={.grid=&grid,.preview=&cloud};clear();
    if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&opt,NULL,NULL,&st)!=1||st.hits<2000||cloud.count<100)return 9;
    clear();if(preview3d_draw(&cloud,&cam)!=1)return 10;
    int center=((PLOT_Y0+PLOT_Y1)/2)*DWIDTH+(PLOT_X0+PLOT_X1)/2;
    if(gint_vram[center]==C_WHITE)return 11;
    cam.yaw=.1f;opt.preview=NULL;clear();
    if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&opt,NULL,NULL,&st)!=1||!st.grid_reused)return 12;
    preview3d_free(&cloud);ray_grid_cache_clear(&grid);used=0;
    memset(&cloud,0,sizeof cloud);cam.yaw=0;
    if(!compile("x^2+y^2+z^2<=1")||!preview3d_begin(&cloud,&vol,&cam))return 13;
    surface.program=&pr;VoxelStats vox;clear();
    if(voxel3d_render_ex(&surface,1,&vars,&vol,&cam,&cloud,NULL,NULL,&vox)!=1||cloud.count<50)return 14;
    clear();cam.pitch=.1f;if(preview3d_draw(&cloud,&cam)!=1)return 15;
    preview3d_free(&cloud);used=0;
    /* 1.08: SH libm/LUT agreement outside the guard, incorrect period
       disabled, and low-Q RAY captures coarse points without polygons. */
    if(!compile("sin(x)+sin(y)+sin(z)=0"))return 21;
    if(!expr_program_continuous(&pr)||expr_proves_x_period(&pr,&vars,10)||
       !expr_proves_x_period(&pr,&vars,6.283185307179586f))return 22;
    TrigFast fast;if(!trigfast_prepare(&fast,&pr,&vars,10))return 23;
    for(int i=0;i<10000;i++){
        vars.v[V_X]=(i%401-200)*.31f;vars.v[V_Y]=(i%293-146)*.43f;vars.v[V_Z]=(i%181-90)*.67f;
        float exact=expr_eval_real(&pr,&vars),value;
        if(trigfast_value(&fast,vars.v[V_X],vars.v[V_Y],vars.v[V_Z],0,&value)&&
           (!isfinite(exact)||fabsf(exact-value)>fast.guard||(exact<0)!=(value<0)))return 24;
    }
    memset(&vars,0,sizeof vars);vol=(Volume){0,0,0,10,4,10};
    cam=(Camera){.yaw=.43083f,.pitch=.6083f,.zoom=1,.radius=10,
                 .eye_x=11.4f,.eye_y=24.8f,.eye_z=19,.perspective=1};
    if(!preview3d_begin(&cloud,&vol,&cam))return 25;
    surface.program=&pr;opt=(RayRenderOptions){.preview=&cloud};clear();
    if(ray3d_render_ex(&surface,1,&vars,&vol,&cam,&opt,NULL,NULL,&st)!=1||
       !st.fast_evaluations||!cloud.aux_count)return 26;
    clear();cam.yaw+=.1f;if(preview3d_draw(&cloud,&cam)!=1)return 27;
    preview3d_free(&cloud);used=0;
    if(!compile("z^2+c"))return 16;
    FractalView f={-.5f,0,.012f,-.8f,.156f,4,20,4,0};painted=0;clear();
    if(fractal_generate_region(&pr,&vars,&f,PLOT_X0,PLOT_Y0,PLOT_X0+44,PLOT_Y1,
            -45,0,block,NULL,NULL,NULL)!=1||painted<=0)return 17;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0+45;x<=PLOT_X1;x++)
        if(gint_vram[y*DWIDTH+x]!=C_WHITE)return 18;
    if(guarded[0]!=0x5aa5||guarded[DWIDTH*DHEIGHT+1]!=0x5aa5)return 19;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 20;
    return 0;
}
