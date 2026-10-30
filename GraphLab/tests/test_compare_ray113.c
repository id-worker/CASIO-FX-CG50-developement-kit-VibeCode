/* Identical images/stats vs 1.12.1, including z-parallel rays, poles,
 * z=f(x,y), two surfaces, cache reuse, cancel and optional malloc failure. */
#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef FAIL_COLUMNS
void *__real_malloc(size_t);static int forced_failures;
void *__wrap_malloc(size_t n){if(n==(size_t)PLOT_W*2*sizeof(float)){forced_failures++;return NULL;}return __real_malloc(n);}
#endif
static Parsed parsed;
static Program prog[2],height[2];
typedef struct {
    const char *a,*b;int q,explicit_z,reuse,cancel;
    float yaw,pitch,zoom,ex,ey,ez,radius;
} Scene;
static const Scene scenes[]={
 {"x^2+y^2+z^2=4",NULL,20,0,0,0,.75f,.55f,1,4.88f,5.24f,4.39f,3},
 {"z+x^2=sqrt(y)",NULL,20,0,0,0,.75f,.55f,1,4.88f,5.24f,4.39f,3},
 {"sin(x)+sin(y)+sin(z)=0",NULL,12,0,1,0,.43f,.61f,1,11.4f,24.8f,19,10},
 {"z=sin(3*x)*cos(2*y)",NULL,32,1,0,0,.75f,.55f,1,3.4f,3.6f,3,3},
 {"1/x=0",NULL,20,0,0,0,0,0,1,0,6,0,3},
 {"x^2+y^2+z^2=1","y=0",20,0,0,0,0,0,1,0,6,0,3},
 {"x^2+y^2+z^2=4",NULL,4,0,0,0,-1.1f,-.4f,.5f,-4,-5,1,3},
 {"x^2+y^2+z^2=4",NULL,48,0,0,0,.75f,.55f,1,4.88f,5.24f,4.39f,3},
 {"z+x^2=sqrt(y)","x^2+y^2+z^2=2",20,0,1,0,1.2f,-.2f,2,1,5,-1,3},
 {"x*y*z=1",NULL,16,0,0,0,.4f,0,1,0,.00005f,0,.001f},
 {"ln(x)+y+z=0",NULL,12,0,0,0,-.7f,.3f,.1f,5,2,-2,3},
 {"z+x^2=sqrt(y)",NULL,20,0,0,1,.75f,.55f,1,4.88f,5.24f,4.39f,3},
};
static int stop(int done,int total,void *ctx){(void)total;(void)ctx;return done<93;}
int main(int argc,char **argv){
 assert(argc==2);FILE *out=fopen(argv[1],"wb");assert(out);host_keys(NULL,0);
 for(size_t t=0;t<sizeof scenes/sizeof *scenes;t++){
  Scene const *s=&scenes[t];RaySurface rs[2];RayRenderOptions opt={0};int n=s->b?2:1;
  const char *f[2]={s->a,s->b};
  for(int i=0;i<n;i++){
   assert(expr_parse(&parsed,f[i],0)&&expr_compile(&parsed,&prog[i],NULL,0));
   rs[i]=(RaySurface){&prog[i],i};
   if(s->explicit_z && expr_compile_explicit_z(&parsed,&height[i]))opt.height[i]=&height[i];
  }
  EvalVars vars={0};Volume v={0,0,0,s->radius,s->q,0};
  Camera cam={.yaw=s->yaw,.pitch=s->pitch,.zoom=s->zoom,.radius=s->radius,
      .eye_x=s->ex,.eye_y=s->ey,.eye_z=s->ez,.perspective=1};
  RayGridCache grid={0};if(s->reuse)opt.grid=&grid;RayStats stats;
  int rounds=s->reuse?2:1;
  for(int r=0;r<rounds;r++){
   if(r)cam.yaw+=.11f;
   dclear(C_WHITE);int rc=ray3d_render_ex(rs,n,&vars,&v,&cam,&opt,s->cancel?stop:NULL,NULL,&stats);
   assert(rc==(s->cancel?0:1));assert(host_vram_guards_ok());
   assert(fwrite(gint_vram,sizeof(uint16_t),DWIDTH*DHEIGHT,out)==DWIDTH*DHEIGHT);
   fprintf(stderr,"scene=%zu round=%d rc=%d rays=%d hits=%d occ=%d eval=%d canceled=%d reuse=%d height=%d fast=%d\n",t,r,rc,stats.rays,stats.hits,stats.occupied,stats.evaluations,stats.canceled,stats.grid_reused,stats.height_fields,stats.fast_evaluations);
  }
  ray_grid_cache_clear(&grid);
 }
 assert(fclose(out)==0);
#ifdef FAIL_COLUMNS
 assert(forced_failures==14); /* one optional 3 KiB buffer per frame */
#endif
 return 0;
}
