#include "../src/intersection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define W IMAP_SIDE
static Parsed parsed;static Program p[2];static EvalVars vars;static IMapPixel image[W*W];
static void compile(int i,char const *str){assert(expr_parse(&parsed,str,0)&&expr_compile(&parsed,&p[i],NULL,0));}
static int mark(int x,int y){return image[y*W+x].z!=INT16_MIN;}
static int components(int *largest)
{
    static uint8_t seen[W*W];static int queue[W*W];memset(seen,0,sizeof seen);int num=0;*largest=0;
    for(int i=0;i<W*W;i++)if(image[i].z!=INT16_MIN&&!seen[i]){
        int head=0,tail=0;queue[tail++]=i;seen[i]=1;
        while(head<tail){int a=queue[head++],x=a%W,y=a/W;
            for(int j=-1;j<=1;j++)for(int k=-1;k<=1;k++){
                int xx=x+k,yy=y+j;if(xx<0||xx>=W||yy<0||yy>=W)continue;
                int b=yy*W+xx;if(!seen[b]&&mark(xx,yy)){seen[b]=1;queue[tail++]=b;}
            }
        }
        if(tail>*largest)*largest=tail;
        num++;
    }
    return num;
}
int main(void)
{
    RaySurface s[2]={{&p[0],0},{&p[1],1}};
    IMapView v={.radius=3,.zmin=-3,.zmax=3,.xy_div=45,.z_div=32,.cursor_steps=64};IMapStats st;
    compile(0,"x^2+y^2+z^2=4");compile(1,"z=0");
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1);int longest,groups=components(&longest);
    fprintf(stderr,"circle map: %d pixels, %d components, largest %d\n",st.pixels,groups,longest);
    assert(st.pixels>300&&groups==1&&longest==st.pixels);
    compile(0,"x^2=1");compile(1,"z=0");
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1);
    for(int y=0;y<W;y++)for(int x=75;x<=105;x++)assert(!mark(x,y));
    assert(st.pixels>250);
    compile(0,"1/x=z");compile(1,"y=0");
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1);
    for(int x=84;x<=96;x++)for(int y=85;y<=95;y++)assert(!mark(x,y));
    assert(st.pixels>100);
    compile(0,"z=5");compile(1,"x=0");v.zmin=-10;v.zmax=10;
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1&&st.pixels>=W-3);
    compile(0,"z=x^2+y^2");compile(1,"z=1");v.zmin=-3;v.zmax=3;
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1);groups=components(&longest);
    fprintf(stderr,"height circle: %d pixels, %d components, largest %d\n",st.pixels,groups,longest);
    assert(st.pixels>120&&groups==1&&longest==st.pixels);
    /* Maximum XY quality computes one cell per map pixel. This still uses
       a finite independent Z grid and is not a guarantee for all features. */
    compile(0,"x^2+y^2+z^2=4");compile(1,"x=0");v.xy_div=180;v.z_div=16;
    assert(imap_build(s,&vars,&v,image,NULL,NULL,&st)==1);
    groups=components(&longest);assert(st.pixels>115&&groups==1&&longest==st.pixels);
    puts("Map shapes: connected circles, separated branches, pole gap, expanded Z and explicit heights passed.");
}
