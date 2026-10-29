/* Separate accelerated/reference builds emit raw frames for comparison. */
#include "../src/ray.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
static Parsed parsed;static Program pr;static EvalVars vars;
int main(int argc,char **argv)
{
    assert(argc==2);host_keys(NULL,0);FILE *f=fopen(argv[1],"wb");assert(f);
    char const *formula[]={"sin(x)+sin(y)+sin(z)=0","2*sin(x)-3*cos(y)+sin(z)+.5=0",
        "sin(-x)-cos(-z)=0","1e30+sin(x)-1e30+sin(y)=0","1/x=0"};
    Volume v={0,0,0,10,12,0};
    Camera c={.yaw=.43083f,.pitch=.6083f,.zoom=1,.radius=10,.eye_x=11.4f,.eye_y=24.8f,.eye_z=19,.perspective=1};
    for(int i=0;i<5;i++){
        assert(expr_parse(&parsed,formula[i],0)&&expr_compile(&parsed,&pr,NULL,0));RaySurface s={&pr,0};
        for(int k=0;k<2;k++){
            v.div=k?20:12;Camera cam=c;if(k)cam.yaw+=.2f;RayStats stats;dclear(C_WHITE);
            assert(ray3d_render(&s,1,&vars,&v,&cam,NULL,NULL,&stats)==1);
            assert(fwrite(gint_vram,sizeof(uint16_t),DWIDTH*DHEIGHT,f)==DWIDTH*DHEIGHT);
#ifdef GL_DISABLE_TRIGFAST
            assert(!stats.fast_evaluations);
#else
            if(i<3)assert(stats.fast_evaluations>0);else assert(!stats.fast_evaluations);
#endif
        }
    }
    assert(host_vram_guards_ok()&&fclose(f)==0);return 0;
}
