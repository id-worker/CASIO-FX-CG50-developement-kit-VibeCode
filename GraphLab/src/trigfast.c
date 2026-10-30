#include "trigfast.h"
#include "trig_table.h"
#include <math.h>
#include <float.h>
#include <string.h>
/* The optimized bit-level implementation is enabled only for the binary32
   representation used by fx-CG50 and the tested GCC host builds. */
#if defined(__SIZEOF_FLOAT__) && __SIZEOF_FLOAT__==4 && FLT_RADIX==2 && FLT_MANT_DIG==24 && FLT_MAX_EXP==128
#define GL_TRIG_BINARY32 1
#else
#define GL_TRIG_BINARY32 0
#endif

enum { T_CONSTANT=0,T_AXIS=1,T_SUM=2 };
/* mass bounds the absolute contributions BEFORE cancellation. Guarding only
   the final coefficients is unsafe: large terms can cancel or overflow in
   the original VM before an algebraically simplified expression is formed. */
typedef struct {float c,sine[3],cosine[3],mass;int kind,axis;} Term;
static int finite_term(Term const *t)
{
    if(!isfinite(t->c)||!isfinite(t->mass)||t->mass>100.0f)return 0;
    for(int i=0;i<3;i++)if(!isfinite(t->sine[i])||!isfinite(t->cosine[i]))return 0;
    return 1;
}
static void scale(Term *t,float s)
{
    t->c*=s;t->mass*=fabsf(s);
    for(int i=0;i<3;i++){t->sine[i]*=s;t->cosine[i]*=s;}
}
int trigfast_prepare(TrigFast *plan,Program const *p,EvalVars const *base,float bound)
{
    if(!plan)return 0;
    memset(plan,0,sizeof *plan);
    /* The absolute sign-error guard below is derived for |coordinate| <= 64.
       Stay inside 63 so finite-difference normal samples also fit. */
    if(!p||!base||!isfinite(bound)||bound>63||bound<0||p->size==0||
       p->size>EXPR_CODE||p->max_stack>8)return 0;
    Term st[8];int sp=0;
    for(int j=0;j<p->size;j++){
        Instr const *in=&p->code[j];Term a,b;
        switch(in->op){
        case I_NUM:
            if(sp>=8||!isfinite(in->number))return 0;
            memset(&st[sp],0,sizeof st[sp]);st[sp].c=in->number;st[sp++].mass=fabsf(in->number);break;
        case I_VAR:
            if(sp>=8||in->aux>=V_COUNT)return 0;
            memset(&st[sp],0,sizeof st[sp]);
            if(in->aux==V_X||in->aux==V_Y||in->aux==V_Z){
                st[sp].kind=T_AXIS;st[sp].axis=in->aux==V_X?0:in->aux==V_Y?1:2;st[sp].c=1;st[sp].mass=1;
            }else {st[sp].c=base->v[in->aux];if(!isfinite(st[sp].c))return 0;st[sp].mass=fabsf(st[sp].c);}
            sp++;break;
        case I_NEG:
            if(sp<1)return 0;
            scale(&st[sp-1],-1);break;
        case I_SQUARE:case I_CUBE:
            if(sp<1||st[sp-1].kind!=T_CONSTANT)return 0;
            a=st[sp-1];st[sp-1].c=in->op==I_SQUARE?a.c*a.c:a.c*a.c*a.c;
            st[sp-1].mass=in->op==I_SQUARE?a.mass*a.mass:a.mass*a.mass*a.mass;break;
        case I_FUNC:
            if(sp<1||(in->aux!=F_SIN&&in->aux!=F_COS))return 0;
            a=st[sp-1];memset(&st[sp-1],0,sizeof st[sp-1]);
            if(a.kind==T_AXIS && (a.c==1||a.c==-1)){
                st[sp-1].kind=T_SUM;st[sp-1].mass=1;
                if(in->aux==F_SIN)st[sp-1].sine[a.axis]=a.c;
                else st[sp-1].cosine[a.axis]=1;
            }else if(a.kind==T_CONSTANT){
                st[sp-1].c=in->aux==F_SIN?sinf(a.c):cosf(a.c);
                st[sp-1].mass=fabsf(st[sp-1].c);
            }else return 0;
            break;
        case I_ADD:case I_SUB:
            if(sp<2)return 0;
            b=st[--sp];a=st[sp-1];
            if(a.kind==T_AXIS||b.kind==T_AXIS)return 0;
            if(in->op==I_SUB)scale(&b,-1);
            a.c+=b.c;a.mass+=b.mass;
            for(int i=0;i<3;i++){a.sine[i]+=b.sine[i];a.cosine[i]+=b.cosine[i];}
            if(b.kind==T_SUM)a.kind=T_SUM;
            st[sp-1]=a;break;
        case I_MUL:case I_DIV:
            if(sp<2)return 0;
            b=st[--sp];a=st[sp-1];
            /* Coordinate transformations are not reassociated. Even if a
               chain such as x*3/3 reduces algebraically to x, its float
               evaluation can accumulate error before sin/cos. */
            if(a.kind==T_AXIS||b.kind==T_AXIS)return 0;
            if(in->op==I_DIV){
                if(b.kind!=T_CONSTANT||b.c==0)return 0;
                scale(&a,1/b.c);
            }else if(b.kind==T_CONSTANT)scale(&a,b.c);
            else if(a.kind==T_CONSTANT){scale(&b,a.c);a=b;}
            else return 0;
            st[sp-1]=a;break;
        default:return 0;
        }
        if(sp && !finite_term(&st[sp-1]))return 0;
    }
    if(sp!=1||st[0].kind!=T_SUM)return 0;
    /* With <=128 bytecodes, mass<=100 at every intermediate, and direct
       unscaled coordinate arguments, .0002*mass conservatively exceeds the
       LUT phase/table/interpolation error plus accumulated float roundoff.
       No sign decision within that band uses this path. */
    plan->constant=st[0].c;plan->guard=.0002f*(1+st[0].mass);
    for(int i=0;i<3;i++){plan->sine[i]=st[0].sine[i];plan->cosine[i]=st[0].cosine[i];}
#if GL_TRIG_BINARY32
    /* Common three-axis sums with exactly one +/-sin or +/-cos term per
       axis can skip interpreting coefficient flags at every field sample. */
    union {float f;uint32_t u;} c;c.f=plan->constant;
    unsigned mode=(c.u<<1)==0?(1u|((c.u>>31)<<7)):0u;
    for(int i=0;i<3&&mode;i++){
        union {float f;uint32_t u;} a,b;a.f=plan->sine[i];b.f=plan->cosine[i];
        uint32_t chosen;unsigned cosine;
        if((b.u<<1)==0&&(a.u==0x3f800000u||a.u==0xbf800000u)){chosen=a.u;cosine=0;}
        else if((a.u<<1)==0&&(b.u==0x3f800000u||b.u==0xbf800000u)){chosen=b.u;cosine=1;}
        else {mode=0;break;}
        mode|=(cosine|((chosen>>31)<<1))<<(1+2*i);
    }
    plan->xyz_mode=mode;
#endif
    plan->valid=1;return 1;
}
#if GL_TRIG_BINARY32
/* Exact binary32 result of n*(1.0f/32767.0f), n in [-32767,32767].
   The constant is exactly 2^-15 + 2^-30. Compute and round its mantissa
   with integers; this changes neither the table nor its output bits. */
static float gl_q15_scaled(int n)
{
    if(!n)return 0.0f;
    uint32_t sign=n<0?0x80000000u:0u;
    unsigned a=(unsigned)(n<0?-n:n),k=a,h=0;
    if(k>=256u){k>>=8;h=8;}
    if(k>=16u){k>>=4;h+=4;}
    if(k>=4u){k>>=2;h+=2;}
    if(k>=2u)h++;
    unsigned base=a<<(23-h),corr;
    if(h<=8)corr=a<<(8-h);
    else {
        unsigned shift=h-8,rem=a&((1u<<shift)-1u),half=1u<<(shift-1);
        corr=a>>shift;
        if(rem>half || (rem==half && (corr&1u)))corr++;
    }
    unsigned sig=base+corr;
    if(sig==0x1000000u){sig>>=1;h++;}
    uint32_t bits=sign|((h+112u)<<23)|(sig&0x007fffffu);
    union {float f;uint32_t u;} result;result.u=bits;return result.f;
}
static uint32_t gl_float_bits(float value)
{
    union {float f;uint32_t u;} bits;bits.f=value;return bits.u;
}
/* Exact (int32_t)(x * 667544.2144301094f) for finite |x| <= 64,
   with the usual IEEE round-to-nearest-even binary32 semantics. The rounded
   constant is 0x4922f983 = 0xa2f983 * 2^-4. Multiply the integer significands,
   round once as binary32, then truncate the rounded result toward zero.
   Unlike a fixed-point approximation, the resulting table phase is unchanged. */
static int32_t gl_table_phase(float x)
{
    uint32_t bits=gl_float_bits(x),mag=bits&0x7fffffffu,exp=mag>>23;
    if(exp<=106u)return 0; /* even the largest input here scales to less than 1 */
    uint32_t significand=(mag&0x7fffffu)|0x800000u;
    uint64_t wide=(uint64_t)significand*UINT32_C(0xa2f983);
    uint32_t hi=(uint32_t)(wide>>32),lo=(uint32_t)wide;
    unsigned high=(hi&0x8000u)?47u:46u,shift=high-23u;
    uint32_t mant=(hi<<(32u-shift))|(lo>>shift);
    uint32_t rem=lo&((1u<<shift)-1u),half=1u<<(shift-1u);
    if(rem>half||(rem==half&&(mant&1u)))mant++;
    if(mant==0x1000000u){mant>>=1;high++;}
    int e=(int)exp+(int)high-154;uint32_t out;
    if(e<0)out=0;else if(e<23)out=mant>>(23-e);else out=mant<<(e-23);
    return (bits>>31)?-(int32_t)out:(int32_t)out;
}
#endif
static float lookup(float x,int cosine)
{
    /* 1024 linear segments and 12-bit integer interpolation avoid several
       software-float operations. The 64-unit input bound keeps the phase in
       int32 range. Unsigned masking gives the correct wrap for negative x. */
#if GL_TRIG_BINARY32
    int32_t phase=gl_table_phase(x);
#else
    int32_t phase=(int32_t)(x*667544.2144301094f);
#endif
    uint32_t bits=(uint32_t)phase;
    unsigned k=((bits>>12)+(cosine?256u:0u))&1023u;
    unsigned frac=bits&4095u;
    int a=gl_sin_table[k],b=gl_sin_table[(k+1u)&1023u];
    int product=(b-a)*(int)frac;
    int delta=product>=0?(product+2048)>>12:-((-product+2048)>>12);
#if GL_TRIG_BINARY32
    return gl_q15_scaled(a+delta);
#else
    return (a+delta)*(1.0f/32767.0f);
#endif
}
int trigfast_value(TrigFast const *p,float x,float y,float z,float target,float *value)
{
    if(!p||!p->valid||!value)return 0;
#if GL_TRIG_BINARY32
    /* IEEE binary32 magnitudes are monotonic. This checks finiteness and
       coordinate bounds exactly, including NaNs and signed zero. */
    uint32_t xb=gl_float_bits(x)&0x7fffffffu,yb=gl_float_bits(y)&0x7fffffffu;
    uint32_t zb=gl_float_bits(z)&0x7fffffffu,tb=gl_float_bits(target)&0x7fffffffu;
    if(xb>0x42800000u||yb>0x42800000u||zb>0x42800000u||tb>=0x7f800000u)return 0;
#else
    if(!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(target)||
       fabsf(x)>64||fabsf(y)>64||fabsf(z)>64)return 0;
#endif
    float axis[3]={x,y,z},sum=p->constant;
#if GL_TRIG_BINARY32
    if(p->xyz_mode){
        unsigned mode=p->xyz_mode;
        float a=lookup(x,(mode>>1)&1u),b=lookup(y,(mode>>3)&1u),c=lookup(z,(mode>>5)&1u);
        if(mode&4u)a=-a;
        if(mode&16u)b=-b;
        if(mode&64u)c=-c;
        /* +0 + -0 is +0; -0 + -0 stays -0 in the original first sum. */
        if(!(mode&128u)&&gl_float_bits(a)==0x80000000u)a=0;
        sum=a;sum+=b;sum+=c;
    }else for(int i=0;i<3;i++){
        uint32_t sb=gl_float_bits(p->sine[i]),cb=gl_float_bits(p->cosine[i]);
        if(sb<<1){
            float v=lookup(axis[i],0);
            if(sb==0xbf800000u)v=-v;
            else if(sb!=0x3f800000u)v=p->sine[i]*v;
            sum+=v;
        }
        if(cb<<1){
            float v=lookup(axis[i],1);
            if(cb==0xbf800000u)v=-v;
            else if(cb!=0x3f800000u)v=p->cosine[i]*v;
            sum+=v;
        }
    }
#else
    for(int i=0;i<3;i++){
        if(p->sine[i]!=0)sum+=p->sine[i]*lookup(axis[i],0);
        if(p->cosine[i]!=0)sum+=p->cosine[i]*lookup(axis[i],1);
    }
#endif
    /* Near zero, the VM computes the actual expression so sign tests and
       final root refinement do not rely on the approximation. */
#if GL_TRIG_BINARY32
    /* Equivalent to isfinite(sum) and fabsf(sum-target)<=guard, but IEEE
       magnitude ordering avoids four soft-float calls. Subtraction is skipped
       when target is +0/-0 because only its result's magnitude is used. */
    uint32_t sums=gl_float_bits(sum),sumabs=sums&0x7fffffffu;
    if(sumabs>=0x7f800000u)return 0;
    uint32_t guard=gl_float_bits(p->guard),guardabs=guard&0x7fffffffu;
    if(guardabs<=0x7f800000u && (!(guard&0x80000000u)||guardabs==0)){
        uint32_t difference=tb?gl_float_bits(sum-target)&0x7fffffffu:sumabs;
        if(difference<=guardabs)return 0;
    }
#else
    if(!isfinite(sum)||fabsf(sum-target)<=p->guard)return 0;
#endif
    *value=sum;return 1;
}

#ifdef GL_TRIG_TEST
float debug_new_lookup(float x,int cosine){return lookup(x,cosine);}
int32_t debug_new_phase(float x){
#if GL_TRIG_BINARY32
    return gl_table_phase(x);
#else
    return (int32_t)(x*667544.2144301094f);
#endif
}
#if GL_TRIG_BINARY32
float debug_new_scale(int x){return gl_q15_scaled(x);}
#else
float debug_new_scale(int x){return x*(1.0f/32767.0f);}
#endif
#endif
