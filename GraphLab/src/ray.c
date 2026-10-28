/* Direct ray casting of general implicit functions.
 * The field is sampled on a coarse grid only to identify candidate cells.
 * For each screen pixel, the ray is searched within those cells using the
 * actual equation. Normals are estimated from the equation, not polygons.
 * All sampling, root finding and numerical normals are approximations.
 */
#include "ray.h"
#include "camera_near.h"
#include "platform.h"
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>

#define RNAN (0.0f/0.0f)
#define RMIN(a,b) ((a)<(b)?(a):(b))
#define RMAX(a,b) ((a)>(b)?(a):(b))

typedef struct { float x,y,z; } RVec;
typedef struct {
    Program const *program; EvalVars vars; unsigned char *cells;
    int implicit,slot,evaluations;
} RayField;
typedef struct { RVec origin,dir; } Ray;
typedef struct { float t; int field; RVec point; } RayHit;

static float field_at(RayField *f,float x,float y,float z)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    f->evaluations++;
    float value=expr_eval_real(f->program,&f->vars);
    if(!isfinite(value))return RNAN;
    return f->implicit?value:z-value;
}
static float ray_value(RayField *f,Volume const *v,Ray const *ray,float t,RVec *world)
{
    RVec p={v->x+v->radius*(ray->origin.x+ray->dir.x*t),
            v->y+v->radius*(ray->origin.y+ray->dir.y*t),
            v->z+v->radius*(ray->origin.z+ray->dir.z*t)};
    if(world)*world=p;
    return field_at(f,p.x,p.y,p.z);
}
static int declared_period(Program const *p,float period,float step,int n)
{
    if(!expr_program_uses(p,V_X))return 1;
    if(!isfinite(period)||period<=0||!isfinite(step)||step<=0)return 0;
    float intervals=period/step;if(intervals<1||intervals>n)return 0;
    int result=(int)lroundf(intervals);
    return fabsf(intervals-result)<=.000001f?result:0;
}
static void set_cell(unsigned char *bits,size_t cell){bits[cell>>3]|=(unsigned char)(1u<<(cell&7));}
static int get_cell(unsigned char const *bits,size_t cell){return !!(bits[cell>>3]&(1u<<(cell&7)));}

/* Bit per candidate voxel; using two rolling sample planes bounds RAM.
   The sign test can miss a surface completely contained in one cell. */
static int build_cells(RayField *field,Volume const *v,float *memory,
                       ProgressFn progress,void *ctx,RayStats *stats)
{
    int n=v->div,d=n+1;size_t plane=(size_t)d*d;
    float *previous=memory,*next=memory+plane;
    float step=2*v->radius/n;
    int period=declared_period(field->program,v->period_x,step,n);
    for(int z=0;z<=n;z++){
        if(progress && !progress(z,n+1,ctx))return 0;
        float zz=v->z-v->radius+step*z;
        for(int y=0;y<=n;y++){
            if((y&7)==0 && progress && !progress(z,n+1,ctx))return 0;
            float yy=v->y-v->radius+step*y;
            for(int x=0;x<=n;x++){
                size_t at=(size_t)y*d+x;
                if(period && x>=period)next[at]=next[at-period];
                else if(z && !field->implicit && isfinite(previous[at]))next[at]=previous[at]+step;
                else next[at]=field_at(field,v->x-v->radius+step*x,yy,zz);
            }
        }
        if(z){
            for(int y=0;y<n;y++){
                if((y&7)==0 && progress && !progress(z,n+1,ctx))return 0;
                for(int x=0;x<n;x++){
                    size_t a=(size_t)y*d+x;
                    float q[8]={previous[a],previous[a+1],previous[a+d],previous[a+d+1],
                                next[a],next[a+1],next[a+d],next[a+d+1]};
                    int neg=0,pos=0,zero=0;
                    for(int j=0;j<8;j++)if(isfinite(q[j])){
                        if(q[j]<0)neg=1;else if(q[j]>0)pos=1;else zero=1;
                    }
                    if((neg && pos)||(zero && (neg||pos))){
                        size_t cell=((size_t)(z-1)*n+y)*n+x;
                        set_cell(field->cells,cell);stats->occupied++;
                    }
                }
            }
        }
        float *swap=previous;previous=next;next=swap;
    }
    return 1;
}
static int opposite(float a,float b)
{
    return isfinite(a)&&isfinite(b)&&((a<0&&b>=0)||(b<0&&a>=0));
}
/* Refine only a sign-changing interval, and reject many poles. */
static int refine_root(RayField *field,Volume const *v,Ray const *ray,
                       float a,float b,float va,float vb,float *answer)
{
    if(va==0){*answer=a;return 1;}if(vb==0){*answer=b;return 1;}
    float scale=fmaxf(fabsf(va),fabsf(vb)),small=fminf(fabsf(va),fabsf(vb));
    if(!isfinite(scale)||scale<=0)return 0;
    float lo=a,hi=b,vlo=va,vhi=vb,best=INFINITY,best_t=(a+b)*.5f;
    for(int k=0;k<7;k++){
        float t;
        if(k==0){float den=fabsf(vlo)+fabsf(vhi);t=den>0&&isfinite(den)?lo+(hi-lo)*(fabsf(vlo)/den):(lo+hi)*.5f;}
        else t=(lo+hi)*.5f;
        float value=ray_value(field,v,ray,t,NULL);
        if(!isfinite(value))return 0;
        if(fabsf(value)<best){best=fabsf(value);best_t=t;}
        if(value==0||fabsf(value)<scale*.00001f)break;
        if((value<0)==(vlo<0)){lo=t;vlo=value;}else{hi=t;vhi=value;}
    }
    if(best>small*.45f+scale*.0001f)return 0;
    *answer=best_t;return 1;
}
static int candidate_hit(RayField *field,Volume const *v,Ray const *ray,
                         float start,float end,float *answer)
{
    if(end<start)return 0;
    /* Four checks inside each candidate interval reduce, but cannot remove,
       missed close pairs of roots or tangent (double) roots. */
    float prev_t=start,prev=ray_value(field,v,ray,start,NULL);
    if(isfinite(prev)&&prev==0){*answer=start;return 1;}
    for(int j=1;j<=4;j++){
        float t=start+(end-start)*(j*.25f);
        float q=ray_value(field,v,ray,t,NULL);
        if(isfinite(q)&&q==0){*answer=t;return 1;}
        if(opposite(prev,q) && refine_root(field,v,ray,prev_t,t,prev,q,answer))return 1;
        prev_t=t;prev=q;
    }
    return 0;
}
static int ray_box(Ray const *r,float near_clip,float *first,float *last)
{
    float near=near_clip,far=1e30f;
    float origin[3]={r->origin.x,r->origin.y,r->origin.z};
    float dir[3]={r->dir.x,r->dir.y,r->dir.z};
    for(int i=0;i<3;i++){
        if(fabsf(dir[i])<1e-12f){if(origin[i]<-1||origin[i]>1)return 0;continue;}
        float t0=(-1-origin[i])/dir[i],t1=(1-origin[i])/dir[i];
        if(t0>t1){float tmp=t0;t0=t1;t1=tmp;}
        near=RMAX(near,t0);far=RMIN(far,t1);
        if(near>far)return 0;
    }
    if(!isfinite(near)||!isfinite(far)||far<near_clip)return 0;
    *first=near;*last=far;return 1;
}
static int first_hit(RayField *fields,int count,Volume const *v,Ray const *r,float near_clip,RayHit *hit)
{
    float first,last;if(!ray_box(r,near_clip,&first,&last))return 0;
    int n=v->div;float step=2.0f/n;
    float origin[3]={r->origin.x,r->origin.y,r->origin.z},dir[3]={r->dir.x,r->dir.y,r->dir.z};
    int cell[3],increment[3];float next[3],delta[3];
    /* A small probe inside the box chooses the cell on the entering side. */
    float inside=first+fminf((last-first)*.0001f,1e-5f);
    for(int j=0;j<3;j++){
        float p=origin[j]+dir[j]*inside;
        float coord=(p+1.0f)/step;
        if(!isfinite(coord))return 0;
        int k=coord<=0?0:coord>=n?n-1:(int)coord;
        cell[j]=k;
        if(fabsf(dir[j])<1e-12f){increment[j]=0;next[j]=delta[j]=1e30f;}
        else {
            increment[j]=dir[j]>0?1:-1;
            float boundary=-1+step*(k+(increment[j]>0));
            next[j]=(boundary-origin[j])/dir[j];delta[j]=step/fabsf(dir[j]);
            if(next[j]<first)next[j]=first;
        }
    }
    float start=first;
    /* At most 3*n plane crossings; the additional bound protects against
       degenerate float arithmetic for extreme camera coordinates. */
    for(int iteration=0;iteration<3*n+8;iteration++){
        if(cell[0]<0||cell[0]>=n||cell[1]<0||cell[1]>=n||cell[2]<0||cell[2]>=n||start>last)break;
        float end=RMIN(last,RMIN(next[0],RMIN(next[1],next[2])));
        if(end<start)end=start;
        size_t index=((size_t)cell[2]*n+cell[1])*n+cell[0];
        int found=-1;float nearest=end+1;
        for(int j=0;j<count;j++)if(get_cell(fields[j].cells,index)){
            float t;if(candidate_hit(&fields[j],v,r,start,RMIN(end,nearest),&t) && t<nearest){nearest=t;found=j;}
        }
        if(found>=0){
            hit->t=nearest;hit->field=found;
            hit->point=(RVec){v->x+v->radius*(r->origin.x+r->dir.x*nearest),
                              v->y+v->radius*(r->origin.y+r->dir.y*nearest),
                              v->z+v->radius*(r->origin.z+r->dir.z*nearest)};
            return 1;
        }
        if(end>=last)break;
        /* Cross every tied plane; tolerance is relative to avoid a stuck ray. */
        float tol=1e-6f*(1+fabsf(end));int advanced=0;
        for(int j=0;j<3;j++)if(next[j]<=end+tol){cell[j]+=increment[j];next[j]+=delta[j];advanced=1;}
        if(!advanced)break;
        start=end;
    }
    return 0;
}
static float derivative(RayField *f,RVec p,int axis,float epsilon,float center)
{
    RVec a=p,b=p;
    if(axis==0){a.x+=epsilon;b.x-=epsilon;}
    else if(axis==1){a.y+=epsilon;b.y-=epsilon;}
    else {a.z+=epsilon;b.z-=epsilon;}
    float plus=field_at(f,a.x,a.y,a.z),minus=field_at(f,b.x,b.y,b.z);
    if(isfinite(plus)&&isfinite(minus))return (plus-minus)*.5f;
    if(isfinite(plus)&&isfinite(center))return plus-center;
    if(isfinite(minus)&&isfinite(center))return center-minus;
    return 0;
}
static int hit_color(RayField *f,Volume const *v,RVec p,int pixel_x,int pixel_y)
{
    /* Scale epsilon to the search grid and float resolution. */
    float epsilon=RMAX(2*v->radius/v->div*.03f,FLT_EPSILON*8*(1+RMAX(fabsf(p.x),RMAX(fabsf(p.y),fabsf(p.z)))));
    float center=field_at(f,p.x,p.y,p.z);
    float x=derivative(f,p,0,epsilon,center),y=derivative(f,p,1,epsilon,center),z=derivative(f,p,2,epsilon,center);
    float scale=RMAX(fabsf(x),RMAX(fabsf(y),fabsf(z))),light=.45f;
    if(isfinite(scale)&&scale>1e-30f){
        x/=scale;y/=scale;z/=scale;float len=sqrtf(x*x+y*y+z*z);
        if(isfinite(len)&&len>0)light=fabsf((x*.35f+y*.45f+z*.82f)/len);
    }
    /* RGB565 has limited shades. Ordered dithering softens visible bands
       without changing the ray/surface geometry or allocating another buffer. */
    static const unsigned char dither[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    int bright=(int)(10+19*light+(dither[pixel_y&3][pixel_x&3]+.5f)/16.0f);
    if(bright<8)bright=8;
    if(bright>31)bright=31;
    switch(f->slot%3){
        case 0:return C_RGB(bright/4,bright*3/4,bright);
        case 1:return C_RGB(bright,bright/3,bright/4);
        default:return C_RGB(bright/3,bright,bright/3);
    }
}
int ray3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *v,Camera const *cam,ProgressFn progress,void *ctx,RayStats *stats)
{
    if(!surfaces||!base||!v||!cam||!stats||count<1||count>RAY_MAX_SURFACES||
       v->div<4||v->div>RAY_MAX_DIV||!isfinite(v->radius)||v->radius<=0||
       !isfinite(v->x)||!isfinite(v->y)||!isfinite(v->z)||
       !isfinite(cam->eye_x)||!isfinite(cam->eye_y)||!isfinite(cam->eye_z)||
       !isfinite(cam->yaw)||!isfinite(cam->pitch)||!isfinite(cam->zoom)||cam->zoom<.1f||cam->zoom>5)return -1;
    memset(stats,0,sizeof *stats);
    int n=v->div,d=n+1;size_t cells=(size_t)n*n*n,bytes=(cells+7)/8,plane=(size_t)d*d;
    unsigned char *bits=calloc((size_t)count,bytes);float *memory=malloc(2*plane*sizeof(float));
    if(!bits||!memory){free(bits);free(memory);return -2;}
    RayField fields[RAY_MAX_SURFACES];memset(fields,0,sizeof fields);
    for(int i=0;i<count;i++){
        if(!surfaces[i].program){free(bits);free(memory);return -1;}
        fields[i].program=surfaces[i].program;fields[i].slot=surfaces[i].slot;
        fields[i].vars=*base;fields[i].cells=bits+(size_t)i*bytes;
        fields[i].implicit=fields[i].program->has_equal||expr_program_uses(fields[i].program,V_Z);
        if(!build_cells(&fields[i],v,memory,progress,ctx,stats)){
            stats->canceled=1;for(int j=0;j<=i;j++)stats->evaluations+=fields[j].evaluations;
            free(bits);free(memory);return 0;
        }
    }
    free(memory);
    float cy=cosf(cam->yaw),sy=sinf(cam->yaw),cp=cosf(cam->pitch),sp=sinf(cam->pitch);
    float focal=72.0f*2.8f*cam->zoom,near_clip=camera_near_normalized(v->radius);
    RVec origin={(cam->eye_x-v->x)/v->radius,(cam->eye_y-v->y)/v->radius,(cam->eye_z-v->z)/v->radius};
    if(!isfinite(origin.x)||!isfinite(origin.y)||!isfinite(origin.z)){free(bits);return -1;}
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++){
        if(progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){
            stats->canceled=1;for(int i=0;i<count;i++)stats->evaluations+=fields[i].evaluations;
            free(bits);return 0;
        }
        float vertical=((PLOT_Y0+PLOT_Y1)*.5f-y)/focal;
        float ry=-sp*vertical-cp,rz=cp*vertical-sp;
        for(int x=PLOT_X0;x<=PLOT_X1;x++){
            if((x&31)==0 && progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){
                stats->canceled=1;for(int i=0;i<count;i++)stats->evaluations+=fields[i].evaluations;
                free(bits);return 0;
            }
            float horizontal=(x-(PLOT_X0+PLOT_X1)*.5f)/focal;
            Ray r={origin,{cy*horizontal+sy*ry,-sy*horizontal+cy*ry,rz}};
            RayHit hit;stats->rays++;
            if(first_hit(fields,count,v,&r,near_clip,&hit)){
                gint_vram[(size_t)y*DWIDTH+x]=(uint16_t)hit_color(&fields[hit.field],v,hit.point,x,y);
                stats->hits++;
            }
        }
    }
    for(int i=0;i<count;i++)stats->evaluations+=fields[i].evaluations;
    free(bits);return 1;
}
