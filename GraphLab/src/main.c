#include "platform.h"
#include "expr.h"
#include "layout.h"
#include "editor_meta.h"
#include "plot.h"
#include "plot2d.h"
#include "voxel.h"
#include "ray.h"
#include "fractal.h"
#include "input.h"
#include "navigation.h"
#include "navigation_input.h"
#include "state.h"
#include "storage.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>
#ifndef GL_HOST
#include <fxlibc/printf.h>
#endif

#define NAVY C_RGB(2,5,10)
#define TEAL C_RGB(1,21,20)
#define PALE C_RGB(27,30,31)
#define BORDER C_RGB(21,25,27)
#define INK C_RGB(2,5,8)
#define MUTED C_RGB(12,15,18)
#define ACCENT C_RGB(1,16,23)
#define WARNING C_RGB(25,6,3)
#define MAXF(a,b) ((a)>(b)?(a):(b))
#define MINF(a,b) ((a)<(b)?(a):(b))

static GraphState app;
static GraphState settings_before;
static Parsed parsed;
static Program programs[CURVE_COUNT], explicit_programs[CURVE_COUNT], scratch_program, fractal_program;
static Relation explicit_relations[CURVE_COUNT];
static uint8_t explicit_valid[CURVE_COUNT];
static char notice[100];
static const int palette[3]={C_RGB(28,6,5),C_RGB(2,10,26),C_RGB(3,21,9)};
static const int surface_palette[2]={C_RGB(4,21,28),C_RGB(28,8,5)};
static const int ray_divs[]={12,20,32,48};
static const float accuracy_values[]={.25f,.5f,1.0f,2.0f};
/* Input mode is separate from the ObjView-style 3D camera slow mode. */
static int alpha_mode,alpha_upper,settings_active;
static void settings(void);
static key_event_t input_key(void)
{
#ifdef GL_HOST
    key_event_t e=getkey();
#else
    /* Receive ALPHA itself; default getkey() consumes it as a one-shot. */
    key_event_t e=getkey_opt(GETKEY_DEFAULT & ~GETKEY_MOD_ALPHA,NULL);
#endif
    alpha_upper=input_apply(&e,&alpha_mode,keydown(KEY_SHIFT),keydown(KEY_ALPHA));
    return e;
}

static void header(char const *title,char const *right)
{
    drect(0,0,DWIDTH-1,17,NAVY);dtext(7,3,C_WHITE,title);
    if(right){int w,h;dsize(right,NULL,&w,&h);(void)h;dtext(DWIDTH-w-7,3,C_RGB(18,27,30),right);}
}
static void footer(char const *a,char const *b,char const *c,char const *d,char const *e,char const *f)
{
    char const *names[]={a,b,c,d,e,f};
    drect(0,203,DWIDTH-1,DHEIGHT-1,NAVY);
    for(int i=0;i<6;i++) {
        int x=2+i*65;
        drect(x,206,x+62,220,i==5?TEAL:C_RGB(5,10,15));
        if(names[i])dtext(x+3,209,C_WHITE,names[i]);
    }
}
static void centered(int y,int color,char const *text)
{
    int w,h;dsize(text,NULL,&w,&h);(void)h;dtext((DWIDTH-w)/2,y,color,text);
}
static void message(char const *title,char const *body)
{
    dclear(C_WHITE);header(title,NULL);dtext(10,58,INK,body);
    dtext(10,88,MUTED,"Press EXE or EXIT to continue.");footer(NULL,NULL,NULL,NULL,NULL,"OK");dupdate();
    for(;;){key_event_t e=input_key();if(e.key==KEY_EXE||e.key==KEY_EXIT||e.key==KEY_F6)return;}
}
static void save_now(void)
{
    int rc=storage_save(&app);
    if(rc==0)snprintf(notice,sizeof notice,"Saved to calculator storage.");
    else if(rc==-2)snprintf(notice,sizeof notice,"Save blocked: files occupied/damaged.");
    else snprintf(notice,sizeof notice,"Save failed. Earlier save was kept.");
}
static void draw_notice(void)
{
    if(notice[0])dtext(9,188,ACCENT,notice);
}
static void clear_notice(void){notice[0]=0;}
static EvalVars base_vars(void)
{
    EvalVars v;memset(&v,0,sizeof v);v.v[V_A]=app.param_a;v.v[V_B]=app.param_b;v.v[V_C]=app.param_c;v.v[V_T]=app.param_t;return v;
}
static int compile_formula(char const *s,Program *out,char *error,size_t error_size)
{
    if(!expr_parse(&parsed,s,0)) {snprintf(error,error_size,"%s @%d",parsed.message,parsed.error_pos+1);return 0;}
    return expr_compile(&parsed,out,error,error_size);
}
static int insert_text(char *text,int *cursor,char const *ins,int back,EditorMeta *meta)
{
    int len=(int)strlen(text),n=(int)strlen(ins);
    if(len+n>=EXPR_MAX)return 0;
    editor_meta_insert(meta,text,*cursor,ins);
    memmove(text+*cursor+n,text+*cursor,(size_t)(len-*cursor+1));
    memcpy(text+*cursor,ins,(size_t)n);*cursor+=n-back;
    return 1;
}
static int editor_has_left_operand(char const *text,int cursor)
{
    while(cursor>0 && isspace((unsigned char)text[cursor-1]))cursor--;
    if(!cursor)return 0;
    unsigned char c=(unsigned char)text[cursor-1];
    return isalnum(c) || c=='.' || c==')' || c=='!';
}
static int aggregate_name_before(char const *text,int cursor)
{
    if(cursor<4)return 0;
    int start=cursor-4;
    /* A name glued to another letter is one identifier, not a sum/product.
       A preceding number can be implicit multiplication, e.g. 2summ(...). */
    if(start>0 && isalpha((unsigned char)text[start-1]))return 0;
    char word[5];
    for(int i=0;i<4;i++)word[i]=(char)tolower((unsigned char)text[start+i]);
    word[4]=0;return !strcmp(word,"summ")||!strcmp(word,"prod");
}
static void delete_text(char *text,int *cursor,EditorMeta *meta)
{
    int len=(int)strlen(text);if(*cursor<=0)return;
    int end=*cursor,start=end-1;
    if(expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) && layout_measure(&parsed)){
        /* Hidden closers of aggregates, fractions and powers are not the
           visible item the user intended to backspace. Move into the field. */
        for(int pass=0;pass<EXPR_NODES;pass++){
            int inner=end;
            for(int i=0;i<parsed.count;i++){
                ExprNode const *n=&parsed.nodes[i];
                if((n->kind==N_SUM||n->kind==N_PROD) && n->op_pos>=0 && n->end==end && n->extra>=0){
                    inner=parsed.nodes[n->extra].end;break;
                }
                if(n->kind==N_FUNC && n->op_pos>=0 && n->end==end && n->left>=0){
                    int last=n->right>=0?n->right:n->left;
                    inner=parsed.nodes[last].end;break;
                }
                if((n->kind==N_DIV||n->kind==N_POW) && n->right>=0){
                    int groups[2]={n->right,n->kind==N_DIV?n->left:-1};
                    for(int j=0;j<2;j++)if(groups[j]>=0){
                        ExprNode const *g=&parsed.nodes[groups[j]];
                        if(g->kind==N_GROUP && g->right>=0 && g->end==end && g->left>=0){
                            inner=parsed.nodes[g->left].end;break;
                        }
                    }
                    if(inner<end)break;
                }
            }
            if(inner>=end)break;
            end=inner;
        }
        start=layout_delete_start(&parsed,end);
        /* Protect function template delimiters too. Backspace from an empty
           unary function removes the function, not just its opening '('. */
        for(int i=0;i<parsed.count;i++){
            ExprNode const *n=&parsed.nodes[i];
            if(n->kind!=N_FUNC || n->op_pos<0 || n->left<0)continue;
            ExprNode const *a=&parsed.nodes[n->left];
            if(end==a->start && start==n->op_pos){
                int other_empty=n->right<0 || parsed.nodes[n->right].kind==N_HOLE;
                if(a->kind==N_HOLE && other_empty){start=n->start;end=n->end;}
                else return;
                break;
            }
            if(n->right>=0){
                ExprNode const *b=&parsed.nodes[n->right];
                if(end==b->start && start>=0 && text[start]==','){
                    *cursor=a->end;return;
                }
            }
        }
        /* Never erase the invisible '(' of a fraction/power field. A wholly
           empty fraction, or an empty denominator, can be removed cleanly. */
        for(int i=0;i<parsed.count;i++){
            ExprNode const *n=&parsed.nodes[i];
            if((n->kind!=N_DIV&&n->kind!=N_POW)||n->right<0)continue;
            if(n->kind==N_DIV && n->left>=0){
                ExprNode const *a=&parsed.nodes[n->left],*b=&parsed.nodes[n->right];
                if(a->kind==N_GROUP && a->left>=0 && start==a->start && end==a->start+1){
                    if(parsed.nodes[a->left].kind==N_HOLE && b->kind==N_GROUP && b->left>=0 &&
                       parsed.nodes[b->left].kind==N_HOLE && b->right>=0){start=a->start;end=b->end;}
                    else return;
                    break;
                }
            }
            ExprNode const *g=&parsed.nodes[n->right];
            if(g->kind!=N_GROUP || g->left<0 || start!=g->start || end!=g->start+1)continue;
            ExprNode const *body=&parsed.nodes[g->left];
            if(body->kind==N_HOLE && g->right>=0){
                start=n->op_pos;end=g->end;
                if(n->kind==N_DIV && n->left>=0){
                    ExprNode const *a=&parsed.nodes[n->left];
                    if(a->kind==N_GROUP && a->left>=0 && parsed.nodes[a->left].kind==N_HOLE)
                        start=a->start;
                }
            }else return;
            break;
        }
    }
    if(start<0 || start>=end || end>len)return;
    editor_meta_delete(meta,len,start,end);
    memmove(text+start,text+end,(size_t)(len-end+1));*cursor=start;
}
static void formula_area(char const *text,int cursor,EditorMeta const *meta)
{
    drect_border(6,37,DWIDTH-7,143,C_WHITE,1,BORDER);
    struct dwindow old=dwindow_set((struct dwindow){9,39,DWIDTH-9,141});
    int ok=meta?expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner):expr_parse(&parsed,text,1);
    if(ok && layout_measure(&parsed)) {
        FormulaCaret c=layout_draw(&parsed,cursor,0,0,INK,ACCENT,1);
        int root=parsed.root;
        int ox=14,oy=52;
        if(parsed.nodes[root].height<70)oy=54+(70-parsed.nodes[root].height)/2;
        if(c.found) {
            if(ox+c.x>DWIDTH-22)ox=DWIDTH-22-c.x;
            if(ox+c.x<12)ox=12-c.x;
            if(oy+c.y+c.h>134)oy=134-c.y-c.h;
            if(oy+c.y<42)oy=42-c.y;
        }
        layout_draw(&parsed,cursor,ox,oy,INK,ACCENT,0);
    } else {
        dtext(14,66,INK,text);
        char prefix[EXPR_MAX];int n=cursor;if(n<0)n=0;if(n>=EXPR_MAX)n=EXPR_MAX-1;
        memcpy(prefix,text,(size_t)n);prefix[n]=0;int w,h;dsize(prefix,NULL,&w,&h);
        dline(14+w,66,14+w,66+h-1,ACCENT);
    }
    dwindow_set(old);
}
static uint32_t allowed_vars(int mode)
{
    uint32_t params=(1u<<V_A)|(1u<<V_B)|(1u<<V_C)|(1u<<V_T);
    if(mode==0)return 0;
    if(mode==1)return params|(1u<<V_X)|(1u<<V_Y)|(1u<<V_N);
    if(mode==2)return params|(1u<<V_X)|(1u<<V_Y)|(1u<<V_Z)|(1u<<V_N);
    if(mode==3)return params|(1u<<V_X)|(1u<<V_Y)|(1u<<V_Z)|(1u<<V_N)|(1u<<V_COUNT);
    return params;
}
/* Older saved formulas may contain a bare divisor (e.g. 1/sin(x)). Add
   hidden grouping for editing so the denominator has a distinct OUTSIDE
   caret. Grouping exactly the parsed divisor preserves operator precedence. */
static void editor_group_divisors(char *text)
{
    if(!expr_parse(&parsed,text,0))return;
    int16_t starts[EXPR_NODES],ends[EXPR_NODES];int count=0;
    for(int i=0;i<parsed.count;i++){
        ExprNode const *n=&parsed.nodes[i];
        if(n->kind!=N_DIV || n->right<0 || n->right>=parsed.count)continue;
        ExprNode const *b=&parsed.nodes[n->right];
        if(b->kind!=N_GROUP && b->start<b->end){starts[count]=b->start;ends[count]=b->end;count++;}
    }
    int len=(int)strlen(text);if(!count || len+2*count>=EXPR_MAX)return;
    char old[EXPR_MAX],out[EXPR_MAX];memcpy(old,text,(size_t)len+1);int k=0;
    for(int pos=0;pos<=len;pos++){
        for(int i=0;i<count;i++)if(ends[i]==pos)out[k++]=')';
        for(int i=0;i<count;i++)if(starts[i]==pos)out[k++]='(';
        if(pos<len)out[k++]=old[pos];
    }
    out[k]=0;
    char error[90];
    /* If extra grouping would exceed a parser/compiler limit, leave the
       original untouched rather than making a formerly valid formula fail. */
    if(compile_formula(out,&scratch_program,error,sizeof error))memcpy(text,out,(size_t)k+1);
}

/* Entering an existing bare numerator gives it a hidden source group. This
   makes later + or - stay in the numerator, just as they do in the already
   grouped denominator. No grouping is added while simply typing a divisor. */
static int editor_group_numerator(char *text,EditorMeta *meta,int *target,int fraction_node)
{
    if(fraction_node<0 || fraction_node>=parsed.count)return 1;
    ExprNode const *n=&parsed.nodes[fraction_node];
    if(n->kind!=N_DIV || n->left<0 || n->left>=parsed.count)return 1;
    ExprNode const *a=&parsed.nodes[n->left];
    if(a->kind==N_GROUP)return 1;
    int start=a->start,end=a->end,len=(int)strlen(text),old_target=*target;
    if(start<0 || end<start || end>len || len+2>=EXPR_MAX)return 0;
    char saved_text[EXPR_MAX];memcpy(saved_text,text,(size_t)len+1);
    EditorMeta saved=*meta;
    int at=start;
    if(!insert_text(text,&at,"(",0,meta))return 0;
    at=end+1;
    if(!insert_text(text,&at,")",0,meta) ||
       !expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) || !layout_measure(&parsed)){
        memcpy(text,saved_text,(size_t)len+1);*meta=saved;return 0;
    }
    /* Both inserted characters surround the target; it remains inside. */
    if(old_target>=start && old_target<=end)*target=old_target+1;
    else if(old_target>end)*target=old_target+2;
    return 1;
}

/* mode: 0 numeric, 1 2D, 2 3D, 3 complex recurrence, 4 real calculator. */
static int editor_seed(char *destination,char const *title,int mode,key_event_t first,int have_first)
{
    char text[EXPR_MAX],error[90]="";snprintf(text,sizeof text,"%s",destination);
    editor_group_divisors(text);
    EditorMeta meta;editor_meta_init(&meta,text);
    int cursor=(int)strlen(text),page=0;
    for(;;) {
        dclear(PALE);header(title,alpha_mode==INPUT_LOWER_LOCK?"ALPHA LOCK ON":
            alpha_mode==INPUT_UPPER_LOCK?"UPPER LOCK ON":alpha_mode==INPUT_ALPHA_ONCE?"ALPHA NEXT":"123 INPUT");
        dtext(10,22,MUTED,"LEFT/RIGHT: cursor   UP/DOWN: fields");
        formula_area(text,cursor,&meta);
        int sum_node=-1,sum_part=layout_sum_field(&parsed,cursor,&sum_node);
        int fraction_part=layout_fraction_field(&parsed,cursor,NULL);
        if(fraction_part>=0){
            dprint(10,151,ACCENT,"FRAC: %s   UP/DOWN: fields",fraction_part?"DENOMINATOR":"NUMERATOR");
            dtext(10,166,MUTED,sum_part>=0?"SHIFT+UP/DOWN: sum limits  |  RIGHT: out":"RIGHT: exit denominator  |  (): one each");
        } else if(sum_part>=0){
            static char const *const parts[]={"LOWER","UPPER","INDEX","EXPRESSION"};
            char const *op=parsed.nodes[sum_node].kind==N_PROD?"PROD":"SUM";
            dprint(10,151,ACCENT,"%s: %s   UP: upper  DOWN: lower",op,parts[sum_part]);
            dtext(10,166,MUTED,"SHIFT+LEFT: index  SHIFT+RIGHT: term");
        } else {
            dtext(10,151,MUTED,"F5: 123 / abc / ABC   |   OPTN: settings");
            dtext(10,166,MUTED,"SHIFT+F5: symbols  |  RIGHT exits a group");
        }
        if(error[0])dtext(10,184,WARNING,error);
        if(page==0)footer("x","y","z","=","a/A/1","DONE");
        else if(page==1)footer("a","b","c","i","a/A/1","DONE");
        else if(page==2)footer("pi","abs","re","im","a/A/1","DONE");
        else if(page==3)footer("conj","min","max","exp","a/A/1","DONE");
        else if(page==4)footer("n","t","floor","ceil","a/A/1","DONE");
        else if(page==5)footer("asin","acos","atan","atan2","a/A/1","DONE");
        else if(page==6)footer("sqrt","ln","log","e","a/A/1","DONE");
        else if(page==7)footer("SUM","INF","!","PROD","a/A/1","DONE");
        else footer("<","<=",">",">=","a/A/1","DONE");
        dupdate();
#ifdef GL_EDITOR_TRACE
        extern void graphlab_editor_trace(char const *,int,char const *);
        graphlab_editor_trace(text,cursor,error);
#endif
        key_event_t ev=have_first?first:input_key();have_first=0;int k=ev.key;
        if(k==KEY_EXIT)return 0;
        if(k==KEY_UP || k==KEY_DOWN){
            if(expr_parse_editor(&parsed,text,meta.mate,meta.comma_owner) && layout_measure(&parsed)){
                int direction=k==KEY_DOWN?1:-1;
                int fraction=layout_fraction_field(&parsed,cursor,NULL);
                int sum=layout_sum_field(&parsed,cursor,NULL);
                int next;
                if(ev.shift && sum>=0)next=layout_sum_step(&parsed,cursor,direction);
                else if(fraction>=0)next=layout_fraction_step(&parsed,cursor,direction);
                else next=layout_sum_step(&parsed,cursor,direction);
                int node=-1,part=layout_fraction_field(&parsed,next,&node);
                if(next!=cursor && part==0 && !editor_group_numerator(text,&meta,&next,node))
                    snprintf(error,sizeof error,"Cannot group numerator: limit reached.");
                else {cursor=next;error[0]=0;}
            }
            continue;
        }
        if(k==KEY_LEFT || k==KEY_RIGHT){
            if(expr_parse_editor(&parsed,text,meta.mate,meta.comma_owner) && layout_measure(&parsed)){
                int op=-1,field=layout_sum_field(&parsed,cursor,&op);
                int next;
                if(ev.shift && field>=0){
                    ExprNode const *n=&parsed.nodes[op];
                    next=k==KEY_LEFT?n->index_end:parsed.nodes[n->extra].end;
                }else next=layout_cursor_step(&parsed,cursor,k==KEY_LEFT?-1:1);
                int old_node=-1,new_node=-1;
                int old_part=layout_fraction_field(&parsed,cursor,&old_node);
                int new_part=layout_fraction_field(&parsed,next,&new_node);
                if(next>cursor && !editor_meta_can_cross(&meta,text,cursor,next))
                    snprintf(error,sizeof error,"Close the inner group with ).");
                else if(next!=cursor && new_part==0 && (new_node!=old_node || old_part!=0) &&
                        !editor_group_numerator(text,&meta,&next,new_node))
                    snprintf(error,sizeof error,"Cannot group numerator: limit reached.");
                else {cursor=next;error[0]=0;}
            }
            else if(k==KEY_LEFT && cursor>0)cursor--;
            else if(k==KEY_RIGHT && cursor<(int)strlen(text))cursor++;
            continue;
        }
        if(k==KEY_OPTN){if(!settings_active)settings();continue;}
        if(k==KEY_DEL){
            if(ev.shift){text[0]=0;cursor=0;editor_meta_init(&meta,text);}
            else delete_text(text,&cursor,&meta);
            error[0]=0;continue;
        }
        if(k==KEY_F5){
            if(ev.shift)page=(page+1)%9;
            else input_cycle_alpha(&alpha_mode);
            continue;
        }
        if(k==KEY_EXE || k==KEY_F6) {
            if(!compile_formula(text,&scratch_program,error,sizeof error))continue;
            if(scratch_program.variables & ~allowed_vars(mode)) {
                snprintf(error,sizeof error,"Unsupported free variable in this mode.");continue;
            }
            if((mode==0||mode==3||mode==4) && scratch_program.has_equal){
                snprintf(error,sizeof error,"Enter an expression without a comparison.");continue;
            }
            strcpy(destination,text);return 1;
        }
        char one[2]={0,0};char const *ins=NULL;int back=0;
        if(ev.alpha && input_alpha_letter(k)){
            one[0]=input_alpha_letter(k);
            if(alpha_upper)one[0]=(char)(one[0]-'a'+'A');
        }
        else if(k>=KEY_1 && k<=KEY_3)one[0]=(char)('1'+(k-KEY_1));
        else if(k>=KEY_4 && k<=KEY_6)one[0]=(char)('4'+(k-KEY_4));
        else if(k>=KEY_7 && k<=KEY_9)one[0]=(char)('7'+(k-KEY_7));
        else if(k==KEY_0)one[0]='0';
        else if(k==KEY_DOT)one[0]='.';
        else if(k==KEY_ADD)one[0]='+';
        else if(k==KEY_SUB || k==KEY_NEG)one[0]='-';
        else if(k==KEY_MUL)one[0]='*';
        else if(k==KEY_DIV || k==KEY_FRAC){
            /* A fraction at an empty position needs two real fields. */
            if(editor_has_left_operand(text,cursor)){ins="/()";back=1;}
            else {ins="()/()";back=4;}
        }
        else if(k==KEY_LEFTP)one[0]='(';
        else if(k==KEY_RIGHTP)one[0]=')';
        else if(k==KEY_COMMA)one[0]=',';
        else if(k==KEY_POWER){ins="^()";back=1;}
        else if(k==KEY_SQUARE){if(ev.shift){ins="sqrt()";back=1;}else ins="^2";}
        else if(k==KEY_XOT)one[0]='x';
        else if(k==KEY_SIN){ins=ev.shift?"asin()":"sin()";back=1;}
        else if(k==KEY_COS){ins=ev.shift?"acos()":"cos()";back=1;}
        else if(k==KEY_TAN){ins=ev.shift?"atan()":"tan()";back=1;}
        else if(k==KEY_LOG){ins="log()";back=1;}
        else if(k==KEY_LN){ins="ln()";back=1;}
        else if(k==KEY_FD || k==KEY_EXP)ins="pi";
        else if(k>=KEY_F1 && k<=KEY_F4) {
            int idx=k-KEY_F1;
            static char const *const page0[]={"x","y","z","="};
            static char const *const page1[]={"a","b","c","i"};
            static char const *const page2[]={"pi","abs()","re()","im()"};
            static char const *const page3[]={"conj()","min(,)","max(,)","exp()"};
            static char const *const page4[]={"n","t","floor()","ceil()"};
            static char const *const page5[]={"asin()","acos()","atan()","atan2(,)"};
            static char const *const page6[]={"sqrt()","ln()","log()","e"};
            static char const *const page7[]={"summ(0,inf,n,)","inf","!","prod(1,5,n,)"};
            static char const *const page8[]={"<","<=",">",">="};
            char const *const *map=page==0?page0:page==1?page1:page==2?page2:
                page==3?page3:page==4?page4:page==5?page5:page==6?page6:page==7?page7:page8;
            ins=map[idx];
            if(page==2 && idx>0)back=1;
            if(page==3)back=(idx==1||idx==2)?2:1;
            if(page==4 && idx>=2)back=1;
            if(page==5)back=(idx==3)?2:1;
            if(page==6 && idx<3)back=1;
            if(page==7 && (idx==0||idx==3))back=1; /* start in the editable term */
        }
        if(one[0])ins=one;
        if(one[0]=='(' && aggregate_name_before(text,cursor)){
            /* Only the typed '(' is added; invisible commas give the four
               empty fields distinct positions. ')' remains a separate key. */
            ins="(,,,";back=3;
        }else if(one[0]==')'){
            int node=-1,field=-1;
            if(expr_parse_editor(&parsed,text,meta.mate,meta.comma_owner) && layout_measure(&parsed))
                field=layout_sum_field(&parsed,cursor,&node);
            if(node>=0 && field==0){
                ExprNode const *n=&parsed.nodes[node],*lo=&parsed.nodes[n->left],*hi=&parsed.nodes[n->right],*body=&parsed.nodes[n->extra];
                /* Typing summ() creates exactly one ')' at the end of the
                   empty template, then moves to its expression field. */
                if(n->op_pos<0 && cursor==lo->start && lo->kind==N_HOLE && hi->kind==N_HOLE &&
                   body->kind==N_HOLE && n->index_start==n->index_end){
                    int end=body->end,term=body->start;
                    if(!insert_text(text,&end,")",0,&meta))snprintf(error,sizeof error,"Expression limit reached.");
                    else {cursor=term;error[0]=0;}
                    continue;
                }
            }
            if(editor_meta_skip(&meta,text,cursor,')')){cursor++;error[0]=0;continue;}
        }else if(one[0]==',' && editor_meta_skip(&meta,text,cursor,',')){
            cursor++;error[0]=0;continue;
        }
        if(ins) {if(!insert_text(text,&cursor,ins,back,&meta))snprintf(error,sizeof error,"Expression limit reached.");else error[0]=0;}
    }
}
static int editor(char *destination,char const *title,int mode)
{
    key_event_t none={0};return editor_seed(destination,title,mode,none,0);
}
#ifdef GL_EDITOR_TEST
/* QEMU exercises the real editor with gint keydev and a synthetic matrix. */
int graphlab_test_editor(char *destination,int mode)
{
    alpha_mode=0;alpha_upper=0;settings_active=0;notice[0]=0;
    return editor(destination,"TEST EDITOR",mode);
}
void graphlab_test_group_divisors(char *text){editor_group_divisors(text);}
#endif
static int begins_new_formula(key_event_t e)
{
    int k=e.key;
    if(k==KEY_DEL || (e.alpha && input_alpha_letter(k)))return 1;
    if((k>=KEY_1 && k<=KEY_3)||(k>=KEY_4 && k<=KEY_6)||(k>=KEY_7 && k<=KEY_9)||k==KEY_0)return 1;
    return k==KEY_XOT || k==KEY_SIN || k==KEY_COS || k==KEY_TAN || k==KEY_LN || k==KEY_LOG;
}

static void examples(Equation *eq,int is3)
{
    static char const *const ex2[]={"y=sin(x)","x^2+y^2=9","y^2=x^3-x","x*y=1","y=1/x","sin(x*y)=0","(x^2+y^2-1)^3=x^2*y^3"};
    static char const *const ex3[]={"z+x^2=sqrt(y)","x^2+y^2+z^2=9","z=sin(x)*cos(y)","x^2+y^2=1","z^2=x^2+y^2","sin(x)+sin(y)+sin(z)=0"};
    char const *const *xs=is3?ex3:ex2;int count=is3?6:7,sel=0;
    for(;;){
        dclear(C_WHITE);header("EXAMPLES","REPLACES SELECTED SLOT");
        for(int i=0;i<count;i++){
            if(i==sel)drect(5,23+i*23,DWIDTH-6,43+i*23,PALE);
            dtext(12,27+i*23,i==sel?ACCENT:INK,xs[i]);
        }
        footer(NULL,NULL,NULL,NULL,NULL,"USE");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT)return;
        if(e.key==KEY_UP)sel=(sel+count-1)%count;
        if(e.key==KEY_DOWN)sel=(sel+1)%count;
        if(e.key==KEY_EXE||e.key==KEY_F6){strcpy(eq->formula,xs[sel]);eq->active=1;return;}
    }
}
static void help(void);
static void settings(void)
{
    /* Keep the world search cube anchored at the origin, independently of
       the observer. Legacy center/follow fields remain only for save import. */
    enum {S_WIDTH,S_RADIUS,S_A,S_B,S_C,S_T,S_JULIA_RE,S_JULIA_IM,
          S_ESCAPE,S_PERIOD,S_ACCURACY,S_RENDER,S_DIV,S_STEP,S_COUNT};
    char const *names[]={"2D line width (px)","3D clip half-size",
        "Parameter a","Parameter b","Parameter c","Parameter t","Julia c: real","Julia c: imaginary",
        "Fractal escape radius","X period (0 = off)","2D accuracy (px)","3D renderer","Voxel grid divisions",
        "Voxel step (world)"};
    float *values[]={NULL,&app.volume_radius,&app.param_a,&app.param_b,&app.param_c,&app.param_t,
        &app.fractal.julia_re,&app.fractal.julia_im,&app.fractal.escape_radius,&app.period_x,NULL,NULL,NULL,NULL};
    int sel=0,off=0,count=S_COUNT;
    settings_active++;
    for(;;){
        if(sel<off)off=sel;
        if(sel>=off+7)off=sel-6;
        dclear(C_WHITE);header("SETTINGS","EXE TO EDIT");
        for(int row=0;row<7 && row+off<count;row++){
            int i=row+off,y=22+row*23;if(i==sel)drect(6,y,DWIDTH-7,y+20,PALE);
            dtext(12,y+5,INK,names[i]);
            if(i==S_WIDTH)dprint(218,y+5,ACCENT,"%d px  LEFT/RIGHT",app.curve_width);
            else if(i==S_ACCURACY)dprint(218,y+5,ACCENT,"%.2g px",(double)accuracy_values[app.accuracy2]);
            else if(i==S_RENDER)dtext(218,y+5,ACCENT,app.render3_mode?"RAY":"VOXEL");
            else if(i==S_DIV)dprint(218,y+5,ACCENT,"N=%d  (6..64)",app.voxel_div);
            else if(i==S_STEP)dprint(218,y+5,ACCENT,"%.6g",(double)(2*app.volume_radius/app.voxel_div));
            else dprint(218,y+5,ACCENT,"%.6g",(double)*values[i]);
        }
        if(notice[0])draw_notice();
        else if(sel==S_RADIUS)dtext(9,188,MUTED,"Cube is fixed at origin: -R to +R.");
        else if(sel==S_WIDTH)dtext(9,188,MUTED,"2D curve thickness in screen pixels.");
        else if(sel>=S_A && sel<=S_T)dtext(9,188,MUTED,"Values used by letters in your formulas.");
        else if(sel==S_JULIA_RE || sel==S_JULIA_IM)dtext(9,188,MUTED,"Julia mode only: fixed complex c.");
        else if(sel==S_ESCAPE)dtext(9,188,MUTED,"Fractal: distance marking escape.");
        else if(sel==S_PERIOD)dtext(9,188,MUTED,"0 = off; use only a true x period.");
        else if(sel==S_ACCURACY)dtext(9,188,MUTED,"Smaller = finer 2D contour refinement.");
        else if(sel==S_RENDER)dtext(9,188,MUTED,"VOXEL: blocks  /  RAY: smooth, slower.");
        else if(sel==S_DIV || sel==S_STEP)dtext(9,188,MUTED,"Step = 2R/N. More N = finer blocks.");
        footer("EDIT",NULL,NULL,"HELP","SAVE","BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6){settings_active--;return;}
        if(e.key==KEY_UP){sel=(sel+count-1)%count;clear_notice();}
        else if(e.key==KEY_DOWN){sel=(sel+1)%count;clear_notice();}
        else if(e.key==KEY_F5)save_now();
        else if(e.key==KEY_F4)help();
        else if(sel==S_WIDTH && (e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_EXE||e.key==KEY_F1)){
            int d=e.key==KEY_LEFT?-1:1;app.curve_width=(uint8_t)((app.curve_width-1+d+5)%5+1);clear_notice();
        }
        else if(sel==S_ACCURACY && (e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_EXE||e.key==KEY_F1)){
            int d=e.key==KEY_LEFT?-1:1;app.accuracy2=(uint8_t)((app.accuracy2+d+4)%4);clear_notice();
        }
        else if(sel==S_RENDER && (e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_EXE||e.key==KEY_F1)){
            app.render3_mode^=1;clear_notice();
        }
        else if(sel==S_DIV && (e.key==KEY_LEFT||e.key==KEY_RIGHT)){
            int n=(int)app.voxel_div+(e.key==KEY_LEFT?-1:1);
            if(n>=VOXEL_MIN_DIV&&n<=VOXEL_MAX_DIV)app.voxel_div=(uint8_t)n;
            clear_notice();
        }
        else if((e.key==KEY_EXE||e.key==KEY_F1) && (values[sel]||sel==S_DIV||sel==S_STEP)) {
            char temp[EXPR_MAX],err[90];
            float old=values[sel]?*values[sel]:sel==S_DIV?(float)app.voxel_div:2*app.volume_radius/app.voxel_div;
            snprintf(temp,sizeof temp,"%.7g",(double)old);
            if(editor(temp,names[sel],0) && compile_formula(temp,&scratch_program,err,sizeof err)) {
                EvalVars vars={0};float x=expr_eval_real(&scratch_program,&vars);
                if(sel==S_DIV||sel==S_STEP){
                    float n=sel==S_DIV?x:(isfinite(x)&&x>0?2*app.volume_radius/x:NAN);
                    if(!isfinite(n)||n<VOXEL_MIN_DIV-.49f||n>VOXEL_MAX_DIV+.49f ||
                       (sel==S_DIV && fabsf(n-roundf(n))>.00001f))
                        snprintf(notice,sizeof notice,"Grid requires an integer N from 6 to 64.");
                    else {app.voxel_div=(uint8_t)lroundf(n);snprintf(notice,sizeof notice,"Grid updated; step is 2R/N. F5 saves.");}
                }else{
                    *values[sel]=x;
                    if(!state_validate(&app)){*values[sel]=old;snprintf(notice,sizeof notice,"Value is not finite or outside allowed range.");}
                    else snprintf(notice,sizeof notice,"Value updated. F5 saves the settings.");
                }
            }
        }
    }
}
static void help(void)
{
    dclear(C_WHITE);header("GRAPH LAB HELP","OFFLINE / fx-CG50");
    dtext(8,24,INK,"PLOT: choose view, then PLOT / EXE draws.");
    dtext(8,42,INK,"OPTN: settings. F5 saves in menus.");
    dtext(8,60,INK,"3D: arrows turn; 8/5 front/back; 4/6 side.");
    dtext(8,78,INK,"3D: 1/0 up/down; ALPHA slow; SHIFT fast.");
    dtext(8,96,INK,"3D F2 resets; F4 quality; OPTN clip range.");
    dtext(8,114,INK,"Editor F5: 123/abc/ABC; SHIFT+F5 symbols.");
    dtext(8,132,INK,"FRAC: UP/DOWN fields; RIGHT exits.");
    dtext(8,148,INK,"SUM: SHIFT+UP/DOWN bounds inside a fraction.");
    dtext(8,164,INK,"SUM: SHIFT+LEFT index, SHIFT+RIGHT term.");
    dtext(8,180,MUTED,"2D: +/- zoom, SHIFT+F2 line width.");
    footer(NULL,NULL,NULL,NULL,NULL,"BACK");dupdate();
    for(;;){key_event_t e=input_key();if(e.key==KEY_EXIT||e.key==KEY_EXE||e.key==KEY_F6)break;}
}

typedef struct {int cancel,menu,next;} Progress;
/* The physical screen retains the previous complete frame while calculations
   draw into VRAM. Polling never calls dupdate(), so partial plots are hidden. */
static int progress(int done,int total,void *context)
{
    (void)done;(void)total;Progress *p=context;key_event_t ev;
    while((ev=pollevent()).type!=KEYEV_NONE){
        if(ev.type==KEYEV_DOWN && ev.key==KEY_EXIT){p->cancel=1;return 0;}
        if(ev.type==KEYEV_DOWN && ev.key==KEY_MENU){p->menu=1;return 0;}
    }
    return 1;
}
static void calculation_begin(void)
{
#ifdef GL_HOST
    host_calculation_begin();
#endif
}
static void calculation_end(void)
{
#ifdef GL_HOST
    host_calculation_end();
#endif
}
/* Retain only non-white plot pixels, allowing grid/axes layer changes without
   re-evaluating the expression. */
static uint16_t *layer_create(void)
{
    return malloc((size_t)PLOT_W*PLOT_H*sizeof(uint16_t));
}
static void layer_capture(uint16_t *layer)
{
    for(int y=0;y<PLOT_H;y++)memcpy(layer+(size_t)y*PLOT_W,
        gint_vram+(size_t)(PLOT_Y0+y)*DWIDTH+PLOT_X0,(size_t)PLOT_W*sizeof(uint16_t));
}
static void layer_blit(uint16_t const *layer)
{
    for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++){
        uint16_t c=layer[(size_t)y*PLOT_W+x];
        if(c!=C_WHITE)gint_vram[(size_t)(PLOT_Y0+y)*DWIDTH+PLOT_X0+x]=c;
    }
}
/* Separate 1-bit line masks keep boundaries above all shaded regions.
   Changing width only recomposes the screen; no formula is re-evaluated. */
#define LINE_MASK_BYTES (((size_t)PLOT_W*PLOT_H+7)/8)
static void line_masks_draw(unsigned char const *masks,int width)
{
    int left=-(width-1)/2,right=width/2;
    for(int slot=0;slot<CURVE_COUNT;slot++){
        unsigned char const *mask=masks+(size_t)slot*LINE_MASK_BYTES;
        for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++){
            size_t i=(size_t)y*PLOT_W+x;if(!(mask[i>>3]&(1u<<(i&7))))continue;
            for(int dy=left;dy<=right;dy++)for(int dx=left;dx<=right;dx++){
                int xx=x+dx,yy=y+dy;
                if(xx>=0&&xx<PLOT_W&&yy>=0&&yy<PLOT_H)
                    gint_vram[(size_t)(PLOT_Y0+yy)*DWIDTH+PLOT_X0+xx]=(uint16_t)palette[slot];
            }
        }
    }
}
static int changed_common(GraphState const *a,GraphState const *b)
{
    return a->param_a!=b->param_a||a->param_b!=b->param_b||a->param_c!=b->param_c||a->param_t!=b->param_t;
}
static int compile_active(int is3)
{
    int count=is3?SURFACE_COUNT:CURVE_COUNT,active=0;
    Equation *eq=is3?app.surface:app.curve;
    for(int i=0;i<count;i++) {
        memset(&programs[i],0,sizeof programs[i]);explicit_valid[i]=0;
        if(!eq[i].active)continue;
        char error[90];
        if(!compile_formula(eq[i].formula,&programs[i],error,sizeof error)) {
            char m[120];snprintf(m,sizeof m,"Slot %d: %.75s",i+1,error);message("INVALID EQUATION",m);return 0;
        }
        if(programs[i].variables & ~allowed_vars(is3?2:1)) {
            message("INVALID EQUATION",is3?"3D: use x, y, z, n and parameters.":"2D: use x, y, n and parameters.");return 0;
        }
        if(!is3)explicit_valid[i]=(uint8_t)expr_compile_explicit_y(&parsed,&explicit_programs[i],&explicit_relations[i]);
        active++;
    }
    if(!active){message("NO EQUATIONS","Turn on at least one equation.");return 0;}
    return 1;
}
static int has_implicit_2d(void)
{
    for(int i=0;i<CURVE_COUNT;i++)if(app.curve[i].active&&!explicit_valid[i])return 1;
    return 0;
}
static PlaneView plane_view(void)
{
    float hw=app.plane_scale*(PLOT_W-1)*.5f,hh=app.plane_scale*(PLOT_H-1)*.5f;
    static const int step[]={6,4,2};int s=step[app.detail2];
    return (PlaneView){.xmin=app.plane_x-hw,.xmax=app.plane_x+hw,.ymin=app.plane_y-hh,.ymax=app.plane_y+hh,
        .cols=(PLOT_W-1)/s,.rows=(PLOT_H-1)/s,.period_x=app.period_x};
}
static int px(PlaneView const *v,float x){return PLOT_X0+(int)lroundf((x-v->xmin)*(PLOT_W-1)/(v->xmax-v->xmin));}
static int py(PlaneView const *v,float y){return PLOT_Y0+(int)lroundf((v->ymax-y)*(PLOT_H-1)/(v->ymax-v->ymin));}
static float axis_unit(void)
{
    float ideal=app.plane_scale*55.0f,power=powf(10.0f,floorf(log10f(ideal)));
    return ideal/power<2?power:ideal/power<5?2*power:5*power;
}
static void draw_grid(PlaneView const *v)
{
    drect(PLOT_X0,PLOT_Y0,PLOT_X1,PLOT_Y1,C_WHITE);
    float unit=axis_unit();if(!app.grid || !isfinite(unit) || unit<=0)return;
    float x=ceilf(v->xmin/unit)*unit;int limit=0;
    while(x<=v->xmax && limit++<30){int xp=px(v,x);if(xp>=PLOT_X0&&xp<=PLOT_X1)dline(xp,PLOT_Y0,xp,PLOT_Y1,C_RGB(28,29,30));x+=unit;}
    float y=ceilf(v->ymin/unit)*unit;limit=0;
    while(y<=v->ymax && limit++<30){int yp=py(v,y);if(yp>=PLOT_Y0&&yp<=PLOT_Y1)dline(PLOT_X0,yp,PLOT_X1,yp,C_RGB(28,29,30));y+=unit;}
}
static void draw_axes2d(PlaneView const *v)
{
    float unit=axis_unit();if(!isfinite(unit)||unit<=0)return;
    int axisx=v->xmin<=0&&v->xmax>=0?px(v,0):PLOT_X0;
    int axisy=v->ymin<=0&&v->ymax>=0?py(v,0):PLOT_Y1;
    if(axisx<PLOT_X0)axisx=PLOT_X0;
    if(axisx>PLOT_X1)axisx=PLOT_X1;
    if(axisy<PLOT_Y0)axisy=PLOT_Y0;
    if(axisy>PLOT_Y1)axisy=PLOT_Y1;
    struct dwindow old=dwindow_set((struct dwindow){PLOT_X0,PLOT_Y0,PLOT_X1+1,PLOT_Y1+1});
    dline(axisx,PLOT_Y0,axisx,PLOT_Y1,MUTED);dline(PLOT_X0,axisy,PLOT_X1,axisy,MUTED);
    float x=ceilf(v->xmin/unit)*unit;int count=0;
    int ly=axisy+3;if(ly>PLOT_Y1-11)ly=axisy-12;if(ly<PLOT_Y0)ly=PLOT_Y0;
    while(x<=v->xmax && count++<30) {
        int xp=px(v,x);dline(xp,axisy-2,xp,axisy+2,MUTED);
        if(xp>=PLOT_X0 && xp<=PLOT_X1-30)dprint(xp+2,ly,MUTED,"%.3g",(double)(fabsf(x)<unit*.0001f?0:x));
        x+=unit;
    }
    float y=ceilf(v->ymin/unit)*unit;count=0;
    int lx=axisx+3;if(lx>PLOT_X1-40)lx=axisx-39;if(lx<PLOT_X0)lx=PLOT_X0;
    while(y<=v->ymax && count++<30) {
        int yp=py(v,y);dline(axisx-2,yp,axisx+2,yp,MUTED);
        if(fabsf(y)>unit*.0001f && yp>=PLOT_Y0+2 && yp<=PLOT_Y1-9)
            dprint(lx,yp-8,MUTED,"%.3g",(double)y);
        y+=unit;
    }
    dwindow_set(old);
}
typedef struct {PlaneView const *view;unsigned char *mask;int dashed,phase;} LineContext;
static void draw_segment(float x0,float y0,float x1,float y1,void *context)
{
    LineContext *c=context;int a=px(c->view,x0),b=py(c->view,y0),d=px(c->view,x1),e=py(c->view,y1);
    if(a<PLOT_X0)a=PLOT_X0;
    if(a>PLOT_X1)a=PLOT_X1;
    if(d<PLOT_X0)d=PLOT_X0;
    if(d>PLOT_X1)d=PLOT_X1;
    if(b<PLOT_Y0)b=PLOT_Y0;
    if(b>PLOT_Y1)b=PLOT_Y1;
    if(e<PLOT_Y0)e=PLOT_Y0;
    if(e>PLOT_Y1)e=PLOT_Y1;
    int dx=abs(d-a),sx=a<d?1:-1,dy=-abs(e-b),sy=b<e?1:-1,err=dx+dy;
    for(;;){
        if(!c->dashed || (c->phase%9)<5){
            size_t i=(size_t)(b-PLOT_Y0)*PLOT_W+(a-PLOT_X0);
            c->mask[i>>3]|=(unsigned char)(1u<<(i&7));
        }
        c->phase++;
        if(a==d && b==e)break;
        int twice=2*err;if(twice>=dy){err+=dy;a+=sx;}if(twice<=dx){err+=dx;b+=sy;}
    }
}
typedef struct {int color;Relation relation;} FillContext;
static int fill_color(int slot)
{
    static const int colors[]={C_RGB(31,25,25),C_RGB(25,28,31),C_RGB(25,31,25)};
    return colors[slot%3];
}
static void fill_span(int x,int first,int last,void *context)
{
    FillContext *c=context;drect(PLOT_X0+x,PLOT_Y0+first,PLOT_X0+x,PLOT_Y0+last,c->color);
}
static void fill_cell(int ix,int iy,int cols,int rows,float const q[4],void *context)
{
    FillContext *c=context;
    for(int k=0;k<4;k++)if(!isfinite(q[k]))return;
    int x0=PLOT_X0+ix*(PLOT_W-1)/cols,x1=PLOT_X0+(ix+1)*(PLOT_W-1)/cols;
    int y0=PLOT_Y0+iy*(PLOT_H-1)/rows,y1=PLOT_Y0+(iy+1)*(PLOT_H-1)/rows;
    int t[4];for(int k=0;k<4;k++)t[k]=expr_relation_true(c->relation,q[k]);
    if(t[0]==t[1]&&t[0]==t[2]&&t[0]==t[3]){
        if(t[0])drect(x0,y0,x1,y1,c->color);
        return;
    }
    for(int y=y0;y<=y1;y++){
        float fy=y1==y0?0:(float)(y-y0)/(y1-y0);
        float left=q[0]*(1-fy)+q[3]*fy,right=q[1]*(1-fy)+q[2]*fy;
        if(!isfinite(left)||!isfinite(right))continue;
        for(int x=x0;x<=x1;x++){
            float fx=x1==x0?0:(float)(x-x0)/(x1-x0);
            float value=left*(1-fx)+right*fx;
            if(expr_relation_true(c->relation,value))dpixel(x,y,c->color);
        }
    }
}
static void probe2d(PlaneView const *view)
{
    size_t bytes=(size_t)DWIDTH*DHEIGHT*sizeof(uint16_t);
    uint16_t *backup=malloc(bytes);
    if(!backup){message("CURSOR MEMORY","Not enough memory for cursor mode.");return;}
    memcpy(backup,gint_vram,bytes);
    int x=(PLOT_X0+PLOT_X1)/2,y=(PLOT_Y0+PLOT_Y1)/2,slot=0;
    while(slot<CURVE_COUNT && !app.curve[slot].active)slot++;
    if(slot>=CURVE_COUNT){free(backup);return;}
    for(;;){
        memcpy(gint_vram,backup,bytes);
        dline(x,PLOT_Y0,x,PLOT_Y1,ACCENT);dline(PLOT_X0,y,PLOT_X1,y,ACCENT);
        float wx=view->xmin+(x-PLOT_X0)*(view->xmax-view->xmin)/(PLOT_W-1);
        float wy=view->ymax-(y-PLOT_Y0)*(view->ymax-view->ymin)/(PLOT_H-1);
        EvalVars v=base_vars();v.v[V_X]=wx;v.v[V_Y]=wy;
        float f=expr_eval_real(&programs[slot],&v);
        if(!programs[slot].has_equal && !expr_program_uses(&programs[slot],V_Y))f=wy-f;
        drect(7,173,350,198,C_WHITE);
        dprint(9,174,INK,"x=%.5g  y=%.5g",(double)wx,(double)wy);
        if(isfinite(f) && programs[slot].relation>=REL_LT)
            dprint(9,187,MUTED,"F%d=%.5g  %s",slot+1,(double)f,
                   expr_relation_true((Relation)programs[slot].relation,f)?"TRUE":"FALSE");
        else if(isfinite(f))dprint(9,187,MUTED,"F%d=%.5g  (zero on curve)",slot+1,(double)f);
        else dprint(9,187,MUTED,"F%d undefined at cursor",slot+1);
        footer("NEXT",NULL,"CENTER",NULL,NULL,"BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6||e.key==KEY_VARS)break;
        int step=e.shift?1:5;
        if(e.key==KEY_LEFT)x-=step;else if(e.key==KEY_RIGHT)x+=step;
        else if(e.key==KEY_UP)y-=step;else if(e.key==KEY_DOWN)y+=step;
        else if(e.key==KEY_F3){x=(PLOT_X0+PLOT_X1)/2;y=(PLOT_Y0+PLOT_Y1)/2;}
        else if(e.key==KEY_F1){for(int n=0;n<CURVE_COUNT;n++){slot=(slot+1)%CURVE_COUNT;if(app.curve[slot].active)break;}}
        if(x<PLOT_X0)x=PLOT_X0;
        if(x>PLOT_X1)x=PLOT_X1;
        if(y<PLOT_Y0)y=PLOT_Y0;
        if(y>PLOT_Y1)y=PLOT_Y1;
    }
    memcpy(gint_vram,backup,bytes);free(backup);
}
/* Edit a real point without conflating movement with rotation. */
static int point_editor(char const *title,float *x,float *y,float *z)
{
    float *v[3]={x,y,z};char const *names[3]={"x","y","z"};int count=z?3:2,sel=0,changed=0;
    for(;;){
        dclear(C_WHITE);header(title,"UP/DOWN: AXIS");
        for(int i=0;i<count;i++){
            if(i==sel)drect(10,38+i*31,DWIDTH-11,64+i*31,PALE);
            dprint(16,45+i*31,INK,"%s = %.7g",names[i],(double)*v[i]);
        }
        dtext(12,157,MUTED,"EXE: enter an exact coordinate.");
        footer("EDIT",NULL,NULL,NULL,NULL,"DONE");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6)return changed;
        if(e.key==KEY_UP)sel=(sel+count-1)%count;
        else if(e.key==KEY_DOWN)sel=(sel+1)%count;
        else if(e.key==KEY_F1||e.key==KEY_EXE){
            char temp[EXPR_MAX],error[90];snprintf(temp,sizeof temp,"%.7g",(double)*v[sel]);
            if(editor(temp,"COORDINATE",0) && compile_formula(temp,&scratch_program,error,sizeof error)){
                EvalVars vars={0};float next=expr_eval_real(&scratch_program,&vars);
                if(isfinite(next)&&fabsf(next)<=1e6f){if(*v[sel]!=next)changed=1;*v[sel]=next;}
                else message("COORDINATE","Value must be finite and within +/-1000000.");
            }
        }
    }
}
static void present2d(PlaneView const *view,uint16_t const *layer,unsigned char const *masks,int ready)
{
    dclear(C_WHITE);header("2D GRAPH",ready?"ARROWS PAN  +/- ZOOM  VARS CURSOR":"SET VIEW: VARS COORDS  +/- ZOOM");
    draw_grid(view);
    if(!app.axes_front)draw_axes2d(view);
    if(ready){layer_blit(layer);line_masks_draw(masks,app.curve_width);}
    if(app.axes_front)draw_axes2d(view);
    drect(7,183,285,198,C_WHITE);
    dprint(9,186,MUTED,"Center %.4g, %.4g  |  %.4g /px",(double)app.plane_x,(double)app.plane_y,(double)app.plane_scale);
    drect(289,183,PLOT_X1,198,C_WHITE);
    dprint(292,186,MUTED,"W%d D%d",app.curve_width,app.detail2+1);
    if(!ready)dtext(9,21,ACCENT,"Choose view; PLOT or EXE draws. OPTN: settings");
    footer("EQS","GRID","RESET","DETAIL",app.axes_front?"FRONT":"BACK","PLOT");dupdate();
}
static void plot2d(void)
{
    if(!compile_active(0))return;
    uint16_t *layer=layer_create();unsigned char *masks=calloc(CURVE_COUNT,LINE_MASK_BYTES);
    if(!layer||!masks){free(layer);free(masks);message("2D MEMORY","Not enough memory for the plot layers.");return;}
    int ready=0,render=0;
    for(;;) {
        PlaneView view=plane_view();
        if(!(view.xmin<view.xmax && view.ymin<view.ymax)){
            /* Do not strand the user in an invalid saved view: provide the
               reset and zoom-out keys before attempting another calculation. */
            dclear(C_WHITE);header("2D ZOOM LIMIT","FLOAT PRECISION");
            dtext(10,60,WARNING,"The selected center and scale exceed float precision.");
            dtext(10,82,INK,"Press - to zoom out, or F3 RESET.");
            dtext(10,104,MUTED,"VARS sets the center; EXIT returns to equations.");
            footer("EQS",NULL,"RESET",NULL,NULL,NULL);dupdate();key_event_t e=input_key();
            if(e.key==KEY_EXIT||e.key==KEY_F1){free(masks);free(layer);return;}
            if(e.key==KEY_SUB)app.plane_scale=MINF(1000,app.plane_scale*1.5f);
            else if(e.key==KEY_F3){app.plane_x=app.plane_y=0;app.plane_scale=.05f;}
            else if(e.key==KEY_VARS)point_editor("2D VIEW CENTER",&app.plane_x,&app.plane_y,NULL);
            ready=0;render=0;continue;
        }
        if(render){
            calculation_begin();dclear(C_WHITE);memset(masks,0,CURVE_COUNT*LINE_MASK_BYTES);
            EvalVars vars=base_vars();Progress pg={0};int failed=0;
            for(int i=0;i<CURVE_COUNT;i++)if(app.curve[i].active) {
                Relation relation=(Relation)programs[i].relation;
                LineContext line={&view,masks+(size_t)i*LINE_MASK_BYTES,relation==REL_LT||relation==REL_GT,0};
                FillContext fill={fill_color(i),relation};PlotStats stats;int rc;
                if(explicit_valid[i]){
                    fill.relation=explicit_relations[i];
                    rc=plot_explicit_y(&explicit_programs[i],explicit_relations[i],&vars,&view,PLOT_W,PLOT_H,
                                       draw_segment,&line,fill_span,&fill,progress,&pg,&stats);
                }else{
                    ContourOptions options={accuracy_values[app.accuracy2],PLOT_W,PLOT_H,fill_cell,&fill};
                    rc=plot_contour_opt(&programs[i],&vars,&view,draw_segment,&line,&options,progress,&pg,&stats);
                }
                if(rc<0){failed=1;break;}if(!rc)break;
            }
            calculation_end();
            if(pg.menu){free(masks);free(layer);gint_osmenu();return;}
            if(pg.cancel){free(masks);free(layer);return;}
            if(failed){free(masks);free(layer);message("PLOT ERROR","Not enough memory or invalid view.");return;}
            layer_capture(layer);ready=1;render=0;
        }
        present2d(&view,layer,masks,ready);key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F1){free(masks);free(layer);return;}
        float move=app.plane_scale*(e.shift?15:45);int changed=0;
        if(e.key==KEY_LEFT){app.plane_x-=move;changed=1;}
        else if(e.key==KEY_RIGHT){app.plane_x+=move;changed=1;}
        else if(e.key==KEY_UP){app.plane_y+=move;changed=1;}
        else if(e.key==KEY_DOWN){app.plane_y-=move;changed=1;}
        else if(e.key==KEY_F2 && e.shift)app.curve_width=(uint8_t)(app.curve_width%5+1);
        else if(e.key==KEY_F2)app.grid^=1;
        else if(e.key==KEY_F3){app.plane_x=app.plane_y=0;app.plane_scale=.05f;changed=1;}
        else if(e.key==KEY_F4){app.detail2=(app.detail2+1)%3;changed=has_implicit_2d();}
        else if(e.key==KEY_F5)app.axes_front^=1;
        else if(e.key==KEY_ADD){app.plane_scale=MAXF(.00001f,app.plane_scale/1.5f);changed=1;}
        else if(e.key==KEY_SUB){app.plane_scale=MINF(1000,app.plane_scale*1.5f);changed=1;}
        else if(e.key==KEY_VARS){
            if(ready)probe2d(&view);else changed=point_editor("2D VIEW CENTER",&app.plane_x,&app.plane_y,NULL);
        }
        else if(e.key==KEY_OPTN){
            settings_before=app;settings();
            changed=changed_common(&app,&settings_before)||app.period_x!=settings_before.period_x||
                (has_implicit_2d() && app.accuracy2!=settings_before.accuracy2);
        }
        else if(e.key==KEY_F6||e.key==KEY_EXE){if(!ready)render=1;}
        app.plane_x=MAXF(-1e6f,MINF(1e6f,app.plane_x));app.plane_y=MAXF(-1e6f,MINF(1e6f,app.plane_y));
        if(changed && ready)render=1;
    }
}
static Camera camera3d(void)
{
    return (Camera){.yaw=app.camera_yaw,.pitch=app.camera_pitch,.zoom=app.camera_zoom,.wire=app.wire,.axes=app.axes,
        .center_x=0,.center_y=0,.center_z=0,.radius=app.volume_radius,
        .eye_x=app.eye_x,.eye_y=app.eye_y,.eye_z=app.eye_z,.perspective=1};
}
static void probe3d(void)
{
    size_t bytes=(size_t)DWIDTH*DHEIGHT*sizeof(uint16_t);uint16_t *backup=malloc(bytes);
    if(!backup){message("3D VALUES","Not enough memory for values.");return;}
    memcpy(backup,gint_vram,bytes);
    float xyz[3]={0,0,0};int axis=0,slot=0;
    while(slot<SURFACE_COUNT && !app.surface[slot].active)slot++;
    if(slot>=SURFACE_COUNT){free(backup);return;}
    for(;;){
        dclear(C_WHITE);header("3D VALUES","ARROWS: SELECT / CHANGE");
        for(int i=0;i<3;i++){
            if(i==axis)drect(8,28+i*24,DWIDTH-9,48+i*24,PALE);
            dprint(13,32+i*24,INK,"%c = %.7g",'x'+i,(double)xyz[i]);
        }
        EvalVars vars=base_vars();vars.v[V_X]=xyz[0];vars.v[V_Y]=xyz[1];vars.v[V_Z]=xyz[2];
        float value=expr_eval_real(&programs[slot],&vars);
        if(!programs[slot].has_equal&&!expr_program_uses(&programs[slot],V_Z))value=xyz[2]-value;
        if(isfinite(value) && programs[slot].relation>=REL_LT)
            dprint(13,111,ACCENT,"F%d = %.7g   %s",slot+1,(double)value,
                   expr_relation_true((Relation)programs[slot].relation,value)?"TRUE":"FALSE");
        else if(isfinite(value))dprint(13,111,ACCENT,"F%d = %.7g   (zero on surface)",slot+1,(double)value);
        else dprint(13,111,WARNING,"F%d undefined / sum not converged",slot+1);
        dprint(13,134,MUTED,"Eye: %.4g, %.4g, %.4g",(double)app.eye_x,(double)app.eye_y,(double)app.eye_z);
        dprint(13,152,MUTED,"Clip cube half-size: %.4g",(double)app.volume_radius);
        dtext(13,172,MUTED,"EXE: exact value   +/-: adjust selected axis");
        footer("NEXT",NULL,NULL,NULL,NULL,"BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6||e.key==KEY_VARS)break;
        if(e.key==KEY_LEFT)axis=(axis+2)%3;else if(e.key==KEY_RIGHT)axis=(axis+1)%3;
        else if(e.key==KEY_UP||e.key==KEY_ADD)xyz[axis]+=app.volume_radius*(e.shift?.01f:.1f);
        else if(e.key==KEY_DOWN||e.key==KEY_SUB)xyz[axis]-=app.volume_radius*(e.shift?.01f:.1f);
        else if(e.key==KEY_F1){for(int j=0;j<SURFACE_COUNT;j++){slot=(slot+1)%SURFACE_COUNT;if(app.surface[slot].active)break;}}
        else if(e.key==KEY_EXE){
            char temp[EXPR_MAX],error[90];snprintf(temp,sizeof temp,"%.7g",(double)xyz[axis]);
            if(editor(temp,"PROBE COORDINATE",0)&&compile_formula(temp,&scratch_program,error,sizeof error)){
                EvalVars zero={0};float value2=expr_eval_real(&scratch_program,&zero);
                if(isfinite(value2)&&fabsf(value2)<=1e6f)xyz[axis]=value2;
            }
        }
    }
    memcpy(gint_vram,backup,bytes);free(backup);
}
static int voxel_needed(void)
{
    if(!app.render3_mode)return 1;
    for(int i=0;i<SURFACE_COUNT;i++)if(app.surface[i].active && programs[i].relation>=REL_LT)return 1;
    return 0;
}
static void present3d(Camera const *cam,uint16_t const *layer,int ready,int slow,int fast,int empty)
{
    char const *mode=slow?"ARROWS: TURN  |  SLOW":fast?"ARROWS: TURN  |  FAST":"ARROWS: TURN";
    char quality[12];
    if(voxel_needed())snprintf(quality,sizeof quality,"V%d%s",app.voxel_div,app.render3_mode?"*":"");
    else snprintf(quality,sizeof quality,"Q%d",ray_divs[app.detail3]);
    dclear(C_WHITE);header("3D GRAPH",mode);
    if(app.axes && !app.axes_front)render3_axes(cam);
    if(ready)layer_blit(layer);
    if(app.axes && app.axes_front)render3_axes(cam);
    drect(7,20,390,33,C_WHITE);
    dprint(9,22,MUTED,"Eye %.3g, %.3g, %.3g  |  clip %.3g  |  %s",
        (double)app.eye_x,(double)app.eye_y,(double)app.eye_z,(double)app.volume_radius,quality);
    if(ready && empty){drect(7,174,310,195,C_WHITE);dtext(9,178,WARNING,"No visible voxels / surface in clip cube.");}
    if(!ready){
        drect(7,173,390,198,C_WHITE);
        dtext(9,175,ACCENT,"8/5: front/back  4/6: side  1/0: up/down");
        dtext(9,187,MUTED,"ALPHA slow  SHIFT fast  |  PLOT/EXE draws");
    }
    footer("EQS","RESET",app.axes?"AX ON":"AX OFF",quality,app.axes_front?"FRONT":"BACK","PLOT");dupdate();
}
static void plot3d(void)
{
    if(!compile_active(1))return;
    uint16_t *layer=NULL; /* allocated after the renderer frees its depth buffer */
    int ready=0,render=0,empty=0,slow_lock=0,alpha_chord=0;
    int alpha_down=keydown(KEY_ALPHA);
    for(;;){
        Camera cam=camera3d();
        if(render){
            free(layer);layer=NULL;calculation_begin();dclear(C_WHITE);
            int use_voxels=voxel_needed();
            Volume vol={.x=0,.y=0,.z=0,.radius=app.volume_radius,
                .div=use_voxels?app.voxel_div:ray_divs[app.detail3],.period_x=app.period_x};
            RaySurface surfaces[SURFACE_COUNT];int n=0;
            for(int i=0;i<SURFACE_COUNT;i++)if(app.surface[i].active)
                surfaces[n++]=(RaySurface){&programs[i],i};
            EvalVars vars=base_vars();Progress pg={0};int rc;
            if(use_voxels){
                VoxelStats stats;rc=voxel3d_render(surfaces,n,&vars,&vol,&cam,progress,&pg,&stats);
                empty=stats.pixels==0;
            }else{
                RayStats stats;rc=ray3d_render(surfaces,n,&vars,&vol,&cam,progress,&pg,&stats);
                empty=stats.hits==0;
            }
            calculation_end();
            if(pg.menu){gint_osmenu();return;}if(pg.cancel)return;
            if(rc<0){message("3D ERROR","Not enough memory or invalid view.");return;}
            layer=layer_create();if(!layer){message("3D MEMORY","Not enough memory for the plot layer.");return;}
            layer_capture(layer);ready=1;render=0;
        }
#ifdef GL_HOST
        int hud_slow=slow_lock||alpha_down,hud_fast=0;
#else
        int hud_slow=slow_lock||keydown(KEY_ALPHA),hud_fast=keydown(KEY_SHIFT);
#endif
        present3d(&cam,layer,ready,hud_slow,hud_fast,empty);
        key_event_t e=navigation_wait();
        if(e.key==KEY_ALPHA && e.type==KEYEV_DOWN){alpha_down=1;alpha_chord=navigation_held(e)!=0;continue;}
        if(e.key==KEY_ALPHA && e.type==KEYEV_UP){
            if(alpha_down && !alpha_chord)slow_lock=!slow_lock;
            alpha_down=0;alpha_chord=0;continue;
        }
#ifdef GL_HOST
        int alpha_held=alpha_down||e.held_alpha;
        int shift_held=e.shift||e.held_shift;
#else
        int alpha_held=keydown(KEY_ALPHA);
        int shift_held=keydown(KEY_SHIFT);
#endif
        uint32_t held=navigation_held(e);
        if(alpha_held && e.type==KEYEV_DOWN && e.key!=KEY_ALPHA)alpha_chord=1;
        if((e.type==KEYEV_DOWN && navigation_key(e.key)) || e.type==KEYEV_NONE || e.type==KEYEV_HOLD){
            if(held && navigation_step(&app,held,slow_lock||alpha_held,shift_held))ready=0;
            if(held && alpha_held)alpha_chord=1;
            continue;
        }
        if(e.type!=KEYEV_DOWN)continue;
        if(e.key==KEY_EXIT||e.key==KEY_F1){free(layer);return;}
        int changed=0;
        if(e.key==KEY_MENU){gint_osmenu();}
        else if(e.key==KEY_F2){app.camera_yaw=.75f;app.camera_pitch=.55f;app.camera_zoom=1;state_default_eye(&app);changed=1;}
        else if(e.key==KEY_F3)app.axes^=1;
        else if(e.key==KEY_F4){
            if(voxel_needed()){
                int n=app.voxel_div;
                app.voxel_div=(uint8_t)(n<12?12:n<20?20:n<32?32:n<48?48:n<64?64:6);
            }else app.detail3=(app.detail3+1)%4;
            changed=1;
        }
        else if(e.key==KEY_F5)app.axes_front^=1;
        else if(e.key==KEY_OPTN){
            settings_before=app;settings();
            changed=changed_common(&app,&settings_before)||app.period_x!=settings_before.period_x||
                app.volume_x!=settings_before.volume_x||app.volume_y!=settings_before.volume_y||
                app.volume_z!=settings_before.volume_z||app.volume_radius!=settings_before.volume_radius||
                app.render3_mode!=settings_before.render3_mode||app.voxel_div!=settings_before.voxel_div;
        }
        else if(e.key==KEY_F6||e.key==KEY_EXE){if(!ready)render=1;}
        else if(e.key==KEY_VARS){
            if(ready)probe3d();else {
                changed=point_editor("3D OBSERVER POSITION",&app.eye_x,&app.eye_y,&app.eye_z);
            }
        }
        if(changed)ready=0; /* viewpoint change hides model until PLOT */
    }
}
static void fractal_block(int x,int y,int w,int h,int iteration,void *context)
{
    FractalView const *v=context;int col=C_BLACK;
    if(iteration<=v->iterations) {
        int phase=(iteration*3)%90,band=phase/30,k=phase%30;
        if(band==0)col=C_RGB(k/5,k/2,8+k*23/30);
        else if(band==1)col=C_RGB(k/2,15+k/2,31-k/2);
        else col=C_RGB(15+k/2,30-k/2,16-k/2);
    }
    drect(x,y,x+w-1,y+h-1,col);
}
static void fractal_header(void)
{
    char right[48];
    snprintf(right,sizeof right,"%s  |  DETAIL %d px",app.fractal.julia?"JULIA":"C=PIXEL",app.fractal.block);
    header("FRACTAL",right);
}
static void plot_fractal(void)
{
    char error[90];
    if(!compile_formula(app.recurrence[app.fractal_slot],&fractal_program,error,sizeof error)){message("INVALID FRACTAL",error);return;}
    if(fractal_program.has_equal || (fractal_program.variables & ~allowed_vars(3))){message("INVALID FRACTAL","Use x, y, z, c, n, i and a, b, t.");return;}
    int dirty=0,ready=0;
    for(;;){
        if(dirty){
            calculation_begin();dclear(C_WHITE);fractal_header();
            footer("SETUP",NULL,"RESET","DETAIL",NULL,"PLOT");
            Progress pg={0};EvalVars vars=base_vars();
            int rc=fractal_generate(&fractal_program,&vars,&app.fractal,fractal_block,&app.fractal,progress,&pg);
            calculation_end();
            if(pg.menu){gint_osmenu();return;}if(pg.cancel)return;
            if(rc<0){message("FRACTAL ERROR","Invalid view or formula.");return;}
            drect(6,16,DWIDTH-7,17,NAVY);dupdate();dirty=0;ready=1;
        }
        if(!ready){
            dclear(C_WHITE);header("FRACTAL VIEW","ARROWS PAN  |  +/- ZOOM");
            dprint(14,49,INK,"Center: %.6g, %.6g",(double)app.fractal.cx,(double)app.fractal.cy);
            dprint(14,70,INK,"Scale: %.6g / pixel",(double)app.fractal.units_per_pixel);
            dprint(14,91,INK,"Detail: %d px blocks  (1 = finest)",app.fractal.block);
            dtext(14,125,ACCENT,"Choose view; PLOT or EXE draws.");
            dtext(14,145,MUTED,"VARS: enter exact center coordinates.");
            footer("SETUP",NULL,"RESET","DETAIL",NULL,"PLOT");dupdate();
        }
        key_event_t e=input_key();if(e.key==KEY_EXIT||e.key==KEY_F1)return;
        float distance=app.fractal.units_per_pixel*(e.shift?15:45);int changed=1;
        if(e.key==KEY_LEFT)app.fractal.cx-=distance;
        else if(e.key==KEY_RIGHT)app.fractal.cx+=distance;
        else if(e.key==KEY_UP)app.fractal.cy+=distance;
        else if(e.key==KEY_DOWN)app.fractal.cy-=distance;
        else if(e.key==KEY_F3){app.fractal.cx=-.5f;app.fractal.cy=0;app.fractal.units_per_pixel=.012f;}
        else if(e.key==KEY_F4){int b=app.fractal.block;app.fractal.block=b>=6?4:b>=4?3:b>=3?2:b>=2?1:6;}
        else if(e.key==KEY_SUB)app.fractal.units_per_pixel=MINF(1000,app.fractal.units_per_pixel*1.5f);
        else if(e.key==KEY_ADD)app.fractal.units_per_pixel=MAXF(.000001f,app.fractal.units_per_pixel/1.5f);
        else if(e.key==KEY_VARS && !ready)changed=point_editor("FRACTAL CENTER",&app.fractal.cx,&app.fractal.cy,NULL);
        else if(e.key==KEY_OPTN){
            uint16_t *saved=ready?layer_create():NULL;
            if(ready && !saved){message("FRACTAL MEMORY","Cannot open settings without restoring the image.");dirty=1;changed=0;}
            else {
                if(saved)layer_capture(saved);
                settings_before=app;settings();
                changed=changed_common(&app,&settings_before)||
                    app.fractal.julia_re!=settings_before.fractal.julia_re||
                    app.fractal.julia_im!=settings_before.fractal.julia_im||
                    app.fractal.escape_radius!=settings_before.fractal.escape_radius;
                if(saved && !changed){
                    dclear(C_WHITE);fractal_header();
                    footer("SETUP",NULL,"RESET","DETAIL",NULL,"PLOT");layer_blit(saved);dupdate();
                }
                free(saved);
            }
        }
        else if(e.key==KEY_F6||e.key==KEY_EXE){if(!ready)dirty=1;changed=0;}
        else changed=0;
        app.fractal.cx=MAXF(-1e6f,MINF(1e6f,app.fractal.cx));app.fractal.cy=MAXF(-1e6f,MINF(1e6f,app.fractal.cy));
        if(changed && ready)dirty=1;
    }
}
static void fractal_menu(void)
{
    for(;;){
        dclear(C_WHITE);header("CUSTOM FRACTAL","z NEXT = EXPRESSION");
        dprint(9,25,INK,"Recurrence %d/%d  (LEFT/RIGHT: slot)",app.fractal_slot+1,FRACTAL_COUNT);
        formula_area(app.recurrence[app.fractal_slot],-1,NULL);
        dprint(10,151,INK,"Mode: %s",app.fractal.julia?"Julia (z0=pixel)":"c=pixel, z0=0");
        dprint(10,168,INK,"Iterations: %d    Block: %d",app.fractal.iterations,app.fractal.block);
        if(notice[0])draw_notice();else dtext(10,188,MUTED,"OPTN: Julia c / escape / parameters");
        footer("EDIT","MODE","ITER","DETAIL","SAVE","PLOT");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT)return;
        if(e.key==KEY_LEFT){app.fractal_slot=(app.fractal_slot+FRACTAL_COUNT-1)%FRACTAL_COUNT;clear_notice();}
        else if(e.key==KEY_RIGHT){app.fractal_slot=(app.fractal_slot+1)%FRACTAL_COUNT;clear_notice();}
        else if(e.key==KEY_F1||e.key==KEY_EXE){if(editor(app.recurrence[app.fractal_slot],"FRACTAL EXPRESSION",3))clear_notice();}
        else if(e.key==KEY_F2){app.fractal.julia^=1;clear_notice();}
        else if(e.key==KEY_F3){int n=app.fractal.iterations;app.fractal.iterations=n<40?40:n<80?80:n<160?160:20;clear_notice();}
        else if(e.key==KEY_F4){int b=app.fractal.block;app.fractal.block=b>=6?4:b>=4?3:b>=3?2:b>=2?1:6;clear_notice();}
        else if(e.key==KEY_F5)save_now();
        else if(e.key==KEY_F6)plot_fractal();
        else if(e.key==KEY_OPTN)settings();
    }
}
static void calculator(void)
{
    static char expression[EXPR_MAX]="1/2+sqrt(9)";
    for(;;){
        dclear(C_WHITE);header("QUICK CALCULATOR","REAL NUMBERS / RADIANS");
        dtext(10,24,MUTED,"F1 or EXE: edit expression");formula_area(expression,-1,NULL);
        char error[90];EvalVars vars=base_vars();
        if(compile_formula(expression,&scratch_program,error,sizeof error)){
            float value=expr_eval_real(&scratch_program,&vars);
            if(isfinite(value))dprint(10,157,ACCENT,"Result: %.8g",(double)value);
            else dtext(10,157,WARNING,"Undefined / overflow / series not converged");
        } else dtext(10,157,WARNING,error);
        dtext(10,177,MUTED,"a, b, c, t are set with F2 PARAM.");
        footer("EDIT","PARAM",NULL,NULL,NULL,"BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6)return;
        if(e.key==KEY_F1||e.key==KEY_EXE)editor(expression,"CALCULATE",4);
        else if(e.key==KEY_F2||e.key==KEY_OPTN)settings();
    }
}
static void equations(int is3)
{
    Equation *eq=is3?app.surface:app.curve;int count=is3?SURFACE_COUNT:CURVE_COUNT,sel=0;
    for(;;){
        dclear(C_WHITE);header(is3?"3D EQUATIONS":"2D EQUATIONS","EXE: EDIT");
        for(int i=0;i<count;i++){
            int y=24+i*38;if(i==sel)drect(5,y,DWIDTH-6,y+34,PALE);
            drect(10,y+7,17,y+14,eq[i].active?(is3?surface_palette[i]:palette[i]):C_LIGHT);
            dprint(23,y+5,INK,"%d %s",i+1,eq[i].active?"ON":"OFF");
            struct dwindow old=dwindow_set((struct dwindow){72,y+3,DWIDTH-9,y+31});
            dtext(73,y+7,INK,eq[i].formula);dwindow_set(old);
        }
        dtext(10,145,MUTED,"EXE: edit   Letter/digit/DEL: new formula");
        dtext(10,163,MUTED,"F3 / OPTN: settings, clipping cube, period.");draw_notice();
        footer("EDIT","ON/OFF","PARAM","EXAMPL","SAVE","PLOT");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT)return;
        if(e.key==KEY_UP){sel=(sel+count-1)%count;clear_notice();}
        else if(e.key==KEY_DOWN){sel=(sel+1)%count;clear_notice();}
        else if(e.key==KEY_F1||e.key==KEY_EXE){if(editor(eq[sel].formula,is3?"EDIT 3D EQUATION":"EDIT 2D EQUATION",is3?2:1)){eq[sel].active=1;clear_notice();}}
        else if(e.key==KEY_F2){eq[sel].active^=1;clear_notice();}
        else if(e.key==KEY_F3||e.key==KEY_OPTN)settings();
        else if(e.key==KEY_F4){examples(&eq[sel],is3);clear_notice();}
        else if(e.key==KEY_F5)save_now();
        else if(e.key==KEY_F6){if(is3)plot3d();else plot2d();}
        else if(begins_new_formula(e)) {
            char fresh[EXPR_MAX]="";
            if(editor_seed(fresh,is3?"NEW 3D EQUATION":"NEW 2D EQUATION",is3?2:1,e,1)) {
                strcpy(eq[sel].formula,fresh);eq[sel].active=1;clear_notice();
            }
        }
    }
}
int main(void)
{
    gint_setrestart(1);alpha_mode=0;alpha_upper=0;settings_active=0;notice[0]=0;
#ifndef GL_HOST
    /* fxlibc excludes floating-point printf formatters until enabled. */
    __printf_enable_fp();
#endif
    state_defaults(&app);
    int loaded=storage_load(&app);
    if(loaded<0)snprintf(notice,sizeof notice,"Save unreadable: defaults loaded; files kept.");
    /* Normalize old 1.02/1.03 views without changing the journal until SAVE.
       If its box used to move or was offset, put the eye back on the origin. */
    int old_box=app.follow_eye || app.volume_x!=0 || app.volume_y!=0 || app.volume_z!=0;
    app.follow_eye=0;app.volume_x=app.volume_y=app.volume_z=0;app.camera_zoom=1;
    if(old_box){
        state_default_eye(&app);
        if(loaded>0)snprintf(notice,sizeof notice,"3D cube reset to origin; F2 resets the view.");
    }
    for(;;){
        dclear(PALE);header("GRAPH LAB","CASIO fx-CG50");
        centered(28,INK,"2D  /  3D  /  CUSTOM FRACTALS");
        drect_border(8,48,DWIDTH-9,176,C_WHITE,1,BORDER);
        dtext(18,58,ACCENT,"F1   2D equations and functions");
        dtext(18,78,ACCENT,"F2   3D implicit surfaces");
        dtext(18,98,ACCENT,"F3   Custom complex fractals");
        dtext(18,118,ACCENT,"F4   Parameters and 3D range");
        dtext(18,138,ACCENT,"OPTN Settings   |   VARS Calculator");
        draw_notice();footer("2D","3D","FRACT","PARAM","SAVE","EXIT");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6)return 0;
        if(e.key==KEY_F1)equations(0);
        else if(e.key==KEY_F2)equations(1);
        else if(e.key==KEY_F3)fractal_menu();
        else if(e.key==KEY_F4)settings();
        else if(e.key==KEY_F5)save_now();
        else if(e.key==KEY_OPTN)settings();
        else if(e.key==KEY_VARS)calculator();
    }
}
