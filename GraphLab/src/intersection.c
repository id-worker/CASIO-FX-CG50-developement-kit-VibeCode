#include "intersection.h"
#include <math.h>
#include <float.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#define EMPTY_Z INT16_MIN
#define SNAN (0.0f/0.0f)
typedef struct {float f,g;} Pair;
typedef struct {float x,y,z;} Vec;
typedef struct {RaySurface const *s;EvalVars vars;IMapStats *stats;} Fields;
static int valid(IMapView const *v,RaySurface const *s,EvalVars const *vars)
{
    if(!v||!s||!vars||!s[0].program||!s[1].program||!isfinite(v->radius)||v->radius<=0||
       !isfinite(v->zmin)||!isfinite(v->zmax)||v->zmax<=v->zmin||
       !isfinite(v->center_x)||!isfinite(v->center_y)||
       v->xy_div<4||v->xy_div>180||v->z_div<1||v->z_div>IMAP_MAX_AUTO_SLICES||
       v->cursor_steps<8||v->cursor_steps>IMAP_MAX_AUTO_SLICES)return 0;
    return s[0].program->relation<REL_LT&&s[1].program->relation<REL_LT;
}
int imap_auto_view_bounds(IMapView *v,float zmin,float zmax)
{
    if(!v||!isfinite(v->radius)||v->radius<=0||!isfinite(zmin)||!isfinite(zmax)||zmin>=zmax)return 0;
    /* A map pixel spans 2*radius/(side-1); ceil keeps every Z interval
       no wider than a pixel even when optional Z extensions are asymmetric. */
    double slices=ceil((double)(IMAP_SIDE-1)*((double)zmax-(double)zmin)/(2.0*(double)v->radius));
    if(!isfinite(slices)||slices<1||slices>IMAP_MAX_AUTO_SLICES)return 0;
    v->zmin=zmin;v->zmax=zmax;v->xy_div=IMAP_SIDE-1;
    v->z_div=(int)slices;v->cursor_steps=v->z_div<8?8:v->z_div;return 1;
}
int imap_auto_view(IMapView *v,float clip_radius)
{
    if(!isfinite(clip_radius)||clip_radius<=0)return 0;
    return imap_auto_view_bounds(v,-clip_radius,clip_radius);
}
static float field(Fields *f,int which,float x,float y,float z)
{
    Program const *p=f->s[which].program;
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    float q=expr_eval_real(p,&f->vars);if(f->stats&&f->stats->evaluations<INT_MAX)f->stats->evaluations++;
    if(!isfinite(q))return SNAN;
    if(!p->has_equal&&!expr_program_uses(p,V_Z))q=z-q;
    return isfinite(q)?q:SNAN;
}
static Pair both(Fields *f,float x,float y,float z)
{
    Pair a={field(f,0,x,y,z),field(f,1,x,y,z)};return a;
}
static float component(Pair p,int which){return which?p.g:p.f;}
static Vec mix(Vec a,Vec b,float t)
{return (Vec){a.x+(b.x-a.x)*t,a.y+(b.y-a.y)*t,a.z+(b.z-a.z)*t};}
static float dist2(Vec a,Vec b)
{float x=a.x-b.x,y=a.y-b.y,z=a.z-b.z;return x*x+y*y+z*z;}
static Vec tri(Vec a,Vec b,Vec c,float u,float v)
{return (Vec){a.x+u*(b.x-a.x)+v*(c.x-a.x),a.y+u*(b.y-a.y)+v*(c.y-a.y),a.z+u*(b.z-a.z)+v*(c.z-a.z)};}

/* Check a root at the exact requested XY. A field is scaled by its own local
   finite differences, not by an arbitrary absolute residual, so a constant
   nonzero field cannot be mistaken for a zero merely by multiplying it by a
   small coefficient.  This is still a finite-precision numerical test. */
static int verify_root(Fields *f,IMapView const *v,float x,float y,float z)
{
    float hxy=fmaxf(v->radius*.001f,1e-5f),hz=fmaxf((v->zmax-v->zmin)/(v->cursor_steps*16.0f),1e-6f);
    /* The input XY is fixed, not an unknown point within a screen pixel.
       Account for floating point rounding, not a radius-dependent XY drift.
       The previous 32-ULP allowance admitted distinct roots from each field
       as if they were common roots (e.g. plane z=x-y). */
    float tx=FLT_EPSILON*4*(1+fmaxf(fabsf(x),fabsf(y)));
    float tz=fmaxf((v->zmax-v->zmin)/(v->cursor_steps*1000000.0f),FLT_EPSILON*4*(1+fabsf(z)));
    for(int k=0;k<2;k++){
        float q=field(f,k,x,y,z),a=field(f,k,x+hxy,y,z),b=field(f,k,x-hxy,y,z);
        float c=field(f,k,x,y+hxy,z),d=field(f,k,x,y-hxy,z);
        float e=field(f,k,x,y,z+hz),g=field(f,k,x,y,z-hz);
        if(!isfinite(q))return 0;
        /* Domain boundaries such as sqrt(y) at y=0 are legitimate. Use a
           one-sided difference where one neighboring sample is undefined. */
        float vals[]={a,b,c,d,e,g},scale=fabsf(q);
        for(int i=0;i<6;i++)if(isfinite(vals[i]))scale=fmaxf(scale,fabsf(vals[i]));
        float delta[3]={0,0,0};
        for(int i=0;i<3;i++){
            float left=vals[2*i],right=vals[2*i+1];
            if(isfinite(left)&&isfinite(right))delta[i]=fabsf(left-right);
            else if(isfinite(left))delta[i]=2*fabsf(left-q);
            else if(isfinite(right))delta[i]=2*fabsf(q-right);
            else if(q!=0)return 0;
        }
        float tol=scale*(FLT_EPSILON*32)+(delta[0]+delta[1])*(tx/(2*hxy))+delta[2]*(tz/(2*hz));
        if(fabsf(q)>tol && q!=0)return 0;
    }
    return 1;
}
static void root_add(Fields *f,IMapView const *v,float x,float y,float z,IMapRoots *r)
{
    if(!isfinite(z)||z<v->zmin||z>v->zmax||!verify_root(f,v,x,y,z))return;
    float eps=fmaxf((v->zmax-v->zmin)/(v->cursor_steps*1000000.0f),FLT_EPSILON*8*(1+fabsf(z)));
    for(int i=0;i<r->count;i++)if(fabsf(r->z[i]-z)<=eps)return;
    if(r->count==IMAP_MAX_ROOTS){r->overflow=1;return;}
    int i=r->count++;while(i>0&&r->z[i-1]>z){r->z[i]=r->z[i-1];i--;}r->z[i]=z;
}
static float bisect(Fields *f,int k,float x,float y,float a,float b,float qa,float qb)
{
    float scale=fmaxf(fabsf(qa),fabsf(qb)),small=fminf(fabsf(qa),fabsf(qb));
    float best=fabsf(qa)<fabsf(qb)?a:b,bv=fminf(fabsf(qa),fabsf(qb));
    for(int i=0;i<22;i++){
        float m=(a+b)*.5f,q=field(f,k,x,y,m);if(!isfinite(q))return SNAN;
        if(fabsf(q)<bv){bv=fabsf(q);best=m;}if(q==0)return m;
        if((q<0)==(qa<0)){a=m;qa=q;}else {b=m;qb=q;}
    }
    /* Reject many poles: an actual bracketed zero should improve decisively. */
    if(!isfinite(scale)||bv>small*.2f+scale*1e-5f)return SNAN;
    return best;
}
static float minimize(Fields *f,int k,float x,float y,float a,float b)
{
    for(int i=0;i<48;i++){
        float u=a+(b-a)/3,v=b-(b-a)/3;
        if(u<=a||v>=b||u>=v)break; /* binary32 no longer narrows */
        float p=field(f,k,x,y,u),q=field(f,k,x,y,v);
        if(!isfinite(p)||!isfinite(q))return SNAN;
        if(fabsf(p)<fabsf(q))b=v;else a=u;
    }
    return (a+b)*.5f;
}
int imap_search(RaySurface const s[2],EvalVars const *base,IMapView const *v,float x,float y,
                ProgressFn progress,void *ctx,IMapRoots *out)
{
    if(!out||!valid(v,s,base)||!isfinite(x)||!isfinite(y))return -1;
    memset(out,0,sizeof *out);Fields f={s,*base,NULL};float step=(v->zmax-v->zmin)/v->cursor_steps;
    Pair prev=both(&f,x,y,v->zmin),before=prev;int allzero=prev.f==0&&prev.g==0;
    for(int i=1;i<=v->cursor_steps;i++){
        if((i&7)==0&&progress&&!progress(i,v->cursor_steps,ctx))return 0;
        float z=v->zmin+step*i;Pair now=both(&f,x,y,z);
        if(!isfinite(now.f)||!isfinite(now.g))out->invalid++;
        if(now.f!=0||now.g!=0)allzero=0;
        for(int k=0;k<2;k++){
            float a=component(prev,k),b=component(now,k),c=component(before,k);
            Program const *p=s[k].program;
            int constant=p->has_equal&&!expr_program_uses(p,V_Z);
            if(!isfinite(a)||!isfinite(b)||constant)continue;
            if(a==0)root_add(&f,v,x,y,z-step,out);
            if(i==v->cursor_steps&&b==0)root_add(&f,v,x,y,z,out);
            if(a!=0&&b!=0&&(a<0)!=(b<0)){
                float q=bisect(&f,k,x,y,z-step,z,a,b);root_add(&f,v,x,y,q,out);
            }
            if(i>=2&&isfinite(c)&&a!=0&&fabsf(a)<fabsf(c)&&fabsf(a)<fabsf(b)){
                float q=minimize(&f,k,x,y,z-2*step,z);root_add(&f,v,x,y,q,out);
            }
        }
        before=prev;prev=now;
    }
    if(allzero){out->count=0;out->overflow=0;out->interval=1;}
    return 1;
}

/* Refine a linear two-field intersection on one tetrahedron face. Both
   adjacent tetrahedra share that face, which helps the projection stay joined.
   Singular faces and excursions outside the triangle are rejected. */
static int face(Fields *f,Vec a,Vec b,Vec c,Pair qa,Pair qb,Pair qc,Vec *point)
{
    float fa=qb.f-qa.f,fb=qc.f-qa.f,ga=qb.g-qa.g,gb=qc.g-qa.g;
    float det=fa*gb-fb*ga;
    float fs=fmaxf(fabsf(qa.f),fmaxf(fabsf(qb.f),fabsf(qc.f)));
    float gs=fmaxf(fabsf(qa.g),fmaxf(fabsf(qb.g),fabsf(qc.g)));
    if(!isfinite(det)||fs==0||gs==0||fabsf(det)<fs*gs*1e-7f)return 0;
    float u=(-qa.f*gb+qa.g*fb)/det,w=(-qa.g*fa+qa.f*ga)/det;
    /* A curved zero set can cross a face just outside the linearly estimated
       triangle. Allow a bounded Newton search, but accept only a refined hit
       actually on the face. */
    if(u<-.15f||w<-.15f||u+w>1.15f)return 0;
    for(int it=0;it<6;it++){
        Vec p=tri(a,b,c,u,w);Pair q=both(f,p.x,p.y,p.z);
        if(!isfinite(q.f)||!isfinite(q.g))return 0;
        if(fabsf(q.f)<=fs*2e-5f&&fabsf(q.g)<=gs*2e-5f){
            if(u<-.001f||w<-.001f||u+w>1.001f)return 0;
            *point=p;return 1;
        }
        float h=.002f;Vec pu=tri(a,b,c,u+h,w),pv=tri(a,b,c,u,w+h);
        Pair qu=both(f,pu.x,pu.y,pu.z),qv=both(f,pv.x,pv.y,pv.z);
        if(!isfinite(qu.f)||!isfinite(qu.g)||!isfinite(qv.f)||!isfinite(qv.g))return 0;
        fa=(qu.f-q.f)/h;fb=(qv.f-q.f)/h;ga=(qu.g-q.g)/h;gb=(qv.g-q.g)/h;
        det=fa*gb-fb*ga;
        if(!isfinite(det)||fabsf(det)<fs*gs*1e-7f)return 0;
        float du=(-q.f*gb+q.g*fb)/det,dw=(-q.g*fa+q.f*ga)/det;
        if(!isfinite(du)||!isfinite(dw)||fabsf(du)>.5f||fabsf(dw)>.5f)return 0;
        u+=du;w+=dw;if(u<-.15f||w<-.15f||u+w>1.15f)return 0;
    }
    return 0;
}
static void pixel_add(IMapPixel *map,IMapView const *v,float x,float y,float z,IMapStats *st)
{
    float px=((x-v->center_x)/v->radius+1)*.5f*(IMAP_SIDE-1),py=(1-(y-v->center_y)/v->radius)*.5f*(IMAP_SIDE-1);
    if(!isfinite(px)||!isfinite(py)||!isfinite(z)||px<-.5f||py<-.5f||px>IMAP_SIDE-.5f||py>IMAP_SIDE-.5f)return;
    int ix=(int)lroundf(px),iy=(int)lroundf(py);if(ix<0||iy<0||ix>=IMAP_SIDE||iy>=IMAP_SIDE)return;
    IMapPixel *p=&map[(size_t)iy*IMAP_SIDE+ix];if(p->z!=EMPTY_Z)return;
    float q=(z-v->zmin)/(v->zmax-v->zmin);if(q<0)q=0;if(q>1)q=1;
    int dx=(int)lroundf((px-ix)*254),dy=(int)lroundf((py-iy)*254);
    if(dx<-127)dx=-127;
    if(dx>127)dx=127;
    if(dy<-127)dy=-127;
    if(dy>127)dy=127;
    p->z=(int16_t)(-32767+(int)lroundf(q*65534));p->dx=(int8_t)dx;p->dy=(int8_t)dy;st->pixels++;
}
static int point_refine(Fields *f,IMapView const *v,Vec *p,float const lo[3],float const hi[3]);
static void segment(Fields *f,IMapPixel *map,IMapView const *v,Vec a,Vec b,IMapStats *st)
{
    /* Supercover traversal: visit every pixel whose area a projected segment
       crosses, not merely one rounded point per unit of its longest axis. */
    float scale=(IMAP_SIDE-1)/(2*v->radius);
    float x0=(a.x-v->center_x+v->radius)*scale,y0=(v->center_y+v->radius-a.y)*scale;
    float x1=(b.x-v->center_x+v->radius)*scale,y1=(v->center_y+v->radius-b.y)*scale;
    if(!isfinite(x0)||!isfinite(y0)||!isfinite(x1)||!isfinite(y1))return;
    int ix=(int)floorf(x0+.5f),iy=(int)floorf(y0+.5f);
    ix=ix<0?0:ix>=IMAP_SIDE?IMAP_SIDE-1:ix;
    iy=iy<0?0:iy>=IMAP_SIDE?IMAP_SIDE-1:iy;
    float dx=x1-x0,dy=y1-y0;int sx=dx>0?1:dx<0?-1:0,sy=dy>0?1:dy<0?-1:0;
    float tx=sx?((float)ix+(sx>0?.5f:-.5f)-x0)/dx:INFINITY;
    float ty=sy?((float)iy+(sy>0?.5f:-.5f)-y0)/dy:INFINITY;
    float stepx=sx?1.0f/fabsf(dx):INFINITY,stepy=sy?1.0f/fabsf(dy):INFINITY;
    float t=0;int guard=0;
    while(ix>=0&&ix<IMAP_SIDE&&iy>=0&&iy<IMAP_SIDE&&t<=1&&guard++<IMAP_SIDE*2+4){
        float end=fminf(1.0f,fminf(tx,ty));if(end<t)end=t;
        if(map[(size_t)iy*IMAP_SIDE+ix].z==EMPTY_Z){
            Vec p=mix(a,b,(t+end)*.5f);
            float unit=2*v->radius/(IMAP_SIDE-1);
            float cx=v->center_x-v->radius+ix*unit,cy=v->center_y+v->radius-iy*unit;
            float lo[3]={fmaxf(v->center_x-v->radius,cx-unit*.5f),
                         fmaxf(v->center_y-v->radius,cy-unit*.5f),v->zmin};
            float hi[3]={fminf(v->center_x+v->radius,cx+unit*.5f),
                         fminf(v->center_y+v->radius,cy+unit*.5f),v->zmax};
            p.x=fmaxf(lo[0],fminf(hi[0],p.x));p.y=fmaxf(lo[1],fminf(hi[1],p.y));
            p.z=fmaxf(lo[2],fminf(hi[2],p.z));
            if(point_refine(f,v,&p,lo,hi))pixel_add(map,v,p.x,p.y,p.z,st);
        }
        if(end>=1)break;
        /* A tie is an exact grid corner: advance diagonally. A zero-area
           corner touch is not inflated into two extra side pixels. */
        if(tx<=ty){ix+=sx;tx+=stepx;}
        if(ty<=end){iy+=sy;ty+=stepy;}
        t=end;
    }
    if(st->segments<INT_MAX)st->segments++;
}

static void tetra(Fields *f,IMapView const *v,IMapPixel *map,Vec const p[4],Pair const q[4],IMapStats *st)
{
    static const int faces[4][3]={{0,1,2},{0,1,3},{0,2,3},{1,2,3}};
    Vec roots[4];int n=0;
    for(int j=0;j<4;j++){
        int a=faces[j][0],b=faces[j][1],c=faces[j][2];Vec r;
        if(!face(f,p[a],p[b],p[c],q[a],q[b],q[c],&r))continue;
        int duplicate=0;float eps=fminf(2*v->radius/v->xy_div,(v->zmax-v->zmin)/v->z_div)*.0001f;
        for(int k=0;k<n;k++)if(dist2(roots[k],r)<eps*eps)duplicate=1;
        if(!duplicate)roots[n++]=r;
    }
    if(n!=2){if(n>2)st->ambiguous++;return;}
    /* Do not bridge across undefined or strongly nonlinear/pole intervals. */
    float sf=0,sg=0;for(int i=0;i<4;i++){sf=fmaxf(sf,fabsf(q[i].f));sg=fmaxf(sg,fabsf(q[i].g));}
    for(int i=1;i<=3;i++){
        Vec m=mix(roots[0],roots[1],i*.25f);Pair val=both(f,m.x,m.y,m.z);
        if(!isfinite(val.f)||!isfinite(val.g)||fabsf(val.f)>sf*.18f||fabsf(val.g)>sg*.18f){st->ambiguous++;return;}
    }
    segment(f,map,v,roots[0],roots[1],st);
}
static void cell(Fields *f,IMapView const *v,IMapPixel *map,int x,int y,int z,Pair const q[8],IMapStats *st)
{
    float fmin=INFINITY,fmax=-INFINITY,gmin=INFINITY,gmax=-INFINITY;
    for(int i=0;i<8;i++){
        if(!isfinite(q[i].f)||!isfinite(q[i].g)){st->invalid++;return;}
        fmin=fminf(fmin,q[i].f);fmax=fmaxf(fmax,q[i].f);
        gmin=fminf(gmin,q[i].g);gmax=fmaxf(gmax,q[i].g);
    }
    if(fmin>0||fmax<0||gmin>0||gmax<0)return;
    if(fmin==0&&fmax==0&&gmin==0&&gmax==0){st->ambiguous++;return;}
    static const int coord[8][3]={{0,0,0},{1,0,0},{1,1,0},{0,1,0},{0,0,1},{1,0,1},{1,1,1},{0,1,1}};
    static const int tet[6][4]={{0,5,1,6},{0,1,2,6},{0,2,3,6},{0,3,7,6},{0,7,4,6},{0,4,5,6}};
    float ds=2*v->radius/v->xy_div,dz=(v->zmax-v->zmin)/v->z_div;Vec corner[8];
    for(int i=0;i<8;i++)corner[i]=(Vec){v->center_x-v->radius+(x+coord[i][0])*ds,v->center_y-v->radius+(y+coord[i][1])*ds,
        z+coord[i][2]==v->z_div?v->zmax:v->zmin+(z+coord[i][2])*dz};
    for(int i=0;i<6;i++){
        Vec pp[4];Pair qq[4];for(int j=0;j<4;j++){pp[j]=corner[tet[i][j]];qq[j]=q[tet[i][j]];}
        tetra(f,v,map,pp,qq,st);
    }
}
static void row(Fields *f,IMapView const *v,int y,int z,Pair *out)
{
    float ds=2*v->radius/v->xy_div,dz=(v->zmax-v->zmin)/v->z_div;
    float wy=v->center_y-v->radius+y*ds,wz=z==v->z_div?v->zmax:v->zmin+z*dz;
    for(int x=0;x<=v->xy_div;x++)out[x]=both(f,v->center_x-v->radius+x*ds,wy,wz);
}
static void row_advance(Fields *f,IMapView const *v,int y,int z,Pair const *old,Pair *out)
{
    float ds=2*v->radius/v->xy_div,dz=(v->zmax-v->zmin)/v->z_div;
    float wy=v->center_y-v->radius+y*ds,wz=z==v->z_div?v->zmax:v->zmin+z*dz;
    for(int x=0;x<=v->xy_div;x++){
        float wx=v->center_x-v->radius+x*ds;Pair p;
        for(int k=0;k<2;k++){
            Program const *pr=f->s[k].program;float a=component(old[x],k),q;
            /* A bare height is z-h(x,y). Re-evaluate instead of adding dz
               thousands of times, which accumulates binary32 drift. */
            if(pr->has_equal&&!expr_program_uses(pr,V_Z)&&isfinite(a))q=a;
            else q=field(f,k,wx,wy,wz);
            if(k)p.g=q;else p.f=q;
        }
        out[x]=p;
    }
}
int imap_build(RaySurface const s[2],EvalVars const *base,IMapView const *v,IMapPixel *map,
               ProgressFn progress,void *ctx,IMapStats *st)
{
    if(!map||!st||!valid(v,s,base))return -1;
    memset(st,0,sizeof *st);for(int i=0;i<IMAP_SIDE*IMAP_SIDE;i++)map[i]=(IMapPixel){EMPTY_Z,0,0};
    Fields f={s,*base,st};int n=v->xy_div,d=n+1;
    Pair *plane=malloc((size_t)d*d*sizeof *plane),*rows=malloc((size_t)4*d*sizeof *rows);
    if(!rows){free(plane);return -2;}if(!plane)st->low_memory=1;
    Pair *p0=rows,*p1=rows+d,*n0=rows+2*d,*n1=rows+3*d;
    if(plane)for(int y=0;y<=n;y++){
        if(progress&&!progress(0,v->z_div,ctx)){free(rows);free(plane);return 0;}
        row(&f,v,y,0,plane+(size_t)y*d);
    }
    for(int z=0;z<v->z_div;z++){
        if(progress&&!progress(z,v->z_div,ctx)){free(rows);free(plane);return 0;}
        if(plane)row_advance(&f,v,0,z+1,plane,n0);
        else {row(&f,v,0,z,p0);row(&f,v,0,z+1,n0);}
        for(int y=0;y<n;y++){
            if((y&7)==0&&progress&&!progress(z,v->z_div,ctx)){free(rows);free(plane);return 0;}
            Pair *a,*b;
            if(plane){a=plane+(size_t)y*d;b=a+d;row_advance(&f,v,y+1,z+1,b,n1);}
            else {row(&f,v,y+1,z,p1);row(&f,v,y+1,z+1,n1);a=p0;b=p1;}
            for(int x=0;x<n;x++){
                if((x&31)==0&&progress&&!progress(z,v->z_div,ctx)){free(rows);free(plane);return 0;}
                Pair q[8]={a[x],a[x+1],b[x+1],b[x],n0[x],n0[x+1],n1[x+1],n1[x]};
                cell(&f,v,map,x,y,z,q,st);
            }
            if(plane)memcpy(a,n0,(size_t)d*sizeof *a);
            else {Pair *tmp=p0;p0=p1;p1=tmp;}
            Pair *tmp=n0;n0=n1;n1=tmp;
        }
        if(plane)memcpy(plane+(size_t)n*d,n0,(size_t)d*sizeof *n0);
    }
    free(rows);free(plane);return 1;
}
/* Specialization for z=h(x,y), with h affine and the other field proven
   continuous. Unknown/domain-limited expressions use imap_build() instead.
   Trace the substituted zero contour on the XY pixel grid, clip by z, then
   validate both original fields inside each candidate pixel. */
typedef struct {float z,q;} HeightPoint;
static int height_affine(Program const *p)
{
    if(!p||!p->size||p->size>EXPR_CODE||p->has_equal||expr_program_uses(p,V_Z))return 0;
    unsigned char degree[EXPR_STACK];int sp=0;
    for(int k=0;k<p->size;k++){
        Instr const *in=&p->code[k];
        switch(in->op){
        case I_NUM:if(!isfinite(in->number)||sp>=EXPR_STACK)return 0;degree[sp++]=0;break;
        case I_VAR:if(in->aux>=V_COUNT||sp>=EXPR_STACK)return 0;
            degree[sp++]=(in->aux==V_X||in->aux==V_Y)?1:0;break;
        case I_NEG:if(sp<1)return 0;break;
        case I_ADD:case I_SUB:
            if(sp<2)return 0;
            degree[sp-2]=degree[sp-2]>degree[sp-1]?degree[sp-2]:degree[sp-1];sp--;break;
        case I_MUL:
            if(sp<2||degree[sp-2]+degree[sp-1]>1)return 0;
            degree[sp-2]+=degree[sp-1];sp--;break;
        default:return 0;
        }
    }
    return sp==1;
}
static HeightPoint height_sample(Fields *f,Program const *h,int other,float x,float y)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;
    float z=expr_eval_real(h,&f->vars);
    if(f->stats&&f->stats->evaluations<INT_MAX)f->stats->evaluations++;
    if(!isfinite(z))return (HeightPoint){SNAN,SNAN};
    return (HeightPoint){z,field(f,other,x,y,z)};
}
static Vec height_edge(Fields *f,Program const *h,int other,Vec a,Vec b,float qa,float qb)
{
    if(qa==0)return a;
    if(qb==0)return b;
    float lo=0,hi=1,best=fabsf(qa)<fabsf(qb)?0:1,bv=fminf(fabsf(qa),fabsf(qb));
    for(int it=0;it<24;it++){
        float t=(lo+hi)*.5f;if(t<=lo||t>=hi)break;
        Vec p=mix(a,b,t);HeightPoint q=height_sample(f,h,other,p.x,p.y);
        if(!isfinite(q.q)||!isfinite(q.z))return (Vec){SNAN,SNAN,SNAN};
        if(fabsf(q.q)<bv){bv=fabsf(q.q);best=t;}if(q.q==0){best=t;break;}
        if((q.q<0)==(qa<0)){lo=t;qa=q.q;}else hi=t;
    }
    Vec p=mix(a,b,best);HeightPoint q=height_sample(f,h,other,p.x,p.y);p.z=q.z;return p;
}
static void add_height_vertex(Vec *roots,int *count,Vec p,float eps)
{
    if(!isfinite(p.x)||!isfinite(p.y)||!isfinite(p.z))return;
    for(int i=0;i<*count;i++)if(dist2(roots[i],p)<=eps*eps)return;
    if(*count<6)roots[(*count)++]=p;
}
static int clip_height_z(IMapView const *v,Vec *a,Vec *b)
{
    float dz=b->z-a->z,lo=0,hi=1;
    if(dz==0)return a->z>=v->zmin&&a->z<=v->zmax;
    float t0=(v->zmin-a->z)/dz,t1=(v->zmax-a->z)/dz;
    if(t0>t1){float t=t0;t0=t1;t1=t;}
    lo=fmaxf(lo,t0);hi=fminf(hi,t1);if(lo>hi)return 0;
    Vec aa=*a,bb=*b;*a=mix(aa,bb,lo);*b=mix(aa,bb,hi);
    a->z=fmaxf(v->zmin,fminf(v->zmax,a->z));b->z=fmaxf(v->zmin,fminf(v->zmax,b->z));return 1;
}
static void height_triangle(Fields *f,IMapView const *v,Program const *h,int other,IMapPixel *map,
                            Vec const p[3],HeightPoint const q[3],IMapStats *st)
{
    for(int i=0;i<3;i++)if(!isfinite(q[i].z)||!isfinite(q[i].q)){if(st->invalid<INT_MAX)st->invalid++;return;}
    Vec roots[6];int n=0;float eps=2*v->radius/v->xy_div*1e-5f;
    float scale=fmaxf(fabsf(q[0].q),fmaxf(fabsf(q[1].q),fabsf(q[2].q)));
    for(int i=0;i<3;i++){
        int j=(i+1)%3;float a=q[i].q,b=q[j].q;
        if(a==0)add_height_vertex(roots,&n,p[i],eps);
        if(b==0)add_height_vertex(roots,&n,p[j],eps);
        if(a!=0&&b!=0&&(a<0)!=(b<0))
            add_height_vertex(roots,&n,height_edge(f,h,other,p[i],p[j],a,b),eps);
    }
    if(n==0)return;
    if(n>2){
        /* If the two fields coincide on sampled vertices, the projection
           may cover an area. Mark only individually verified sample points;
           do not invent a connection through an indeterminate triangle. */
        for(int i=0;i<n;i++)if(roots[i].z>=v->zmin&&roots[i].z<=v->zmax)
            segment(f,map,v,roots[i],roots[i],st);
        if(st->ambiguous<INT_MAX)st->ambiguous++;
        return;
    }
    if(n==1){if(roots[0].z>=v->zmin&&roots[0].z<=v->zmax)segment(f,map,v,roots[0],roots[0],st);return;}
    for(int i=1;i<=3;i++){
        Vec m=mix(roots[0],roots[1],i*.25f);HeightPoint val=height_sample(f,h,other,m.x,m.y);
        if(!isfinite(val.z)||!isfinite(val.q)||fabsf(val.q)>scale*.18f+FLT_EPSILON*32){
            if(st->ambiguous<INT_MAX)st->ambiguous++;
            return;
        }
    }
    if(clip_height_z(v,&roots[0],&roots[1]))segment(f,map,v,roots[0],roots[1],st);
}
static int build_affine_height(RaySurface const s[2],EvalVars const *base,IMapView const *v,
                               Program const *h,int which,IMapPixel *map,ProgressFn progress,void *ctx,IMapStats *st)
{
    memset(st,0,sizeof *st);for(int i=0;i<IMAP_SIDE*IMAP_SIDE;i++)map[i]=(IMapPixel){EMPTY_Z,0,0};
    Fields f={s,*base,st};int n=v->xy_div,d=n+1,other=1-which;
    HeightPoint *rows=malloc((size_t)2*d*sizeof *rows);if(!rows)return -2;
    HeightPoint *a=rows,*b=rows+d;float unit=2*v->radius/n;
    for(int x=0;x<=n;x++)a[x]=height_sample(&f,h,other,v->center_x-v->radius+x*unit,v->center_y-v->radius);
    for(int y=0;y<n;y++){
        if(progress&&!progress(y,n,ctx)){free(rows);return 0;}
        float wy=v->center_y-v->radius+(y+1)*unit;
        for(int x=0;x<=n;x++)b[x]=height_sample(&f,h,other,v->center_x-v->radius+x*unit,wy);
        for(int x=0;x<n;x++){
            if((x&31)==0&&progress&&!progress(y,n,ctx)){free(rows);return 0;}
            float wx=v->center_x-v->radius+x*unit,sy=wy-unit;
            Vec c[4]={{wx,sy,a[x].z},{wx+unit,sy,a[x+1].z},{wx+unit,wy,b[x+1].z},{wx,wy,b[x].z}};
            HeightPoint q[4]={a[x],a[x+1],b[x+1],b[x]};
            Vec p0[3]={c[0],c[1],c[2]},p1[3]={c[0],c[2],c[3]};
            HeightPoint q0[3]={q[0],q[1],q[2]},q1[3]={q[0],q[2],q[3]};
            height_triangle(&f,v,h,other,map,p0,q0,st);height_triangle(&f,v,h,other,map,p1,q1,st);
        }
        HeightPoint *tmp=a;a=b;b=tmp;
    }
    free(rows);return 1;
}
int imap_build_auto(RaySurface const s[2],EvalVars const *base,IMapView const *v,
                    Program const *height[2],IMapPixel *map,ProgressFn progress,void *ctx,IMapStats *st)
{
    if(!map||!st||!valid(v,s,base))return -1;
    if(height)for(int i=0;i<2;i++)
        if(height_affine(height[i])&&expr_program_continuous(s[1-i].program))
            return build_affine_height(s,base,v,height[i],i,map,progress,ctx,st);
    return imap_build(s,base,v,map,progress,ctx,st);
}

int imap_nearest(IMapPixel const *map,IMapView const *v,float x,float y,IMapPoint *out)
{
    if(!map||!v||!out||!isfinite(x)||!isfinite(y)||v->radius<=0||v->zmax<=v->zmin)return 0;
    float best=INFINITY;int found=0;float unit=2*v->radius/(IMAP_SIDE-1);
    for(int iy=0;iy<IMAP_SIDE;iy++)for(int ix=0;ix<IMAP_SIDE;ix++){
        IMapPixel const *p=&map[(size_t)iy*IMAP_SIDE+ix];if(p->z==EMPTY_Z)continue;
        float wx=v->center_x-v->radius+(ix+p->dx/254.0f)*unit,wy=v->center_y+v->radius-(iy+p->dy/254.0f)*unit;
        float dx=wx-x,dy=wy-y,d=dx*dx+dy*dy;
        if(d<best){best=d;found=1;*out=(IMapPoint){wx,wy,v->zmin+(p->z+32767)*(v->zmax-v->zmin)/65534.0f};}
    }
    return found;
}

/* Refine both constraints while staying in a pixel's world-coordinate
   footprint. We solve in the best-conditioned pair of coordinate directions,
   holding the third fixed. Failure remains an unverified candidate, not a hit. */
static int point_refine(Fields *f,IMapView const *v,Vec *p,float const lo[3],float const hi[3])
{
    for(int it=0;it<12;it++){
        if(verify_root(f,v,p->x,p->y,p->z))return 1;
        Pair q=both(f,p->x,p->y,p->z);
        if(!isfinite(q.f)||!isfinite(q.g))return 0;
        float a[3]={p->x,p->y,p->z},j[2][3];
        for(int axis=0;axis<3;axis++){
            float width=hi[axis]-lo[axis];
            float h=fmaxf(width*.015f,FLT_EPSILON*16*(1+fabsf(a[axis])));
            Vec plus=*p,minus=*p;
            if(axis==0){plus.x+=h;minus.x-=h;}
            else if(axis==1){plus.y+=h;minus.y-=h;}
            else {plus.z+=h;minus.z-=h;}
            Pair u=both(f,plus.x,plus.y,plus.z),w=both(f,minus.x,minus.y,minus.z);
            float uv[2]={u.f,u.g},wv[2]={w.f,w.g},qv[2]={q.f,q.g};
            for(int k=0;k<2;k++){
                if(isfinite(uv[k])&&isfinite(wv[k]))j[k][axis]=(uv[k]-wv[k])/(2*h);
                else if(isfinite(uv[k]))j[k][axis]=(uv[k]-qv[k])/h;
                else if(isfinite(wv[k]))j[k][axis]=(qv[k]-wv[k])/h;
                else j[k][axis]=0;
            }
        }
        float scale0=fmaxf(fabsf(j[0][0]),fmaxf(fabsf(j[0][1]),fabsf(j[0][2])));
        float scale1=fmaxf(fabsf(j[1][0]),fmaxf(fabsf(j[1][1]),fabsf(j[1][2])));
        if(!isfinite(scale0)||!isfinite(scale1)||scale0<=1e-30f||scale1<=1e-30f)return 0;
        for(int k=0;k<3;k++){j[0][k]/=scale0;j[1][k]/=scale1;}
        float f0=q.f/scale0,g0=q.g/scale1,best=0;int ia=-1,ib=-1;
        for(int i=0;i<3;i++)for(int jx=i+1;jx<3;jx++){
            float det=j[0][i]*j[1][jx]-j[0][jx]*j[1][i];
            if(fabsf(det)>fabsf(best)){best=det;ia=i;ib=jx;}
        }
        if(ia<0||fabsf(best)<1e-6f)return 0;
        float da=(-f0*j[1][ib]+g0*j[0][ib])/best;
        float db=(-g0*j[0][ia]+f0*j[1][ia])/best;
        if(!isfinite(da)||!isfinite(db))return 0;
        int moved=0;
        for(int trial=0;trial<12;trial++){
            float factor=ldexpf(1.0f,-trial),b[3]={a[0],a[1],a[2]};
            b[ia]+=factor*da;b[ib]+=factor*db;
            if(b[ia]<lo[ia]||b[ia]>hi[ia]||b[ib]<lo[ib]||b[ib]>hi[ib])continue;
            Pair next=both(f,b[0],b[1],b[2]);
            if(!isfinite(next.f)||!isfinite(next.g))continue;
            float old=fabsf(q.f/scale0)+fabsf(q.g/scale1);
            float val=fabsf(next.f/scale0)+fabsf(next.g/scale1);
            if(val<old){*p=(Vec){b[0],b[1],b[2]};moved=1;break;}
        }
        if(!moved)return 0;
    }
    return verify_root(f,v,p->x,p->y,p->z);
}
static void pixel_hit_add(Fields *f,IMapView const *v,IMapPixelHits *out,Vec p,
                          float const lo[3],float const hi[3])
{
    if(p.x<lo[0]||p.x>hi[0]||p.y<lo[1]||p.y>hi[1]||p.z<lo[2]||p.z>hi[2])return;
    if(!point_refine(f,v,&p,lo,hi))return;
    float eps=fmaxf((v->zmax-v->zmin)/(v->cursor_steps*1000000.0f),
                    FLT_EPSILON*8*(1+fabsf(p.x)+fabsf(p.y)+fabsf(p.z)));
    for(int i=0;i<out->count;i++){
        Vec a={out->point[i].x,out->point[i].y,out->point[i].z};
        if(dist2(a,p)<=eps*eps)return;
    }
    if(out->count>=IMAP_MAX_ROOTS){out->overflow=1;return;}
    int i=out->count++;while(i>0&&out->point[i-1].z>p.z){out->point[i]=out->point[i-1];i--;}
    out->point[i]=(IMapPoint){p.x,p.y,p.z};
}
static void pixel_hit_face(Fields *f,IMapView const *v,IMapPixelHits *out,
                           Vec a,Vec b,Vec c,Vec d,Pair qa,Pair qb,Pair qc,Pair qd,
                           float const lo[3],float const hi[3])
{
    Vec r;
    if(face(f,a,b,c,qa,qb,qc,&r))pixel_hit_add(f,v,out,r,lo,hi);
    if(face(f,a,c,d,qa,qc,qd,&r))pixel_hit_add(f,v,out,r,lo,hi);
}
int imap_pixel_search(RaySurface const s[2],EvalVars const *base,IMapView const *v,
                      int ix,int iy,IMapPixel const *map,ProgressFn progress,void *ctx,IMapPixelHits *out)
{
    if(!out||!valid(v,s,base)||ix<0||ix>=IMAP_SIDE||iy<0||iy>=IMAP_SIDE)return -1;
    memset(out,0,sizeof *out);Fields f={s,*base,NULL};
    float unit=2*v->radius/(IMAP_SIDE-1);
    float cx=v->center_x-v->radius+ix*unit,cy=v->center_y+v->radius-iy*unit;
    float lo[3]={fmaxf(v->center_x-v->radius,cx-unit*.5f),
                 fmaxf(v->center_y-v->radius,cy-unit*.5f),v->zmin};
    float hi[3]={fminf(v->center_x+v->radius,cx+unit*.5f),
                 fminf(v->center_y+v->radius,cy+unit*.5f),v->zmax};
    if(map){
        IMapPixel const *a=&map[(size_t)iy*IMAP_SIDE+ix];
        if(a->z!=EMPTY_Z){
            Vec p={cx+a->dx*unit/254.0f,cy-a->dy*unit/254.0f,
                   v->zmin+(a->z+32767)*(v->zmax-v->zmin)/65534.0f};
            pixel_hit_add(&f,v,out,p,lo,hi);
        }
    }
    /* Independently probe the pixel centre as well as its boundary faces.
       This finds vertical coincidences and some interior features which have
       no intersection with the four side faces. The centre is a candidate,
       not a substitute for scanning the rest of the pixel. */
    IMapRoots central;int central_rc=imap_search(s,base,v,cx,cy,progress,ctx,&central);
    if(central_rc<=0)return central_rc;
    for(int i=0;i<central.count;i++)pixel_hit_add(&f,v,out,(Vec){cx,cy,central.z[i]},lo,hi);
    if(central.interval){
        pixel_hit_add(&f,v,out,(Vec){cx,cy,v->zmin},lo,hi);
        pixel_hit_add(&f,v,out,(Vec){cx,cy,(v->zmin+v->zmax)*.5f},lo,hi);
        pixel_hit_add(&f,v,out,(Vec){cx,cy,v->zmax},lo,hi);
        if(out->count)out->overflow=1; /* many sampled Z, not an exhaustive list */
    }
    if(central.overflow)out->overflow=1;
    if(out->overflow)return 1;
    Pair prev[4],next[4];float xp[4]={lo[0],hi[0],hi[0],lo[0]},yp[4]={lo[1],lo[1],hi[1],hi[1]};
    float dz=(v->zmax-v->zmin)/v->z_div;
    for(int j=0;j<4;j++)prev[j]=both(&f,xp[j],yp[j],v->zmin);
    for(int z=0;z<v->z_div;z++){
        if((z&7)==0&&progress&&!progress(z,v->z_div,ctx))return 0;
        float z0=v->zmin+z*dz,z1=z==v->z_div-1?v->zmax:v->zmin+(z+1)*dz;
        Vec bottom[4],top[4];
        for(int j=0;j<4;j++){
            bottom[j]=(Vec){xp[j],yp[j],z0};top[j]=(Vec){xp[j],yp[j],z1};
            next[j]=both(&f,xp[j],yp[j],z1);
            if(!isfinite(prev[j].f)||!isfinite(prev[j].g)||!isfinite(next[j].f)||!isfinite(next[j].g))out->invalid++;
        }
        pixel_hit_face(&f,v,out,bottom[0],bottom[1],bottom[2],bottom[3],
                       prev[0],prev[1],prev[2],prev[3],lo,hi);
        for(int j=0;j<4;j++){
            int k=(j+1)&3;
            pixel_hit_face(&f,v,out,bottom[j],bottom[k],top[k],top[j],
                           prev[j],prev[k],next[k],next[j],lo,hi);
        }
        if(z==v->z_div-1)
            pixel_hit_face(&f,v,out,top[0],top[1],top[2],top[3],next[0],next[1],next[2],next[3],lo,hi);
        memcpy(prev,next,sizeof prev);
        if(out->overflow)break; /* intentionally bounded UI list; user sees + */
    }
    return 1;
}
