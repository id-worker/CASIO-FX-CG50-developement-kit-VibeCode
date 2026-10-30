/* Reproduce the user's Q and clip on host; camera is an assumption. */
#include "ray.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
static Program p[2],height[2];static Parsed parsed;
static void make(int i,const char *s){assert(expr_parse(&parsed,s,0)&&expr_compile(&parsed,&p[i],NULL,0));if(i==1)assert(expr_compile_explicit_z(&parsed,&height[i]));}
int main(int argc,char **argv){
 assert(argc>=4);host_keys(NULL,0);make(0,"sin(x)+sin(y)+sin(z)=0");make(1,"z=x-y");
 int q=atoi(argv[2]), turn=atoi(argv[3]);float radius=argc>4?strtof(argv[4],NULL):10;int swap=argc>5?atoi(argv[5]):0;
 EvalVars vars={0};Volume v={.radius=radius,.div=q};float yaw0=.75f,pitch=.55f,distance=2.8f*fminf(v.radius,3.0f),cp=cosf(pitch);
 Camera c={.yaw=yaw0+(turn*.065f),.pitch=pitch,.zoom=1,.radius=radius,.eye_x=distance*cp*sinf(yaw0),.eye_y=distance*cp*cosf(yaw0),.eye_z=distance*sinf(pitch),.perspective=1};
 RaySurface surfaces[2]={{&p[swap?1:0],0},{&p[swap?0:1],1}};RayStats st;RayGridCache cache={0};
 RayRenderOptions opt={.grid=&cache,.height={swap?&height[1]:NULL,swap?NULL:&height[1]}};dclear(C_WHITE);
 assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&opt,NULL,NULL,&st)==1);
 fprintf(stderr,"R=%.2f q=%d swap=%d turn_steps=%d eye=(%.8f %.8f %.8f) yaw=%.6f pitch=%.6f rays=%d hits=%d candidate_cells=%d field_evals=%d trig_table=%d height=%d grid_reused=%d\n",radius,q,swap,turn,c.eye_x,c.eye_y,c.eye_z,c.yaw,c.pitch,st.rays,st.hits,st.occupied,st.evaluations,st.fast_evaluations,st.height_fields,st.grid_reused);
 FILE *f=fopen(argv[1],"wb");assert(f);assert(fwrite(gint_vram,sizeof(uint16_t),DWIDTH*DHEIGHT,f)==DWIDTH*DHEIGHT);fclose(f);
 dclear(C_WHITE);assert(ray3d_render_ex(surfaces,2,&vars,&v,&c,&opt,NULL,NULL,&st)==1);
 fprintf(stderr,"same view with grid cache: rays=%d hits=%d candidate_cells=%d field_evals=%d trig_table=%d grid_reused=%d\n",st.rays,st.hits,st.occupied,st.evaluations,st.fast_evaluations,st.grid_reused);
 ray_grid_cache_clear(&cache);return 0;
}
