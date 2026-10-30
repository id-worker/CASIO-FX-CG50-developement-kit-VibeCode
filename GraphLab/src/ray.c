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

/* Target binary32 classification/absolute value exactly match OpenLibm's
   bit operations while avoiding a library call in the per-sample loops. */
#if defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__==4 && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128
#define GL_RAY_BINARY32 1
#if defined(__GNUC__)
#define RINLINE static inline __attribute__((always_inline))
#else
#define RINLINE static inline
#endif
RINLINE uint32_t ray_bits(float value)
{
    union {float f;uint32_t u;} bits;bits.f=value;return bits.u;
}
RINLINE int ray_isfinite(float value)
{
    return (ray_bits(value)&0x7f800000u)!=0x7f800000u;
}
RINLINE int ray_zero(float value)
{
    return (ray_bits(value)&0x7fffffffu)==0;
}
/* Only call this predicate for finite values; -0 is not negative in C. */
RINLINE int ray_negative_finite(float value)
{
    uint32_t bits=ray_bits(value);return (bits>>31)&&((bits&0x7fffffffu)!=0);
}
RINLINE float ray_fabs(float value)
{
    union {float f;uint32_t u;} bits;bits.f=value;bits.u&=0x7fffffffu;
    return bits.f;
}
/* Exact offset+value when the offset is signed zero. Preserve the IEEE sign
   of zero and let the original addition handle all NaN payloads. */
RINLINE float ray_add_offset(float offset,float value)
{
    uint32_t ob=ray_bits(offset);
    if((ob&0x7fffffffu)==0){
        uint32_t vb=ray_bits(value);
        if((vb&0x7fffffffu)<=0x7f800000u){
            if(ob==0&&vb==0x80000000u)return 0.0f;
            return value;
        }
    }
    return offset+value;
}
#undef RINLINE
#else
#define GL_RAY_BINARY32 0
#define ray_isfinite(value) isfinite(value)
#define ray_fabs(value) fabsf(value)
static inline int ray_zero(float value){return isfinite(value)&&value==0;}
static inline int ray_negative_finite(float value){return value<0;}
static inline float ray_add_offset(float offset,float value){return offset+value;}
#endif

typedef struct { float x,y,z; } RVec;
/* Optional direct execution of one VM operation, with the same binary32
   operation and operand order. More complicated programs use the VM. */
typedef struct {uint8_t valid,op,a,b;float constant_a,constant_b;} RaySmall;
typedef struct {
    Program const *program; EvalVars vars; unsigned char *cells;
    TrigFast fast;RaySmall small;
    int implicit,slot,evaluations,continuous,fast_evaluations,real_fast;
} RayField;
typedef struct { RVec origin,dir; } Ray;
typedef struct { float t; int field; RVec point; int clip_axis; } RayHit;
typedef struct { float t,value; int valid; } RayCachedSample;
/* All rays in one screen row have the same vertical direction. Cache only
   identical divisions; do not approximate roots or reorder any geometry. */
typedef struct {
    float origin,dir,step,inv_step,t0,t1,delta,boundary[RAY_MAX_DIV+1];
    unsigned char valid[RAY_MAX_DIV+1];
    int parallel,outside,power_of_two;
} RayRowZ;
typedef struct { float x,y; } RayColumn;

static int small_operand(Instr const *in,uint8_t *which,float *value)
{
    if(in->op==I_NUM){*which=255;*value=in->number;return 1;}
    if(in->op==I_VAR&&in->aux<V_COUNT){*which=in->aux;*value=0;return 1;}
    return 0;
}
static void small_prepare(RayField *field)
{
    Program const *p=field->program;RaySmall *s=&field->small;
    memset(s,0,sizeof *s);
    if(!field->real_fast||p->size<1||p->size>3||
       !small_operand(&p->code[0],&s->a,&s->constant_a))return;
    if(p->size==1){s->op=255;s->valid=1;return;}
    if(p->size==2){
        int op=p->code[1].op;
        if(op==I_NEG||op==I_SQUARE||op==I_CUBE){s->op=(uint8_t)op;s->valid=1;}
        return;
    }
    int op=p->code[2].op;
    if((op==I_ADD||op==I_SUB||op==I_MUL||op==I_DIV)&&
       small_operand(&p->code[1],&s->b,&s->constant_b)){
        s->op=(uint8_t)op;s->valid=1;
    }
}
static float small_arg(RayField const *f,uint8_t which,float constant,float x,float y,float z)
{
    if(which==255)return constant;
    if(which==V_X)return x;
    if(which==V_Y)return y;
    if(which==V_Z)return z;
    return f->vars.v[which];
}
static float small_value(RayField const *f,float x,float y,float z)
{
    RaySmall const *s=&f->small;
    float a=small_arg(f,s->a,s->constant_a,x,y,z),b;
    switch(s->op){
    case 255:return a;
    case I_NEG:return -a;
    case I_SQUARE:return a*a;
    case I_CUBE:return a*a*a;
    default:break;
    }
    b=small_arg(f,s->b,s->constant_b,x,y,z);
    switch(s->op){
    case I_ADD:return a+b;
    case I_SUB:return a-b;
    case I_MUL:return a*b;
    case I_DIV:return b==0?RNAN:a/b;
    default:return RNAN;
    }
}
#if defined(GL_HOST) || defined(GL_SMALL_TEST)
/* Test-only entry, not included in the calculator executable. */
int ray_small_host_test(Program const *p,EvalVars const *vars,float x,float y,float z,float *result)
{
    if(!p||!vars||!result)return 0;
    RayField f;memset(&f,0,sizeof f);f.program=p;f.vars=*vars;f.real_fast=expr_real_fast_usable(p);
    small_prepare(&f);if(!f.small.valid)return 0;
    *result=small_value(&f,x,y,z);return 1;
}
#endif

static float field_at(RayField *f,float x,float y,float z)
{
    f->evaluations++;
    float value;
    if(f->fast.valid && trigfast_value(&f->fast,x,y,z,f->implicit?0:z,&value))f->fast_evaluations++;
    else if(f->small.valid)value=small_value(f,x,y,z);
    else {
        /* The fast plan takes coordinates directly. Only the VM needs its
           variable array updated; every fallback writes all three first. */
        f->vars.v[V_X]=x;f->vars.v[V_Y]=y;f->vars.v[V_Z]=z;
        value=f->real_fast?expr_eval_real_fast(f->program,&f->vars):expr_eval_real(f->program,&f->vars);
    }
    if(!ray_isfinite(value))return RNAN;
    return f->implicit?value:z-value;
}
static float ray_value(RayField *f,Volume const *v,Ray const *ray,float t,RVec *world)
{
    RVec p={ray_add_offset(v->x,v->radius*(ray->origin.x+ray->dir.x*t)),
            ray_add_offset(v->y,v->radius*(ray->origin.y+ray->dir.y*t)),
            ray_add_offset(v->z,v->radius*(ray->origin.z+ray->dir.z*t))};
    if(world)*world=p;
    return field_at(f,p.x,p.y,p.z);
}
static int declared_period(Program const *p,EvalVars const *base,float period,float step,int n)
{
    if(!expr_program_uses(p,V_X))return 1;
    if(!ray_isfinite(period)||period<=0||!ray_isfinite(step)||step<=0)return 0;
    float intervals=period/step;if(intervals<1||intervals>n)return 0;
    if(!expr_proves_x_period(p,base,period))return 0;
    int result=(int)lroundf(intervals);
    return ray_fabs(intervals-result)<=.000001f?result:0;
}
static void set_cell(unsigned char *bits,size_t cell){bits[cell>>3]|=(unsigned char)(1u<<(cell&7));}
static int get_cell(unsigned char const *bits,size_t cell){return !!(bits[cell>>3]&(1u<<(cell&7)));}

/* One bit per candidate search cell; two rolling sample planes bound RAM.
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
        if(!ray_isfinite(q[a])||!ray_isfinite(q[b])||q[a]==q[b]||
           !((q[a]<=0&&q[b]>=0)||(q[b]<=0&&q[a]>=0)))continue;
        float aa=ray_fabs(q[a]),bb=ray_fabs(q[b]),t;
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
                else if(z && !field->implicit && ray_isfinite(previous[at]))next[at]=previous[at]+step;
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
                    int candidate=0;
                    if(field->program->relation>=REL_LT){
                        /* Inequalities are volumes. Undefined points are outside;
                           retain cells straddling a domain edge as well as a
                           sign change. No cubes or polygon faces are drawn. */
                        int inside=0,outside=0;
                        for(int j=0;j<8;j++){
                            if(expr_relation_true(field->program->relation,q[j]))inside=1;
                            else outside=1;
                        }
                        candidate=inside&&outside;
                    }else{
                        int neg=0,pos=0,zero=0;
                        for(int j=0;j<8;j++)if(ray_isfinite(q[j])){
                            if(q[j]<0)neg=1;else if(q[j]>0)pos=1;else zero=1;
                        }
                        candidate=(neg&&pos)||(zero&&(neg||pos));
                    }
                    if(candidate){
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
                field->evaluations++;
                samples[at]=field->real_fast?expr_eval_real_fast(field->program,&field->vars):
                                             expr_eval_real(field->program,&field->vars);
            }
        }
    }
    for(int y=0;y<n;y++){
        if((y&7)==0&&progress&&!progress(y,n,ctx)){free(samples);return 0;}
        for(int x=0;x<n;x++){
            float q[9],low=INFINITY,high=-INFINITY,curvature=0;
            for(int j=0;j<3;j++)for(int i=0;i<3;i++){
                float value=samples[(size_t)(2*y+j)*d+2*x+i];q[j*3+i]=value;
                if(ray_isfinite(value)){if(value<low)low=value;if(value>high)high=value;}
            }
            if(!ray_isfinite(low)||!ray_isfinite(high))continue;
            if(preview&&preview->aux&&field->continuous&&ray_isfinite(q[4])){
                float h=(q[4]-v->z)/v->radius;
                uint16_t color=field->slot==0?C_RGB(6,18,25):field->slot==1?C_RGB(25,8,6):C_RGB(8,25,8);
                preview3d_aux_add(preview,(2.0f*x+1)/n-1,(2.0f*y+1)/n-1,h,color);
            }
            for(int j=0;j<3;j++){
                float a=q[j*3],b=q[j*3+1],c=q[j*3+2];
                if(ray_isfinite(a)&&ray_isfinite(b)&&ray_isfinite(c))curvature=fmaxf(curvature,ray_fabs(a-2*b+c));
                a=q[j];b=q[3+j];c=q[6+j];
                if(ray_isfinite(a)&&ray_isfinite(b)&&ray_isfinite(c))curvature=fmaxf(curvature,ray_fabs(a-2*b+c));
            }
            float margin=.10f*curvature+FLT_EPSILON*8*(1+fmaxf(ray_fabs(low),ray_fabs(high)));
            float lo=(low-margin-(v->z-v->radius))/step;
            float hi=(high+margin-(v->z-v->radius))/step;
            if(ray_isfinite(lo)&&ray_isfinite(hi)&&(hi<0||lo>n))continue;
            /* Overflow while estimating a bound is not evidence of an empty
               column; conservatively search the whole column instead. */
            int z0=0,z1=n-1;
            if(ray_isfinite(lo)&&ray_isfinite(hi)){
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
static int same_float(float a,float b)
{
#if defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__==4 && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128
    union {float f;uint32_t u;} x,y;x.f=a;y.f=b;
    return x.u==y.u;
#else
    return memcmp(&a,&b,sizeof a)==0;
#endif
}
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
    return ray_isfinite(a)&&ray_isfinite(b)&&ray_negative_finite(a)!=ray_negative_finite(b);
}
/* Refine only a sign-changing interval, and reject many poles. */
static int refine_root(RayField *field,Volume const *v,Ray const *ray,
                       float a,float b,float va,float vb,float *answer)
{
    if(ray_zero(va)){*answer=a;return 1;}if(ray_zero(vb)){*answer=b;return 1;}
    float scale=fmaxf(ray_fabs(va),ray_fabs(vb)),small=fminf(ray_fabs(va),ray_fabs(vb));
    if(!ray_isfinite(scale)||scale<=0)return 0;
    float lo=a,hi=b,vlo=va,vhi=vb,best=INFINITY,best_t=(a+b)*.5f;
    float close_enough=scale*.00001f;
#if GL_RAY_BINARY32
    uint32_t best_abs=0x7f800000u,close_abs=ray_bits(close_enough)&0x7fffffffu;
#endif
    for(int k=0;k<7;k++){
        float t;
        if(k==0){float den=ray_fabs(vlo)+ray_fabs(vhi);t=den>0&&ray_isfinite(den)?lo+(hi-lo)*(ray_fabs(vlo)/den):(lo+hi)*.5f;}
        else t=(lo+hi)*.5f;
        float value=ray_value(field,v,ray,t,NULL);
        if(!ray_isfinite(value))return 0;
#if GL_RAY_BINARY32
        /* All operands here are finite/nonnegative; IEEE magnitude bits have
           exactly the same ordering, including both signs of zero. */
        uint32_t magnitude=ray_bits(value)&0x7fffffffu;
        if(magnitude<best_abs){best_abs=magnitude;best=ray_fabs(value);best_t=t;}
        if(magnitude==0||magnitude<close_abs)break;
#else
        if(ray_fabs(value)<best){best=ray_fabs(value);best_t=t;}
        if(ray_zero(value)||ray_fabs(value)<close_enough)break;
#endif
        if(ray_negative_finite(value)==ray_negative_finite(vlo)){lo=t;vlo=value;}else{hi=t;vhi=value;}
    }
    if(!field->continuous && best>small*.45f+scale*.0001f)return 0;
    *answer=best_t;return 1;
}
static int candidate_hit(RayField *field,Volume const *v,Ray const *ray,
                         float start,float end,float *answer,RayCachedSample *cached)
{
    if(end<start)return 0;
    /* Four checks inside each candidate interval reduce, but cannot remove,
       missed close pairs of roots or tangent (double) roots. Reuse only a
       bit-identical prior sample for the same surface on this same ray;
       the bit check matters for +0/-0-sensitive formulas such as atan2. */
    float prev_t=start,prev;
    if(cached->valid&&same_float(cached->t,start))prev=cached->value;
    else prev=ray_value(field,v,ray,start,NULL);
    if(ray_zero(prev)){*answer=start;return 1;}
    /* These four weights are the exact binary32 values of j*.25f. The
       interval difference is unchanged but no longer recomputed four times. */
    static const float weights[4]={.25f,.5f,.75f,1.0f};
    float span=end-start;
    for(int j=1;j<=4;j++){
        float t=start+(j==4?span:span*weights[j-1]);
        float q=ray_value(field,v,ray,t,NULL);
        if(ray_zero(q)){*answer=t;return 1;}
        if(opposite(prev,q) && refine_root(field,v,ray,prev_t,t,prev,q,answer))return 1;
        prev_t=t;prev=q;
    }
    cached->t=prev_t;cached->value=prev;cached->valid=1;
    return 0;
}
/* Build the z slab and per-boundary divisions once per scanline. The cache
   indexes the exact same float expression previously evaluated per ray. */
static void ray_row_z_init(RayRowZ *row,float origin,float dir,float step,int n)
{
    row->origin=origin;row->dir=dir;row->step=step;
    row->power_of_two=(n&(n-1))==0;row->inv_step=n*.5f;
    row->parallel=ray_fabs(dir)<1e-12f;
    row->outside=row->parallel && (origin<-1||origin>1);
    memset(row->valid,0,(size_t)n+1);
    if(!row->parallel){
        row->t0=(-1-origin)/dir;row->t1=(1-origin)/dir;
        if(row->t0>row->t1){float tmp=row->t0;row->t0=row->t1;row->t1=tmp;}
        row->delta=step/ray_fabs(dir);
    }
}
static float ray_row_z_boundary(RayRowZ *row,int k)
{
    if(!row->valid[k]){
        float boundary=-1+row->step*k;
        row->boundary[k]=(boundary-row->origin)/row->dir;row->valid[k]=1;
    }
    return row->boundary[k];
}
static int ray_box(Ray const *r,float near_clip,float *first,float *last,RayRowZ const *row)
{
    float near=near_clip,far=1e30f;
    float origin[2]={r->origin.x,r->origin.y};
    float dir[2]={r->dir.x,r->dir.y};
    for(int i=0;i<2;i++){
        if(ray_fabs(dir[i])<1e-12f){if(origin[i]<-1||origin[i]>1)return 0;continue;}
        float t0=(-1-origin[i])/dir[i],t1=(1-origin[i])/dir[i];
        if(t0>t1){float tmp=t0;t0=t1;t1=tmp;}
        near=RMAX(near,t0);far=RMIN(far,t1);
        if(near>far)return 0;
    }
    if(row->parallel){if(row->outside)return 0;}
    else{
        near=RMAX(near,row->t0);far=RMIN(far,row->t1);
        if(near>far)return 0;
    }
    if(!ray_isfinite(near)||!ray_isfinite(far)||far<near_clip)return 0;
    *first=near;*last=far;return 1;
}
static int first_hit(RayField *fields,int count,Volume const *v,Ray const *r,
                     float near_clip,RayHit *hit,RayRowZ *row)
{
    float first,last;if(!ray_box(r,near_clip,&first,&last,row))return 0;
    int n=v->div;float step=row->step;
    float origin[3]={r->origin.x,r->origin.y,r->origin.z},dir[3]={r->dir.x,r->dir.y,r->dir.z};
    int cell[3],increment[3];float next[3],delta[3];
    /* A small probe inside the box chooses the cell on the entering side. */
    float inside=first+fminf((last-first)*.0001f,1e-5f);
    for(int j=0;j<3;j++){
        float p=origin[j]+dir[j]*inside;
        /* For a power-of-two grid step (Q4/8/16/32/64), division and
           multiplication by its exact reciprocal have identical binary32
           results; all other Q settings keep the original division. */
#if defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__==4 && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128
        float coord=row->power_of_two?(p+1.0f)*row->inv_step:(p+1.0f)/step;
#else
        float coord=(p+1.0f)/step;
#endif
        if(!ray_isfinite(coord))return 0;
        int k=coord<=0?0:coord>=n?n-1:(int)coord;
        cell[j]=k;
        if(ray_fabs(dir[j])<1e-12f){increment[j]=0;next[j]=delta[j]=1e30f;}
        else {
            increment[j]=dir[j]>0?1:-1;
            if(j==2){
                next[j]=ray_row_z_boundary(row,k+(increment[j]>0));delta[j]=row->delta;
            }else{
                float boundary=-1+step*(k+(increment[j]>0));
                next[j]=(boundary-origin[j])/dir[j];delta[j]=step/ray_fabs(dir[j]);
            }
            if(next[j]<first)next[j]=first;
        }
    }
    float start=first;RayCachedSample samples[RAY_MAX_SURFACES]={{0}};
    /* At most 3*n plane crossings; the additional bound protects against
       degenerate float arithmetic for extreme camera coordinates. */
    for(int iteration=0;iteration<3*n+8;iteration++){
        if(cell[0]<0||cell[0]>=n||cell[1]<0||cell[1]>=n||cell[2]<0||cell[2]>=n||start>last)break;
        float end=RMIN(last,RMIN(next[0],RMIN(next[1],next[2])));
        if(end<start)end=start;
        size_t index=((size_t)cell[2]*n+cell[1])*n+cell[0];
        int found=-1;float nearest=end+1;
        for(int j=0;j<count;j++)if(get_cell(fields[j].cells,index)){
            float t;if(candidate_hit(&fields[j],v,r,start,RMIN(end,nearest),&t,&samples[j]) && t<nearest){nearest=t;found=j;}
        }
        if(found>=0){
            hit->t=nearest;hit->field=found;
            hit->point=(RVec){ray_add_offset(v->x,v->radius*(r->origin.x+r->dir.x*nearest)),
                              ray_add_offset(v->y,v->radius*(r->origin.y+r->dir.y*nearest)),
                              ray_add_offset(v->z,v->radius*(r->origin.z+r->dir.z*nearest))};
            return 1;
        }
        if(end>=last)break;
        /* Cross every tied plane; tolerance is relative to avoid a stuck ray. */
        float tol=1e-6f*(1+ray_fabs(end));int advanced=0;
        for(int j=0;j<3;j++)if(next[j]<=end+tol){cell[j]+=increment[j];next[j]+=delta[j];advanced=1;}
        if(!advanced)break;
        start=end;
    }
    return 0;
}
/* A volume boundary is a change of membership, not necessarily a zero:
   1/x > 0 has a real boundary at a pole, and sqrt(x) > -1 at a domain edge.
   Bisect membership rather than accepting a false equation root. As with
   all finite sampling here, narrow components and tangencies can be missed. */
static int candidate_volume(RayField *field,Volume const *v,Ray const *ray,
                            float start,float end,float *answer,RayCachedSample *cached)
{
    if(end<start)return 0;
    float prev_t=start,prev;
    if(cached->valid&&same_float(cached->t,start))prev=cached->value;
    else prev=ray_value(field,v,ray,start,NULL);
    int prev_in=expr_relation_true(field->program->relation,prev);
    static const float weights[4]={.25f,.5f,.75f,1.0f};
    float span=end-start;
    for(int j=1;j<=4;j++){
        float t=start+(j==4?span:span*weights[j-1]);
        float q=ray_value(field,v,ray,t,NULL);
        int in=expr_relation_true(field->program->relation,q);
        if(in!=prev_in){
            /* Preserve accurate existing root refinement for continuous
               finite sign changes; otherwise refine the set boundary. */
            if(opposite(prev,q)&&refine_root(field,v,ray,prev_t,t,prev,q,answer))return 1;
            float lo=prev_t,hi=t;
            for(int k=0;k<12;k++){
                float mid=(lo+hi)*.5f;
                if(mid==lo||mid==hi)break;
                float value=ray_value(field,v,ray,mid,NULL);
                if(expr_relation_true(field->program->relation,value)==prev_in)lo=mid;
                else hi=mid;
            }
            *answer=(lo+hi)*.5f;return 1;
        }
        prev_t=t;prev=q;prev_in=in;
    }
    cached->t=prev_t;cached->value=prev;cached->valid=1;
    return 0;
}
/* Axis of the clipped cube face nearest the hit in normalized coordinates.
   Only its absolute light component is needed by the two-sided shader. */
static int clip_face_axis(Ray const *r,float t)
{
    float p[3]={r->origin.x+r->dir.x*t,r->origin.y+r->dir.y*t,r->origin.z+r->dir.z*t};
    int axis=0;float best=ray_fabs(ray_fabs(p[0])-1);
    for(int i=1;i<3;i++){
        float d=ray_fabs(ray_fabs(p[i])-1);
        if(d<best){axis=i;best=d;}
    }
    return axis;
}
static void volume_set_hit(RayHit *hit,Volume const *v,Ray const *r,float t,int field,int clip_axis)
{
    hit->t=t;hit->field=field;hit->clip_axis=clip_axis;
    hit->point=(RVec){ray_add_offset(v->x,v->radius*(r->origin.x+r->dir.x*t)),
                      ray_add_offset(v->y,v->radius*(r->origin.y+r->dir.y*t)),
                      ray_add_offset(v->z,v->radius*(r->origin.z+r->dir.z*t))};
}
static int first_hit_mixed(RayField *fields,int count,Volume const *v,Ray const *r,
                     float near_clip,RayHit *hit,RayRowZ *row)
{
    float first,last;if(!ray_box(r,near_clip,&first,&last,row))return 0;
    int n=v->div;float step=row->step;
    float origin[3]={r->origin.x,r->origin.y,r->origin.z},dir[3]={r->dir.x,r->dir.y,r->dir.z};
    int cell[3],increment[3];float next[3],delta[3];
    /* A small probe inside the box chooses the cell on the entering side. */
    float probe=fminf((last-first)*.0001f,1e-5f);
    float inside=first+probe;
    /* When the eye is outside the clip cube, a filled region intersecting
       the entry face is visible there, before any other object can be hit.
       When the eye is inside, never put a fictitious plane at near_clip. */
    int eye_in_box=ray_fabs(origin[0])<=1&&ray_fabs(origin[1])<=1&&ray_fabs(origin[2])<=1;
    if(!eye_in_box){
        for(int j=0;j<count;j++)if(fields[j].program->relation>=REL_LT){
            float value=ray_value(&fields[j],v,r,inside,NULL);
            if(expr_relation_true(fields[j].program->relation,value)){
                volume_set_hit(hit,v,r,first,j,clip_face_axis(r,first));return 1;
            }
        }
    }
    for(int j=0;j<3;j++){
        float p=origin[j]+dir[j]*inside;
        /* For a power-of-two grid step (Q4/8/16/32/64), division and
           multiplication by its exact reciprocal have identical binary32
           results; all other Q settings keep the original division. */
#if defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__==4 && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128
        float coord=row->power_of_two?(p+1.0f)*row->inv_step:(p+1.0f)/step;
#else
        float coord=(p+1.0f)/step;
#endif
        if(!ray_isfinite(coord))return 0;
        int k=coord<=0?0:coord>=n?n-1:(int)coord;
        cell[j]=k;
        if(ray_fabs(dir[j])<1e-12f){increment[j]=0;next[j]=delta[j]=1e30f;}
        else {
            increment[j]=dir[j]>0?1:-1;
            if(j==2){
                next[j]=ray_row_z_boundary(row,k+(increment[j]>0));delta[j]=row->delta;
            }else{
                float boundary=-1+step*(k+(increment[j]>0));
                next[j]=(boundary-origin[j])/dir[j];delta[j]=step/ray_fabs(dir[j]);
            }
            if(next[j]<first)next[j]=first;
        }
    }
    float start=first;RayCachedSample samples[RAY_MAX_SURFACES]={{0}};
    /* At most 3*n plane crossings; the additional bound protects against
       degenerate float arithmetic for extreme camera coordinates. */
    for(int iteration=0;iteration<3*n+8;iteration++){
        if(cell[0]<0||cell[0]>=n||cell[1]<0||cell[1]>=n||cell[2]<0||cell[2]>=n||start>last)break;
        float end=RMIN(last,RMIN(next[0],RMIN(next[1],next[2])));
        if(end<start)end=start;
        size_t index=((size_t)cell[2]*n+cell[1])*n+cell[0];
        int found=-1;float nearest=end+1;
        for(int j=0;j<count;j++)if(get_cell(fields[j].cells,index)){
            float t;
            int boundary=fields[j].program->relation>=REL_LT
                ?candidate_volume(&fields[j],v,r,start,RMIN(end,nearest),&t,&samples[j])
                :candidate_hit(&fields[j],v,r,start,RMIN(end,nearest),&t,&samples[j]);
            if(boundary&&t<nearest){nearest=t;found=j;}
        }
        if(found>=0){volume_set_hit(hit,v,r,nearest,found,-1);return 1;}
        if(end>=last)break;
        /* Cross every tied plane; tolerance is relative to avoid a stuck ray. */
        float tol=1e-6f*(1+ray_fabs(end));int advanced=0;
        for(int j=0;j<3;j++)if(next[j]<=end+tol){cell[j]+=increment[j];next[j]+=delta[j];advanced=1;}
        if(!advanced)break;
        start=end;
    }
    /* A camera inside a filled region sees its far clip face if there was
       no earlier surface or material boundary. Sample just inside the box. */
    for(int j=0;j<count;j++)if(fields[j].program->relation>=REL_LT){
        float value=ray_value(&fields[j],v,r,last-probe,NULL);
        if(expr_relation_true(fields[j].program->relation,value)){
            volume_set_hit(hit,v,r,last,j,clip_face_axis(r,last));return 1;
        }
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
    if(ray_isfinite(plus)&&ray_isfinite(minus))return (plus-minus)*.5f;
    if(!*have_center){*center=field_at(f,p.x,p.y,p.z);*have_center=1;}
    if(ray_isfinite(plus)&&ray_isfinite(*center))return plus-*center;
    if(ray_isfinite(minus)&&ray_isfinite(*center))return *center-minus;
    return 0;
}
static int hit_color(RayField *f,RVec p,int pixel_x,int pixel_y,float epsilon_base)
{
    /* Scale epsilon to the search grid and float resolution. */
    float epsilon=RMAX(epsilon_base,FLT_EPSILON*8*(1+RMAX(ray_fabs(p.x),RMAX(ray_fabs(p.y),ray_fabs(p.z)))));
    float center=0;int have_center=0;
    float x=derivative(f,p,0,epsilon,&center,&have_center);
    float y=derivative(f,p,1,epsilon,&center,&have_center);
    /* For z=f(x,y), the z derivative is exactly one. In the same
       unscaled units returned by derivative(), its value is epsilon. */
    float z=f->implicit?derivative(f,p,2,epsilon,&center,&have_center):epsilon;
    float scale=RMAX(ray_fabs(x),RMAX(ray_fabs(y),ray_fabs(z))),light=.45f;
    if(ray_isfinite(scale)&&scale>1e-30f){
        x/=scale;y/=scale;z/=scale;float len=sqrtf(x*x+y*y+z*z);
        if(ray_isfinite(len)&&len>0)light=ray_fabs((x*.35f+y*.45f+z*.82f)/len);
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
/* Clipped solid faces have the box normal, not the field gradient. */
static int clip_color(RayField const *f,int axis,int pixel_x,int pixel_y)
{
    static const unsigned char dither[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
    float light=axis==0?.35f:axis==1?.45f:.82f;
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
       v->div<4||v->div>RAY_MAX_DIV||!ray_isfinite(v->radius)||v->radius<=0||
       !ray_isfinite(v->x)||!ray_isfinite(v->y)||!ray_isfinite(v->z)||
       !ray_isfinite(cam->eye_x)||!ray_isfinite(cam->eye_y)||!ray_isfinite(cam->eye_z)||
       !ray_isfinite(cam->yaw)||!ray_isfinite(cam->pitch)||!ray_isfinite(cam->zoom)||cam->zoom<.1f||cam->zoom>5)return -1;
    int screen_step=options?options->screen_step:1;
    if(screen_step==0)screen_step=1;
    if(screen_step<1||screen_step>32)return -1;
    ScreenRefine *refine=options?options->refine:NULL;
    uint16_t *layer=options?options->layer:NULL;
    int final_stream=options?options->final_stream:0;
    if(final_stream && (screen_step!=1||!refine||!refine->seed_done||
                        refine->last_step!=2||refine->grid_step!=2))return -1;
    if(refine&&(!layer||!refine_stage_allowed(refine,screen_step)))return -1;
    memset(stats,0,sizeof *stats);
    RVec origin={(cam->eye_x-v->x)/v->radius,(cam->eye_y-v->y)/v->radius,(cam->eye_z-v->z)/v->radius};
    if(!ray_isfinite(origin.x)||!ray_isfinite(origin.y)||!ray_isfinite(origin.z))return -1;
    int n=v->div,d=n+1;size_t cells=(size_t)n*n*n,bytes=(cells+7)/8,plane=(size_t)d*d;
    RayField fields[RAY_MAX_SURFACES];memset(fields,0,sizeof fields);
    int has_volume=0;
    for(int i=0;i<count;i++){
        if(!surfaces[i].program||surfaces[i].slot<0||surfaces[i].slot>2)return -1;
        Program const *height=options?options->height[i]:NULL;
        if(height&&(height->has_equal||expr_program_uses(height,V_Z)||surfaces[i].program->relation>=REL_LT))return -1;
        if(surfaces[i].program->relation>=REL_LT)has_volume=1;
        fields[i].program=height?height:surfaces[i].program;fields[i].slot=surfaces[i].slot;
        fields[i].vars=*base;
        fields[i].implicit=height?0:(fields[i].program->has_equal||expr_program_uses(fields[i].program,V_Z));
        fields[i].continuous=expr_program_continuous(fields[i].program);
        fields[i].real_fast=expr_real_fast_usable(fields[i].program);
        small_prepare(&fields[i]);
#ifndef GL_DISABLE_TRIGFAST
        float bound=v->radius+fmaxf(ray_fabs(v->x),fmaxf(ray_fabs(v->y),ray_fabs(v->z)));
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
    /* Horizontal components are identical for every scanline. The buffer is
       optional: allocation failure retains the previous per-pixel formula. */
    RayColumn *columns=malloc((size_t)PLOT_W*sizeof *columns);
    if(columns)for(int x=PLOT_X0;x<=PLOT_X1;x++){
        float horizontal=(x-(PLOT_X0+PLOT_X1)*.5f)/focal;
        columns[x-PLOT_X0]=(RayColumn){cy*horizontal,-sy*horizontal};
    }
    /* Both expressions were previously re-evaluated for every ray/hit. */
    float ray_step=2.0f/n,epsilon_base=2*v->radius/v->div*.03f;
    int result=1;
    /* The completed step-2 lattice covers even/even coordinates. Skip them
       outright rather than looping over and looking them up again. Advancing
       one row at a time lets the UI publish real intermediate results. */
    for(int y=PLOT_Y0;y<=PLOT_Y1;y+=screen_step){
        if(progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){result=0;break;}
        float vertical=((PLOT_Y0+PLOT_Y1)*.5f-y)/focal;
        float ry=-sp*vertical-cp,rz=cp*vertical-sp;
        float row_x=sy*ry,row_y=cy*ry;
        RayRowZ row;ray_row_z_init(&row,origin.z,rz,ray_step,n);
        int even_row=final_stream&& !((y-PLOT_Y0)&1);
        int x_start=PLOT_X0+even_row,x_stride=even_row?2:screen_step;
        for(int x=x_start;x<=PLOT_X1;x+=x_stride){
            int check_x=even_row?(x-x_start)/2:screen_step==1?x:(x-PLOT_X0)/screen_step;
            if((check_x&31)==0 && progress && !progress(y-PLOT_Y0,PLOT_H,ctx)){result=0;break;}
            int px=x-PLOT_X0,py=y-PLOT_Y0;uint16_t color=C_WHITE;
            /* An even seed lies wholly on the completed even/even lattice,
               so none of this pass's positions can come from that seed. */
            int reused=refine&&(!final_stream||(refine->seed_step&1))&&refine_get(refine,px,py,&color);
            if(reused){
                stats->reused_rays++;
                if(color!=C_WHITE)stats->hits++;
            }else{
                RayColumn column;
                if(columns)column=columns[px];
                else{
                    float horizontal=(x-(PLOT_X0+PLOT_X1)*.5f)/focal;
                    column=(RayColumn){cy*horizontal,-sy*horizontal};
                }
                Ray r={origin,{column.x+row_x,column.y+row_y,rz}};
                RayHit hit;stats->rays++;
                int found=has_volume?first_hit_mixed(fields,count,v,&r,near_clip,&hit,&row)
                                    :first_hit(fields,count,v,&r,near_clip,&hit,&row);
                if(found){
                    color=(uint16_t)(has_volume&&hit.clip_axis>=0
                        ?clip_color(&fields[hit.field],hit.clip_axis,x,y)
                        :hit_color(&fields[hit.field],hit.point,x,y,epsilon_base));
                    stats->hits++;
                    if(options&&options->preview)
                        preview3d_add(options->preview,x,y,hit.point.x,hit.point.y,hit.point.z,color);
                }
                if(refine&&!final_stream)refine_store(refine,px,py,color);
            }
            if(refine){
                /* Old top-left samples already occupy the correct child tile.
                   Other cached samples only need copying after non-nested
                   user-selected spacings. Misses clear previously coarse hits. */
                if(final_stream)layer[(size_t)py*PLOT_W+px]=color;
                else if(!reused||!refine_keep(refine,screen_step,px,py))
                    refine_tile(layer,px,py,screen_step,color);
            }else if(color!=C_WHITE){
                if(screen_step==1)gint_vram[(size_t)y*DWIDTH+x]=color;
                else for(int ty=y;ty<y+screen_step&&ty<=PLOT_Y1;ty++)
                    for(int tx=x;tx<x+screen_step&&tx<=PLOT_X1;tx++)
                        gint_vram[(size_t)ty*DWIDTH+tx]=color;
            }
        }
        if(!result)break;
    }
    stats->canceled=result==0;
    if(result==1&&refine)refine_commit(refine,screen_step);
    for(int i=0;i<count;i++){stats->evaluations+=fields[i].evaluations;stats->fast_evaluations+=fields[i].fast_evaluations;}
    free(columns);
    if(owned)free(bits);
    return result;
}
int ray3d_render(RaySurface const *surfaces,int count,EvalVars const *base,
                 Volume const *v,Camera const *cam,ProgressFn progress,void *ctx,RayStats *stats)
{
    return ray3d_render_ex(surfaces,count,base,v,cam,NULL,progress,ctx,stats);
}
#ifdef GL_RAY_TEST
int debug_ray_isfinite(float x){return ray_isfinite(x);}
float debug_ray_fabs(float x){return ray_fabs(x);}
int debug_ray_zero(float x){return ray_zero(x);}
int debug_ray_negative_finite(float x){return ray_negative_finite(x);}
int debug_ray_opposite(float x,float y){return opposite(x,y);}
int debug_ray_same(float x,float y){return same_float(x,y);}
float debug_ray_add_offset(float a,float b){return ray_add_offset(a,b);}
#endif
