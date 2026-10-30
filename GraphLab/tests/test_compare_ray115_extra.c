/* Additional 1.14-vs-1.15 host frames, including nonzero clip centres,
   signed zeros, poles, explicit heights and Preview point recapture. */
#include "ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
static Parsed parsed;static Program program,height;
typedef struct {const char *formula;int q,height,preview;float cx,cy,cz,r,ex,ey,ez,yaw,pitch;} Scene;
static const Scene scenes[]={
 {"sin(x)-cos(y)+sin(z)=0",8,0,1,0,0,0,10,18.7f,20.1f,16.9f,.75f,.55f},
 {"cos(x)+cos(y)+cos(z)=0",12,0,1,0,0,0,10,13,19,17,.6f,.5f},
 {"-sin(x)-sin(y)-sin(z)=0",4,0,1,0,0,0,3,5,6,5,.75f,.55f},
 {"atan2(y,x)+z=0",8,0,1,0,-0.0f,0,3,0,6,0,0,0},
 {"1/x+1/(y-z)=0",8,0,0,0,0,0,3,0,6,0,0,0},
 {"z=sin(x)+cos(y)",12,1,1,0,0,0,3,4.8f,5.2f,4.4f,.75f,.55f},
 {"x^2+y^2+z^2=4",8,0,1,2.5f,-1.5f,.75f,3,7.0f,3.5f,5,.75f,.55f},
 {"x+y-z=0",16,0,1,-0.0f,0,-0.0f,3,4,5,4,.75f,.55f},
 {"x*y*z=1",8,0,0,-2,1,1.25f,4,5,6,6,.75f,.55f},
 {"1/x=0",8,0,0,0,0,0,3,0,6,0,0,0},
 {"z=abs(x)-abs(y)",6,1,1,0,0,0,3,4.8f,5.2f,4.4f,.75f,.55f},
 {"z=sin(x)+sin(y)",32,1,0,0,0,0,3,4.8f,5.2f,4.4f,.75f,.55f},
};
int main(int argc,char **argv){
 assert(argc==2);FILE *out=fopen(argv[1],"wb");assert(out);host_keys(NULL,0);
 for(unsigned i=0;i<sizeof scenes/sizeof scenes[0];i++){
  Scene const *s=&scenes[i];assert(expr_parse(&parsed,s->formula,0)&&expr_compile(&parsed,&program,NULL,0));
  RaySurface surface={&program,0};RayRenderOptions opt={0};
  if(s->height){assert(expr_compile_explicit_z(&parsed,&height));opt.height[0]=&height;}
  EvalVars vars={0};Volume vol={s->cx,s->cy,s->cz,s->r,s->q,0};
  Camera cam={.yaw=s->yaw,.pitch=s->pitch,.zoom=1,.radius=s->r,.eye_x=s->ex,.eye_y=s->ey,.eye_z=s->ez,.perspective=1};
  Preview3D cloud={0};if(s->preview){preview3d_set_step(&cloud,4);assert(preview3d_begin(&cloud,&vol,&cam));opt.preview=&cloud;}
  RayStats stats;dclear(C_WHITE);int result=ray3d_render_ex(&surface,1,&vars,&vol,&cam,&opt,NULL,NULL,&stats);
  assert(result==1&&host_vram_guards_ok());
  assert(fwrite(gint_vram,sizeof(uint16_t),DWIDTH*DHEIGHT,out)==DWIDTH*DHEIGHT);
  if(s->preview){
   assert(fwrite(&cloud.count,sizeof cloud.count,1,out)==1);
   assert(fwrite(&cloud.aux_count,sizeof cloud.aux_count,1,out)==1);
   assert(fwrite(cloud.point,sizeof *cloud.point,(size_t)cloud.count,out)==(size_t)cloud.count);
   if(cloud.aux_count)assert(fwrite(cloud.aux,sizeof *cloud.aux,(size_t)cloud.aux_count,out)==(size_t)cloud.aux_count);
   cam.yaw+=.03f;dclear(C_WHITE);assert(preview3d_draw(&cloud,&cam)==1);
   assert(fwrite(gint_vram,sizeof(uint16_t),DWIDTH*DHEIGHT,out)==DWIDTH*DHEIGHT);
  }
  fprintf(stderr,"%u rays=%d hits=%d occ=%d eval=%d fast=%d height=%d preview=%d aux=%d\n",i,stats.rays,stats.hits,stats.occupied,stats.evaluations,stats.fast_evaluations,stats.height_fields,cloud.count,cloud.aux_count);
  preview3d_free(&cloud);
 }
 assert(fclose(out)==0);return 0;
}
