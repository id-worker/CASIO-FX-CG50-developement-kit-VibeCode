/* Temporary preview model. Coordinates, surfaces and z-buffer are independent
   of the final RAY result. The scanline approach is adapted from our
   MIT-licensed ObjView project; see its LICENSE and GraphLab's LICENSE. */
#include "mesh_preview.h"
#include "camera_near.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#ifndef GL_HOST
#include <gint/defs/attributes.h>
#define PM_HOT GILRAM
#else
#define PM_HOT
#endif
#define HASH_SIZE 16384
#define EMPTY_INDEX UINT16_MAX
#define PM_RECIP_COUNT 512
/* Fast exact signed division for short raster spans. The table is prepared
   once during model construction. The correction retains exactly the C11
   truncation-toward-zero result, including for negative gradients. */
static uint32_t span_recip[PM_RECIP_COUNT];
static void span_recip_init(void)
{
    if(span_recip[2])return;
    for(int i=2;i<PM_RECIP_COUNT;i++)
        span_recip[i]=(uint32_t)((UINT64_C(1)<<32)/(unsigned)i);
}
static inline int32_t span_divide(int32_t value,int span)
{
    if(span==1)return value;
    if(span<PM_RECIP_COUNT){
        uint32_t n=value<0?0u-(uint32_t)value:(uint32_t)value;
        uint32_t q=(uint32_t)(((uint64_t)n*span_recip[span])>>32);
        if(n-q*(uint32_t)span>=(uint32_t)span)q++;
        return value<0?-(int32_t)q:(int32_t)q;
    }
    return value/span;
}
#ifdef GL_TEST_PREVIEW_MATH
int preview_test_span_divide(int32_t value,int span)
{
    span_recip_init();return span_divide(value,span);
}
#endif

typedef struct {PreviewMesh *mesh;uint16_t *hash;} Builder;
typedef struct {
    int x,y,z;             /* normalized camera-space Q15 */
    int16_t sx,sy;
    uint16_t depth;
    uint8_t flags,pad;
} PMProject;
#define OUT_NEAR 1
#define OUT_LEFT 2
#define OUT_RIGHT 4
#define OUT_TOP 8
#define OUT_BOTTOM 16
#define OUT_GUARD 32

void preview_mesh_free(PreviewMesh *m)
{
    if(!m)return;
    free(m->vertices);free(m->triangles);free(m->projected);free(m->depth);
    memset(m,0,sizeof *m);
}
static unsigned point_hash(Point3 p)
{
    uint32_t h=(uint16_t)p.x*0x9e3779b1u;
    h^=(uint16_t)p.y*0x85ebca6bu;h^=(uint16_t)p.z*0xc2b2ae35u;
    h^=h>>16;return h&(HASH_SIZE-1);
}
static int vertex_index(Builder *b,Point3 p)
{
    PreviewMesh *m=b->mesh;unsigned h=point_hash(p);
    for(unsigned i=0;i<HASH_SIZE;i++,h=(h+1)&(HASH_SIZE-1)){
        uint16_t idx=b->hash[h];
        if(idx==EMPTY_INDEX){
            if(m->vertex_count>=PM_MAX_VERTICES){m->overflow=1;return -1;}
            idx=(uint16_t)m->vertex_count++;m->vertices[idx]=p;b->hash[h]=idx;return idx;
        }
        Point3 q=m->vertices[idx];
        if(p.x==q.x&&p.y==q.y&&p.z==q.z)return idx;
    }
    m->overflow=1;return -1;
}
static uint16_t triangle_color(Triangle3 const *t)
{
    float ax=(float)t->p[1].x-t->p[0].x,ay=(float)t->p[1].y-t->p[0].y,az=(float)t->p[1].z-t->p[0].z;
    float bx=(float)t->p[2].x-t->p[0].x,by=(float)t->p[2].y-t->p[0].y,bz=(float)t->p[2].z-t->p[0].z;
    float nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;
    float len=sqrtf(nx*nx+ny*ny+nz*nz);
    float light=len>0?fabsf((nx*.35f+ny*.45f+nz*.82f)/len):0;
    int bright=(int)(10+19*light);if(bright<8)bright=8;if(bright>31)bright=31;
    if(t->slot==0)return (uint16_t)C_RGB(bright/4,bright*3/4,bright);
    if(t->slot==1)return (uint16_t)C_RGB(bright,bright/3,bright/4);
    return (uint16_t)C_RGB(bright/3,bright,bright/3);
}
static int add_triangle(Triangle3 const *t,void *ctx)
{
    Builder *b=ctx;PreviewMesh *m=b->mesh;
    if(m->triangle_count>=PM_MAX_TRIANGLES){m->overflow=1;return 0;}
    int a=vertex_index(b,t->p[0]),c=vertex_index(b,t->p[1]),d=vertex_index(b,t->p[2]);
    if(a<0||c<0||d<0)return 0;
    if(a==c||c==d||a==d)return 1;
    m->triangles[m->triangle_count++]=(PMTriangle){(uint16_t)a,(uint16_t)c,(uint16_t)d,triangle_color(t)};
    return 1;
}
/* For globally continuous bytecode only, form the preview mesh directly from
   linearly interpolated coarse sample values. Unlike the final RAY, this is
   deliberately approximate. Unproven fields still use plot_surface's pole
   guards and VM refinement below. No interpolation is used for a NaN cell. */
typedef struct {Point3 p;float value;} PMCorner;
static Point3 linear_root(PMCorner a,PMCorner b)
{
    float aa=fabsf(a.value),bb=fabsf(b.value),t;
    if(aa==0)t=0;else if(bb==0)t=1;
    else if(aa>bb)t=1/(1+bb/aa);
    else {float k=aa/bb;t=k/(1+k);}
    return (Point3){(int16_t)lroundf(a.p.x+(b.p.x-a.p.x)*t),
                    (int16_t)lroundf(a.p.y+(b.p.y-a.p.y)*t),
                    (int16_t)lroundf(a.p.z+(b.p.z-a.p.z)*t)};
}
static int linear_tetra(Builder *b,PMCorner const c[4],int slot)
{
    int plus[4],minus[4],np=0,nm=0;
    for(int i=0;i<4;i++){
        if(!isfinite(c[i].value))return 1;
        if(c[i].value<0)minus[nm++]=i;else plus[np++]=i;
    }
    if(!np||!nm)return 1;
    Triangle3 t={.slot=(uint8_t)slot};
    if(np==1||nm==1){
        int k=np==1?plus[0]:minus[0];int *other=np==1?minus:plus;
        for(int j=0;j<3;j++)t.p[j]=linear_root(c[k],c[other[j]]);
        return add_triangle(&t,b);
    }
    Point3 a=linear_root(c[plus[0]],c[minus[0]]),d=linear_root(c[plus[0]],c[minus[1]]);
    Point3 e=linear_root(c[plus[1]],c[minus[0]]),f=linear_root(c[plus[1]],c[minus[1]]);
    t.p[0]=a;t.p[1]=d;t.p[2]=e;if(!add_triangle(&t,b))return 0;
    t.p[0]=d;t.p[1]=f;t.p[2]=e;return add_triangle(&t,b);
}
static int linear_surface(Builder *builder,RaySurface const *surface,EvalVars const *base,
                          Volume const *volume,ProgressFn progress,void *ctx)
{
    int n=volume->div,d=n+1;size_t plane=(size_t)d*d;
    float *rows=malloc(2*plane*sizeof *rows);if(!rows)return -2;
    float *prev=rows,*next=rows+plane,step=2*volume->radius/n;
    EvalVars vars=*base;Program const *pr=surface->program;
    int implicit=pr->has_equal||expr_program_uses(pr,V_Z);
    static const int co[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const int tetra[6][4]={{0,5,1,6},{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6}};
    int16_t norm[49];for(int j=0;j<=n;j++)norm[j]=(int16_t)lroundf(-16000.0f+32000.0f*j/n);
    for(int z=0;z<=n;z++){
        if(progress&&!progress(z,n+1,ctx)){free(rows);return 0;}
        vars.v[V_Z]=volume->z-volume->radius+step*z;
        for(int y=0;y<=n;y++){
            if((y&7)==0&&progress&&!progress(z,n+1,ctx)){free(rows);return 0;}
            vars.v[V_Y]=volume->y-volume->radius+step*y;
            for(int x=0;x<=n;x++){
                size_t at=(size_t)y*d+x;
                if(z&&!implicit&&isfinite(prev[at]))next[at]=prev[at]+step;
                else{
                    vars.v[V_X]=volume->x-volume->radius+step*x;
                    float q=expr_eval_real(pr,&vars);next[at]=isfinite(q)?(implicit?q:vars.v[V_Z]-q):NAN;
                }
            }
        }
        if(z)for(int y=0;y<n;y++){
            if((y&7)==0&&progress&&!progress(z,n+1,ctx)){free(rows);return 0;}
            for(int x=0;x<n;x++){
                size_t at=(size_t)y*d+x;
                float q[8]={prev[at],prev[at+1],prev[at+d+1],prev[at+d],
                            next[at],next[at+1],next[at+d+1],next[at+d]};
                int neg=0,pos=0,valid=1;
                for(int k=0;k<8;k++){
                    if(!isfinite(q[k])){valid=0;break;}
                    if(q[k]<0)neg=1;else pos=1;
                }
                if(!valid||!neg||!pos)continue;
                PMCorner corners[8];
                for(int k=0;k<8;k++)corners[k]=(PMCorner){
                    {norm[x+co[k][0]],norm[y+co[k][1]],norm[z-1+co[k][2]]},q[k]};
                for(int k=0;k<6;k++){
                    PMCorner c[4]={corners[tetra[k][0]],corners[tetra[k][1]],
                                   corners[tetra[k][2]],corners[tetra[k][3]]};
                    if(!linear_tetra(builder,c,surface->slot)){free(rows);return 0;}
                }
            }
        }
        float *swap=prev;prev=next;next=swap;
    }
    free(rows);return 1;
}

int preview_mesh_build(PreviewMesh *m,RaySurface const *surface,int count,EvalVars const *vars,
                       Volume const *volume,ProgressFn progress,void *ctx)
{
    if(!m||!surface||!vars||!volume||count<1||count>2||volume->div<4||!isfinite(volume->radius)||volume->radius<=0)return -1;
    preview_mesh_free(m);
    /* An inequality represents a filled 3D volume, not just its zero
       boundary. The old point-cloud preview is safer for those scenes. */
    for(int i=0;i<count;i++)if(!surface[i].program||surface[i].program->relation>=REL_LT)return -1;
    m->vertices=malloc(PM_MAX_VERTICES*sizeof *m->vertices);
    m->triangles=malloc(PM_MAX_TRIANGLES*sizeof *m->triangles);
    uint16_t *hash=malloc(HASH_SIZE*sizeof *hash);
    if(!m->vertices||!m->triangles||!hash){free(hash);preview_mesh_free(m);return -2;}
    Builder b={m,hash};int result=-1;
    int div=volume->div<20?volume->div:20;
    if(div<4)div=4;
    for(;;){
        memset(hash,0xff,HASH_SIZE*sizeof *hash);
        m->vertex_count=m->triangle_count=m->overflow=0;
        Volume sampled=*volume;sampled.div=div;
        result=1;
        for(int i=0;i<count;i++){
            int rc;
            if(expr_program_continuous(surface[i].program))
                rc=linear_surface(&b,&surface[i],vars,&sampled,progress,ctx);
            else {
                PlotStats stats;
                rc=plot_surface_stream(surface[i].program,vars,&sampled,surface[i].slot,
                                       add_triangle,&b,progress,ctx,&stats);
            }
            if(rc!=1){result=m->overflow?-1:rc;break;}
        }
        if(!m->overflow)break;
        /* Retry the WHOLE cube at lower resolution, not just the first cells. */
        if(div<=6)break;
        div=div>12?div-4:div-2;if(div<4)div=4;
    }
    free(hash);
    if(result!=1||!m->triangle_count){preview_mesh_free(m);return result<=0?result:-1;}
    /* Shrinking is optional: a failed realloc leaves the old block intact. */
    Point3 *v=realloc(m->vertices,(size_t)m->vertex_count*sizeof *v);if(v)m->vertices=v;
    PMTriangle *t=realloc(m->triangles,(size_t)m->triangle_count*sizeof *t);if(t)m->triangles=t;
    m->volume=*volume;m->div=div;m->valid=1;span_recip_init();return 1;
}

/* Adapted from ObjView's integer scanlines. The cached reciprocal depths are
   limited to 0..32766, keeping interpolated signed intermediates in range. */
PM_HOT static void triangle_scan(uint16_t *depth,
        int x0,int y0,int z0,int x1,int y1,int z1,int x2,int y2,int z2,uint16_t color)
{
    int t;
    if(y0>y1){t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;t=z0;z0=z1;z1=t;}
    if(y1>y2){t=x1;x1=x2;x2=t;t=y1;y1=y2;y2=t;t=z1;z1=z2;z2=t;}
    if(y0>y1){t=x0;x0=x1;x1=t;t=y0;y0=y1;y1=t;t=z0;z0=z1;z1=t;}
    if(y2<PLOT_Y0||y0>PLOT_Y1||y0==y2)return;
    int h=y2-y0;
    int32_t dxl=(int32_t)(((int64_t)(x2-x0)*65536)/h);
    int32_t dzl=(int32_t)(((int64_t)(z2-z0)*32768)/h);
    for(int part=0;part<2;part++){
        int ys=part?y1:y0,ye=part?y2:y1,sh=ye-ys;
        if(sh<=0)continue;
        int start=ys<PLOT_Y0?PLOT_Y0:ys;
        int end=part?ye:(y1<y2?y1-1:y1);if(end>PLOT_Y1)end=PLOT_Y1;
        if(start>end)continue;
        int xb=part?x1:x0,zb=part?z1:z0,xt=part?x2:x1,zt=part?z2:z1;
        int32_t dxs=(int32_t)(((int64_t)(xt-xb)*65536)/sh);
        int32_t dzs=(int32_t)(((int64_t)(zt-zb)*32768)/sh);
        int64_t xl=(int64_t)x0*65536+(int64_t)dxl*(start-y0);
        int64_t zl=(int64_t)z0*32768+(int64_t)dzl*(start-y0);
        int64_t xs=(int64_t)xb*65536+(int64_t)dxs*(start-ys);
        int64_t zs=(int64_t)zb*32768+(int64_t)dzs*(start-ys);
        for(int y=start;y<=end;y++){
            int a=(int)(xl>>16),b=(int)(xs>>16);int32_t da=(int32_t)zl,db=(int32_t)zs;
            xl+=dxl;xs+=dxs;zl+=dzl;zs+=dzs;
            if(a>b){t=a;a=b;b=t;int32_t dt=da;da=db;db=dt;}
            if(b<PLOT_X0||a>PLOT_X1)continue;
            int span=b-a;int32_t dz=span?span_divide(db-da,span):0;
            if(a<PLOT_X0){da+=dz*(PLOT_X0-a);a=PLOT_X0;}
            if(b>PLOT_X1)b=PLOT_X1;
            uint16_t *zrow=depth+(size_t)(y-PLOT_Y0)*PLOT_W;
            uint16_t *vrow=gint_vram+(size_t)y*DWIDTH;
            for(int x=a;x<=b;x++,da+=dz){
                int value=(int)(da>>15);if(value<1)value=1;if(value>32766)value=32766;
                int index=x-PLOT_X0;
                if(value>=zrow[index]){zrow[index]=(uint16_t)value;vrow[x]=color;}
            }
        }
    }
}
typedef struct {int x,y,z;} PMEye;
static int64_t plane_value(PMEye p,int plane,int focal,int nearq)
{
    /* These describe the actual plot rectangle in camera space. */
    int cx=(PLOT_X0+PLOT_X1)/2,cy=(PLOT_Y0+PLOT_Y1)/2;
    if(plane==0)return (int64_t)p.z-nearq;
    if(plane==1)return (int64_t)focal*p.x+(int64_t)(cx-PLOT_X0)*p.z;
    if(plane==2)return (int64_t)(PLOT_X1-cx)*p.z-(int64_t)focal*p.x;
    if(plane==3)return (int64_t)(cy-PLOT_Y0)*p.z-(int64_t)focal*p.y;
    return (int64_t)(PLOT_Y1-cy)*p.z+(int64_t)focal*p.y;
}
static int clip_triangle(PMEye const *in,PMEye *out,int focal,int nearq)
{
    PMEye a[12],b[12];memcpy(a,in,3*sizeof *a);int n=3;
    for(int plane=0;plane<5;plane++){
        if(n<3)return 0;
        int count=0;PMEye prev=a[n-1];int64_t pv=plane_value(prev,plane,focal,nearq);
        for(int i=0;i<n;i++){
            PMEye cur=a[i];int64_t cv=plane_value(cur,plane,focal,nearq);
            if((pv<0)!=(cv<0)){
                int64_t den=pv-cv;if(!den||count>=11)return 0;
                PMEye v={prev.x+(int)((int64_t)(cur.x-prev.x)*pv/den),
                         prev.y+(int)((int64_t)(cur.y-prev.y)*pv/den),
                         prev.z+(int)((int64_t)(cur.z-prev.z)*pv/den)};
                if(plane==0)v.z=nearq;
                b[count++]=v;
            }
            if(cv>=0){if(count>=11)return 0;b[count++]=cur;}
            prev=cur;pv=cv;
        }
        n=count;memcpy(a,b,(size_t)n*sizeof *a);
    }
    memcpy(out,a,(size_t)n*sizeof *out);return n;
}
static PMProject project_one(PMEye p,int focal,int nearq,int reference)
{
    PMProject out={.x=p.x,.y=p.y,.z=p.z};
    if(p.z<nearq){out.flags=OUT_NEAR;return out;}
    int sx=(PLOT_X0+PLOT_X1)/2+(int)((int64_t)focal*p.x/p.z);
    int sy=(PLOT_Y0+PLOT_Y1)/2-(int)((int64_t)focal*p.y/p.z);
    if(sx<PLOT_X0)out.flags|=OUT_LEFT;
    if(sx>PLOT_X1)out.flags|=OUT_RIGHT;
    if(sy<PLOT_Y0)out.flags|=OUT_TOP;
    if(sy>PLOT_Y1)out.flags|=OUT_BOTTOM;
    if(sx< -2048||sx>2048||sy< -2048||sy>2048)out.flags|=OUT_GUARD;
    if(sx< -2048)sx=-2048;
    if(sx>2048)sx=2048;
    if(sy< -2048)sy=-2048;
    if(sy>2048)sy=2048;
    out.sx=(int16_t)sx;out.sy=(int16_t)sy;
    int64_t d=(int64_t)reference*32766/p.z;
    if(d<1)d=1;
    if(d>32766)d=32766;
    out.depth=(uint16_t)d;return out;
}
int preview_mesh_draw(PreviewMesh *m,Camera const *cam)
{
    if(!m||!m->valid||!cam||!isfinite(cam->yaw)||!isfinite(cam->pitch)||!isfinite(cam->zoom)||
       cam->zoom<.1f||cam->zoom>5||!isfinite(cam->eye_x)||!isfinite(cam->eye_y)||!isfinite(cam->eye_z))return 0;
    span_recip_init();
    Volume const *v=&m->volume;float ex=(cam->eye_x-v->x)/v->radius,ey=(cam->eye_y-v->y)/v->radius,ez=(cam->eye_z-v->z)/v->radius;
    /* Out-of-range coordinates fall back to the preexisting float cloud. */
    if(!isfinite(ex)||!isfinite(ey)||!isfinite(ez)||fabsf(ex)>4||fabsf(ey)>4||fabsf(ez)>4)return 0;
    if(!m->projected)m->projected=malloc((size_t)m->vertex_count*sizeof(PMProject));
    if(!m->depth)m->depth=malloc((size_t)PLOT_W*PLOT_H*sizeof(uint16_t));
    if(!m->projected||!m->depth){free(m->projected);m->projected=NULL;free(m->depth);m->depth=NULL;return 0;}
    memset(m->depth,0,(size_t)PLOT_W*PLOT_H*sizeof(uint16_t));
    PMProject *pr=m->projected;
    float cy=cosf(cam->yaw),sy=sinf(cam->yaw),cp=cosf(cam->pitch),sp=sinf(cam->pitch);
    float matrix[3][3]={{cy,-sy,0},{-sp*sy,-sp*cy,cp},{-cp*sy,-cp*cy,-sp}};
    int coeff[3][3],offset[3];
    for(int i=0;i<3;i++){
        for(int j=0;j<3;j++)coeff[i][j]=(int)lroundf(matrix[i][j]*16384);
        offset[i]=(int)lroundf(-(matrix[i][0]*ex+matrix[i][1]*ey+matrix[i][2]*ez)*16000);
    }
    int focal=(int)lroundf(72*2.8f*cam->zoom);
    int nearq=(int)ceilf(camera_near_normalized(v->radius)*16000);if(nearq<1)nearq=1;
    /* Avoid a single very-close vertex wasting all the z-buffer precision. */
    float center=cp*(sy*ex+cy*ey)+sp*ez;
    int reference=(int)lroundf(fmaxf(.025f,center-2.0f)*16000);
    if(reference<nearq)reference=nearq;
    for(int i=0;i<m->vertex_count;i++){
        Point3 p=m->vertices[i];int q[3];
        for(int j=0;j<3;j++)q[j]=(p.x*coeff[j][0]+p.y*coeff[j][1]+p.z*coeff[j][2])/16384+offset[j];
        pr[i]=project_one((PMEye){q[0],q[1],q[2]},focal,nearq,reference);
    }
    for(int i=0;i<m->triangle_count;i++){
        PMTriangle const *t=&m->triangles[i];
        PMProject const *a=&pr[t->a],*b=&pr[t->b],*c=&pr[t->c];
        unsigned both=a->flags&b->flags&c->flags;
        if(both&(OUT_NEAR|OUT_LEFT|OUT_RIGHT|OUT_TOP|OUT_BOTTOM))continue;
        unsigned any=a->flags|b->flags|c->flags;
        if(!(any&(OUT_NEAR|OUT_GUARD))){
            triangle_scan(m->depth,a->sx,a->sy,a->depth,b->sx,b->sy,b->depth,c->sx,c->sy,c->depth,t->color);
        }else{
            PMEye in[3]={{a->x,a->y,a->z},{b->x,b->y,b->z},{c->x,c->y,c->z}},out[12];
            int n=clip_triangle(in,out,focal,nearq);if(n<3)continue;
            PMProject first=project_one(out[0],focal,nearq,reference);
            for(int j=1;j+1<n;j++){
                PMProject u=project_one(out[j],focal,nearq,reference),w=project_one(out[j+1],focal,nearq,reference);
                if((first.flags|u.flags|w.flags)&(OUT_NEAR|OUT_GUARD))continue;
                triangle_scan(m->depth,first.sx,first.sy,first.depth,u.sx,u.sy,u.depth,w.sx,w.sy,w.depth,t->color);
            }
        }
    }
    return 1;
}
