#include "expr.h"
#include <ctype.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef NAN
#define NAN (0.0f / 0.0f)
#endif
#define PI_F 3.14159265358979323846f
#define E_F 2.71828182845904523536f

static void fail(Parsed *p, char const *msg, int pos)
{
    if(p->error) return;
    p->error = 1;
    p->error_pos = pos;
    snprintf(p->message, sizeof p->message, "%s", msg);
}
static void spaces(Parsed *p)
{
    while(p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int node(Parsed *p, NodeKind kind, int a, int b, int start, int end, int op, int aux, float value)
{
    if(p->count >= EXPR_NODES) { fail(p, "Expression is too long", p->pos); return -1; }
    int k = p->count++;
    ExprNode *n = &p->nodes[k];
    memset(n, 0, sizeof *n);
    n->kind=(uint8_t)kind; n->left=(int16_t)a; n->right=(int16_t)b;
    n->start=(int16_t)start; n->end=(int16_t)end; n->op_pos=(int16_t)op;
    n->aux=(uint8_t)aux; n->value=value;
    return k;
}
static int hole(Parsed *p)
{
    if(!p->tolerant) { fail(p, "Expected an expression", p->pos); return -1; }
    return node(p,N_HOLE,-1,-1,p->pos,p->pos,-1,0,0);
}
static int enter(Parsed *p)
{
    if(++p->depth > EXPR_DEPTH) { fail(p,"Too many nested levels",p->pos); p->depth--; return 0; }
    return 1;
}
static int add(Parsed *p);
static int unary(Parsed *p);

static int nameeq(char const *s, int n, char const *word)
{
    int len=(int)strlen(word);
    if(n!=len) return 0;
    for(int i=0;i<n;i++) if(tolower((unsigned char)s[i]) != word[i]) return 0;
    return 1;
}
int expr_var_id(char letter)
{
    switch(tolower((unsigned char)letter)) {
        case 'x':return V_X;case 'y':return V_Y;case 'z':return V_Z;
        case 'a':return V_A;case 'b':return V_B;case 'c':return V_C;
        case 't':return V_T;case 'n':return V_N;case 'd':return V_D;
        case 'e':return V_E;case 'f':return V_F;case 'g':return V_G;
        case 'h':return V_H;case 'i':return V_I;case 'j':return V_J;
        case 'k':return V_K;case 'l':return V_L;case 'm':return V_M;
        case 'o':return V_O;case 'p':return V_P;case 'q':return V_Q;
        case 'r':return V_R;case 's':return V_S;case 'u':return V_U;
        case 'v':return V_V;case 'w':return V_W;default:return -1;
    }
}
static int function_id(char const *s, int n)
{
    static const char *const names[F_COUNT]={
        "sin","cos","tan","asin","acos","atan","sqrt","abs","ln","log",
        "exp","floor","ceil","re","im","conj","min","max","atan2"
    };
    for(int i=0;i<F_COUNT;i++) if(nameeq(s,n,names[i])) return i;
    return -1;
}
static int is_binary_func(int f) { return f == F_MIN || f == F_MAX || f == F_ATAN2; }
/* During editing, do not let an incomplete nested construct consume a
   delimiter that was already present in its enclosing template. */
static int owns_close(Parsed const *p,int open)
{
    return !p->edit_mate || p->edit_mate[p->pos]<0 || p->edit_mate[p->pos]==open;
}
static int owns_comma(Parsed const *p,int open)
{
    return !p->edit_comma_owner || p->edit_comma_owner[p->pos]<0 || p->edit_comma_owner[p->pos]==open;
}

static int primary(Parsed *p)
{
    spaces(p);
    int start=p->pos;
    if(p->pos>=p->len) return hole(p);
    char c=p->src[p->pos];
    if(c=='(') {
        if(!enter(p)) return -1;
        p->pos++;
        int a=add(p);
        spaces(p);
        int close=-1;
        if(p->pos<p->len && p->src[p->pos]==')' && owns_close(p,start)) close=p->pos++;
        else if(!p->tolerant) fail(p,"Missing closing parenthesis",p->pos);
        p->depth--;
        return node(p,N_GROUP,a,close,start,p->pos,start,close>=0,0);
    }
    if(isdigit((unsigned char)c) || c=='.') {
        char *end=NULL;
        float v=strtof(p->src+p->pos,&end);
        if(end==p->src+p->pos) {
            if(p->tolerant) { p->pos++; return node(p,N_RAW,-1,-1,start,p->pos,-1,0,0); }
            fail(p,"Invalid number",p->pos); return -1;
        }
        p->pos=(int)(end-p->src);
        if(!isfinite(v) && !p->tolerant) { fail(p,"Number is too large",start); return -1; }
        return node(p,N_NUM,-1,-1,start,p->pos,-1,0,v);
    }
    if(isalpha((unsigned char)c)) {
        p->pos++;
        while(p->pos<p->len && (isalpha((unsigned char)p->src[p->pos]) || isdigit((unsigned char)p->src[p->pos]))) p->pos++;
        int n=p->pos-start;
        char const *s=p->src+start;
        if(n==1) {
            int v=expr_var_id(c);
            if(v>=0) {
                if(tolower((unsigned char)c)=='i' && !(p->bound_mask & (1u<<v)))
                    return node(p,N_CONST_I,-1,-1,start,p->pos,-1,0,0);
                if(tolower((unsigned char)c)=='e' && !(p->bound_mask & (1u<<v)))
                    return node(p,N_NUM,-1,-1,start,p->pos,-1,0,E_F);
                return node(p,N_VAR,-1,-1,start,p->pos,-1,v,0);
            }
        }
        if(nameeq(s,n,"pi")) return node(p,N_NUM,-1,-1,start,p->pos,-1,0,PI_F);
        if(nameeq(s,n,"inf")) return node(p,N_INF,-1,-1,start,p->pos,-1,0,INFINITY);
        /* SUM/PROD bind their own editable one-letter index in the body only. */
        if(nameeq(s,n,"summ") || nameeq(s,n,"prod")) {
            int product=nameeq(s,n,"prod");
            spaces(p);
            if(p->pos>=p->len || p->src[p->pos]!='(') {
                if(p->tolerant)return node(p,N_RAW,-1,-1,start,p->pos,-1,0,0);
                fail(p,product?"prod needs (lower,upper,index,term)":"summ needs (lower,upper,index,term)",start);return -1;
            }
            if(!enter(p))return -1;
            int open=p->pos++,lo=add(p),hi=-1,body=-1;
            spaces(p);
            if(p->pos<p->len && p->src[p->pos]==',' && owns_comma(p,open))p->pos++;
            else if(!p->tolerant)fail(p,product?"prod needs four arguments":"summ needs four arguments",p->pos);
            hi=add(p);spaces(p);
            if(p->pos<p->len && p->src[p->pos]==',' && owns_comma(p,open))p->pos++;
            else if(!p->tolerant)fail(p,product?"prod needs four arguments":"summ needs four arguments",p->pos);
            spaces(p);int is=p->pos,ie=is,idx=-1;
            if(p->pos<p->len && isalpha((unsigned char)p->src[p->pos])) {
                idx=expr_var_id(p->src[p->pos++]);ie=p->pos;
                while(p->pos<p->len && (isalpha((unsigned char)p->src[p->pos]) || isdigit((unsigned char)p->src[p->pos])))p->pos++;
                if(p->pos!=ie)idx=-1;
                ie=p->pos;
            }
            if(idx<0 && !p->tolerant)fail(p,product?"prod index must be one letter":"summ index must be one letter",is);
            spaces(p);
            if(p->pos<p->len && p->src[p->pos]==',' && owns_comma(p,open))p->pos++;
            else if(!p->tolerant)fail(p,product?"prod needs four arguments":"summ needs four arguments",p->pos);
            uint32_t saved=p->bound_mask;
            if(idx>=0)p->bound_mask|=1u<<idx;
            body=add(p);p->bound_mask=saved;spaces(p);
            int closed=0;
            if(p->pos<p->len && p->src[p->pos]==')' && owns_close(p,open)){p->pos++;closed=1;}
            else if(!p->tolerant)fail(p,"Missing closing parenthesis",p->pos);
            p->depth--;
            int k=node(p,product?N_PROD:N_SUM,lo,hi,start,p->pos,open,idx>=0?idx:255,0);
            if(k>=0){p->nodes[k].extra=(int16_t)body;p->nodes[k].index_start=(int16_t)is;p->nodes[k].index_end=(int16_t)ie;
                if(!closed)p->nodes[k].op_pos=(int16_t)(-open-2);}
            return k;
        }
        int f=function_id(s,n);
        spaces(p);
        if(f<0 || p->pos>=p->len || p->src[p->pos]!='(') {
            if(p->tolerant) return node(p,N_RAW,-1,-1,start,p->pos,-1,0,0);
            fail(p,f<0 ? "Unknown name" : "Function needs ( )",start); return -1;
        }
        if(!enter(p)) return -1;
        int open=p->pos++;
        int a=add(p), b=-1;
        spaces(p);
        if(is_binary_func(f)) {
            if(p->pos<p->len && p->src[p->pos]==',' && owns_comma(p,open)) { p->pos++; b=add(p); }
            else if(!p->tolerant) fail(p,"Function needs two arguments",p->pos);
            else b=hole(p);
            spaces(p);
        }
        int closed=0;
        if(p->pos<p->len && p->src[p->pos]==')' && owns_close(p,open)) { p->pos++; closed=1; }
        else if(!p->tolerant) fail(p,"Missing closing parenthesis",p->pos);
        p->depth--;
        int k=node(p,N_FUNC,a,b,start,p->pos,open,f,0);
        if(k>=0 && !closed) p->nodes[k].op_pos=(int16_t)(-open-2); /* missing ')' */
        return k;
    }
    if(c==')' || c==',' || c=='=' || c=='<' || c=='>') return hole(p);
    if(p->tolerant) { p->pos++; return node(p,N_RAW,-1,-1,start,p->pos,-1,0,0); }
    fail(p,"Unexpected character",p->pos); return -1;
}
static int power(Parsed *p)
{
    int a=primary(p); spaces(p);
    while(!p->error && a>=0 && p->pos<p->len && p->src[p->pos]=='!') {
        int op=p->pos++;a=node(p,N_FACT,a,-1,p->nodes[a].start,p->pos,op,0,0);spaces(p);
    }
    if(!p->error && p->pos<p->len && p->src[p->pos]=='^') {
        int op=p->pos++;
        if(!enter(p)) return -1;
        int b=unary(p);
        p->depth--;
        if(a<0 || b<0) return -1;
        a=node(p,N_POW,a,b,p->nodes[a].start,p->nodes[b].end,op,0,0);
    }
    return a;
}
static int unary(Parsed *p)
{
    spaces(p);
    if(p->pos<p->len && (p->src[p->pos]=='-' || p->src[p->pos]=='+')) {
        int op=p->pos++; char c=p->src[op];
        if(!enter(p)) return -1;
        int a=unary(p); p->depth--;
        if(a<0) return -1;
        if(c=='-') return node(p,N_NEG,a,-1,op,p->nodes[a].end,op,0,0);
        /* Preserve unary + for rendering; semantic value is unchanged. */
        return node(p,N_ADD,node(p,N_HOLE,-1,-1,op,op,-1,0,0),a,op,p->nodes[a].end,op,1,0);
    }
    return power(p);
}
static int starts_primary(Parsed *p)
{
    if(p->pos>=p->len) return 0;
    char c=p->src[p->pos];
    return c=='(' || c=='.' || isdigit((unsigned char)c) || isalpha((unsigned char)c);
}
static int mul(Parsed *p)
{
    int a=unary(p);
    while(!p->error && a>=0) {
        spaces(p);
        int op=p->pos, kind;
        if(p->pos<p->len && p->src[p->pos]=='*') { kind=N_MUL; p->pos++; }
        else if(p->pos<p->len && p->src[p->pos]=='/') { kind=N_DIV; p->pos++; }
        else if(starts_primary(p)) {
            int prev=p->pos-1;
            while(prev>=0 && isspace((unsigned char)p->src[prev])) prev--;
            int next_is_num=isdigit((unsigned char)p->src[p->pos]) || p->src[p->pos]=='.';
            int prev_is_num=prev>=0 && (isdigit((unsigned char)p->src[prev]) || p->src[prev]=='.');
            /* Adjacent numeric literals like 1.2.3 are invalid, not multiplication. */
            if(next_is_num && prev_is_num) break;
            kind=N_MUL; op=-1;
        }
        else break;
        int old=p->pos, b=unary(p);
        if(b<0 || p->error) return -1;
        if(p->pos==old && p->nodes[b].kind==N_HOLE) {
            a=node(p,(NodeKind)kind,a,b,p->nodes[a].start,p->nodes[b].end,op,0,0);
            break;
        }
        a=node(p,(NodeKind)kind,a,b,p->nodes[a].start,p->nodes[b].end,op,0,0);
    }
    return a;
}
static int add(Parsed *p)
{
    int a=mul(p);
    while(!p->error && a>=0) {
        spaces(p);
        if(p->pos>=p->len || (p->src[p->pos]!='+' && p->src[p->pos]!='-')) break;
        int op=p->pos++; int kind=(p->src[op]=='+')?N_ADD:N_SUB;
        int b=mul(p);
        if(b<0 || p->error) return -1;
        a=node(p,(NodeKind)kind,a,b,p->nodes[a].start,p->nodes[b].end,op,0,0);
    }
    return a;
}
static int depth_ok(Parsed const *p)
{
    uint8_t height[EXPR_NODES]={0};
    for(int k=0;k<p->count;k++){
        ExprNode const *n=&p->nodes[k];int a=-1,b=-1,c=-1;
        switch(n->kind){
            case N_GROUP:case N_NEG:case N_FACT:a=n->left;break;
            case N_FUNC:case N_ADD:case N_SUB:case N_MUL:case N_DIV:case N_POW:case N_EQ:
                a=n->left;b=n->right;break;
            case N_SUM:case N_PROD:a=n->left;b=n->right;c=n->extra;break;
            default:break;
        }
        unsigned h=1;
        if(a>=0 && a<k && h<(unsigned)height[a]+1)h=(unsigned)height[a]+1;
        if(b>=0 && b<k && h<(unsigned)height[b]+1)h=(unsigned)height[b]+1;
        if(c>=0 && c<k && h<(unsigned)height[c]+1)h=(unsigned)height[c]+1;
        if(h>EXPR_TREE_DEPTH)return 0;
        height[k]=(uint8_t)h;
    }
    return 1;
}
static int parse_with_owners(Parsed *p,char const *src,int tolerant,const int16_t *mate,const int16_t *comma_owner)
{
    memset(p,0,sizeof *p);
    p->src=src; p->len=(int)strlen(src); p->tolerant=!!tolerant; p->root=-1;
    if(p->tolerant){p->edit_mate=mate;p->edit_comma_owner=comma_owner;}
    if(p->len>=EXPR_MAX) { fail(p,"Expression is too long",EXPR_MAX-1); return 0; }
    int a=add(p);
    spaces(p);
    if(!p->error && a>=0 && p->pos<p->len &&
       (p->src[p->pos]=='=' || p->src[p->pos]=='<' || p->src[p->pos]=='>')) {
        int op=p->pos++;char c=p->src[op];
        int equal=c!='=' && p->pos<p->len && p->src[p->pos]=='=';
        if(equal)p->pos++;
        Relation relation=c=='='?REL_EQ:c=='<'?(equal?REL_LE:REL_LT):(equal?REL_GE:REL_GT);
        int b=add(p);
        if(b>=0) a=node(p,N_EQ,a,b,p->nodes[a].start,p->nodes[b].end,op,relation,0);
    }
    spaces(p);
    if(!p->error && p->pos<p->len) fail(p,
        (p->src[p->pos]=='='||p->src[p->pos]=='<'||p->src[p->pos]=='>')?
        "One relation per formula":"Unexpected character",p->pos);
    p->root=a;
    if(!p->error && a>=0 && !depth_ok(p))fail(p,"Expression tree too deep",p->pos);
    return !p->error && a>=0;
}
int expr_parse(Parsed *p,char const *src,int tolerant)
{
    return parse_with_owners(p,src,tolerant,NULL,NULL);
}
int expr_parse_editor(Parsed *p,char const *src,const int16_t *mate,const int16_t *comma_owner)
{
    return parse_with_owners(p,src,1,mate,comma_owner);
}

static int emit(Program *out, OpCode op, int aux, float value)
{
    if(out->size>=EXPR_CODE) return 0;
    Instr *i=&out->code[out->size++]; i->op=(uint8_t)op; i->aux=(uint8_t)aux; i->number=value;
    return 1;
}
static int ungroup(Parsed const *p,int k)
{
    while(k>=0 && k<p->count && p->nodes[k].kind==N_GROUP)k=p->nodes[k].left;
    return k;
}
static int build(Parsed const *p, Program *out, int k, int *sp, int *max)
{
    if(k<0 || k>=p->count) return 0;
    ExprNode const *n=&p->nodes[k];
    switch(n->kind) {
        case N_NUM:case N_INF: if(!emit(out,I_NUM,0,n->value)) return 0; (*sp)++; break;
        case N_VAR: if(!emit(out,I_VAR,n->aux,0)) return 0; out->variables|=1u<<n->aux; (*sp)++; break;
        case N_CONST_I: if(!emit(out,I_IMAG,0,0)) return 0; out->variables|=1u<<V_COUNT; (*sp)++; break;
        case N_GROUP: return build(p,out,n->left,sp,max);
        case N_NEG: if(!build(p,out,n->left,sp,max) || !emit(out,I_NEG,0,0)) return 0; break;
        case N_FACT: if(!build(p,out,n->left,sp,max) || !emit(out,I_FACT,0,0)) return 0; break;
        case N_SUM:case N_PROD: {
            if(n->aux>=V_COUNT || !build(p,out,n->left,sp,max) || !build(p,out,n->right,sp,max))return 0;
            int at=out->size;if(!emit(out,n->kind==N_PROD?I_PROD:I_SUM,n->aux,0))return 0;
            uint32_t before=out->variables;out->variables=0;
            int inner_sp=0,inner_max=0;
            if(!build(p,out,n->extra,&inner_sp,&inner_max) || inner_sp!=1)return 0;
            out->variables=before | (out->variables & ~(1u<<n->aux));
            out->code[at].number=(float)(out->size-at-1);
            (*sp)--;break;
        }
        case N_FUNC:
            if(!build(p,out,n->left,sp,max)) return 0;
            if(is_binary_func(n->aux)) { if(!build(p,out,n->right,sp,max)) return 0; (*sp)--; }
            if(!emit(out,I_FUNC,n->aux,0)) return 0;
            break;
        case N_DIV: {
            int lk=ungroup(p,n->left),rk=ungroup(p,n->right);
            ExprNode const *l=lk>=0?&p->nodes[lk]:NULL,*r=rk>=0?&p->nodes[rk]:NULL;
            if(l && r && l->kind==N_POW && r->kind==N_FACT) {
                int exponent=ungroup(p,l->right),factor=ungroup(p,r->left);
                if(exponent>=0 && factor>=0 && p->nodes[exponent].kind==N_VAR &&
                   p->nodes[factor].kind==N_VAR && p->nodes[exponent].aux==p->nodes[factor].aux) {
                    /* x^n/n! via scaled products avoids intermediate float overflow. */
                    if(!build(p,out,l->left,sp,max)||!build(p,out,l->right,sp,max)||!emit(out,I_POWFACT,0,0))return 0;
                    (*sp)--;break;
                }
            }
            if(!build(p,out,n->left,sp,max)||!build(p,out,n->right,sp,max)||!emit(out,I_DIV,0,0))return 0;
            (*sp)--;break;
        }
        case N_ADD:
            if(n->aux==1) return build(p,out,n->right,sp,max); /* unary + */
            /* fallthrough */
        case N_SUB: case N_MUL: case N_POW: case N_EQ: {
            if(!build(p,out,n->left,sp,max)) return 0;
            int rk=ungroup(p,n->right);
            ExprNode const *r=(rk>=0 && rk<p->count)?&p->nodes[rk]:NULL;
            if(n->kind==N_POW && r && r->kind==N_NUM && (r->value==2.0f || r->value==3.0f)) {
                if(!emit(out,r->value==2.0f?I_SQUARE:I_CUBE,0,0)) return 0;
                break;
            }
            if(!build(p,out,n->right,sp,max)) return 0;
            (*sp)--;
            OpCode op = n->kind==N_ADD ? I_ADD : n->kind==N_SUB || n->kind==N_EQ ? I_SUB :
                n->kind==N_MUL ? I_MUL : n->kind==N_DIV ? I_DIV : I_POW;
            if(n->kind==N_EQ){out->has_equal=1;out->relation=n->aux;}
            if(!emit(out,op,0,0)) return 0;
            break;
        }
        default: return 0;
    }
    if(*sp>*max) *max=*sp;
    return *sp>=0 && *sp<=EXPR_STACK;
}
int expr_compile(Parsed const *p, Program *out, char *error, size_t error_size)
{
    memset(out,0,sizeof *out);
    int sp=0,max=0;
    if(p->error || !build(p,out,p->root,&sp,&max) || sp!=1) {
        if(error && error_size) snprintf(error,error_size,"%s",p->error ? p->message : "Expression is incomplete or too complex");
        return 0;
    }
    out->max_stack=(uint16_t)max;
    if(error && error_size) *error=0;
    return 1;
}
int expr_program_uses(Program const *p, VarId v) { return (p->variables & (1u<<v)) != 0; }
int expr_program_continuous(Program const *p)
{
    if(!p||p->size==0||p->size>EXPR_CODE)return 0;
    for(int i=0;i<p->size;i++){
        Instr const *in=&p->code[i];
        switch(in->op){
        case I_NUM:if(!isfinite(in->number))return 0;break;
        case I_VAR:case I_NEG:case I_ADD:case I_SUB:case I_MUL:
        case I_SQUARE:case I_CUBE:break;
        case I_FUNC:
            if(in->aux!=F_SIN&&in->aux!=F_COS&&in->aux!=F_ATAN&&in->aux!=F_ABS&&
               in->aux!=F_MIN&&in->aux!=F_MAX&&in->aux!=F_RE&&in->aux!=F_IM&&in->aux!=F_CONJ)return 0;
            break;
        default:return 0;
        }
    }
    return 1;
}
int expr_relation_true(Relation relation,float value)
{
    if(!isfinite(value))return 0;
    switch(relation){
        case REL_EQ:return value==0;
        case REL_LT:return value<0;case REL_LE:return value<=0;
        case REL_GT:return value>0;case REL_GE:return value>=0;
        default:return 0;
    }
}
int expr_compile_explicit_y(Parsed const *p,Program *out,Relation *normalized)
{
    if(!p||!out||p->error||p->root<0||p->root>=p->count)return 0;
    ExprNode const *root=&p->nodes[p->root];int k=p->root;Relation relation=REL_EQ;
    if(root->kind==N_EQ){
        int left=ungroup(p,root->left),right=ungroup(p,root->right);
        if(left>=0 && p->nodes[left].kind==N_VAR && p->nodes[left].aux==V_Y){
            k=root->right;relation=(Relation)root->aux;
        }else if(right>=0 && p->nodes[right].kind==N_VAR && p->nodes[right].aux==V_Y){
            k=root->left;relation=(Relation)root->aux;
            if(relation==REL_LT)relation=REL_GT;else if(relation==REL_LE)relation=REL_GE;
            else if(relation==REL_GT)relation=REL_LT;else if(relation==REL_GE)relation=REL_LE;
        }else return 0;
    }
    memset(out,0,sizeof *out);int sp=0,max=0;
    if(!build(p,out,k,&sp,&max)||sp!=1||out->has_equal||expr_program_uses(out,V_Y)){
        memset(out,0,sizeof *out);return 0;
    }
    out->max_stack=(uint16_t)max;
    if(normalized)*normalized=relation;
    return 1;
}
int expr_compile_explicit_z(Parsed const *p,Program *out)
{
    if(!p||!out||p->error||p->root<0||p->root>=p->count)return 0;
    ExprNode const *root=&p->nodes[p->root];int k=p->root;
    if(root->kind==N_EQ){
        if(root->aux!=REL_EQ)return 0;
        int left=ungroup(p,root->left),right=ungroup(p,root->right);
        if(left>=0&&p->nodes[left].kind==N_VAR&&p->nodes[left].aux==V_Z)k=root->right;
        else if(right>=0&&p->nodes[right].kind==N_VAR&&p->nodes[right].aux==V_Z)k=root->left;
        else return 0;
    }
    memset(out,0,sizeof *out);int sp=0,max=0;
    if(!build(p,out,k,&sp,&max)||sp!=1||out->has_equal||expr_program_uses(out,V_Z)){
        memset(out,0,sizeof *out);return 0;
    }
    out->max_stack=(uint16_t)max;return 1;
}
int expr_compile_zero_surface(Parsed const *p,Program *out)
{
    if(!p||!out||p->error||p->root<0||p->root>=p->count)return 0;
    ExprNode const *root=&p->nodes[p->root];
    if(root->kind!=N_EQ||root->aux!=REL_EQ)return 0;
    int left=ungroup(p,root->left),right=ungroup(p,root->right),k=-1;
    if(left>=0&&p->nodes[left].kind==N_NUM&&p->nodes[left].value==0)k=right;
    else if(right>=0&&p->nodes[right].kind==N_NUM&&p->nodes[right].value==0)k=left;
    if(k<0)return 0;
    int changed=0;
    for(;;){
        k=ungroup(p,k);if(k<0||k>=p->count)return 0;
        ExprNode const *n=&p->nodes[k];
        if(n->kind==N_FUNC&&n->aux==F_ABS){k=n->left;changed=1;continue;}
        if(n->kind==N_POW){
            int r=ungroup(p,n->right);
            if(r>=0&&p->nodes[r].kind==N_NUM){
                float exponent=p->nodes[r].value;
                if(exponent>=2&&exponent<=16&&exponent==floorf(exponent)){
                    k=n->left;changed=1;continue;
                }
            }
        }
        break;
    }
    if(!changed)return 0;
    memset(out,0,sizeof *out);int sp=0,max=0;
    if(!build(p,out,k,&sp,&max)||sp!=1||out->has_equal){memset(out,0,sizeof *out);return 0;}
    out->has_equal=1;out->relation=REL_EQ;out->max_stack=(uint16_t)max;return 1;
}

static float eval_func_real(int f,float a,float b)
{
    switch(f) {
        case F_SIN:return sinf(a); case F_COS:return cosf(a); case F_TAN:return tanf(a);
        case F_ASIN:return asinf(a); case F_ACOS:return acosf(a); case F_ATAN:return atanf(a);
        case F_SQRT:return a<0?NAN:sqrtf(a); case F_ABS:return fabsf(a);
        case F_LN:return a<=0?NAN:logf(a); case F_LOG:return a<=0?NAN:log10f(a);
        case F_EXP:return expf(a); case F_FLOOR:return floorf(a); case F_CEIL:return ceilf(a);
        case F_RE:case F_CONJ:return a; case F_IM:return 0;
        case F_MIN:return a<b?a:b; case F_MAX:return a>b?a:b; case F_ATAN2:return atan2f(a,b);
        default:return NAN;
    }
}
/* Track a constant shift when x becomes x+period. This intentionally proves
   only common forms: sums/products by known constants and sin/cos of an
   affine phase. Unknown bytecode disables reuse rather than guessing. */
typedef struct { float delta,number; unsigned char shift,constant,uses_x; } PeriodTerm;
static int period_turns(float delta,float cycle)
{
    if(!isfinite(delta)||!isfinite(cycle)||cycle<=0)return 0;
    if(delta==0)return 1;
    float turns=delta/cycle;
    if(!isfinite(turns)||fabsf(turns)>1000000)return 0;
    float nearest=roundf(turns);
    /* A tiny but nonzero phase shift is not a zero-turn period. Accepting
       it would incorrectly tile slowly varying functions such as sin(1e-8x). */
    return nearest!=0&&fabsf(turns-nearest)<=0.000001f;
}
int expr_proves_x_period(Program const *p,EvalVars const *base,float period)
{
    if(!p||!base||!isfinite(period)||period<=0||p->size==0||p->size>EXPR_CODE)return 0;
    if(!expr_program_uses(p,V_X))return 1;
    PeriodTerm st[EXPR_STACK];int sp=0;
    for(int i=0;i<p->size;i++){
        Instr const *in=&p->code[i];PeriodTerm a,b,r={0};
        switch(in->op){
        case I_NUM:
            if(sp>=EXPR_STACK||!isfinite(in->number))return 0;
            st[sp++]=(PeriodTerm){0,in->number,1,1,0};break;
        case I_VAR:
            if(sp>=EXPR_STACK||in->aux>=V_COUNT)return 0;
            if(in->aux==V_X)st[sp++]=(PeriodTerm){period,0,1,0,1};
            else if(in->aux==V_Y||in->aux==V_Z)st[sp++]=(PeriodTerm){0,0,1,0,0};
            else if(isfinite(base->v[in->aux]))st[sp++]=(PeriodTerm){0,base->v[in->aux],1,1,0};
            else return 0;
            break;
        case I_NEG:
            if(sp<1)return 0;
            if(st[sp-1].shift)st[sp-1].delta=-st[sp-1].delta;
            if(st[sp-1].constant)st[sp-1].number=-st[sp-1].number;
            break;
        case I_SQUARE:case I_CUBE:
            if(sp<1)return 0;
            a=st[sp-1];r.uses_x=a.uses_x;
            if(a.shift&&a.delta==0){r.shift=1;r.delta=0;}
            if(a.constant){r.constant=1;r.number=in->op==I_SQUARE?a.number*a.number:a.number*a.number*a.number;}
            st[sp-1]=r;break;
        case I_FUNC:
            if(sp<1)return 0;
            b=(PeriodTerm){0,0,1,1,0};
            if(is_binary_func(in->aux)){if(sp<2)return 0;b=st[--sp];}
            a=st[sp-1];r.uses_x=(unsigned char)(a.uses_x||b.uses_x);
            if(a.constant&&b.constant){r.number=eval_func_real(in->aux,a.number,b.number);r.constant=(unsigned char)isfinite(r.number);}
            if(!r.uses_x){r.shift=1;r.delta=0;}
            else if((in->aux==F_SIN||in->aux==F_COS)&&a.shift&&period_turns(a.delta,6.2831853071795864769f)){
                r.shift=1;r.delta=0;
            }else if((in->aux==F_ABS||in->aux==F_ATAN||in->aux==F_MIN||in->aux==F_MAX||
                      in->aux==F_RE||in->aux==F_IM||in->aux==F_CONJ)&&
                     a.shift&&a.delta==0&&b.shift&&b.delta==0){r.shift=1;r.delta=0;}
            st[sp-1]=r;break;
        case I_ADD:case I_SUB:case I_MUL:case I_DIV:case I_POW:
            if(sp<2)return 0;
            b=st[--sp];a=st[sp-1];r.uses_x=(unsigned char)(a.uses_x||b.uses_x);
            if(a.constant&&b.constant){
                switch(in->op){
                case I_ADD:r.number=a.number+b.number;break;
                case I_SUB:r.number=a.number-b.number;break;
                case I_MUL:r.number=a.number*b.number;break;
                case I_DIV:r.number=b.number!=0?a.number/b.number:NAN;break;
                default:r.number=powf(a.number,b.number);break;
                }
                r.constant=(unsigned char)isfinite(r.number);
            }
            if(!r.uses_x){r.shift=1;r.delta=0;}
            else if(in->op==I_ADD||in->op==I_SUB){
                if(a.shift&&b.shift){r.delta=a.delta+(in->op==I_ADD?b.delta:-b.delta);r.shift=(unsigned char)isfinite(r.delta);}
            }else if(in->op==I_MUL){
                if(a.shift&&b.shift&&a.delta==0&&b.delta==0){r.shift=1;r.delta=0;}
                else if(a.constant&&b.shift){r.delta=a.number*b.delta;r.shift=(unsigned char)isfinite(r.delta);}
                else if(b.constant&&a.shift){r.delta=b.number*a.delta;r.shift=(unsigned char)isfinite(r.delta);}
            }else if(in->op==I_DIV){
                if(b.constant&&b.number!=0&&a.shift){r.delta=a.delta/b.number;r.shift=(unsigned char)isfinite(r.delta);}
            }else if(a.shift&&a.delta==0&&b.constant&&b.number>=0&&floorf(b.number)==b.number){r.shift=1;r.delta=0;}
            st[sp-1]=r;break;
        default:return 0;
        }
    }
    return sp==1&&st[0].shift&&st[0].delta==0;
}
typedef struct {int work,depth;} EvalCtx;
static float factorial_real(float value)
{
    static float table[35];static int ready;
    if(!ready){table[0]=1;for(int i=1;i<=34;i++)table[i]=table[i-1]*(float)i;ready=1;}
    if(!isfinite(value) || value<0 || value>34 || floorf(value)!=value)return NAN;
    return table[(int)value];
}
static int sum_limits(float lo,float hi,int *first,int *count,int *infinite)
{
    if(!isfinite(lo) || floorf(lo)!=lo || fabsf(lo)>100000 || isnan(hi))return 0;
    *first=(int)lo;*infinite=isinf(hi) && hi>0;
    if(*infinite){*count=EXPR_SUM_MAX_TERMS;return 1;}
    if(!isfinite(hi) || floorf(hi)!=hi || fabsf(hi)>100000)return 0;
    if(hi<lo){*count=0;return 1;}
    float span=hi-lo+1;if(span>EXPR_SUM_MAX_TERMS)return 0;
    *count=(int)span;return 1;
}
static float eval_real_range(Program const *p,EvalVars const *vars,int start,int end,EvalCtx *ctx)
{
    float st[EXPR_STACK]; int sp=0;
    for(int j=start;j<end;j++) {
        Instr const *in=&p->code[j]; float a,b;
        switch(in->op) {
            case I_NUM:if(sp>=EXPR_STACK)return NAN;st[sp++]=in->number;break;
            case I_VAR:if(sp>=EXPR_STACK || in->aux>=V_COUNT)return NAN;st[sp++]=vars->v[in->aux];break;
            case I_IMAG:return NAN;
            case I_NEG:if(sp<1)return NAN;st[sp-1]=-st[sp-1];break;
            case I_SQUARE:if(sp<1)return NAN;st[sp-1]*=st[sp-1];break;
            case I_CUBE:if(sp<1)return NAN;a=st[sp-1];st[sp-1]=a*a*a;break;
            case I_FACT:if(sp<1)return NAN;st[sp-1]=factorial_real(st[sp-1]);break;
            case I_SUM:case I_PROD: {
                if(sp<2 || in->aux>=V_COUNT || ctx->depth>=EXPR_SUM_MAX_DEPTH)return NAN;
                float hi=st[--sp],lo=st[sp-1];int first,count,infinite;
                int body=j+1,after=body+(int)in->number,product=in->op==I_PROD;
                if(in->number<1 || after>end || !sum_limits(lo,hi,&first,&count,&infinite))return NAN;
                EvalVars local=*vars;local.real_overrides|=1u<<in->aux;
                float result=product?1.0f:0.0f;int quiet=0,converged=!infinite;ctx->depth++;
                for(int k=0;k<count;k++){
                    if(--ctx->work<0){ctx->depth--;return NAN;}
                    local.v[in->aux]=(float)(first+k);
                    float term=eval_real_range(p,&local,body,after,ctx);
                    if(!isfinite(term)){ctx->depth--;return NAN;}
                    result=product?result*term:result+term;
                    if(!isfinite(result)){ctx->depth--;return NAN;}
                    if(infinite){
                        int small=product?fabsf(term-1.0f)<=1e-6f:
                            fabsf(term)<=1e-6f*(1+fabsf(result));
                        if(small)quiet++;else quiet=0;
                        if(k>=15 && quiet>=8){converged=1;break;}
                    }
                }
                ctx->depth--;if(!converged)return NAN;
                st[sp-1]=result;j=after-1;break;
            }
            case I_POWFACT:
                if(sp<2)return NAN;
                b=st[--sp];a=st[sp-1];
                if(!isfinite(a)||!isfinite(b)||b<0||b>EXPR_SUM_MAX_TERMS||floorf(b)!=b)return NAN;
                st[sp-1]=1;for(int k=1;k<=(int)b;k++)st[sp-1]*=a/(float)k;
                break;
            case I_FUNC:
                if(sp<1)return NAN;
                b=0;
                if(is_binary_func(in->aux)) { if(sp<2)return NAN; b=st[--sp]; }
                a=st[sp-1]; st[sp-1]=eval_func_real(in->aux,a,b); break;
            default:
                if(sp<2)return NAN;
                b=st[--sp]; a=st[sp-1];
                switch(in->op) {
                    case I_ADD:st[sp-1]=a+b;break; case I_SUB:st[sp-1]=a-b;break;
                    case I_MUL:st[sp-1]=a*b;break; case I_DIV:st[sp-1]=b==0?NAN:a/b;break;
                    case I_POW:st[sp-1]=powf(a,b);break; default:return NAN;
                }
        }
    }
    return sp==1 ? st[0] : NAN;
}
float expr_eval_real(Program const *p, EvalVars const *vars)
{
    if(!p || !vars)return NAN;
    EvalCtx ctx={EXPR_SUM_MAX_WORK,0};return eval_real_range(p,vars,0,p->size,&ctx);
}
static Complex cx(float re,float im) { Complex z={re,im};return z; }
static Complex cmul(Complex a,Complex b) { return cx(a.re*b.re-a.im*b.im,a.re*b.im+a.im*b.re); }
static Complex cdiv_(Complex a,Complex b)
{
    float d=b.re*b.re+b.im*b.im;
    if(d==0 || !isfinite(d)) return cx(NAN,NAN);
    return cx((a.re*b.re+a.im*b.im)/d,(a.im*b.re-a.re*b.im)/d);
}
static Complex clog_(Complex a)
{
    float r=hypotf(a.re,a.im);
    return r==0 ? cx(NAN,NAN) : cx(logf(r),atan2f(a.im,a.re));
}
static Complex cexp_(Complex a)
{
    float r=expf(a.re); return cx(r*cosf(a.im),r*sinf(a.im));
}
static Complex cpow_(Complex a,Complex b)
{
    /* Integer powers avoid log(0), and are much cheaper in series/fractals. */
    if(b.im==0 && isfinite(b.re) && floorf(b.re)==b.re && fabsf(b.re)<=128) {
        int n=(int)b.re,negative=n<0;if(negative)n=-n;
        Complex r=cx(1,0),base=a;
        while(n){if(n&1)r=cmul(r,base);n>>=1;if(n)base=cmul(base,base);}
        return negative?cdiv_(cx(1,0),r):r;
    }
    if(a.re==0 && a.im==0) {
        if(b.im==0 && b.re>0)return cx(0,0);
        return cx(NAN,NAN);
    }
    return cexp_(cmul(b,clog_(a)));
}
static Complex func_complex(int f, Complex a, Complex b)
{
    switch(f) {
        case F_SIN:return cx(sinf(a.re)*coshf(a.im),cosf(a.re)*sinhf(a.im));
        case F_COS:return cx(cosf(a.re)*coshf(a.im),-sinf(a.re)*sinhf(a.im));
        case F_TAN:return cdiv_(func_complex(F_SIN,a,b),func_complex(F_COS,a,b));
        case F_SQRT: {
            float r=hypotf(a.re,a.im);
            float x=sqrtf(fmaxf(0,(r+a.re)*0.5f));
            float y=sqrtf(fmaxf(0,(r-a.re)*0.5f));
            if(a.im<0) y=-y;
            return cx(x,y);
        }
        case F_ABS:return cx(hypotf(a.re,a.im),0);
        case F_LN:return clog_(a);
        case F_LOG:{ Complex z=clog_(a); return cx(z.re/logf(10.0f),z.im/logf(10.0f)); }
        case F_EXP:return cexp_(a);
        case F_RE:return cx(a.re,0); case F_IM:return cx(a.im,0); case F_CONJ:return cx(a.re,-a.im);
        default:
            if(a.im!=0 || (is_binary_func(f) && b.im!=0)) return cx(NAN,NAN);
            return cx(eval_func_real(f,a.re,b.re),0);
    }
}
static Complex eval_complex_range(Program const *p,EvalVars const *vars,int start,int end,EvalCtx *ctx)
{
    Complex st[EXPR_STACK]; int sp=0;
    for(int j=start;j<end;j++) {
        Instr const *in=&p->code[j]; Complex a,b;
        switch(in->op) {
            case I_NUM:if(sp>=EXPR_STACK)return cx(NAN,NAN);st[sp++]=cx(in->number,0);break;
            case I_VAR:
                if(sp>=EXPR_STACK || in->aux>=V_COUNT)return cx(NAN,NAN);
                st[sp++]=(vars->real_overrides&(1u<<in->aux))?cx(vars->v[in->aux],0):
                    in->aux==V_Z?vars->cz:in->aux==V_C?vars->cc:cx(vars->v[in->aux],0);break;
            case I_IMAG:if(sp>=EXPR_STACK)return cx(NAN,NAN);st[sp++]=cx(0,1);break;
            case I_NEG:if(sp<1)return cx(NAN,NAN);st[sp-1].re=-st[sp-1].re;st[sp-1].im=-st[sp-1].im;break;
            case I_SQUARE:if(sp<1)return cx(NAN,NAN);st[sp-1]=cmul(st[sp-1],st[sp-1]);break;
            case I_CUBE:if(sp<1)return cx(NAN,NAN);a=st[sp-1];st[sp-1]=cmul(cmul(a,a),a);break;
            case I_FACT:if(sp<1 || st[sp-1].im!=0)return cx(NAN,NAN);st[sp-1]=cx(factorial_real(st[sp-1].re),0);break;
            case I_SUM:case I_PROD: {
                if(sp<2 || in->aux>=V_COUNT || ctx->depth>=EXPR_SUM_MAX_DEPTH)return cx(NAN,NAN);
                Complex hi=st[--sp],lo=st[sp-1];int first,count,infinite;
                int body=j+1,after=body+(int)in->number,product=in->op==I_PROD;
                if(lo.im!=0 || hi.im!=0 || in->number<1 || after>end || !sum_limits(lo.re,hi.re,&first,&count,&infinite))return cx(NAN,NAN);
                EvalVars local=*vars;local.real_overrides|=1u<<in->aux;
                Complex result=cx(product?1.0f:0.0f,0);int quiet=0,converged=!infinite;ctx->depth++;
                for(int k=0;k<count;k++){
                    if(--ctx->work<0){ctx->depth--;return cx(NAN,NAN);}
                    local.v[in->aux]=(float)(first+k);
                    Complex term=eval_complex_range(p,&local,body,after,ctx);
                    if(!isfinite(term.re)||!isfinite(term.im)){ctx->depth--;return cx(NAN,NAN);}
                    if(product)result=cmul(result,term);
                    else {result.re+=term.re;result.im+=term.im;}
                    if(!isfinite(result.re)||!isfinite(result.im)){ctx->depth--;return cx(NAN,NAN);}
                    if(infinite){
                        int small=product?hypotf(term.re-1.0f,term.im)<=1e-6f:
                            hypotf(term.re,term.im)<=1e-6f*(1+hypotf(result.re,result.im));
                        if(small)quiet++;else quiet=0;
                        if(k>=15 && quiet>=8){converged=1;break;}
                    }
                }
                ctx->depth--;if(!converged)return cx(NAN,NAN);
                st[sp-1]=result;j=after-1;break;
            }
            case I_POWFACT:
                if(sp<2)return cx(NAN,NAN);
                b=st[--sp];a=st[sp-1];
                if(b.im!=0||!isfinite(b.re)||b.re<0||b.re>EXPR_SUM_MAX_TERMS||floorf(b.re)!=b.re)return cx(NAN,NAN);
                st[sp-1]=cx(1,0);for(int k=1;k<=(int)b.re;k++)st[sp-1]=cmul(st[sp-1],cx(a.re/k,a.im/k));
                break;
            case I_FUNC:
                if(sp<1)return cx(NAN,NAN);
                b=cx(0,0);
                if(is_binary_func(in->aux)) { if(sp<2)return cx(NAN,NAN); b=st[--sp]; }
                a=st[sp-1];st[sp-1]=func_complex(in->aux,a,b);break;
            default:
                if(sp<2)return cx(NAN,NAN);
                b=st[--sp];a=st[sp-1];
                switch(in->op) {
                    case I_ADD:st[sp-1]=cx(a.re+b.re,a.im+b.im);break;
                    case I_SUB:st[sp-1]=cx(a.re-b.re,a.im-b.im);break;
                    case I_MUL:st[sp-1]=cmul(a,b);break;
                    case I_DIV:st[sp-1]=cdiv_(a,b);break;
                    case I_POW:st[sp-1]=cpow_(a,b);break;default:return cx(NAN,NAN);
                }
        }
    }
    return sp==1?st[0]:cx(NAN,NAN);
}
Complex expr_eval_complex(Program const *p,EvalVars const *vars)
{
    if(!p||!vars)return cx(NAN,NAN);
    EvalCtx ctx={EXPR_SUM_MAX_WORK,0};return eval_complex_range(p,vars,0,p->size,&ctx);
}
