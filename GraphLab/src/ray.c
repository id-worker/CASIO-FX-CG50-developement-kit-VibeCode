/* Direct ray casting of general implicit functions.
 * The field is sampled on a coarse grid only to identify candidate cells.
 * For each screen pixel, the ray is searched within those cells using the
 * actual equation. Normals are estimated from the equation, not polygons.
 * All sampling, root finding and numerical normals are approximations.
 */
#include "ray.h"
#include "trigfast.h"
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
    TrigFast fast;
    int implicit,slot,evaluations,continuous,fast_evaluations;
} RayField;
typedef struct { RVec origin,dir; } Ray;
typedef struct { float t; int field; RVec point; } RayHit;

static float field_at(RayField *f,float x,float y,float z)
{
    f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
    f->evaluations++;
    float value;
    if(trigfast_value(&f->fast,x,y,z,f->implicit?0:z,&value))f->fast_evaluations++;
    else value=expr_eval_real(f->program,&f->vars);
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
static int declared_period(Program const *p,EvalVars const *base,float period,float step,int n)
{
    if(!expr_program_uses(p,V_X))return 1;
    if(!isfinite(period)||period<=0||!isfinite(step)||step<=0)return 0;
    float intervals=period/step;if(intervals<1||intervals>n)return 0;
    if(!expr_proves_x_period(p,base,period))return 0;
    int result=(int)lroundf(intervals);
    return fabsf(intervals-result)<=.000001f?result:0;
}
static void set_cell(unsigned char *bits,size_t cell){bits[cell>>3]|=(unsigned char)(1u<<(cell&7));}
static int get_cell(unsigned char const *bits,size_t cell){return !!(bits[cell>>3]&(1u<<(cell&7)));}

/* Bit per candidate voxel; using two rolling sample planes bounds RAM.
   The sign test can miss a surface completely contained in one cell. */
/* A coarse point on a sign-changing grid edge is useful for the preview
   after the camera moves beyond the originally visible pixels. It never
   participates in the final ray result. Only continuous fields qualify. */
static void capture_edge_point(Preview3D *preview,RayField const *field,
                               Volume const *v,int x,int y,int z,float const q[8])
{
    if(!preview||!preview->aux||!field->continuous)return;
    static const unsigned char edge[12][2]={{0,1},{2,3},{4,5},{6,7},
        {0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    unsigned start=((unsigned)x*11u+(unsigned)y*7u+(unsigned)z*3u)%12u;
    for(unsigned j=0;j<12;j++){
        int a=edge[(start+j)%12u][0],b=edge[(start+j)%12u][1];
        if(!isfinite(q[a])||!isfinite(q[b])||q[a]==q[b]||
           !((q[a]<=0&&q[b]>=0)||(q[b]<=0&&q[a]>=0)))continue;
        float aa=fabsf(q[a]),bb=fabsf(q[b]),t;
        if(aa==0)t=0;else if(bb==0)t=1;
        else if(aa>bb)t=1/(1+bb/aa);
        else {float k=aa/bb;t=k/(1+k);}
        float u=(x+(a&1)+t*((b&1)-(a&1)))*2/v->div-1;
        float w=(y+!!(a&2)+t*(!!(b&2)-!!(a&2)))*2/v->div-1;
        float h=(z+!!(a&4)+t*(!!(b&4)-!!(a&4)))*2/v->div-1;
        uint16_t color=field->slot==0?C_RGB(6,18,25):field->slot==1?C_RGB(25,8,6):C_RGB(8,25,8);
        preview3d_aux_add(preview,u,w,h,color);return;
    }
}
static int build_cells(RayField *field,Volume const *v,float *memory,Preview3D *preview,
                       ProgressFn progress,void *ctx,RayStats *stats)
{
    int n=v->div,d=n+1;size_t plane=(size_t)d*d;
    float *previous=memory,*next=memory+plane;
    float step=2*v->radius/n;
    int period=declared_period(field->program,&field->vars,v->period_x,step,n);
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
                        capture_edge_point(preview,field,v,x,y,z-1,q);
                    }
                }
            }
        }
        float *swap=previous;previous=next;next=swap;
    }
    return 1;
}
/* Height fields need only a 2-D sample lattice. Nine samples per xy cell and
   a curvature-dependent safety margin help discover extrema between the four
   original corners. This is conservative in many ordinary cases, but is not
   an interval bound: arbitrarily fast oscillations can still be missed. */
static int build_height_cells(RayField *field,Volume const *v,Preview3D *preview,
                              ProgressFn progress,void *ctx,RayStats *stats)
{
    int n=v->div,d=2*n+1;float half=v->radius/n,step=2*v->radius/n;
    float *samples=malloc((size_t)d*d*sizeof *samples);if(!samples)return -2;
    int period=declared_period(field->program,&field->vars,v->period_x,half,2*n);
    for(int y=0;y<d;y++){
        if(progress&&!progress(y,d,ctx)){free(samples);return 0;}
        for(int x=0;x<d;x++){
            if((x&31)==0&&progress&&!progress(y,d,ctx)){free(samples);return 0;}
            size_t at=(size_t)y*d+x;
            if(period&&x>=period)samples[at]=samples[at-period];
            else {
                field->vars.v[V_X]=v->x-v->radius+half*x;
                field->vars.v[V_Y]=v->y-v->radius+half*y;
                field->vars.v[V_Z]=v->z;
                field->evaluations++;samples[at]=expr_eval_real(field->program,&field->vars);
            }
        }
    }
    for(int y=0;y<n;y++){
        if((y&7)==0&&progress&&!progress(y,n,ctx)){free(samples);return 0;}
        for(int x=0;x<n;x++){
            float q[9],low=INFINITY,high=-INFINITY,curvature=0;
            for(int j=0;j<3;j++)for(int i=0;i<3;i++){
                float value=samples[(size_t)(2*y+j)*d+2*x+i];q[j*3+i]=value;
                if(isfinite(value)){if(value<low)low=value;if(value>high)high=value;}
            }
            if(!isfinite(low)||!isfinite(high))continue;
            if(preview&&preview->aux&&field->continuous&&isfinite(q[4])){
                float h=(q[4]-v->z)/v->radius;
                uint16_t color=field->slot==0?C_RGB(6,18,25):field->slot==1?C_RGB(25,8,6):C_RGB(8,25,8);
                preview3d_aux_add(preview,(2.0f*x+1)/n-1,(2.0f*y+1)/n-1,h,color);
            }
            for(int j=0;j<3;j++){
                float a=q[j*3],b=q[j*3+1],c=q[j*3+2];
                if(isfinite(a)&&isfinite(b)&&isfinite(c))curvature=fmaxf(curvature,fabsf(a-2*b+c));
                a=q[j];b=q[3+j];c=q[6+j];
                if(isfinite(a)&&isfinite(b)&&isfinite(c))curvature=fmaxf(curvature,fabsf(a-2*b+c));
            }
            float margin=.10f*curvature+FLT_EPSILON*8*(1+fmaxf(fabsf(low),fabsf(high)));
            float lo=(low-margin-(v->z-v->radius))/step;
            float hi=(high+margin-(v->z-v->radius))/step;
            if(isfinite(lo)&&isfinite(hi)&&(hi<0||lo>n))continue;
            /* Overflow while estimating a bound is not evidence of an empty
               column; conservatively search the whole column instead. */
            int z0=0,z1=n-1;
            if(isfinite(lo)&&isfinite(hi)){
                z0=lo<=0?0:lo>=n?n-1:(int)floorf(lo);
                z1=hi<=0?0:hi>=n?n-1:(int)floorf(hi);
                if(z0>0&&lo==(float)z0)z0--;
            }
            for(int z=z0;z<=z1;z++){
                set_cell(field->cells,((size_t)z*n+y)*n+x);stats->occupied++;
            }
        }
    }
    free(samples);return 1;
}
static int same_float(float a,float b){return memcmp(&a,&b,sizeof a)==0;}
static int same_program(Program const *a,Program const *b)
{
    if(a->size!=b->size||a->max_stack!=b->max_stack||a->variables!=b->variables||
       a->has_equal!=b->has_equal||a->relation!=b->relation||a->size>EXPR_CODE)return 0;
    for(int i=0;i<a->size;i++)if(a->code[i].op!=b->code[i].op||a->code[i].aux!=b->code[i].aux||
                                   !same_float(a->code[i].number,b->code[i].number))return 0;
    return 1;
}
static int cache_matches(RayGridCache const *cache,RayField const *fields,int count,
                         EvalVars const *base,Volume const *v)
{
    if(!cache||!cache->valid||!cache->bits||cache->count!=count||
       cache->volume.div!=v->div||!same_float(cache->volume.x,v->x)||
       !same_float(cache->volume.y,v->y)||!same_float(cache->volume.z,v->z)||
       !same_float(cache->volume.radius,v->radius)||!same_float(cache->volume.period_x,v->period_x)||
       memcmp(cache->base.v,base->v,sizeof base->v)||
       !same_float(cache->base.cz.re,base->cz.re)||!same_float(cache->base.cz.im,base->cz.im)||
       !same_float(cache->base.cc.re,base->cc.re)||!same_float(cache->base.cc.im,base->cc.im)||
       cache->base.real_overrides!=base->real_overrides)return 0;
    for(int i=0;i<count;i++)if(cache->height[i]!=(unsigned char)!fields[i].implicit||
                                  !same_program(&cache->signature[i],fields[i].program))return 0;
    return 1;
}
void ray_grid_cache_clear(RayGridCache *cache)
{
    if(cache){free(cache->bits);memset(cache,0,sizeof *cache);}
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
    if(!field->continuous && best>small*.45f+scale*.0001f)return 0;
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
static float derivative(RayField *f,RVec p,int axis,float epsilon,float *center,int *have_center)
{
    RVec a=p,b=p;
    if(axis==0){a.x+=epsilon;b.x-=epsilon;}
    else if(axis==1){a.y+=epsilon;b.y-=epsilon;}
    else {a.z+=epsilon;b.z-=epsilon;}
    float plus=field_at(f,a.x,a.y,a.z),minus=field_at(f,b.x,b.y,b.z);
    if(isfinite(plus)&&isfinite(minus))return (plus-minus)*.5f;
    if(!*have_center){*center=field_at(f,p.x,p.y,p.z);*have_center=1;}
    if(isfinite(plus)&&isfinite(*center))return plus-*center;
    if(isfinite(minus)&&isfinite(*center))return *center-minus;
    return 0;
}
static int hit_color(RayField *f,Volume const *v,RVec p,int pixel_x,int pixel_y)
{
    /* Scale epsilon to the search grid and float resolution. */
    float epsilon=RMAX(2*v->radius/v->div*.03f,FLT_EPSILON*8*(1+RMAX(fabsf(p.x),RMAX(fabsf(p.y),fabsf(p.z)))));
    float center=0;int have_center=0;
    float x=derivative(f,p,0,epsilon,&center,&have_center);
    float y=derivative(f,p,1,epsilon,&center,&have_center);
    /* For z=f(x,y), the z derivative is exactly one. In the same
       unscaled units returned by derivative(), its value is epsilon. */
    float z=f->implicit?derivative(f,p,2,epsilon,&center,&have_center):epsilon;
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
int ray3d_render_ex(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *v,Camera const *cam,RayRenderOptions const *options,
                 ProgressFn progress,void *ctx,RayStats *stats)
{
    if(!surfaces||!base||!v||!cam||!stats||count<1||count>RAY_MAX_SURFACES||
       v->div<4||v->div>RAY_MAX_DIV||!isfinite(v->radius)||v->radius<=0||
       !isfinite(v->x)||!isfinite(v->y)||!isfinite(v->z)||
       !isfinite(cam->eye_x)||!isfinite(cam->eye_y)||!isfinite(cam->eye_z)||
       !isfinite(cam->yaw)||!isfinite(cam->pitch)||!isfinite(cam->zoom)||cam->zoom<.1f||cam->zoom>5)return -1;
    memset(stats,0,sizeof *stats);
    RVec origin={(cam->eye_x-v->x)/v->radius,(cam->eye_y-v->y)/v->radius,(cam->eye_z-v->z)/v->radius};
    if(!isfinite(origin.x)||!isfinite(origin.y)||!isfinite(origin.z))return -1;
    int n=v->div,d=n+1;size_t cells=(size_t)n*n*n,bytes=(cells+7)/8,plane=(size_t)d*d;
    RayField fields[RAY_MAX_SURFACES];memset(fields,0,sizeof fields);
    for(int i=0;i<count;i++){
        if(!surfaces[i].program||surfaces[i].slot<0||surfaces[i].slot>2)return -1;
        Program const *height=options?options->height[i]:NULL;
        if(height&&(height->has_equal||expr_program_uses(height,V_Z)))return -1;
        fields[i].program=height?height:surfaces[i].program;fields[i].slot=surfaces[i].slot;
        fields[i].vars=*base;
        fields[i].implicit=height?0:(fields[i].program->has_equal||expr_program_uses(fields[i].program,V_Z));
        fields[i].continuous=expr_program_continuous(fields[i].program);
#ifndef GL_DISABLE_TRIGFAST
        float bound=v->radius+fmaxf(fabsf(v->x),fmaxf(fabsf(v->y),fabsf(v->z)));
        (void)trigfast_prepare(&fields[i].fast,fields[i].program,base,bound);
#endif
        if(!fields[i].implicit)stats->height_fields++;
    }
    RayGridCache *cache=options?options->grid:NULL;
    Preview3D *preview=options?options->preview:NULL;
    unsigned char *bits=NULL;int owned=1;
    if(cache_matches(cache,fields,count,base,v)&&cache->bytes==(size_t)count*bytes){
        bits=cache->bits;owned=0;stats->grid_reused=1;stats->occupied=cache->occupied;
    }else{
        if(cache)ray_grid_cache_clear(cache);
        bits=calloc((size_t)count,bytes);if(!bits)return -2;
        if(preview)(void)preview3d_aux_begin(preview); /* optional */
        float *memory=NULL;int result=1;
        for(int i=0;i<count;i++){
            fields[i].cells=bits+(size_t)i*bytes;
            if(!fields[i].implicit)result=build_height_cells(&fields[i],v,preview,progress,ctx,stats);
            if(fields[i].implicit||result==-2){
                if(!memory)memory=malloc(2*plane*sizeof(float));
                if(!memory){result=-2;break;}
                result=build_cells(&fields[i],v,memory,preview,progress,ctx,stats);
            }
            if(result!=1)break;
        }
        free(memory);
        if(result!=1){
            stats->canceled=result==0;
            for(int i=0;i<count;i++){stats->evaluations+=fields[i].evaluations;stats->fast_evaluations+=fields[i].fast_evaluations;}
            free(bits);return result;
        }
        if(cache){
            cache->bits=bits;cache->bytes=(size_t)count*bytes;cache->count=count;
            cache->occupied=stats->occupied;cache->volume=*v;cache->base=*base;
            for(int i=0;i<count;i++){
                cache->signature[i]=*fields[i].program;cache->height[i]=(unsigned char)!fields[i].implicit;
            }
            cache->valid=1;owned=0;
        }
    }
    for(int i=0;i<count;i++)fields[i].cells=bits+(size_t)i*bytes;
    float cy=cosf(cam->yaw),sy=sinf(cam->yaw),cp=cosf(cam->pitch),sp=sinf(cam->pitch);
    float focal=72.0f*2.8f*cam->zoom,near_clip=camera_near_normalized(v->radius);
    int result=1;
    for(int y=PLOT_Y0;y<=PLOT_Y1;y++){
        if(progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){result=0;break;}
        float vertical=((PLOT_Y0+PLOT_Y1)*.5f-y)/focal;
        float ry=-sp*vertical-cp,rz=cp*vertical-sp;
        for(int x=PLOT_X0;x<=PLOT_X1;x++){
            if((x&31)==0 && progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){result=0;break;}
            float horizontal=(x-(PLOT_X0+PLOT_X1)*.5f)/focal;
            Ray r={origin,{cy*horizontal+sy*ry,-sy*horizontal+cy*ry,rz}};
            RayHit hit;stats->rays++;
            if(first_hit(fields,count,v,&r,near_clip,&hit)){
                uint16_t color=(uint16_t)hit_color(&fields[hit.field],v,hit.point,x,y);
                gint_vram[(size_t)y*DWIDTH+x]=color;stats->hits++;
                if(options&&options->preview)
                    preview3d_add(options->preview,x,y,hit.point.x,hit.point.y,hit.point.z,color);
            }
        }
        if(!result)break;
    }
    stats->canceled=result==0;
    for(int i=0;i<count;i++){stats->evaluations+=fields[i].evaluations;stats->fast_evaluations+=fields[i].fast_evaluations;}
    if(owned)free(bits);
    return result;
}
int ray3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *v,Camera const *cam,ProgressFn progress,void *ctx,RayStats *stats)
{
    return ray3d_render_ex(surfaces,count,base,v,cam,NULL,progress,ctx,stats);
}
