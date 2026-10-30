/* Big-endian SuperH CPU/library checks only: not the Casio OS or hardware. */
#include "../src/intersection.h"
#include <math.h>
#include <stdlib.h>
#include <stdint.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[512*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;for(;;){}}
static Parsed parsed;static Program pr[2],height;static EvalVars vars;static IMapPixel map[IMAP_SIDE*IMAP_SIDE];
static int compile(int i,char const *s){return expr_parse(&parsed,s,0)&&expr_compile(&parsed,&pr[i],NULL,0);}
int main(void)
{
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    if(!compile(0,"x^2+y^2+z^2=4")||!compile(1,"x=0"))return 1;
    IMapView v={.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=32,.cursor_steps=128};
    RaySurface s[2]={{&pr[0],0},{&pr[1],1}};IMapRoots r;IMapStats st;IMapPoint near;
    if(imap_search(s,&vars,&v,0,1.2f,NULL,NULL,&r)!=1||r.count!=2||fabsf(r.z[0]+1.6f)>.001f||fabsf(r.z[1]-1.6f)>.001f)return 2;
    if(imap_search(s,&vars,&v,.6f,1.2f,NULL,NULL,&r)!=1||r.count)return 3;
    if(imap_build(s,&vars,&v,map,NULL,NULL,&st)!=1||st.pixels<70)return 4;
    if(!imap_nearest(map,&v,.6f,1.2f,&near)||fabsf(near.x)>.02f||fabsf(near.y-1.2f)>.1f)return 5;
    if(!compile(0,"1/(z-0.123)=0"))return 6;
    if(imap_search(s,&vars,&v,0,1,NULL,NULL,&r)!=1||r.count)return 7;
    if(!compile(0,"z+x^2=sqrt(y)"))return 8;
    if(imap_search(s,&vars,&v,0,0,NULL,NULL,&r)!=1||r.count!=1||fabsf(r.z[0])>.001f)return 9;
    IMapView automatic={.radius=10};
    if(!imap_auto_view(&automatic,10)||automatic.xy_div!=180||automatic.z_div!=180)return 11;
    if(!compile(0,"x=0")||!compile(1,"z=0.123")||!expr_compile_explicit_z(&parsed,&height))return 12;
    v=(IMapView){.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=32,.cursor_steps=64};
    Program const *hp[2]={NULL,&height};used=0;
    if(imap_build_auto(s,&vars,&v,hp,map,NULL,NULL,&st)!=1||st.pixels<170)return 13;
    IMapPixelHits hits;
    if(imap_pixel_search(s,&vars,&v,90,90,map,NULL,NULL,&hits)!=1||hits.count<1)return 14;
    for(int i=0;i<hits.count;i++)if(fabsf(hits.point[i].x)>.0001f||fabsf(hits.point[i].z-.123f)>.0001f)return 15;
    if(!compile(0,"sin(x)+sin(y)+sin(z)=0")||!compile(1,"z=x-y"))return 16;
    if(imap_search(s,&vars,&v,6.28293562f,3.90293574f,NULL,NULL,&r)!=1||r.count)return 17;
    for(int i=0;i<1024;i++)if(target_stack[i]!=0xa5)return 10;
    return 0;
}
