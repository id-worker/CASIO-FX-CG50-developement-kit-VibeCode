#include "plot.h"
#include "camera_near.h"
#include "plot2d.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#define GL_NAN (0.0f/0.0f)
#define GRID_POINTS(n) ((size_t)((n)+1)*((n)+1)*((n)+1))

typedef struct {float x,y,z,v;} Sample;
typedef struct {Program const *pr;EvalVars vars;int implicit,evaluations;} Field;
static float sample_field(Field *f,float x,float y,float z)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    f->evaluations++;
    float v=expr_eval_real(f->pr,&f->vars);
    if(!isfinite(v))return GL_NAN;
    return f->implicit?v:z-v;
}
static float sample_2d(Field *f,float x,float y)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=0;
    f->evaluations++;
    float v=expr_eval_real(f->pr,&f->vars);
    if(!isfinite(v))return GL_NAN;
    return f->implicit?v:y-v;
}
static int crosses(float a,float b)
{
    return isfinite(a) && isfinite(b) && ((a<0 && b>=0) || (b<0 && a>=0));
}
/* Reject many false crossings at poles such as 1/x=0. No finite sampling
 * method can guarantee detection of every discontinuity or tangency. */
static int root_on_edge_pixels(Field *f,Sample a,Sample b,int dims,Sample *r,
                               float edge_pixels,float tolerance_px)
{
    if(!crosses(a.v,b.v))return 0;
    if(a.v==0) {*r=a;return 1;}if(b.v==0){*r=b;return 1;}
    float lo=0,hi=1,vl=a.v,vh=b.v;
    float best=INFINITY,best_t=0.5f;
    float scale=fmaxf(fabsf(a.v),fabsf(b.v));
    float small=fminf(fabsf(a.v),fabsf(b.v));
    if(!isfinite(scale) || scale<=0)return 0;
    for(int it=0;it<6;it++) {
        float t;
        if(it==0) {
            float den=fabsf(vl)+fabsf(vh);
            t=den>0 && isfinite(den) ? fabsf(vl)/den : 0.5f;
        } else t=(lo+hi)*0.5f;
        float x=a.x+(b.x-a.x)*t,y=a.y+(b.y-a.y)*t,z=a.z+(b.z-a.z)*t;
        float v=dims==2?sample_2d(f,x,y):sample_field(f,x,y,z);
        if(!isfinite(v))return 0;
        if(fabsf(v)<best) {best=fabsf(v);best_t=t;}
        if(v==0 || fabsf(v)<scale*0.00001f)break;
        if((v<0)==(vl<0)) {lo=t;vl=v;}else{hi=t;vh=v;}
        /* Once the remaining bracket is below the configured screen-space
           tolerance, more VM evaluations cannot reliably change many pixels. */
        if(tolerance_px>0 && edge_pixels>0 && (hi-lo)*edge_pixels<=tolerance_px &&
           best<=small*.45f+scale*.0001f)break;
    }
    if(best > small*0.45f + scale*0.0001f) return 0;
    r->x=a.x+(b.x-a.x)*best_t;r->y=a.y+(b.y-a.y)*best_t;r->z=a.z+(b.z-a.z)*best_t;r->v=0;
    return 1;
}
static int root_on_edge(Field *f,Sample a,Sample b,int dims,Sample *r)
{
    return root_on_edge_pixels(f,a,b,dims,r,0,0);
}
/* Only reuse declared periods when exactly aligned to the sample grid.
   A period of zero disables user-declared reuse. A proven x-independent
   program is safely reused regardless of the user setting. */
static int x_period_steps(Program const *pr,float period,float dx,int n)
{
    if(!expr_program_uses(pr,V_X))return 1;
    if(!isfinite(period) || period<=0 || !isfinite(dx) || dx<=0)return 0;
    float steps=period/dx;if(steps<1 || steps>(float)n)return 0;
    int count=(int)lroundf(steps);
    return fabsf(steps-count)<=0.000001f?count:0;
}
int plot_contour_opt(Program const *pr, EvalVars const *base,PlaneView const *view,
                     SegmentFn segment,void *segment_ctx,ContourOptions const *options,
                     ProgressFn progress,void *progress_ctx,PlotStats *stats)
{
    if(!pr || !base || !view || !segment || !stats || view->cols<1 || view->cols>400 || view->rows<1 || view->rows>250 ||
       !isfinite(view->xmin) || !isfinite(view->xmax) || !isfinite(view->ymin) || !isfinite(view->ymax) ||
       view->xmin>=view->xmax || view->ymin>=view->ymax) return -1;
    memset(stats,0,sizeof *stats);stats->all_zero=1;
    int n=view->cols,m=view->rows;
    float *rows=malloc((size_t)(n+1)*2*sizeof(float));if(!rows)return -2;
    float *prev=rows,*next=rows+n+1;
    Field f={.pr=pr,.vars=*base,.implicit=pr->has_equal || expr_program_uses(pr,V_Y)};
    float dx=(view->xmax-view->xmin)/n,dy=(view->ymax-view->ymin)/m;
    int period=x_period_steps(pr,view->period_x,dx,n);
    for(int ix=0;ix<=n;ix++) {
        if((ix%32)==0 && progress && !progress(0,m,progress_ctx)){stats->canceled=1;stats->evaluations=f.evaluations;free(rows);return 0;}
        prev[ix]=period && ix>=period?prev[ix-period]:sample_2d(&f,view->xmin+dx*ix,view->ymax);stats->samples++;
        if(!isfinite(prev[ix]))stats->invalid++;else if(prev[ix]!=0)stats->all_zero=0;
    }
    for(int iy=0;iy<m;iy++) {
        if(progress && !progress(iy,m,progress_ctx)){stats->canceled=1;free(rows);return 0;}
        float y0=view->ymax-dy*iy,y1=view->ymax-dy*(iy+1);
        for(int ix=0;ix<=n;ix++) {
            if((ix%32)==0 && progress && !progress(iy,m,progress_ctx)){stats->canceled=1;stats->evaluations=f.evaluations;free(rows);return 0;}
            next[ix]=period && ix>=period?next[ix-period]:sample_2d(&f,view->xmin+dx*ix,y1);stats->samples++;
            if(!isfinite(next[ix]))stats->invalid++;else if(next[ix]!=0)stats->all_zero=0;
        }
        for(int ix=0;ix<n;ix++) {
            if((ix%32)==0 && progress && !progress(iy,m,progress_ctx)){stats->canceled=1;stats->evaluations=f.evaluations;free(rows);return 0;}
            stats->cells++;
            float x0=view->xmin+dx*ix,x1=view->xmin+dx*(ix+1);
            Sample c[4]={{x0,y0,0,prev[ix]},{x1,y0,0,prev[ix+1]},
                         {x1,y1,0,next[ix+1]},{x0,y1,0,next[ix]}};
            if(options && options->fill && pr->relation>=REL_LT){
                float q[4]={c[0].v,c[1].v,c[2].v,c[3].v};
                options->fill(ix,iy,n,m,q,options->fill_ctx);
            }
            if(!isfinite(c[0].v)||!isfinite(c[1].v)||!isfinite(c[2].v)||!isfinite(c[3].v))continue;
            Sample e[4];int crossed[4],nc=0;
            for(int edge=0;edge<4;edge++) {
                int j=(edge+1)%4;
                if(crosses(c[edge].v,c[j].v)) {
                    float pixels=options?((edge&1)?(float)(options->pixel_h-1)/m:(float)(options->pixel_w-1)/n):0;
                    if(root_on_edge_pixels(&f,c[edge],c[j],2,&e[edge],pixels,options?options->tolerance_px:0))crossed[nc++]=edge;
                    else stats->rejected++;
                }
            }
            if(nc==2) {
                Sample a=e[crossed[0]],b=e[crossed[1]];
                if(a.x!=b.x || a.y!=b.y){segment(a.x,a.y,b.x,b.y,segment_ctx);stats->segments++;}
            } else if(nc==4) {
                float center=sample_2d(&f,(x0+x1)*0.5f,(y0+y1)*0.5f);
                int pair=((center<0)==(c[0].v<0));
                int a=pair?0:3,b=pair?1:0,cc=pair?2:1,d=pair?3:2;
                segment(e[a].x,e[a].y,e[b].x,e[b].y,segment_ctx);
                segment(e[cc].x,e[cc].y,e[d].x,e[d].y,segment_ctx);stats->segments+=2;
            }
        }
        float *swap=prev;prev=next;next=swap;
    }
    if(progress && !progress(m,m,progress_ctx)){stats->canceled=1;stats->evaluations=f.evaluations;free(rows);return 0;}
    stats->evaluations=f.evaluations;free(rows);return 1;
}
int plot_contour(Program const *pr,EvalVars const *base,PlaneView const *view,
                 SegmentFn segment,void *segment_ctx,ProgressFn progress,void *progress_ctx,PlotStats *stats)
{
    return plot_contour_opt(pr,base,view,segment,segment_ctx,NULL,progress,progress_ctx,stats);
}
int mesh_init(Mesh *m,int capacity)
{
    if(!m || capacity<1 || capacity>PLOT_MAX_TRIANGLES)return 0;
    memset(m,0,sizeof *m);m->tri=malloc((size_t)capacity*sizeof(Triangle3));
    if(!m->tri)return 0;
    m->capacity=capacity;return 1;
}
void mesh_free(Mesh *m) {if(m){free(m->tri);memset(m,0,sizeof *m);}}
static int16_t quant(float x,float center,float radius)
{
    float q=(x-center)/radius*16000.0f;
    if(q>16000)q=16000;
    if(q<-16000)q=-16000;
    return (int16_t)lroundf(q);
}
static int put_triangle(Mesh *m,Sample a,Sample b,Sample c,Volume const *v,int slot)
{
    Point3 p[3]={{quant(a.x,v->x,v->radius),quant(a.y,v->y,v->radius),quant(a.z,v->z,v->radius)},
                 {quant(b.x,v->x,v->radius),quant(b.y,v->y,v->radius),quant(b.z,v->z,v->radius)},
                 {quant(c.x,v->x,v->radius),quant(c.y,v->y,v->radius),quant(c.z,v->z,v->radius)}};
    if((p[0].x==p[1].x && p[0].y==p[1].y && p[0].z==p[1].z) ||
       (p[1].x==p[2].x && p[1].y==p[2].y && p[1].z==p[2].z) ||
       (p[0].x==p[2].x && p[0].y==p[2].y && p[0].z==p[2].z))return 1;
    Triangle3 tri;memcpy(tri.p,p,sizeof p);tri.slot=(uint8_t)slot;tri.pad=0;
    if(m->emit){if(!m->emit(&tri,m->emit_ctx))return 0;m->count++;return 1;}
    if(m->count>=m->capacity){m->truncated=1;return 0;}
    m->tri[m->count++]=tri;return 1;
}
static int tetra(Mesh *m,Field *f,Sample c[4],Volume const *v,int slot,PlotStats *stats)
{
    int plus[4],minus[4],np=0,nm=0;
    for(int j=0;j<4;j++) {
        if(!isfinite(c[j].v))return 1;
        if(c[j].v<0)minus[nm++]=j;else plus[np++]=j;
    }
    if(np==0 || nm==0)return 1;
    Sample e[4];
    if(np==1 || nm==1) {
        int q=(np==1?plus[0]:minus[0]);int *other=(np==1?minus:plus);
        for(int j=0;j<3;j++) if(!root_on_edge(f,c[q],c[other[j]],3,&e[j])){stats->rejected++;return 1;}
        return put_triangle(m,e[0],e[1],e[2],v,slot);
    }
    if(!root_on_edge(f,c[plus[0]],c[minus[0]],3,&e[0]) ||
       !root_on_edge(f,c[plus[0]],c[minus[1]],3,&e[1]) ||
       !root_on_edge(f,c[plus[1]],c[minus[0]],3,&e[2]) ||
       !root_on_edge(f,c[plus[1]],c[minus[1]],3,&e[3])) {stats->rejected++;return 1;}
    return put_triangle(m,e[0],e[1],e[2],v,slot) && put_triangle(m,e[1],e[3],e[2],v,slot);
}
int plot_surface(Mesh *mesh,Program const *pr,EvalVars const *base,Volume const *v,
                 int slot,ProgressFn progress,void *progress_ctx,PlotStats *stats)
{
    if(!mesh || (!mesh->tri && !mesh->emit) || !pr || !base || !v || !stats || v->div<4 || v->div>PLOT_MAX_DIV ||
       !isfinite(v->radius) || v->radius<=0 || !isfinite(v->x) || !isfinite(v->y) || !isfinite(v->z))return -1;
    memset(stats,0,sizeof *stats);stats->all_zero=1;
    int n=v->div,d=n+1;size_t plane=(size_t)d*d;
    float *grid=malloc(plane*2*sizeof(float));if(!grid)return -2;
    float *prev=grid,*next=grid+plane;
    Field f={.pr=pr,.vars=*base,.implicit=pr->has_equal || expr_program_uses(pr,V_Z)};
    float step=2*v->radius/n;int period=x_period_steps(pr,v->period_x,step,n);
    static const int co[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const int ts[6][4]={{0,5,1,6},{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6}};
    /* For explicit z=f(x,y), reuse a previous sample with an increment.
       This saves evaluations but has ordinary float round-off. */
    for(int iz=0;iz<=n;iz++) {
        if(progress && !progress(iz,n+1,progress_ctx)){
            stats->canceled=1;stats->evaluations=f.evaluations;free(grid);return 0;
        }
        float z=v->z-v->radius+iz*step;
        for(int iy=0;iy<=n;iy++) {
            if((iy%8)==0 && progress && !progress(iz,n+1,progress_ctx)){
                stats->canceled=1;stats->evaluations=f.evaluations;free(grid);return 0;
            }
            for(int ix=0;ix<=n;ix++) {
            size_t at=(size_t)iy*d+ix;
            float q;
            if(period && ix>=period)q=next[at-period];
            else if(iz>0 && !f.implicit && isfinite(prev[at]))q=prev[at]+step;
            else {
                float x=v->x-v->radius+ix*step,y=v->y-v->radius+iy*step;
                q=sample_field(&f,x,y,z);
            }
            next[at]=q;stats->samples++;
            if(!isfinite(q))stats->invalid++;else if(q!=0)stats->all_zero=0;
            }
        }
        if(iz>0)for(int iy=0;iy<n;iy++) {
            if((iy%4)==0 && progress && !progress(iz,n+1,progress_ctx)){
                stats->canceled=1;stats->evaluations=f.evaluations;free(grid);return 0;
            }
            for(int ix=0;ix<n;ix++) {
            Sample c[8];stats->cells++;
            for(int k=0;k<8;k++) {
                int ax=ix+co[k][0],ay=iy+co[k][1],az=iz-1+co[k][2];
                float *layer=co[k][2]?next:prev;
                c[k]=(Sample){v->x-v->radius+ax*step,v->y-v->radius+ay*step,v->z-v->radius+az*step,layer[(size_t)ay*d+ax]};
            }
            for(int j=0;j<6;j++) {
                Sample tet[4]={c[ts[j][0]],c[ts[j][1]],c[ts[j][2]],c[ts[j][3]]};
                if(!tetra(mesh,&f,tet,v,slot,stats)){
                    stats->evaluations=f.evaluations;free(grid);
                    if(mesh->emit){stats->canceled=1;return 0;}return 1;
                }
            }
            }
        }
        float *swap=prev;prev=next;next=swap;
    }
    if(progress && !progress(n+1,n+1,progress_ctx)){stats->canceled=1;stats->evaluations=f.evaluations;free(grid);return 0;}
    stats->evaluations=f.evaluations;free(grid);return 1;
}
int plot_surface_stream(Program const *pr,EvalVars const *base,Volume const *v,
                        int slot,TriangleFn emit,void *emit_ctx,
                        ProgressFn progress,void *progress_ctx,PlotStats *stats)
{
    if(!emit)return -1;
    Mesh sink={.emit=emit,.emit_ctx=emit_ctx};
    return plot_surface(&sink,pr,base,v,slot,progress,progress_ctx,stats);
}

typedef struct {int x,y,z;} Proj;
typedef struct {float cy,sy,cp,sp,scale;} Transform;
static Proj project(Point3 p,Transform const *tr)
{
    float x=p.x/16000.0f,y=p.y/16000.0f,z=p.z/16000.0f;
    float rx=tr->cy*x-tr->sy*y,ry=tr->sy*x+tr->cy*y;
    float vertical=tr->cp*z-tr->sp*ry,depth=-tr->cp*ry-tr->sp*z;
    float scale=tr->scale;
    Proj q={(PLOT_X0+PLOT_X1)/2+(int)lroundf(scale*rx),
            (PLOT_Y0+PLOT_Y1)/2-(int)lroundf(scale*vertical),
            (int)lroundf((depth+2.0f)*16000.0f)};
    if(q.z<1)q.z=1;
    if(q.z>65534)q.z=65534;
    return q;
}
static int shade(Triangle3 const *t)
{
    float ax=(float)t->p[1].x-t->p[0].x,ay=(float)t->p[1].y-t->p[0].y,az=(float)t->p[1].z-t->p[0].z;
    float bx=(float)t->p[2].x-t->p[0].x,by=(float)t->p[2].y-t->p[0].y,bz=(float)t->p[2].z-t->p[0].z;
    float nx=ay*bz-az*by,ny=az*bx-ax*bz,nz=ax*by-ay*bx;
    float len=sqrtf(nx*nx+ny*ny+nz*nz);
    float light=len>0?fabsf((nx*.35f+ny*.45f+nz*.82f)/len):0;
    int bright=(int)(10.0f+19.0f*light);if(bright>31)bright=31;if(bright<8)bright=8;
    switch(t->slot%3) {
        case 0:return C_RGB(bright/4,bright*3/4,bright);
        case 1:return C_RGB(bright,bright/3,bright/4);
        default:return C_RGB(bright/3,bright,bright/3);
    }
}
static void zpixel(uint16_t *zb,int x,int y,int z,int color,int bias)
{
    if(x<PLOT_X0 || x>PLOT_X1 || y<PLOT_Y0 || y>PLOT_Y1 || z<0 || z>65535)return;
    int idx=(y-PLOT_Y0)*PLOT_W+(x-PLOT_X0);
    if(z<=zb[idx]+bias) {if(z<zb[idx])zb[idx]=(uint16_t)z;gint_vram[y*DWIDTH+x]=(uint16_t)color;}
}
static void zline(uint16_t *zb,Proj a,Proj b,int color)
{
    int dx=abs(b.x-a.x),sx=a.x<b.x?1:-1,dy=-abs(b.y-a.y),sy=a.y<b.y?1:-1,err=dx+dy;
    int steps=dx>-dy?dx:-dy;int j=0;
    for(;;){int z=steps?a.z+(int)(((int64_t)(b.z-a.z)*j)/steps):a.z;zpixel(zb,a.x,a.y,z,color,16);
        if(a.x==b.x&&a.y==b.y)break;
        int e=2*err;
        if(e>=dy){err+=dy;a.x+=sx;}
        if(e<=dx){err+=dx;a.y+=sy;}
        j++;
    }
}
static int edge(Proj a,Proj b,int x,int y) {return (b.x-a.x)*(y-a.y)-(b.y-a.y)*(x-a.x);}
static void raster(uint16_t *zb,Proj a,Proj b,Proj c,int color)
{
    int area=edge(a,b,c.x,c.y);if(!area)return;
    if(area<0){Proj t=b;b=c;c=t;area=-area;}
    int minx=a.x<b.x?(a.x<c.x?a.x:c.x):(b.x<c.x?b.x:c.x);
    int maxx=a.x>b.x?(a.x>c.x?a.x:c.x):(b.x>c.x?b.x:c.x);
    int miny=a.y<b.y?(a.y<c.y?a.y:c.y):(b.y<c.y?b.y:c.y);
    int maxy=a.y>b.y?(a.y>c.y?a.y:c.y):(b.y>c.y?b.y:c.y);
    if(minx<PLOT_X0)minx=PLOT_X0;
    if(maxx>PLOT_X1)maxx=PLOT_X1;
    if(miny<PLOT_Y0)miny=PLOT_Y0;
    if(maxy>PLOT_Y1)maxy=PLOT_Y1;
    if(minx>maxx || miny>maxy)return;
    int64_t dzdx=(((int64_t)(b.z-a.z)*(c.y-a.y)-(int64_t)(c.z-a.z)*(b.y-a.y))*65536)/area;
    int64_t dzdy=(((int64_t)(b.x-a.x)*(c.z-a.z)-(int64_t)(c.x-a.x)*(b.z-a.z))*65536)/area;
    int64_t zrow=(int64_t)a.z*65536+dzdx*(minx-a.x)+dzdy*(miny-a.y);
    int e0row=edge(a,b,minx,miny),e1row=edge(b,c,minx,miny),e2row=edge(c,a,minx,miny);
    for(int y=miny;y<=maxy;y++) {
        int e0=e0row,e1=e1row,e2=e2row;int64_t z=zrow;
        for(int x=minx;x<=maxx;x++) {
            if(e0>=0 && e1>=0 && e2>=0)zpixel(zb,x,y,(int)(z/65536),color,0);
            e0-=b.y-a.y;e1-=c.y-b.y;e2-=a.y-c.y;z+=dzdx;
        }
        e0row+=b.x-a.x;e1row+=c.x-b.x;e2row+=a.x-c.x;zrow+=dzdy;
    }
}
int mesh_render(Mesh const *mesh,Camera const *cam,ProgressFn progress,void *progress_ctx)
{
    if(!mesh || !cam || !isfinite(cam->yaw) || !isfinite(cam->pitch) || !isfinite(cam->zoom) || cam->zoom<0.1f || cam->zoom>5.0f)return -1;
    uint16_t *zb=malloc((size_t)PLOT_W*PLOT_H*sizeof(uint16_t));if(!zb)return -2;
    for(int i=0;i<PLOT_W*PLOT_H;i++)zb[i]=65535;
    Transform tr={cosf(cam->yaw),sinf(cam->yaw),cosf(cam->pitch),sinf(cam->pitch),72.0f*cam->zoom};
    for(int i=0;i<mesh->count;i++) {
        if((i%64)==0 && progress && !progress(i,mesh->count,progress_ctx)){free(zb);return 0;}
        Triangle3 const *t=&mesh->tri[i];Proj a=project(t->p[0],&tr),b=project(t->p[1],&tr),c=project(t->p[2],&tr);
        raster(zb,a,b,c,shade(t));
    }
    if(cam->wire) for(int i=0;i<mesh->count;i++) {
        if((i%64)==0 && progress && !progress(i,mesh->count,progress_ctx)){free(zb);return 0;}
        Triangle3 const *t=&mesh->tri[i];Proj a=project(t->p[0],&tr),b=project(t->p[1],&tr),c=project(t->p[2],&tr);
        zline(zb,a,b,C_DARK);zline(zb,b,c,C_DARK);zline(zb,c,a,C_DARK);
    }
    if(cam->axes) {
        float radius=cam->radius>0?cam->radius:1.0f;
        float ox=-cam->center_x/radius,oy=-cam->center_y/radius,oz=-cam->center_z/radius;
        /* A world axis exists within the cube only if its two fixed zero
           coordinates are inside the corresponding ranges. */
        if(isfinite(oy)&&isfinite(oz)&&fabsf(oy)<=1&&fabsf(oz)<=1) {
            Point3 a={-16000,(int16_t)lroundf(oy*16000),(int16_t)lroundf(oz*16000)};
            Point3 b={16000,a.y,a.z};zline(zb,project(a,&tr),project(b,&tr),C_RED);
        }
        if(isfinite(ox)&&isfinite(oz)&&fabsf(ox)<=1&&fabsf(oz)<=1) {
            Point3 a={(int16_t)lroundf(ox*16000),-16000,(int16_t)lroundf(oz*16000)};
            Point3 b={a.x,16000,a.z};zline(zb,project(a,&tr),project(b,&tr),C_GREEN);
        }
        if(isfinite(ox)&&isfinite(oy)&&fabsf(ox)<=1&&fabsf(oy)<=1) {
            Point3 a={(int16_t)lroundf(ox*16000),(int16_t)lroundf(oy*16000),-16000};
            Point3 b={a.x,a.y,16000};zline(zb,project(a,&tr),project(b,&tr),C_BLUE);
        }
    }
    free(zb);return 1;
}

/* Perspective rendering for streaming surfaces. Camera space is measured in
   units of the clipping cube radius; near clipping precedes projection. */
typedef struct {float x,y,z;} EyePoint;
struct Render3D {
    Camera cam;uint16_t *depth;
    float cy,sy,cp,sp,ex,ey,ez,center_depth,near_clip,inv_near,depth_scale;
    float focal,clip_left,clip_right,clip_bottom,clip_top;
};
static int camera_prepare(Render3D *r,Camera const *c)
{
    if(!r||!c||!isfinite(c->yaw)||!isfinite(c->pitch)||!isfinite(c->zoom)||
       !isfinite(c->radius)||c->radius<=0||c->zoom<.1f||c->zoom>5 ||
       !isfinite(c->eye_x)||!isfinite(c->eye_y)||!isfinite(c->eye_z) ||
       !isfinite(c->center_x)||!isfinite(c->center_y)||!isfinite(c->center_z))return 0;
    r->cam=*c;r->cy=cosf(c->yaw);r->sy=sinf(c->yaw);r->cp=cosf(c->pitch);r->sp=sinf(c->pitch);
    r->ex=(c->eye_x-c->center_x)/c->radius;
    r->ey=(c->eye_y-c->center_y)/c->radius;
    r->ez=(c->eye_z-c->center_z)/c->radius;
    r->center_depth=r->cp*(r->sy*r->ex+r->cy*r->ey)+r->sp*r->ez;
    if(!isfinite(r->center_depth))return 0;
    r->near_clip=camera_near_normalized(c->radius);
    r->focal=72.0f*2.8f*c->zoom;
    float screen_x=(PLOT_X0+PLOT_X1)*.5f,screen_y=(PLOT_Y0+PLOT_Y1)*.5f;
    r->clip_left=(PLOT_X0-screen_x)/r->focal;r->clip_right=(PLOT_X1-screen_x)/r->focal;
    r->clip_bottom=(screen_y-PLOT_Y1)/r->focal;r->clip_top=(screen_y-PLOT_Y0)/r->focal;
    float near=fmaxf(.04f,r->center_depth-2.0f),far=fmaxf(near+.001f,r->center_depth+2.0f);
    r->inv_near=1.0f/near;
    float span=r->inv_near-1.0f/far;
    if(!isfinite(span) || span<=0)return 0;
    r->depth_scale=65532.0f/span;
    return isfinite(r->depth_scale);
}
static EyePoint eye_point(Render3D const *r,float x,float y,float z)
{
    x-=r->ex;y-=r->ey;z-=r->ez;
    float ry=r->sy*x+r->cy*y;
    return (EyePoint){r->cy*x-r->sy*y,r->cp*z-r->sp*ry,-r->cp*ry-r->sp*z};
}
static int project_eye(Render3D const *r,EyePoint p,Proj *out)
{
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||p.z<r->near_clip)return 0;
    float factor=r->focal/p.z;
    float x=(PLOT_X0+PLOT_X1)*.5f+factor*p.x;
    float y=(PLOT_Y0+PLOT_Y1)*.5f-factor*p.y;
    /* Reciprocal camera depth is affine in projected screen coordinates,
       unlike camera depth itself. This preserves correct z-buffer ordering. */
    float z=(r->inv_near-1.0f/p.z)*r->depth_scale+1.0f;
    if(!isfinite(x)||!isfinite(y)||!isfinite(z))return 0;
    if(x< -2048)x=-2048;
    if(x>2048)x=2048;
    if(y< -2048)y=-2048;
    if(y>2048)y=2048;
    if(z<1)z=1;
    if(z>65534)z=65534;
    *out=(Proj){(int)lroundf(x),(int)lroundf(y),(int)lroundf(z)};return 1;
}
static EyePoint interpolate_eye(EyePoint a,EyePoint b,float t)
{
    return (EyePoint){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};
}
static int clip_near(Render3D const *r,EyePoint const *in,int n,EyePoint *out)
{
    int count=0;EyePoint a=in[n-1];int ain=a.z>=r->near_clip;
    for(int i=0;i<n;i++) {
        EyePoint b=in[i];int bin=b.z>=r->near_clip;
        if(ain!=bin) {
            float t=(r->near_clip-a.z)/(b.z-a.z);
            EyePoint p=interpolate_eye(a,b,t);p.z=r->near_clip;out[count++]=p;
        }
        if(bin)out[count++]=b;
        a=b;ain=bin;
    }
    return count;
}
Render3D *render3_begin(Camera const *cam)
{
    Render3D *r=malloc(sizeof *r);if(!r)return NULL;
    if(!camera_prepare(r,cam)){free(r);return NULL;}
    r->depth=malloc((size_t)PLOT_W*PLOT_H*sizeof(uint16_t));
    if(!r->depth){free(r);return NULL;}
    for(int i=0;i<PLOT_W*PLOT_H;i++)r->depth[i]=65535;
    return r;
}
int render3_triangle(Triangle3 const *t,void *ctx)
{
    Render3D *r=ctx;if(!r||!t)return 0;
    EyePoint in[3],poly[5];
    for(int i=0;i<3;i++)in[i]=eye_point(r,t->p[i].x/16000.0f,t->p[i].y/16000.0f,t->p[i].z/16000.0f);
    int n=clip_near(r,in,3,poly);if(n<3)return 1;
    Proj a,b,c;if(!project_eye(r,poly[0],&a))return 1;
    int color=shade(t);
    for(int i=1;i<n-1;i++) {
        if(!project_eye(r,poly[i],&b)||!project_eye(r,poly[i+1],&c))continue;
        raster(r->depth,a,b,c,color);
        if(r->cam.wire){zline(r->depth,a,b,C_DARK);zline(r->depth,b,c,C_DARK);zline(r->depth,c,a,C_DARK);}
    }
    return 1;
}
void render3_end(Render3D *r){if(r){free(r->depth);free(r);}}
/* Project and clip the whole 3D segment in camera space. Independently
   clamping projected X/Y endpoints bends axes and breaks their intersection. */
static int axis_project(Render3D const *r,EyePoint p,float *x,float *y)
{
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z)||p.z<r->near_clip*.9999f)return 0;
    if(p.z<r->near_clip)p.z=r->near_clip;
    *x=(PLOT_X0+PLOT_X1)*.5f+r->focal*p.x/p.z;
    *y=(PLOT_Y0+PLOT_Y1)*.5f-r->focal*p.y/p.z;
    return isfinite(*x)&&isfinite(*y);
}
static EyePoint axis_world_eye(Render3D const *r,float x,float y,float z)
{
    /* Subtract the eye before scaling: near the origin this avoids
       cancellation between two large normalized cube coordinates. */
    x=(x-r->cam.eye_x)/r->cam.radius;
    y=(y-r->cam.eye_y)/r->cam.radius;
    z=(z-r->cam.eye_z)/r->cam.radius;
    float ry=r->sy*x+r->cy*y;
    return (EyePoint){r->cy*x-r->sy*y,r->cp*z-r->sp*ry,-r->cp*ry-r->sp*z};
}
static EyePoint axis_direction(Render3D const *r,int axis)
{
    float inv=1.0f/r->cam.radius;
    if(axis==0)return (EyePoint){r->cy*inv,-r->sp*r->sy*inv,-r->cp*r->sy*inv};
    if(axis==1)return (EyePoint){-r->sy*inv,-r->sp*r->cy*inv,-r->cp*r->cy*inv};
    return (EyePoint){0,r->cp*inv,-r->sp*inv};
}
static EyePoint axis_at(EyePoint origin,EyePoint direction,float world)
{
    return (EyePoint){origin.x+direction.x*world,origin.y+direction.y*world,origin.z+direction.z*world};
}
int render3_project_world(Camera const *cam,float x,float y,float z,float *sx,float *sy)
{
    Render3D r;if(!sx||!sy||!camera_prepare(&r,cam))return 0;
    return axis_project(&r,axis_world_eye(&r,x,y,z),sx,sy);
}
static float axis_plane(Render3D const *r,EyePoint p,int plane)
{
    switch(plane){
        case 0:return p.z-r->near_clip;
        case 1:return p.x-r->clip_left*p.z;
        case 2:return r->clip_right*p.z-p.x;
        case 3:return p.y-r->clip_bottom*p.z;
        default:return r->clip_top*p.z-p.y;
    }
}
static float axis_plane_direction(Render3D const *r,EyePoint p,int plane)
{
    switch(plane){
        case 0:return p.z;
        case 1:return p.x-r->clip_left*p.z;
        case 2:return r->clip_right*p.z-p.x;
        case 3:return p.y-r->clip_bottom*p.z;
        default:return r->clip_top*p.z-p.y;
    }
}
static int clip_axis(Render3D const *r,EyePoint origin,EyePoint direction,float *first,float *last)
{
    /* Solve frustum planes directly in world units. Interpolating between
       +/-R in float loses the shared origin when R/distance is very large. */
    float lo=*first,hi=*last;
    for(int plane=0;plane<5;plane++){
        float value=axis_plane(r,origin,plane),slope=axis_plane_direction(r,direction,plane);
        if(!isfinite(value)||!isfinite(slope))return 0;
        if(fabsf(slope)<1e-30f){if(value<0)return 0;continue;}
        float crossing=-value/slope;if(!isfinite(crossing))return 0;
        if(slope>0){if(crossing>lo)lo=crossing;}else if(crossing<hi)hi=crossing;
        if(lo>hi)return 0;
    }
    *first=lo;*last=hi;return 1;
}
static float nice_axis_unit(float ideal)
{
    if(!isfinite(ideal)||ideal<=0)return 0;
    /* Avoid log/pow for every camera frame on the calculator's soft FPU. */
    float power=1;
    for(int i=0;i<20 && ideal>=10*power;i++)power*=10;
    for(int i=0;i<20 && ideal<power;i++)power*=.1f;
    float ratio=ideal/power;
    return ratio<=1?power:ratio<=2?2*power:ratio<=5?5*power:10*power;
}
static float axis_metric(Render3D const *r,EyePoint direction,EyePoint origin)
{
    float ax=direction.x*origin.z-origin.x*direction.z;
    float ay=direction.y*origin.z-origin.y*direction.z;
    return r->focal*sqrtf(ax*ax+ay*ay);
}
static float axis_local_unit(float metric,EyePoint eye,float world)
{
    if(!isfinite(metric)||metric<1e-12f)return 0;
    /* At large world coordinates adjacent float values eventually coincide. */
    float precision=fmaxf(1e-7f,fabsf(world)*1e-6f);
    return nice_axis_unit(fmaxf(55.0f*eye.z*eye.z/metric,precision));
}
static int clip_screen(int value,int low,int high){return value<low?low:value>high?high:value;}
static int axis_geometry(Render3D const *r,int axis,Axis3Geometry *g)
{
    if(axis<0||axis>2||!g)return 0;
    Camera const *c=&r->cam;float center[3]={c->center_x,c->center_y,c->center_z};
    int a=(axis+1)%3,b=(axis+2)%3;
    if(fabsf(center[a])>c->radius || fabsf(center[b])>c->radius)return 0;
    float start=center[axis]-c->radius,end=center[axis]+c->radius;
    EyePoint origin=axis_world_eye(r,0,0,0),direction=axis_direction(r,axis);
    float t0=start,t1=end;if(!clip_axis(r,origin,direction,&t0,&t1))return 0;
    EyePoint e0=axis_at(origin,direction,t0),e1=axis_at(origin,direction,t1);
    float sx0,sy0,sx1,sy1;
    if(!axis_project(r,e0,&sx0,&sy0)||!axis_project(r,e1,&sx1,&sy1))return 0;
    memset(g,0,sizeof *g);
    g->x0=clip_screen((int)lroundf(sx0),PLOT_X0,PLOT_X1);
    g->y0=clip_screen((int)lroundf(sy0),PLOT_Y0,PLOT_Y1);
    g->x1=clip_screen((int)lroundf(sx1),PLOT_X0,PLOT_X1);
    g->y1=clip_screen((int)lroundf(sy1),PLOT_Y0,PLOT_Y1);
    g->world_start=t0;g->world_end=t1;
    float dx=sx1-sx0,dy=sy1-sy0,length=sqrtf(dx*dx+dy*dy);
    if(!isfinite(length)||length<8.0f)return 1;
    float metric=axis_metric(r,direction,origin);
    int samples=(int)(length/50.0f)+1;if(samples>AXIS3_MAX_TICKS)samples=AXIS3_MAX_TICKS;
    /* Uniform samples in screen space -> perspective-correct world points.
       Choose a local 1/2/5 step, so nearby ticks refine without requiring an
       unbounded iteration through every tick compressed into the far end. */
    for(int j=0;j<samples;j++){
        float u=(j+.5f)/(float)samples;
        float denominator=(1-u)*e1.z+u*e0.z;
        if(!isfinite(denominator)||denominator<=0)continue;
        float w=(u*e0.z)/denominator;
        float world=g->world_start+(g->world_end-g->world_start)*w;
        EyePoint ref=axis_at(origin,direction,world);
        float step=axis_local_unit(metric,ref,world);if(!isfinite(step)||step<=0)continue;
        if(j==samples/2)g->reference_step=step;
        float value=roundf(world/step)*step;
        if(!isfinite(value)||value<g->world_start-step*.0001f||value>g->world_end+step*.0001f)continue;
        EyePoint point=axis_at(origin,direction,value);float x,y;
        if(!axis_project(r,point,&x,&y) || x<PLOT_X0-1 || x>PLOT_X1+1 || y<PLOT_Y0-1 || y>PLOT_Y1+1)continue;
        int ix=clip_screen((int)lroundf(x),PLOT_X0,PLOT_X1),iy=clip_screen((int)lroundf(y),PLOT_Y0,PLOT_Y1),duplicate=0;
        for(int k=0;k<g->count;k++){
            int tx=ix-g->tick[k].x,ty=iy-g->tick[k].y;
            if(tx*tx+ty*ty<25*25){duplicate=1;break;}
        }
        if(!duplicate && g->count<AXIS3_MAX_TICKS)g->tick[g->count++]=(Axis3Tick){fabsf(value)<step*.0001f?0:value,ix,iy};
    }
    return 1;
}
int render3_axis_geometry(Camera const *cam,int axis,Axis3Geometry *geometry)
{
    Render3D r;if(!camera_prepare(&r,cam))return 0;
    return axis_geometry(&r,axis,geometry);
}
static void axis_draw(Render3D const *r,int axis,int color)
{
    Axis3Geometry g;if(!axis_geometry(r,axis,&g))return;
    dline(g.x0,g.y0,g.x1,g.y1,color);
    float dx=(float)(g.x1-g.x0),dy=(float)(g.y1-g.y0),length=sqrtf(dx*dx+dy*dy);
    int tx=length>0?(int)lroundf(-3*dy/length):2,ty=length>0?(int)lroundf(3*dx/length):2;
    for(int i=0;i<g.count;i++){
        Axis3Tick const *p=&g.tick[i];
        dline(p->x-tx,p->y-ty,p->x+tx,p->y+ty,color);
        if((p->value!=0 || axis==2) && p->x>=PLOT_X0+2 && p->x<=PLOT_X1-42 &&
            p->y>=PLOT_Y0+2 && p->y<=PLOT_Y1-12)
            dprint(p->x+4,p->y+2,color,"%.5g",(double)p->value);
    }
    if(g.x1>=PLOT_X0 && g.x1<PLOT_X1-10 && g.y1>=PLOT_Y0+9 && g.y1<PLOT_Y1-10)
        dtext(g.x1+2,g.y1-9,color,axis==0?"X":axis==1?"Y":"Z");
}
void render3_axes(Camera const *cam)
{
    Render3D r;if(!camera_prepare(&r,cam))return;
    struct dwindow old=dwindow_set((struct dwindow){PLOT_X0,PLOT_Y0,PLOT_X1+1,PLOT_Y1+1});
    axis_draw(&r,0,C_RED);axis_draw(&r,1,C_RGB(0,18,0));axis_draw(&r,2,C_BLUE);
    dwindow_set(old);
}
