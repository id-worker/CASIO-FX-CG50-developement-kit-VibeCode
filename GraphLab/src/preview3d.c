/* Reprojection of sparse visible points; no equation evaluation or mesh. */
#include "preview3d.h"
#include "camera_near.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#define ZW ((PLOT_W+1)/2)
#define ZH ((PLOT_H+1)/2)

void preview3d_set_step(Preview3D *p,int step)
{
    if(!p)return;
    int next=(step==2||step==4||step==8)?step:PREVIEW3D_STEP;
    if(p->point&&p->step!=next)preview3d_free(p);
    p->step=next;
}
int preview3d_begin(Preview3D *p,Volume const *v,Camera const *c)
{
    if(!p||!v||!c||!isfinite(v->radius)||v->radius<=0||v->div<1||
       !isfinite(v->x)||!isfinite(v->y)||!isfinite(v->z)||
       !isfinite(c->eye_x)||!isfinite(c->eye_y)||!isfinite(c->eye_z)||
       !isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->zoom)||c->zoom<.1f||c->zoom>5)return 0;
    int step=(p->step==2||p->step==4||p->step==8)?p->step:PREVIEW3D_STEP;
    preview3d_free(p);p->volume=*v;p->captured=*c;p->step=step;
    p->capacity=((PLOT_W+step-1)/step)*((PLOT_H+step-1)/step);
    p->point=malloc((size_t)p->capacity*sizeof *p->point);
    if(!p->point){p->capacity=0;return 0;}
    return 1;
}
int preview3d_recapture(Preview3D *p,Volume const *v,Camera const *c,int retain_aux)
{
    if(!p||!v||!c)return 0;
    Volume const *old=&p->volume;
    if(!retain_aux||!p->point||old->x!=v->x||old->y!=v->y||old->z!=v->z||
       old->radius!=v->radius||old->div!=v->div||old->period_x!=v->period_x||
       !isfinite(c->eye_x)||!isfinite(c->eye_y)||!isfinite(c->eye_z)||
       !isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->zoom)||c->zoom<.1f||c->zoom>5)
        return preview3d_begin(p,v,c);
    p->count=0;p->captured=*c;free(p->depth);p->depth=NULL;
    return 1;
}
void preview3d_free(Preview3D *p)
{
    if(p){free(p->point);free(p->aux);free(p->depth);memset(p,0,sizeof *p);}
}
int preview3d_aux_begin(Preview3D *p)
{
    if(!p||!p->point)return 0;
    if(!p->aux)p->aux=malloc(PREVIEW3D_AUX_CAPACITY*sizeof *p->aux);
    p->aux_count=p->aux_seen=0;return p->aux!=NULL;
}
static int16_t quant(float v)
{
    if(v<-1)v=-1;
    if(v>1)v=1;
    return (int16_t)lroundf(v*32767.0f);
}
void preview3d_aux_add(Preview3D *p,float x,float y,float z,uint16_t color)
{
    if(!p||!p->aux||!isfinite(x)||!isfinite(y)||!isfinite(z)||
       fabsf(x)>1.0001f||fabsf(y)>1.0001f||fabsf(z)>1.0001f)return;
    uint32_t n=(uint32_t)++p->aux_seen;int pos;
    if(p->aux_count<PREVIEW3D_AUX_CAPACITY)pos=p->aux_count++;
    else {
        /* A deterministic bounded reservoir keeps points from the entire
           cube, rather than just the first scan planes when Q is high. */
        uint32_t h=n+0x9e3779b9u;h^=h>>16;h*=0x7feb352du;h^=h>>15;
        h*=0x846ca68bu;h^=h>>16;pos=(int)(h%n);
        if(pos>=PREVIEW3D_AUX_CAPACITY)return;
    }
    p->aux[pos]=(Preview3DPoint){quant(x),quant(y),quant(z),color};
}
void preview3d_add(Preview3D *p,int x,int y,float wx,float wy,float wz,uint16_t color)
{
    if(!p||!p->point||p->count>=p->capacity||x<PLOT_X0||x>PLOT_X1||y<PLOT_Y0||y>PLOT_Y1||
       ((x-PLOT_X0)&(p->step-1))||((y-PLOT_Y0)&(p->step-1))||
       !isfinite(wx)||!isfinite(wy)||!isfinite(wz))return;
    Volume const *v=&p->volume;
    float a=(wx-v->x)/v->radius,b=(wy-v->y)/v->radius,c=(wz-v->z)/v->radius;
    /* Allow only tiny clipping/rounding errors: do not invent off-cube points. */
    if(!isfinite(a)||!isfinite(b)||!isfinite(c)||
       fabsf(a)>1.0001f||fabsf(b)>1.0001f||fabsf(c)>1.0001f)return;
    p->point[p->count++]=(Preview3DPoint){quant(a),quant(b),quant(c),color};
}
static int floor2(int a){int q=a/2;return a<0&&(a%2)?q-1:q;}
/* Fixed-point projection for ordinary camera ranges. It still uses precisely
   the saved points and a depth buffer: no equations and no polygons. Keeping
   15-bit input coordinates and 14-bit rotation coefficients bounds every
   per-point product to signed 32-bit on SH. Unusual cameras use the float
   reference path below. */
#ifndef GL_PREVIEW_FORCE_FLOAT
static int round_ratio(int n,int d)
{
    return n>=0?(n+d/2)/d:-((-n+d/2)/d);
}
static int fixed_preview(Preview3D const *p,Camera const *c,uint16_t *depth)
{
    Volume const *v=&p->volume;float r=v->radius;
    float ex=(c->eye_x-v->x)/r,ey=(c->eye_y-v->y)/r,ez=(c->eye_z-v->z)/r;
    float ox=(p->captured.eye_x-v->x)/r,oy=(p->captured.eye_y-v->y)/r,oz=(p->captured.eye_z-v->z)/r;
    float ratio=c->zoom/p->captured.zoom;
    if(!isfinite(ex)||!isfinite(ey)||!isfinite(ez)||!isfinite(ox)||!isfinite(oy)||!isfinite(oz)||
       fabsf(ex)>4||fabsf(ey)>4||fabsf(ez)>4||fabsf(ox)>4||fabsf(oy)>4||fabsf(oz)>4||
       !isfinite(ratio)||ratio<.25f||ratio>4)return 0;
    float cy=cosf(c->yaw),sy=sinf(c->yaw),cp=cosf(c->pitch),sp=sinf(c->pitch);
    float oc=cosf(p->captured.yaw),os=sinf(p->captured.yaw);
    float op=cosf(p->captured.pitch),ot=sinf(p->captured.pitch);
    float m[4][3]={{cy,-sy,0},{-sp*sy,-sp*cy,cp},{-cp*sy,-cp*cy,-sp},
                   {-op*os,-op*oc,-ot}};
    int coeff[4][3],offset[4];
    for(int a=0;a<4;a++){
        for(int b=0;b<3;b++)coeff[a][b]=(int)lroundf(m[a][b]*16384.0f);
        float xx=a==3?ox:ex,yy=a==3?oy:ey,zz=a==3?oz:ez;
        offset[a]=(int)lroundf(-(m[a][0]*xx+m[a][1]*yy+m[a][2]*zz)*32767.0f);
    }
    int focal=(int)lroundf(72.0f*2.8f*c->zoom);
    float nf=camera_near_normalized(r)*32767.0f;
    if(!isfinite(nf)||nf>400000||nf<0)return 0;
    int nearq=(int)ceilf(nf);if(nearq<1)nearq=1;
    int scale=(int)lroundf(p->step*ratio*256.0f);
    for(int i=0;i<ZW*ZH;i++)depth[i]=65535;
    for(int i=0;i<p->count+p->aux_count;i++){
        int aux=i>=p->count;
        Preview3DPoint const *q=aux?&p->aux[i-p->count]:&p->point[i];
        int x[4]={0,0,0,0};
        for(int j=0;j<(aux?3:4);j++)
            x[j]=(q->x*coeff[j][0]+q->y*coeff[j][1]+q->z*coeff[j][2])/16384+offset[j];
        int zz=x[2];if(zz<nearq)continue;
        /* Close to the camera, subpixel fixed-point error can be amplified.
           If such a point might be on screen, restart with float projection. */
        if(zz<64){
            if(abs(x[0])*focal<=(PLOT_W/2+16)*zz&&abs(x[1])*focal<=(PLOT_H/2+16)*zz)return 0;
            continue;
        }
        int den=zz*2;
        int px=round_ratio((PLOT_W-1)*zz+2*focal*x[0],den);
        int py=round_ratio((PLOT_H-1)*zz-2*focal*x[1],den);
        if(px<-12||px>PLOT_W+11||py<-12||py>PLOT_H+11)continue;
        int spread=aux?3:(x[3]>0?round_ratio(x[3]*scale,zz*256):2);
        if(spread<2)spread=2;
        if(spread>12)spread=12;
        int x0=floor2(px-spread/2),x1=floor2(px+(spread-1)/2);
        int y0=floor2(py-spread/2),y1=floor2(py+(spread-1)/2);
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>=ZW)x1=ZW-1;
        if(y1>=ZH)y1=ZH-1;
        int dk=1+(zz-nearq)/16;if(dk>32766)dk=32766;
        /* Visible samples take precedence over the coarser auxiliary ones;
           auxiliary samples still depth-test correctly against each other. */
        uint16_t key=(uint16_t)(dk+(aux?32768:0));
        for(int by=y0;by<=y1;by++)for(int bx=x0;bx<=x1;bx++){
            size_t di=(size_t)by*ZW+bx;
            if(key>depth[di])continue;
            depth[di]=key;
            int xx0=PLOT_X0+2*bx,yy0=PLOT_Y0+2*by;
            for(int j=0;j<2&&yy0+j<=PLOT_Y1;j++)for(int k=0;k<2&&xx0+k<=PLOT_X1;k++)
                gint_vram[(size_t)(yy0+j)*DWIDTH+xx0+k]=q->color;
        }
    }
    return 1;
}
#endif
int preview3d_draw(Preview3D *p,Camera const *c)
{
    if(!p||!p->point||!c||!isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->zoom)||
       c->zoom<.1f||c->zoom>5||!isfinite(c->eye_x)||!isfinite(c->eye_y)||!isfinite(c->eye_z))return -1;
    if(!p->depth){p->depth=malloc((size_t)ZW*ZH*sizeof *p->depth);if(!p->depth)return -2;}
    uint16_t *depth=p->depth;
#ifndef GL_PREVIEW_FORCE_FLOAT
    if(fixed_preview(p,c,depth))return 1;
    /* The fixed path can reject a point after drawing others. Its caller
       supplies a cleared framebuffer, so erase only the plot before retrying. */
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++)
        gint_vram[(size_t)y*DWIDTH+x]=C_WHITE;
#endif
    Volume const *v=&p->volume;float r=v->radius;
    float ex=(c->eye_x-v->x)/r,ey=(c->eye_y-v->y)/r,ez=(c->eye_z-v->z)/r;
    float ox=(p->captured.eye_x-v->x)/r,oy=(p->captured.eye_y-v->y)/r,oz=(p->captured.eye_z-v->z)/r;
    if(!isfinite(ex)||!isfinite(ey)||!isfinite(ez)||!isfinite(ox)||!isfinite(oy)||!isfinite(oz))return -1;
    float cy=cosf(c->yaw),sy=sinf(c->yaw),cp=cosf(c->pitch),sp=sinf(c->pitch);
    float ocy=cosf(p->captured.yaw),osy=sinf(p->captured.yaw);
    float ocp=cosf(p->captured.pitch),osp=sinf(p->captured.pitch);
    float center_depth=cp*(sy*ex+cy*ey)+sp*ez,near_clip=camera_near_normalized(r);
    if(!isfinite(center_depth))return -1;
    /* Map reciprocal depth into 16-bit cells. The points were originally
       visible; a new view may expose missing hidden parts, hence PREVIEW. */
    float near=fmaxf(near_clip,fminf(.04f,2.0f/v->div/32.0f));
    near=fmaxf(near,center_depth-2);float far=fmaxf(near+.001f,center_depth+2);
    float inv_near=1/near,span=inv_near-1/far;
    if(!isfinite(span)||span<=0)return -1;
    float depth_scale=65532.0f/span,focal=72.0f*2.8f*c->zoom;
    for(int i=0;i<ZW*ZH;i++)depth[i]=65535;
    for(int i=0;i<p->count+p->aux_count;i++){
        int aux=i>=p->count;
        Preview3DPoint const *q=aux?&p->aux[i-p->count]:&p->point[i];
        float x=q->x/32767.0f,y=q->y/32767.0f,z=q->z/32767.0f;
        float dx=x-ex,dy=y-ey,dz=z-ez,ry=sy*dx+cy*dy;
        float xx=cy*dx-sy*dy,yy=cp*dz-sp*ry,zz=-cp*ry-sp*dz;
        if(!isfinite(zz)||zz<near_clip)continue;
        float sx=(PLOT_X0+PLOT_X1)*.5f+focal*xx/zz;
        float sy2=(PLOT_Y0+PLOT_Y1)*.5f-focal*yy/zz;
        if(!isfinite(sx)||!isfinite(sy2)||sx<PLOT_X0-12||sx>PLOT_X1+12||sy2<PLOT_Y0-12||sy2>PLOT_Y1+12)continue;
        float odx=x-ox,ody=y-oy,odz=z-oz,ory=osy*odx+ocy*ody;
        float oldz=-ocp*ory-osp*odz;
        float size=aux?3.0f:p->step*(oldz/zz)*(c->zoom/p->captured.zoom);
        if(!isfinite(size))continue;
        if(size<2)size=2;
        if(size>12)size=12;
        int spread=(int)lroundf(size),px=(int)lroundf(sx)-PLOT_X0,py=(int)lroundf(sy2)-PLOT_Y0;
        int x0=floor2(px-spread/2),x1=floor2(px+(spread-1)/2);
        int y0=floor2(py-spread/2),y1=floor2(py+(spread-1)/2);
        if(x0<0)x0=0;
        if(y0<0)y0=0;
        if(x1>=ZW)x1=ZW-1;
        if(y1>=ZH)y1=ZH-1;
        float zf=(inv_near-1/zz)*depth_scale+1;if(!isfinite(zf))continue;
        if(zf<1)zf=1;
        if(zf>65532)zf=65532;
        uint16_t key=(uint16_t)((int)lroundf(zf*.5f)+(aux?32768:0));
        for(int by=y0;by<=y1;by++)for(int bx=x0;bx<=x1;bx++){
            size_t di=(size_t)by*ZW+bx;
            if(key>depth[di])continue;
            depth[di]=key;
            int xx0=PLOT_X0+2*bx,yy0=PLOT_Y0+2*by;
            for(int j=0;j<2&&yy0+j<=PLOT_Y1;j++)for(int k=0;k<2&&xx0+k<=PLOT_X1;k++)
                gint_vram[(size_t)(yy0+j)*DWIDTH+xx0+k]=q->color;
        }
    }
    return 1;
}
