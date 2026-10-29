#include "layout.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>
#include <ctype.h>

/* Layout is synchronous. A shared scratch string avoids placing a 160-byte
 * temporary on every recursive frame of a deeply nested formula. */
static char scratch_text[EXPR_MAX];
static int max2(int a,int b) { return a>b?a:b; }
static void text_size(char const *s,int *w,int *h) { dsize(s,NULL,w,h); }
static char const *relation_text(unsigned r)
{
    switch(r){case REL_LT:return "<";case REL_LE:return "<=";
        case REL_GT:return ">";case REL_GE:return ">=";default:return "=";}
}
static void slice(Parsed const *p,int start,int end,char *buf,int cap)
{
    if(start<0)start=0;
    if(end>p->len)end=p->len;
    if(end<start)end=start;
    int n=end-start; if(n>=cap)n=cap-1;
    memcpy(buf,p->src+start,(size_t)n);buf[n]=0;
}
/* The one outer source group that defines a numerator, denominator or
   exponent is implicit in the mathematical layout. Keep additional nested
   groups visible, especially parentheses typed by the user. */
static int visual_part(Parsed const *p,int k)
{
    if(k>=0 && k<p->count && p->nodes[k].kind==N_GROUP)k=p->nodes[k].left;
    return k;
}
static int function_comma(Parsed const *p,ExprNode const *n)
{
    if(n->left<0 || n->right<0 || n->left>=p->count || n->right>=p->count)return -1;
    int at=p->nodes[n->left].end;
    while(at<p->len && isspace((unsigned char)p->src[at]))at++;
    int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
    if(at>=p->len || p->src[at]!=',')return -1;
    if(p->edit_comma_owner && p->edit_comma_owner[at]>=0 && p->edit_comma_owner[at]!=open)return -1;
    return at;
}
static int measure_node(Parsed *p,int k,int depth)
{
    if(k<0 || k>=p->count || depth>EXPR_NODES) return 0;
    ExprNode *n=&p->nodes[k]; int w=0,h=0,base=0;
    char *tmp=scratch_text;
    switch(n->kind) {
        case N_HOLE:w=9;h=14;base=10;break;
        case N_INF:w=14;h=14;base=10;break;
        case N_NUM:case N_VAR:case N_CONST_I:case N_RAW:
            slice(p,n->start,n->end,tmp,EXPR_MAX);text_size(tmp,&w,&h);base=h-3;
            if(w==0) w=1;
            break;
        case N_GROUP: {
            if(!measure_node(p,n->left,depth+1)) return 0;
            ExprNode *a=&p->nodes[n->left];int pw,ph;
            text_size("(",&pw,&ph);base=max2(a->baseline,ph-3);
            h=max2(base-a->baseline+a->height,base-(ph-3)+ph);
            w=a->width+pw+(n->right>=0?pw:0);
            break;
        }
        case N_FUNC: {
            if(!measure_node(p,n->left,depth+1))return 0;
            if(n->right>=0 && !measure_node(p,n->right,depth+1))return 0;
            ExprNode *a=&p->nodes[n->left];
            if(n->aux==F_SQRT && n->right<0) {w=a->width+11;h=a->height+5;base=a->baseline+5;break;}
            int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
            slice(p,n->start,open,tmp,EXPR_MAX);
            int nw,nh,pw,ph,cw,ch; text_size(tmp,&nw,&nh);text_size("(",&pw,&ph);text_size(",",&cw,&ch);
            base=max2(a->baseline,nh-3);base=max2(base,ph-3);
            int children=a->width,comma=function_comma(p,n);
            h=base-a->baseline+a->height;
            if(comma>=0) {
                ExprNode *b=&p->nodes[n->right];base=max2(base,b->baseline);children+=cw+b->width;
            }
            h=max2(base-a->baseline+a->height,base-(nh-3)+nh);
            if(comma>=0) {ExprNode *b=&p->nodes[n->right];h=max2(h,base-b->baseline+b->height);}
            w=nw+pw+(n->op_pos>=0?pw:0)+children;
            break;
        }
        case N_SUM:case N_PROD: {
            if(!measure_node(p,n->left,depth+1) || !measure_node(p,n->right,depth+1) ||
               !measure_node(p,n->extra,depth+1))return 0;
            ExprNode *lo=&p->nodes[n->left],*up=&p->nodes[n->right],*term=&p->nodes[n->extra];
            int iw,ih,eqw,eqh;
            if(n->index_end>n->index_start)slice(p,n->index_start,n->index_end,tmp,EXPR_MAX);
            else strcpy(tmp,"n");
            text_size(tmp,&iw,&ih);text_size("=",&eqw,&eqh);
            int sw=max2(20,max2(up->width,iw+eqw+lo->width))+4;
            int mb=max2(15,term->baseline);
            int mh=max2(mb+5,mb-term->baseline+term->height);
            int lowh=max2(lo->height,ih);
            w=sw+5+term->width;h=up->height+4+mh+3+lowh;base=up->height+4+mb;
            break;
        }
        case N_FACT: {
            if(!measure_node(p,n->left,depth+1))return 0;
            ExprNode *a=&p->nodes[n->left];int sw,sh;text_size("!",&sw,&sh);
            base=max2(a->baseline,sh-3);h=max2(base-a->baseline+a->height,base-(sh-3)+sh);w=a->width+sw;break;
        }
        case N_NEG: {
            if(!measure_node(p,n->left,depth+1))return 0;
            ExprNode *a=&p->nodes[n->left];int sw,sh;text_size("-",&sw,&sh);
            base=max2(a->baseline,sh-3);h=max2(base-a->baseline+a->height,base-(sh-3)+sh);w=sw+a->width;
            break;
        }
        case N_DIV: {
            if(!measure_node(p,n->left,depth+1) || !measure_node(p,n->right,depth+1))return 0;
            int ak=visual_part(p,n->left),bk=visual_part(p,n->right);
            if(ak<0||bk<0)return 0;
            ExprNode *a=&p->nodes[ak],*b=&p->nodes[bk];
            w=max2(a->width,b->width)+6;h=a->height+b->height+5;base=a->height+3;
            break;
        }
        case N_POW: {
            if(!measure_node(p,n->left,depth+1) || !measure_node(p,n->right,depth+1))return 0;
            int bk=visual_part(p,n->right);if(bk<0)return 0;
            ExprNode *a=&p->nodes[n->left],*b=&p->nodes[bk];
            base=max2(a->baseline,b->baseline+6);
            h=max2(base-a->baseline+a->height,base-b->baseline-6+b->height);w=a->width+b->width;
            break;
        }
        case N_ADD:case N_SUB:case N_MUL:case N_EQ: {
            if(n->kind==N_ADD && n->aux==1) {
                if(!measure_node(p,n->right,depth+1))return 0;
                ExprNode *b=&p->nodes[n->right];int sw,sh;text_size("+",&sw,&sh);
                base=max2(b->baseline,sh-3);w=sw+b->width;h=max2(base-b->baseline+b->height,base-(sh-3)+sh);break;
            }
            if(!measure_node(p,n->left,depth+1) || !measure_node(p,n->right,depth+1))return 0;
            ExprNode *a=&p->nodes[n->left],*b=&p->nodes[n->right];int sw=0,sh=0;
            if(n->op_pos>=0) text_size(n->kind==N_EQ?relation_text(n->aux):n->kind==N_ADD?"+":n->kind==N_SUB?"-":"*",&sw,&sh);
            base=max2(a->baseline,b->baseline);if(sw)base=max2(base,sh-3);
            h=max2(base-a->baseline+a->height,base-b->baseline+b->height);
            if(sw)h=max2(h,base-(sh-3)+sh);
            w=a->width+sw+b->width;break;
        }
        default:return 0;
    }
    if(w<1)w=1;
    if(h<1)h=1;
    if(base<0)base=0;
    /* Incomplete formulas are bounded by EXPR_MAX, but reject arithmetic overflow. */
    if(w>30000 || h>30000 || base>30000)return 0;
    n->width=(int16_t)w;n->height=(int16_t)h;n->baseline=(int16_t)base;
    return 1;
}
int layout_measure(Parsed *p) { return p->root>=0 && measure_node(p,p->root,0); }

typedef struct { Parsed const *p;int cursor,ink,cursor_color,dry;FormulaCaret c; } Draw;
static void mark(Draw *d,int pos,int x,int y,int h)
{
    if(pos==d->cursor) {d->c.x=x;d->c.y=y;d->c.h=h;d->c.found=1;}
}
static void text(Draw *d,int x,int y,char const *s,int start,int end)
{
    int fullw,h;text_size(s,&fullw,&h);(void)fullw;
    if(!d->dry)dtext(x,y,d->ink,s);
    if(start<0 || end<start || d->cursor<start || d->cursor>end)return;
    int count=d->cursor-start, len=(int)strlen(s);
    if(count>len)count=len;
    if(count<0)count=0;
    char prefix[EXPR_MAX]; if(count>=EXPR_MAX)count=EXPR_MAX-1;
    memcpy(prefix,s,(size_t)count);prefix[count]=0;
    int w,unused;text_size(prefix,&w,&unused);mark(d,d->cursor,x+w,y,h);
}
static void child(Draw *d,int k,int x,int y,int depth);
static void math_child(Draw *d,int k,int x,int y,int depth)
{
    if(k<0 || k>=d->p->count || depth>EXPR_TREE_DEPTH)return;
    ExprNode const *n=&d->p->nodes[k];
    if(n->kind!=N_GROUP){child(d,k,x,y,depth);return;}
    int visible=visual_part(d->p,k);if(visible<0)return;
    ExprNode const *v=&d->p->nodes[visible];
    mark(d,n->start,x,y,v->height);mark(d,n->start+1,x,y,v->height);
    /* Hide only the grouping layer supplied by this fraction/power. */
    child(d,n->left,x,y,depth+1);
    if(n->right>=0){mark(d,n->right,x+v->width,y,v->height);mark(d,n->right+1,x+v->width,y,v->height);}
    else mark(d,n->end,x+v->width,y,v->height);
}
static void draw_node(Draw *d,int k,int x,int y,int depth)
{
    if(k<0 || k>=d->p->count || depth>EXPR_NODES)return;
    ExprNode const *n=&d->p->nodes[k];int tw,th;char *tmp=scratch_text;
    switch(n->kind) {
        case N_INF: {
            /* Built-in gint font is ASCII-only; draw the infinity glyph. */
            if(!d->dry){
                int yy=y+6;
                dline(x+1,yy,x+3,yy-2,d->ink);dline(x+3,yy-2,x+5,yy-2,d->ink);
                dline(x+5,yy-2,x+9,yy+2,d->ink);dline(x+9,yy+2,x+11,yy+2,d->ink);
                dline(x+11,yy+2,x+13,yy,d->ink);dline(x+13,yy,x+11,yy-2,d->ink);
                dline(x+11,yy-2,x+9,yy-2,d->ink);dline(x+9,yy-2,x+5,yy+2,d->ink);
                dline(x+5,yy+2,x+3,yy+2,d->ink);dline(x+3,yy+2,x+1,yy,d->ink);
            }
            if(d->cursor>=n->start && d->cursor<n->end)mark(d,d->cursor,x,y,n->height);
            if(d->cursor==n->end)mark(d,d->cursor,x+n->width,y,n->height);
            break;
        }
        case N_HOLE:
            if(!d->dry)drect_border(x,y,x+n->width-1,y+n->height-1,C_WHITE,1,C_BLACK);
            mark(d,n->start,x+1,y,n->height);break;
        case N_NUM:case N_VAR:case N_CONST_I:case N_RAW:
            slice(d->p,n->start,n->end,tmp,EXPR_MAX);text(d,x,y,tmp,n->start,n->end);break;
        case N_GROUP: {
            ExprNode const *a=&d->p->nodes[n->left];text_size("(",&tw,&th);
            int ty=y+n->baseline-(th-3);
            text(d,x,ty,"(",n->start,n->start+1);
            child(d,n->left,x+tw,y+n->baseline-a->baseline,depth+1);
            int close=n->right;
            if(close>=0)text(d,x+tw+a->width,ty,")",close,close+1);
            else mark(d,n->end,x+tw+a->width,ty,th);
            break;
        }
        case N_FUNC: {
            ExprNode const *a=&d->p->nodes[n->left];
            int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
            int closed=n->op_pos>=0;
            if(n->aux==F_SQRT && n->right<0) {
                if(!d->dry) {
                    int bot=y+n->height-3;
                    dline(x,bot-4,x+2,bot-1,d->ink);dline(x+2,bot-1,x+5,y+3,d->ink);
                    dline(x+5,y+3,x+n->width-1,y+3,d->ink);
                }
                if(d->cursor>=n->start && d->cursor<=open)mark(d,d->cursor,x,y,n->height);
                child(d,n->left,x+10,y+5,depth+1);
                if(closed)mark(d,n->end,x+n->width,y+n->baseline-8,12);
                break;
            }
            slice(d->p,n->start,open,tmp,EXPR_MAX);text_size(tmp,&tw,&th);
            int ty=y+n->baseline-(th-3);text(d,x,ty,tmp,n->start,open);x+=tw;
            int pw,ph;text_size("(",&pw,&ph);ty=y+n->baseline-(ph-3);text(d,x,ty,"(",open,open+1);x+=pw;
            child(d,n->left,x,y+n->baseline-a->baseline,depth+1);x+=a->width;
            int comma=function_comma(d->p,n);
            if(comma>=0) {
                ExprNode const *b=&d->p->nodes[n->right];int cw,ch;text_size(",",&cw,&ch);
                text(d,x,y+n->baseline-(ch-3),",",comma,comma+1);x+=cw;
                child(d,n->right,x,y+n->baseline-b->baseline,depth+1);x+=b->width;
            }
            int close=closed?n->end-1:-1;
            if(closed)text(d,x,ty,")",close,close+1);
            else mark(d,n->end,x,ty,ph);
            break;
        }
        case N_SUM:case N_PROD: {
            ExprNode const *lo=&d->p->nodes[n->left],*up=&d->p->nodes[n->right],*term=&d->p->nodes[n->extra];
            int iw,ih,ew,eh;
            if(n->index_end>n->index_start)slice(d->p,n->index_start,n->index_end,tmp,EXPR_MAX);
            else strcpy(tmp,"n");
            text_size(tmp,&iw,&ih);text_size("=",&ew,&eh);
            int sw=n->width-5-term->width,mb=max2(15,term->baseline);
            int mh=max2(mb+5,mb-term->baseline+term->height);
            int middle=y+up->height+4,lower=middle+mh+3;
            int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
            int closed=n->op_pos>=0;
            int gx=x+(sw-16)/2,gy=middle;
            if(!d->dry){
                dline(gx,gy,gx+15,gy,d->ink);
                if(n->kind==N_PROD){
                    dline(gx+2,gy,gx+2,gy+18,d->ink);dline(gx+13,gy,gx+13,gy+18,d->ink);
                }else{
                    dline(gx,gy,gx+8,gy+9,d->ink);dline(gx+8,gy+9,gx,gy+18,d->ink);
                    dline(gx,gy+18,gx+15,gy+18,d->ink);
                }
            }
            /* The source position before the operator must be drawn on its
               left, not on the SUM/PROD symbol or in a hidden argument. */
            if(d->cursor==n->start)mark(d,d->cursor,x,middle+mb-10,14);
            else if(d->cursor>n->start && d->cursor<=open)mark(d,d->cursor,gx,gy,19);
            child(d,n->right,x+(sw-up->width)/2,y,depth+1);
            int baseline=max2(lo->baseline,ih-3);
            int ix=x+(sw-(iw+ew+lo->width))/2;
            if(n->index_end>n->index_start){
                slice(d->p,n->index_start,n->index_end,tmp,EXPR_MAX);
                text(d,ix,lower+baseline-(ih-3),tmp,n->index_start,n->index_end);
            } else {
                if(!d->dry)drect_border(ix,lower,ix+iw-1,lower+ih-1,C_WHITE,1,C_BLACK);
                mark(d,n->index_start,ix,lower,ih);
            }
            text(d,ix+iw,lower+baseline-(eh-3),"=",-1,-1);
            child(d,n->left,ix+iw+ew,lower+baseline-lo->baseline,depth+1);
            child(d,n->extra,x+sw+5,middle+mb-term->baseline,depth+1);
            /* Source commas and the closing parenthesis are structural. */
            int c1=lo->end,c2=up->end,c3=n->index_end;
            while(c1<d->p->len && d->p->src[c1]!=' ' && d->p->src[c1]!=',' && d->p->src[c1]!=')')c1++;
            if(!d->c.found && c1<d->p->len && d->p->src[c1]==',')mark(d,c1+1,x+(sw-up->width)/2,y,up->height);
            if(!d->c.found && c2<d->p->len && d->p->src[c2]==',')mark(d,c2+1,ix,lower,ih);
            if(!d->c.found && c3<d->p->len && d->p->src[c3]==',')mark(d,c3+1,x+sw+5,middle+mb-term->baseline,term->height);
            if(closed)mark(d,n->end,x+n->width+3,middle+mb-10,14);
            break;
        }
        case N_FACT: {
            ExprNode const *a=&d->p->nodes[n->left];text_size("!",&tw,&th);
            child(d,n->left,x,y+n->baseline-a->baseline,depth+1);
            text(d,x+a->width,y+n->baseline-(th-3),"!",n->op_pos,n->op_pos+1);break;
        }
        case N_NEG: {
            ExprNode const *a=&d->p->nodes[n->left];text_size("-",&tw,&th);
            text(d,x,y+n->baseline-(th-3),"-",n->op_pos,n->op_pos+1);
            child(d,n->left,x+tw,y+n->baseline-a->baseline,depth+1);break;
        }
        case N_DIV: {
            int ak=visual_part(d->p,n->left),bk=visual_part(d->p,n->right);if(ak<0||bk<0)break;
            ExprNode const *a=&d->p->nodes[ak],*b=&d->p->nodes[bk];
            int ax=x+(n->width-a->width)/2,bx=x+(n->width-b->width)/2;
            math_child(d,n->left,ax,y,depth+1);
            if(!d->dry)dline(x+1,y+a->height+2,x+n->width-2,y+a->height+2,d->ink);
            if(d->cursor==n->op_pos)mark(d,d->cursor,ax+a->width,y,a->height);
            if(d->cursor==n->op_pos+1)mark(d,d->cursor,bx,y+a->height+5,b->height);
            math_child(d,n->right,bx,y+a->height+5,depth+1);
            ExprNode const *group=&d->p->nodes[n->right];
            if(group->kind==N_GROUP && group->right>=0)mark(d,n->end,x+n->width,y+n->baseline-8,14);
            break;
        }
        case N_POW: {
            int bk=visual_part(d->p,n->right);if(bk<0)break;
            ExprNode const *a=&d->p->nodes[n->left],*b=&d->p->nodes[bk];
            child(d,n->left,x,y+n->baseline-a->baseline,depth+1);
            if(d->cursor==n->op_pos+1)mark(d,d->cursor,x+a->width,y+n->baseline-b->baseline-6,b->height);
            math_child(d,n->right,x+a->width,y+n->baseline-b->baseline-6,depth+1);
            ExprNode const *group=&d->p->nodes[n->right];
            if(group->kind==N_GROUP && group->right>=0)mark(d,n->end,x+n->width,y+n->baseline-a->baseline,a->height);
            break;
        }
        case N_ADD:case N_SUB:case N_MUL:case N_EQ: {
            if(n->kind==N_ADD && n->aux==1) {
                ExprNode const *b=&d->p->nodes[n->right];text_size("+",&tw,&th);
                text(d,x,y+n->baseline-(th-3),"+",n->op_pos,n->op_pos+1);
                child(d,n->right,x+tw,y+n->baseline-b->baseline,depth+1);break;
            }
            ExprNode const *a=&d->p->nodes[n->left],*b=&d->p->nodes[n->right];
            child(d,n->left,x,y+n->baseline-a->baseline,depth+1);x+=a->width;
            if(n->op_pos>=0) {
                const char *op=n->kind==N_EQ?relation_text(n->aux):n->kind==N_ADD?"+":n->kind==N_SUB?"-":"*";
                text_size(op,&tw,&th);text(d,x,y+n->baseline-(th-3),op,n->op_pos,n->op_pos+(int)strlen(op));x+=tw;
            }
            child(d,n->right,x,y+n->baseline-b->baseline,depth+1);break;
        }
    }
}
static void child(Draw *d,int k,int x,int y,int depth) {draw_node(d,k,x,y,depth);}
FormulaCaret layout_draw(Parsed const *p,int cursor,int x,int y,int ink,int cursor_color,int dry_run)
{
    Draw d={.p=p,.cursor=cursor,.ink=ink,.cursor_color=cursor_color,.dry=dry_run,.c={0,0,0,0}};
    if(p->root>=0)draw_node(&d,p->root,x,y,0);
    /* Whitespace is ignored by the parser but can still occur in imported
       formulas. Anchor a cursor inside it to the nearest neighboring token. */
    if(!d.c.found && cursor>=0 && cursor<=p->len) {
        int left=cursor,right=cursor;
        while(left>0 && isspace((unsigned char)p->src[left-1]))left--;
        while(right<p->len && isspace((unsigned char)p->src[right]))right++;
        if(left!=cursor || right!=cursor){
            Draw alt={.p=p,.cursor=left,.ink=ink,.cursor_color=cursor_color,.dry=1,.c={0,0,0,0}};
            draw_node(&alt,p->root,x,y,0);
            if(!alt.c.found && right!=left){alt.cursor=right;draw_node(&alt,p->root,x,y,0);}
            if(alt.c.found)d.c=alt.c;
        }
    }
    if(!dry_run && d.c.found && cursor>=0) dline(d.c.x,d.c.y,d.c.x,d.c.y+max2(8,d.c.h-1),cursor_color);
    return d.c;
}

static int hidden_cursor(Parsed const *p,int at)
{
    for(int i=0;i<p->count;i++) {
        ExprNode const *n=&p->nodes[i];
        if(n->kind==N_INF && at>n->start && at<n->end)return 1;
        if(n->kind==N_DIV || n->kind==N_POW) {
            int groups[2]={n->kind==N_DIV?n->left:-1,n->right};
            for(int j=0;j<2;j++){
                int g=groups[j];
                if(g>=0 && g<p->count && p->nodes[g].kind==N_GROUP &&
                   at==p->nodes[g].start)return 1;
            }
            /* The position after a hidden numerator ')' is the same drawn
               point as just before it; do not require two arrow presses. */
            if(n->kind==N_DIV && n->left>=0 && n->left<p->count){
                ExprNode const *a=&p->nodes[n->left];
                if(a->kind==N_GROUP && a->right>=0 && at==a->right+1 && at==n->op_pos)return 1;
            }
        }
        if(n->kind==N_SUM || n->kind==N_PROD || (n->kind==N_FUNC && n->aux==F_SQRT && n->right<0)) {
            int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
            if(at>n->start && at<=open)return 1;
        }
    }
    return 0;
}
int layout_cursor_step(Parsed const *p,int cursor,int direction)
{
    if(!p || direction==0)return cursor;
    if(direction>0){
        /* Left/right follow the main expression line: enter the term from
           immediately before the operator. Bounds/index remain reachable
           through UP/DOWN and SHIFT+LEFT, including for nested operators. */
        for(int i=0;i<p->count;i++){
            ExprNode const *n=&p->nodes[i];
            if((n->kind==N_SUM||n->kind==N_PROD) && n->start==cursor &&
               n->extra>=0 && n->extra<p->count){
                int term=p->nodes[n->extra].start;
                if(term>cursor && term<=p->len)return term;
            }
        }
    }
    int aggregate=-1,part=layout_sum_field(p,cursor,&aggregate);
    if(aggregate>=0){
        ExprNode const *n=&p->nodes[aggregate];
        ExprNode const *lo=&p->nodes[n->left],*hi=&p->nodes[n->right],*body=&p->nodes[n->extra];
        /* Follow the *displayed* layout instead of the hidden source commas.
           The index and lower bound share the bottom row, the term is right. */
        if(direction>0){
            if(part==2 && cursor==n->index_end)return lo->start;
            if((part==0 && cursor==lo->end)||(part==1 && cursor==hi->end))return body->start;
            if(part==3 && cursor==body->end && n->op_pos>=0)return n->end;
        }else{
            if((part==2 && cursor==n->index_start)||(part==1 && cursor==hi->start))
                return n->start;
            if(part==0 && cursor==lo->start)return n->index_end;
            if(part==3 && cursor==body->start)return n->start;
        }
    }
    int at=cursor;
    do {at+=direction>0?1:-1;} while(at>0 && at<p->len && hidden_cursor(p,at));
    if(at<0)at=0;
    if(at>p->len)at=p->len;
    return at;
}

/* The closing source ')' of a grouped denominator has a separate outside
   caret, while its hidden opening is an entry point into the denominator. */
static int fraction_part(Parsed const *p,ExprNode const *n,int cursor)
{
    if(n->kind!=N_DIV || n->left<0 || n->right<0 ||
       n->left>=p->count || n->right>=p->count)return -1;
    int ak=visual_part(p,n->left),bk=visual_part(p,n->right);
    if(ak<0 || bk<0 || ak>=p->count || bk>=p->count)return -1;
    ExprNode const *a=&p->nodes[ak],*b=&p->nodes[bk],*group=&p->nodes[n->right];
    int which=-1;
    if((cursor>=a->start && cursor<=a->end) || cursor==n->op_pos)which=0;
    if((cursor>=b->start && cursor<=b->end) || cursor==n->op_pos+1)which=1;
    if(group->kind==N_GROUP && group->right>=0 && cursor==n->end)which=-1;
    return which;
}
int layout_fraction_field(Parsed const *p,int cursor,int *fraction_node)
{
    if(fraction_node)*fraction_node=-1;
    if(!p)return -1;
    int best=-1,part=-1,span=EXPR_MAX+1;
    for(int i=0;i<p->count;i++){
        ExprNode const *n=&p->nodes[i];int which=fraction_part(p,n,cursor);
        if(which>=0 && n->end-n->start<span){best=i;part=which;span=n->end-n->start;}
    }
    if(fraction_node)*fraction_node=best;
    return part;
}
int layout_fraction_step(Parsed const *p,int cursor,int direction)
{
    if(!p || !direction)return cursor;
    int best=-1,span=EXPR_MAX+1;
    for(int i=0;i<p->count;i++){
        ExprNode const *n=&p->nodes[i];int part=fraction_part(p,n,cursor);
        /* At the top/bottom of an inner fraction, continue to the nearest
           enclosing fraction that actually has a field in this direction. */
        if(((direction<0 && part==1)||(direction>0 && part==0)) && n->end-n->start<span){
            best=i;span=n->end-n->start;
        }
    }
    if(best<0)return cursor;
    ExprNode const *n=&p->nodes[best];
    int target=visual_part(p,direction<0?n->left:n->right);
    return target>=0 && target<p->count?p->nodes[target].end:cursor;
}

/* SUM/PROD editable parts: lower, upper, index, term. Choose the
   smallest containing operator so nested expressions work naturally. */
int layout_sum_field(Parsed const *p,int cursor,int *sum_node)
{
    if(!p)return -1;
    int best=-1,part=-1,span=EXPR_MAX+1;
    for(int i=0;i<p->count;i++){
        ExprNode const *n=&p->nodes[i];
        if((n->kind!=N_SUM&&n->kind!=N_PROD)||n->left<0||n->right<0||n->extra<0)continue;
        ExprNode const *lo=&p->nodes[n->left],*hi=&p->nodes[n->right],*body=&p->nodes[n->extra];
        int candidate=-1;
        if(cursor>=lo->start && cursor<=lo->end)candidate=0;
        else if(cursor>=hi->start && cursor<=hi->end)candidate=1;
        else if(cursor>=n->index_start && cursor<=n->index_end)candidate=2;
        else if(cursor>=body->start && cursor<=body->end)candidate=3;
        if(candidate>=0 && n->end-n->start<span){best=i;part=candidate;span=n->end-n->start;}
    }
    if(sum_node)*sum_node=best;
    return part;
}
int layout_sum_step(Parsed const *p,int cursor,int direction)
{
    int k=-1,part=layout_sum_field(p,cursor,&k);
    if(part<0 || !direction)return cursor;
    ExprNode const *n=&p->nodes[k];
    /* UP selects the visibly upper limit, DOWN the lower. LEFT from the
       beginning of the lower limit selects the index; RIGHT returns to body. */
    return direction<0?p->nodes[n->right].end:p->nodes[n->left].end;
}
int layout_sum_delimiter(Parsed const *p,int cursor,char delimiter)
{
    if(!p || !p->src || cursor<0 || cursor>p->len)return -1;
    int k=-1,part=layout_sum_field(p,cursor,&k);
    if(k<0)return -1;
    ExprNode const *n=&p->nodes[k],*lo=&p->nodes[n->left],*hi=&p->nodes[n->right],*body=&p->nodes[n->extra];
    if(delimiter==','){
        int end=part==0?lo->end:part==1?hi->end:part==2?n->index_end:-1;
        if(cursor!=end)return -1;
        int at=cursor;while(at<n->end && isspace((unsigned char)p->src[at]))at++;
        if(at>=n->end || p->src[at]!=',')return -1;
        return part==0?hi->start:part==1?n->index_start:body->start;
    }
    if(delimiter==')' && n->op_pos>=0){
        /* When the user types empty summ() or prod(), the auto-created
           structural commas already separate the four holes. Finish the
           typed parentheses by putting the caret in the term, like F1/F4. */
        if(part==0 && cursor==lo->start && lo->kind==N_HOLE && hi->kind==N_HOLE &&
           body->kind==N_HOLE && n->index_start==n->index_end &&
           n->end>0 && p->src[n->end-1]==')')return body->start;
        if(part==3 && cursor==body->end){
            int at=cursor;while(at<n->end && isspace((unsigned char)p->src[at]))at++;
            if(at==n->end-1 && p->src[at]==')')return n->end;
        }
    }
    return -1;
}
int layout_close_delimiter(Parsed const *p,int cursor)
{
    if(!p || !p->src || cursor<0 || cursor>=p->len || p->src[cursor]!=')')return -1;
    int best=-1,span=EXPR_MAX+1;
    for(int i=0;i<p->count;i++){
        ExprNode const *n=&p->nodes[i];
        int closed=(n->kind==N_GROUP && n->right==cursor) ||
                   (n->kind==N_FUNC && n->op_pos>=0 && n->end-1==cursor);
        if(closed && n->end-n->start<span){best=n->end;span=n->end-n->start;}
    }
    return best;
}
int layout_delete_start(Parsed const *p,int cursor)
{
    if(!p || cursor<=0)return cursor>0?cursor-1:0;
    int k=-1,part=layout_sum_field(p,cursor,&k);
    if(k>=0){
        ExprNode const *n=&p->nodes[k];
        int starts[]={p->nodes[n->left].start,p->nodes[n->right].start,
                      n->index_start,p->nodes[n->extra].start};
        /* Do not let backspace eat an invisible separator or the '(' when
           the caret is at the beginning of an editable aggregate field. */
        if(part>=0 && cursor==starts[part])return cursor;
    }
    /* Infinity is a single displayed glyph, not three separate letters. */
    for(int i=0;i<p->count;i++){
        ExprNode const *n=&p->nodes[i];
        if(n->kind==N_INF && cursor==n->end)return n->start;
    }
    return cursor-1;
}
