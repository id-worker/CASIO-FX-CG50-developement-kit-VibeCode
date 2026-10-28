#include "../src/plot.h"
#include "host_gint.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
static Camera camera(float distance,float yaw,float pitch,float sideways,float upwards,float radius)
{
    float cy=cosf(yaw),sy=sinf(yaw),cp=cosf(pitch),sp=sinf(pitch);
    /* Forward is (-cp*sy,-cp*cy,-sp), camera right is (cy,-sy,0),
       up is (-sp*sy,-sp*cy,cp). */
    return (Camera){.yaw=yaw,.pitch=pitch,.zoom=1,.radius=radius,.perspective=1,
        .eye_x=distance*(cp*sy+sideways*cy-upwards*sp*sy),
        .eye_y=distance*(cp*cy-sideways*sy-upwards*sp*cy),
        .eye_z=distance*(sp+upwards*cp)};
}
static float line_distance(Axis3Geometry const *g,float x,float y)
{
    float dx=(float)(g->x1-g->x0),dy=(float)(g->y1-g->y0),len=sqrtf(dx*dx+dy*dy);
    if(len<1)return 0;
    return fabsf(dx*(y-g->y0)-dy*(x-g->x0))/len;
}
static unsigned rnd=0x9fc341u;
static float randomf(void){rnd^=rnd<<13;rnd^=rnd>>17;rnd^=rnd<<5;return (rnd&65535)/65535.0f;}
int main(void)
{
    host_keys(NULL,0);int tested=0,marks=0;
    float distances[]={8.4f,3.0f,.8f,.2f,.03f,.005f,.0005f,.00005f};float previous=100;
    for(unsigned i=0;i<sizeof distances/sizeof *distances;i++){
        Camera c=camera(distances[i],.75f,.55f,0,0,3);Axis3Geometry g;
        assert(render3_axis_geometry(&c,0,&g) && g.count>=2 && g.reference_step>0);
        assert(g.reference_step<previous);previous=g.reference_step;
        if(distances[i]<.1f){int nonzero=0;for(int j=0;j<g.count;j++)nonzero+=g.tick[j].value!=0;assert(nonzero>=2);}
    }
    for(int n=0;n<4000;n++){
        float distance=powf(10,-4.0f+4.9f*randomf());
        float yaw=-3.0f+6.0f*randomf(),pitch=-1.25f+2.5f*randomf();
        Camera c=camera(distance,yaw,pitch,-.40f+.8f*randomf(),-.22f+.44f*randomf(),3);
        float ox,oy;if(!render3_project_world(&c,0,0,0,&ox,&oy) ||
            ox<PLOT_X0+3 || ox>PLOT_X1-3 || oy<PLOT_Y0+3 || oy>PLOT_Y1-3)continue;
        for(int axis=0;axis<3;axis++){
            Axis3Geometry g;if(!render3_axis_geometry(&c,axis,&g)){
                fprintf(stderr,"missing axis %d at case %d distance %g\n",axis,n,(double)distance);return 1;
            }
            float error=line_distance(&g,ox,oy);
            if(error>1.8f){fprintf(stderr,"axis %d misses origin by %.3f px at case %d, dist %.7g\n",axis,(double)error,n,(double)distance);return 1;}
            assert(g.x0>=PLOT_X0&&g.x0<=PLOT_X1&&g.x1>=PLOT_X0&&g.x1<=PLOT_X1);
            assert(g.y0>=PLOT_Y0&&g.y0<=PLOT_Y1&&g.y1>=PLOT_Y0&&g.y1<=PLOT_Y1);
            assert(g.world_start>=-3.0001f && g.world_end<=3.0001f && g.world_start<=g.world_end);
            for(int j=0;j<g.count;j++){
                float world[3]={0,0,0},x,y;world[axis]=g.tick[j].value;
                assert(render3_project_world(&c,world[0],world[1],world[2],&x,&y));
                if(fabsf(x-g.tick[j].x)>1.25f || fabsf(y-g.tick[j].y)>1.25f){
                    fprintf(stderr,"wrong tick value projection, case %d\n",n);return 1;
                }
                marks++;
            }
            tested++;
        }
    }
    assert(tested>10000 && marks>10000 && host_vram_guards_ok());
    printf("Perspective axes: %d visible lines through a shared origin; %d tick projections; adaptive close-up scales passed.\n",tested,marks);
}
