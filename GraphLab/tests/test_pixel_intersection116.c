#include "../src/intersection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
static Parsed parsed;static Program p[2];static EvalVars base;
static void make(int i,const char*s){assert(expr_parse(&parsed,s,0));assert(expr_compile(&parsed,&p[i],NULL,0));}
static int ix_of(IMapView const*v,float x){return (int)lroundf((x-v->center_x+v->radius)/(2*v->radius)*(IMAP_SIDE-1));}
static int iy_of(IMapView const*v,float y){return (int)lroundf((v->center_y+v->radius-y)/(2*v->radius)*(IMAP_SIDE-1));}
static float e(int k,IMapPoint p0){EvalVars v=base;v.v[V_X]=p0.x;v.v[V_Y]=p0.y;v.v[V_Z]=p0.z;return expr_eval_real(&p[k],&v);}
static int cancel_pixel(int done,int total,void *ctx){(void)total;return done<*(int*)ctx;}
static void verify(IMapPixelHits *h){for(int i=0;i<h->count;i++){
  float f=e(0,h->point[i]),g=e(1,h->point[i]);
  printf("  point (%.9g %.9g %.9g) residual %.9g %.9g\n",(double)h->point[i].x,(double)h->point[i].y,(double)h->point[i].z,(double)f,(double)g);
  assert(isfinite(f)&&isfinite(g)&&fabsf(f)<.0001f&&fabsf(g)<.0001f);
}}
int main(void){
 RaySurface s[2]={{&p[0],0},{&p[1],1}};IMapStats st;IMapPoint near;IMapPixelHits h;IMapRoots r;
 IMapView av={.radius=10};assert(imap_auto_view(&av,10));
 assert(av.xy_div==180&&av.z_div==180&&av.cursor_steps==180&&av.zmin==-10&&av.zmax==10);
 av.radius=5;assert(imap_auto_view(&av,10)&&av.z_div==360&&av.cursor_steps==360);
 av.radius=3;assert(imap_auto_view_bounds(&av,-8,10)&&av.z_div==540&&av.zmin==-8&&av.zmax==10);
 av.radius=1.5f;assert(imap_auto_view_bounds(&av,-8,10)&&av.z_div==1080);
 av.radius=10000;assert(imap_auto_view(&av,.1f)&&av.z_div==1&&av.cursor_steps==8);
 av.radius=.001f;assert(!imap_auto_view(&av,10000)); /* no silent precision downgrade */
 IMapView v={.radius=10,.zmin=-3,.zmax=3,.xy_div=45,.z_div=32,.cursor_steps=64};
 IMapPixel *map=malloc(IMAP_SIDE*IMAP_SIDE*sizeof *map);assert(map);
 make(0,"sin(x)+sin(y)+sin(z)=0");make(1,"z=x-y");
 assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1);
 assert(imap_nearest(map,&v,-3.4f,-1.1f,&near));
 int ix=ix_of(&v,near.x),iy=iy_of(&v,near.y);
 assert(imap_search(s,&base,&v,near.x,near.y,NULL,NULL,&r)==1&&r.count==0);
 assert(imap_pixel_search(s,&base,&v,ix,iy,map,NULL,NULL,&h)==1);
 printf("NEAR pixel (%d %d): %d, overflow=%d\n",ix,iy,h.count,h.overflow);
 verify(&h);assert(h.count>0);
 assert(imap_search(s,&base,&v,6.28293562f,3.90293574f,NULL,NULL,&r)==1&&r.count==0);
 assert(imap_search(s,&base,&v,0,1.2f,NULL,NULL,&r)==1&&r.count==1);
 v=(IMapView){.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=64,.cursor_steps=128};
 make(0,"x^2+y^2+z^2=4");make(1,"x=0");
 assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1);
 assert(imap_pixel_search(s,&base,&v,90,90,map,NULL,NULL,&h)==1);
 printf("sphere/vertical plane pixel (90,90): %d overflow=%d\n",h.count,h.overflow);verify(&h);
 int neg=0,pos=0;for(int i=0;i<h.count;i++){if(h.point[i].z< -1.99f)neg++;if(h.point[i].z>1.99f)pos++;}
 assert(neg>0&&pos>0);
 make(0,"z=1");make(1,"z=-1");
 assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1 && st.pixels==0);
 assert(imap_pixel_search(s,&base,&v,90,90,map,NULL,NULL,&h)==1&&h.count==0);
 make(0,"z=0.123");make(1,"x=0");
 assert(imap_build(s,&base,&v,map,NULL,NULL,&st)==1&&st.pixels>100);
 assert(imap_pixel_search(s,&base,&v,90,90,map,NULL,NULL,&h)==1&&h.count>0);
 printf("between slices plane (90,90): %d\n",h.count);verify(&h);
 /* Two genuine nearby roots are not merged just because they are close. */
 v=(IMapView){.radius=3,.zmin=0,.zmax=1,.xy_div=180,.z_div=180,.cursor_steps=32768};
 make(0,"(z-0.1000)*(z-0.1005)=0");make(1,"x=0");
 assert(imap_search(s,&base,&v,0,0,NULL,NULL,&r)==1&&r.count==2);
 assert(fabsf(r.z[0]-.1f)<.00001f&&fabsf(r.z[1]-.1005f)<.00001f);
 /* Degenerate coincident planes and a vertical line have many valid values. */
 v=(IMapView){.radius=3,.zmin=-3,.zmax=3,.xy_div=32,.z_div=32,.cursor_steps=64};
 make(0,"z=0");make(1,"z=0");
 assert(imap_pixel_search(s,&base,&v,90,90,NULL,NULL,NULL,&h)==1&&h.count>0);
 verify(&h);
 make(0,"x=0");make(1,"y=0");
 assert(imap_pixel_search(s,&base,&v,90,90,NULL,NULL,NULL,&h)==1&&h.count>=3&&h.overflow);
 verify(&h);
 /* Invalid positions, undefined domains and cancellation do not claim hits. */
 assert(imap_pixel_search(s,&base,&v,-1,90,NULL,NULL,NULL,&h)==-1);
 assert(imap_pixel_search(s,&base,&v,90,181,NULL,NULL,NULL,&h)==-1);
 int lim=0;assert(imap_pixel_search(s,&base,&v,90,90,NULL,cancel_pixel,&lim,&h)==0);
 make(0,"1/(z-0.123)=0");make(1,"x=0");
 assert(imap_pixel_search(s,&base,&v,90,90,NULL,NULL,NULL,&h)==1&&h.count==0);
 free(map);puts("pixel search experiment passed");
}
