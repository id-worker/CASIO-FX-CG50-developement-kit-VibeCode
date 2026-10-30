#include "../src/intersection.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>
static Parsed parsed;static Program p[2];static EvalVars vars;static IMapPixel map[IMAP_SIDE*IMAP_SIDE];
static void compile(int k,char const *text){assert(expr_parse(&parsed,text,0)&&expr_compile(&parsed,&p[k],NULL,0));}
int main(void)
{
    RaySurface s[2]={{&p[0],0},{&p[1],1}};IMapView v={.radius=3,.zmin=-6,.zmax=6,.xy_div=24,.z_div=24,.cursor_steps=64};
    IMapRoots roots;IMapStats stats;IMapPoint pt;
    for(int i=0;i<25;i++){
        float xc=(i%5-2)*.25f,yy=((i*7)%17-8)*.2f;
        int ax=i%5-2,by=(i*3)%5-2,c=(i*2)%5-2;char f[100],g[100];
        snprintf(f,sizeof f,"x=%.3f",(double)xc);
        snprintf(g,sizeof g,"z=(%d)*x+(%d)*y+(%d)",ax,by,c);
        compile(0,f);compile(1,g);
        float expected=ax*xc+by*yy+c;
        assert(imap_search(s,&vars,&v,xc,yy,NULL,NULL,&roots)==1&&roots.count==1&&fabsf(roots.z[0]-expected)<.002f);
        assert(imap_search(s,&vars,&v,xc+.1f,yy,NULL,NULL,&roots)==1&&roots.count==0);
        assert(imap_build(s,&vars,&v,map,NULL,NULL,&stats)==1&&stats.pixels>20);
        assert(imap_nearest(map,&v,xc+.1f,yy,&pt)&&fabsf(pt.x-xc)<.02f);
        assert(fabsf(pt.z-(ax*pt.x+by*pt.y+c))<.05f);
    }
    compile(0,"x=0");compile(1,"y=0");
    assert(imap_search(s,&vars,&v,0,0,NULL,NULL,&roots)==1&&roots.interval);
    assert(imap_build(s,&vars,&v,map,NULL,NULL,&stats)==1&&stats.pixels>=1);
    puts("Intersection: 25 different affine surface pairs, exact cursor checks and vertical line passed.");
}
