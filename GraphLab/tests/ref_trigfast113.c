#include "trigfast.h"
#include "trig_table.h"
#include <math.h>
#include <string.h>

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
    plan->valid=1;return 1;
}
static float lookup(float x,int cosine)
{
    /* 1024 linear segments and 12-bit integer interpolation avoid several
       software-float operations. The 64-unit input bound keeps the phase in
       int32 range. Unsigned masking gives the correct wrap for negative x. */
    int32_t phase=(int32_t)(x*667544.2144301094f);
    uint32_t bits=(uint32_t)phase;
    unsigned k=((bits>>12)+(cosine?256u:0u))&1023u;
    unsigned frac=bits&4095u;
    int a=gl_sin_table[k],b=gl_sin_table[(k+1u)&1023u];
    int product=(b-a)*(int)frac;
    int delta=product>=0?(product+2048)>>12:-((-product+2048)>>12);
    return (a+delta)*(1.0f/32767.0f);
}
int trigfast_value(TrigFast const *p,float x,float y,float z,float target,float *value)
{
    if(!p||!p->valid||!value||!isfinite(x)||!isfinite(y)||!isfinite(z)||!isfinite(target)||
       fabsf(x)>64||fabsf(y)>64||fabsf(z)>64)return 0;
    float axis[3]={x,y,z},sum=p->constant;
    for(int i=0;i<3;i++){
        if(p->sine[i]!=0)sum+=p->sine[i]*lookup(axis[i],0);
        if(p->cosine[i]!=0)sum+=p->cosine[i]*lookup(axis[i],1);
    }
    /* Near zero, the VM computes the actual expression so sign tests and
       final root refinement do not rely on the approximation. */
    if(!isfinite(sum)||fabsf(sum-target)<=p->guard)return 0;
    *value=sum;return 1;
}

/* Test-only wrapper: the previous implementation, no public API change. */
float debug_old_lookup(float x,int cosine){return lookup(x,cosine);}
