/* Test the actual private ray/cell traversal with known analytic rays.
   Include the implementation here so individual hit distances can be
   checked without changing the production interface. */
#include "../src/ray.c"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>

static Parsed parsed;
static Program programs[2];
static unsigned char bits[2][(48*48*48+7)/8];
static float memory[2*49*49];
static RayField fields[2];
static char const *current;
static RayStats stats;
static Volume volume={0,0,0,3,8,0};

static void prepare(char const *a,char const *b)
{
    current=a;char const *formula[2]={a,b};
    memset(fields,0,sizeof fields);memset(bits,0,sizeof bits);memset(&stats,0,sizeof stats);
    for(int i=0;i<(b?2:1);i++){
        assert(expr_parse(&parsed,formula[i],0));
        assert(expr_compile(&parsed,&programs[i],NULL,0));
        fields[i].program=&programs[i];fields[i].implicit=1;fields[i].slot=i;
        fields[i].cells=bits[i];fields[i].continuous=expr_program_continuous(&programs[i]);
        fields[i].real_fast=expr_real_fast_usable(&programs[i]);small_prepare(&fields[i]);
        assert(build_cells(&fields[i],&volume,memory,NULL,NULL,NULL,&stats)==1);
    }
}
static RayHit shoot(float ox,float oy,float oz,float dx,float dy,float dz,int count,int expected)
{
    Ray r={{ox/volume.radius,oy/volume.radius,oz/volume.radius},{dx,dy,dz}};
    RayRowZ row;ray_row_z_init(&row,r.origin.z,r.dir.z,2.0f/volume.div,volume.div);
    RayHit hit={0};int found=first_hit_mixed(fields,count,&volume,&r,.00001f,&hit,&row);
    if(found!=expected){fprintf(stderr,"unexpected hit %d (wanted %d), at %.3f %.3f %.3f, %s\n",found,expected,ox,oy,oz,current);assert(0);}
    return hit;
}
static void nearf(float got,float want,float tolerance)
{if(fabsf(got-want)>tolerance){fprintf(stderr,"hit value %.8g != %.8g\n",(double)got,(double)want);assert(0);}}
int main(void)
{
    host_keys(NULL,0);
    /* Bounded sphere, same geometry from outside or from inside. */
    char const *spheres[]={"x^2+y^2+z^2<4","x^2+y^2+z^2<=4",
                           "4>x^2+y^2+z^2","4>=x^2+y^2+z^2"};
    for(unsigned i=0;i<sizeof spheres/sizeof *spheres;i++){
        prepare(spheres[i],NULL);RayHit h=shoot(0,6,0,0,-1,0,1,1);
        nearf(h.point.y,2,.015f);assert(h.clip_axis==-1&&h.field==0);
        h=shoot(0,0,0,0,-1,0,1,1);nearf(h.point.y,-2,.015f);assert(h.clip_axis==-1);
        shoot(2.8f,6,2.8f,0,-1,0,1,0);
    }
    /* Entire, empty, complementary half-spaces and clipping faces. */
    prepare("x^2+y^2+z^2<100",NULL);
    RayHit h=shoot(0,6,0,0,-1,0,1,1);nearf(h.point.y,3,.0001f);assert(h.clip_axis==1);
    h=shoot(0,0,0,0,-1,0,1,1);nearf(h.point.y,-3,.0001f);assert(h.clip_axis==1);
    prepare("x^2+y^2+z^2<0",NULL);shoot(0,6,0,0,-1,0,1,0);
    prepare("y>0",NULL);h=shoot(0,6,0,0,-1,0,1,1);nearf(h.point.y,3,.0001f);assert(h.clip_axis==1);
    prepare("y<0",NULL);h=shoot(0,6,0,0,-1,0,1,1);nearf(h.point.y,0,.003f);assert(h.clip_axis==-1);
    prepare("y<=0",NULL);h=shoot(0,6,0,0,-1,0,1,1);nearf(h.point.y,0,.003f);
    prepare("y>=0",NULL);h=shoot(0,1,0,0,-1,0,1,1);nearf(h.point.y,0,.003f);
    /* Pole and undefined-to-defined transition are set boundaries, not roots. */
    prepare("1/y>0",NULL);h=shoot(0,-6,0,0,1,0,1,1);nearf(h.point.y,0,.003f);
    prepare("sqrt(y)>-1",NULL);h=shoot(0,-6,0,0,1,0,1,1);nearf(h.point.y,0,.003f);
    prepare("sqrt(y)>0.5",NULL);h=shoot(0,-6,0,0,1,0,1,1);nearf(h.point.y,.25f,.01f);
    /* Mixed scene: order is nearest surface, not whichever slot is first. */
    prepare("y<0","y=1");h=shoot(0,6,0,0,-1,0,2,1);nearf(h.point.y,1,.01f);assert(h.field==1&&h.clip_axis==-1);
    prepare("y=1","y<0");h=shoot(0,6,0,0,-1,0,2,1);nearf(h.point.y,1,.01f);assert(h.field==0);
    prepare("y=1","y>0");h=shoot(0,6,0,0,-1,0,2,1);nearf(h.point.y,3,.0001f);assert(h.field==1&&h.clip_axis==1);
    assert(host_vram_guards_ok());
    puts("Ray volumes: analytic < <= > >=, inside/outside clipping, pole/domain boundaries and mixed-scene depth passed.");
}
