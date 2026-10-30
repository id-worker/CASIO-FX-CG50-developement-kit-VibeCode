#include "section.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#define SNAN (0.0f/0.0f)

typedef struct {float u,v;} SPoint;
typedef struct {SPoint a,b;} SSegment;
typedef struct {RaySurface surface;EvalVars vars;SectionView view;SectionResult *result;} SField;
static void xyz(SectionView const *view,float u,float v,float *x,float *y,float *z)
{
    if(view->plane==0){*x=u;*y=v;*z=view->fixed;}
    else if(view->plane==1){*x=u;*y=view->fixed;*z=v;}
    else {*x=view->fixed;*y=u;*z=v;}
}
static float value(SField *f,float u,float v)
{
    float x,y,z;xyz(&f->view,u,v,&x,&y,&z);
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    float q=expr_eval_real(f->surface.program,&f->vars);f->result->evaluations++;
    if(!isfinite(q)){f->result->invalid++;return SNAN;}
    if(!f->surface.program->has_equal && !expr_program_uses(f->surface.program,V_Z))q=z-q;
    return isfinite(q)?q:SNAN;
}
static float dist2(SPoint a,SPoint b){float u=a.u-b.u,v=a.v-b.v;return u*u+v*v;}
static SPoint mix(SPoint a,SPoint b,float t){return (SPoint){a.u+(b.u-a.u)*t,a.v+(b.v-a.v)*t};}
/* The same sort of bounded root/pole guard as the existing contour renderer:
   it rejects many, but necessarily not all, discontinuities. */
static int edge_root(SField *f,SPoint a,SPoint b,float va,float vb,SPoint *out)
{
    if(!isfinite(va)||!isfinite(vb))return 0;
    if(va==0){*out=a;return 1;}
    if(vb==0){*out=b;return 1;}
    if((va<0)==(vb<0))return 0;
    float scale=fmaxf(fabsf(va),fabsf(vb)),small=fminf(fabsf(va),fabsf(vb));
    if(!isfinite(scale)||scale<=0)return 0;
    float lo=0,hi=1,vl=va,best=INFINITY,best_t=.5f;
    for(int i=0;i<8;i++){
        float t;
        if(!i){float den=fabsf(va)+fabsf(vb);t=isfinite(den)&&den>0?fabsf(va)/den:.5f;}
        else t=(lo+hi)*.5f;
        SPoint p=mix(a,b,t);float q=value(f,p.u,p.v);
        if(!isfinite(q))return 0;
        if(fabsf(q)<best){best=fabsf(q);best_t=t;}
        if(q==0||best<=scale*1e-6f)break;
        if((q<0)==(vl<0)){lo=t;vl=q;}else hi=t;
    }
    if(best>small*.45f+scale*.0001f)return 0;
    *out=mix(a,b,best_t);return 1;
}
static int make_segments(SField *f,SPoint const c[4],float const q[4],float step,SSegment out[4],int *nonisolated)
{
    for(int i=0;i<4;i++)if(!isfinite(q[i]))return 0;
    if(q[0]==0&&q[1]==0&&q[2]==0&&q[3]==0){*nonisolated=1;return 0;}
    int flat=0;
    for(int i=0;i<4;i++)if(q[i]==0&&q[(i+1)%4]==0){out[flat++]=(SSegment){c[i],c[(i+1)%4]};}
    if(flat)return flat;
    SPoint roots[4];int n=0;
    for(int i=0;i<4;i++){
        SPoint p;
        if(!edge_root(f,c[i],c[(i+1)%4],q[i],q[(i+1)%4],&p))continue;
        int duplicate=0;
        for(int k=0;k<n;k++)if(dist2(p,roots[k])<step*step*1e-10f){duplicate=1;break;}
        if(!duplicate)roots[n++]=p;
    }
    if(n==1){out[0]=(SSegment){roots[0],roots[0]};return 1;}
    if(n==2){out[0]=(SSegment){roots[0],roots[1]};return 1;}
    if(n==4){
        /* Resolve the marching-squares saddle using a center sample. */
        float center=value(f,(c[0].u+c[2].u)*.5f,(c[0].v+c[2].v)*.5f);
        if(!isfinite(center))return 0;
        int pair=(center<0)==(q[0]<0);
        if(pair){out[0]=(SSegment){roots[0],roots[1]};out[1]=(SSegment){roots[2],roots[3]};}
        else {out[0]=(SSegment){roots[3],roots[0]};out[1]=(SSegment){roots[1],roots[2]};}
        return 2;
    }
    return 0;
}
static float cross(SPoint a,SPoint b){return a.u*b.v-a.v*b.u;}
static SPoint diff(SPoint a,SPoint b){return (SPoint){a.u-b.u,a.v-b.v};}
/* Segment intersection produces a candidate only. Newton refinement and
   residual/gradient checks below must accept it before it becomes a result. */
static int line_candidate(SSegment a,SSegment b,float step,SPoint *out,int *overlap)
{
    SPoint r=diff(a.b,a.a),s=diff(b.b,b.a),d=diff(b.a,a.a);
    float aa=dist2(a.a,a.b),bb=dist2(b.a,b.b),tiny=step*step*1e-10f;
    if(aa<tiny||bb<tiny){
        SPoint p=aa<tiny?a.a:b.a;SSegment line=aa<tiny?b:a;
        SPoint dir=diff(line.b,line.a);float len=dist2(line.a,line.b);
        float t=len>tiny?((p.u-line.a.u)*dir.u+(p.v-line.a.v)*dir.v)/len:0;
        if(t<0)t=0;
        if(t>1)t=1;
        if(dist2(p,mix(line.a,line.b,t))<=step*step*1e-6f){*out=p;return 1;}
        return 0;
    }
    float det=cross(r,s);
    if(fabsf(det)<1e-7f*step*step){
        /* Coincident line pieces have no unique point to report. */
        if(fabsf(cross(d,r))<=1e-5f*step*step){
            float t0=(d.u*r.u+d.v*r.v)/aa;
            SPoint end=diff(b.b,a.a);float t1=(end.u*r.u+end.v*r.v)/aa;
            if(fmaxf(fminf(t0,t1),0)<=fminf(fmaxf(t0,t1),1)+1e-4f)*overlap=1;
        }
        return 0;
    }
    float t=cross(d,s)/det,w=cross(d,r)/det;
    if(t<-.001f||t>1.001f||w<-.001f||w>1.001f)return 0;
    if(t<0)t=0;
    if(t>1)t=1;
    *out=mix(a.a,a.b,t);return 1;
}
static int refine(SField *f,SField *g,SPoint *p,SPoint c0,float step)
{
    float h=step*.01f;
    float tiny=fmaxf(step*2e-4f,FLT_EPSILON*32*(1+fmaxf(fabsf(p->u),fabsf(p->v))));
    for(int it=0;it<7;it++){
        float f0=value(f,p->u,p->v),g0=value(g,p->u,p->v);
        float fx1=value(f,p->u+h,p->v),fx0=value(f,p->u-h,p->v);
        float fy1=value(f,p->u,p->v+h),fy0=value(f,p->u,p->v-h);
        float gx1=value(g,p->u+h,p->v),gx0=value(g,p->u-h,p->v);
        float gy1=value(g,p->u,p->v+h),gy0=value(g,p->u,p->v-h);
        if(!isfinite(f0)||!isfinite(g0)||!isfinite(fx1)||!isfinite(fx0)||!isfinite(fy1)||
           !isfinite(fy0)||!isfinite(gx1)||!isfinite(gx0)||!isfinite(gy1)||!isfinite(gy0))return 0;
        float fx=(fx1-fx0)/(2*h),fy=(fy1-fy0)/(2*h),gx=(gx1-gx0)/(2*h),gy=(gy1-gy0)/(2*h);
        float scale=fmaxf(fabsf(fx),fabsf(fy))*fmaxf(fabsf(gx),fabsf(gy));
        float det=fx*gy-fy*gx;
        if(!isfinite(det)||!isfinite(scale)||scale<=1e-25f||fabsf(det)<scale*1e-4f)return 0;
        float du=(-f0*gy+g0*fy)/det,dv=(-g0*fx+f0*gx)/det;
        if(!isfinite(du)||!isfinite(dv)||fabsf(du)>step*1.5f||fabsf(dv)>step*1.5f)return 0;
        if(fabsf(du)<=tiny&&fabsf(dv)<=tiny){
            if(fabsf(f0)>(fabsf(fx)+fabsf(fy))*tiny+1e-7f ||
               fabsf(g0)>(fabsf(gx)+fabsf(gy))*tiny+1e-7f)return 0;
            return p->u>=c0.u-step*.1f&&p->u<=c0.u+step*1.1f&&
                   p->v>=c0.v-step*.1f&&p->v<=c0.v+step*1.1f;
        }
        p->u+=du;p->v+=dv;
    }
    return 0;
}
static void add_point(SectionResult *res,SectionView const *view,SPoint p,float step)
{
    float x,y,z;xyz(view,p.u,p.v,&x,&y,&z);
    for(int i=0;i<res->count;i++){
        float dx=res->point[i].x-x,dy=res->point[i].y-y,dz=res->point[i].z-z;
        if(dx*dx+dy*dy+dz*dz<step*step*.01f)return;
    }
    if(res->count>=SECTION_MAX_POINTS){res->overflow=1;return;}
    res->point[res->count++]=(SectionPoint){x,y,z};
}
int section_compute(RaySurface const *surface,int count,EvalVars const *base,SectionView const *view,
                    SectionSegmentFn segment,void *segment_ctx,ProgressFn progress,void *progress_ctx,SectionResult *res)
{
    if(!surface||!base||!view||!res||count<1||count>2||view->plane<0||view->plane>2||
       !isfinite(view->fixed)||!isfinite(view->radius)||view->radius<=0||view->divisions<4||view->divisions>128)return -1;
    for(int i=0;i<count;i++)if(!surface[i].program||surface[i].slot<0||surface[i].slot>2)return -1;
    memset(res,0,sizeof *res);
    if(fabsf(view->fixed)>view->radius){res->outside=1;return 1;}
    int n=view->divisions;float step=2*view->radius/n;
    float *memory=malloc((size_t)count*2*(n+1)*sizeof *memory);if(!memory)return -2;
    float *prev[2]={NULL,NULL},*next[2]={NULL,NULL};SField field[2];
    for(int k=0;k<count;k++){
        field[k]=(SField){surface[k],*base,*view,res};res->whole[k]=1;
        prev[k]=memory+(size_t)k*2*(n+1);next[k]=prev[k]+n+1;
        for(int x=0;x<=n;x++){
            if((x&15)==0&&progress&&!progress(0,n,progress_ctx)){free(memory);return 0;}
            prev[k][x]=value(&field[k],-view->radius+x*step,-view->radius);
            if(prev[k][x]!=0)res->whole[k]=0;
        }
    }
    for(int y=0;y<n;y++){
        if(progress&&!progress(y,n,progress_ctx)){free(memory);return 0;}
        float v=-view->radius+y*step;
        for(int k=0;k<count;k++)for(int x=0;x<=n;x++){
            if((x&31)==0&&progress&&!progress(y,n,progress_ctx)){free(memory);return 0;}
            next[k][x]=value(&field[k],-view->radius+x*step,v+step);
            if(next[k][x]!=0)res->whole[k]=0;
        }
        for(int x=0;x<n;x++){
            if((x&31)==0&&progress&&!progress(y,n,progress_ctx)){free(memory);return 0;}
            float u=-view->radius+x*step;
            SPoint c[4]={{u,v},{u+step,v},{u+step,v+step},{u,v+step}};
            SSegment lines[2][4];int size[2]={0,0};
            for(int k=0;k<count;k++){
                float q[4]={prev[k][x],prev[k][x+1],next[k][x+1],next[k][x]};
                size[k]=make_segments(&field[k],c,q,step,lines[k],&res->nonisolated);
                for(int j=0;j<size[k];j++){
                    res->segments[k]++;
                    if(segment)segment(lines[k][j].a.u,lines[k][j].a.v,lines[k][j].b.u,lines[k][j].b.v,surface[k].slot,segment_ctx);
                }
            }
            if(count==2)for(int i=0;i<size[0];i++)for(int j=0;j<size[1];j++){
                SPoint p;int overlap=0;
                if(line_candidate(lines[0][i],lines[1][j],step,&p,&overlap)){
                    if(refine(&field[0],&field[1],&p,c[0],step) &&
                       fabsf(p.u)<=view->radius+step*.001f&&fabsf(p.v)<=view->radius+step*.001f)
                        add_point(res,view,p,step);
                }else if(overlap)res->nonisolated=1;
            }
        }
        for(int k=0;k<count;k++){float *tmp=prev[k];prev[k]=next[k];next[k]=tmp;}
    }
    free(memory);return 1;
}
int section_auto(RaySurface const *surface,int count,EvalVars const *base,int plane,float radius,
                 ProgressFn progress,void *ctx,float *fixed,int *found)
{
    if(!fixed||!found||!isfinite(radius)||radius<=0||plane<0||plane>2)return -1;
    int best=-1;float chosen=0;int any=0;
    /* Center first, then evenly spaced interior and boundary sections. */
    static int const positions[]={0,-1,1,-2,2,-3,3,-4,4};
    for(unsigned i=0;i<sizeof positions/sizeof *positions;i++){
        if(progress&&!progress((int)i,9,ctx))return 0;
        SectionView view={plane,radius*positions[i]/4.0f,radius,20};SectionResult result;
        int rc=section_compute(surface,count,base,&view,NULL,NULL,progress,ctx,&result);
        if(rc!=1)return rc;
        int score;
        if(count==2){
            int trace=(result.whole[0]&&result.segments[1])||(result.whole[1]&&result.segments[0]);
            int overlap=result.nonisolated&&result.segments[0]&&result.segments[1];
            score=(result.count+(trace||overlap?1:0))*10000+result.segments[0]+result.segments[1];
            if(result.count||trace||overlap)any=1;
        }
        else {score=result.segments[0];if(result.segments[0])any=1;}
        /* A whole-plane coincidence is not a finite set of crossings. */
        if(score>best){best=score;chosen=view.fixed;}
    }
    *fixed=chosen;*found=any;return 1;
}
