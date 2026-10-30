#include "platform.h"
#include "expr.h"
#include "layout.h"
#include "editor_meta.h"
#include "plot.h"
#include "plot2d.h"
#include "ray.h"
#include "intersection.h"
#include "fractal.h"
#include "scroll.h"
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

/* UI is deliberately two-color; plot/surface/fractal palettes remain colored. */
#define NAVY C_BLACK
#define PALE C_WHITE
#define BORDER C_BLACK
#define INK C_BLACK
#define MUTED C_BLACK
#define ACCENT C_BLACK
#define WARNING C_BLACK
#define MAXF(a,b) ((a)>(b)?(a):(b))
#define MINF(a,b) ((a)<(b)?(a):(b))

static GraphState app;
/* A settings dialog cannot change formula text. Store only the fields used
   for invalidation instead of keeping a second 6 KiB GraphState in RAM. */
typedef struct {
    float param_a,param_b,param_c,param_t,period_x;
    float volume_x,volume_y,volume_z,volume_radius;
    float julia_re,julia_im,escape_radius;
    uint8_t accuracy2,preview_quality,fast_ray_step,fast_ray_q;
} SettingsSnapshot;
static SettingsSnapshot settings_before;
static void snapshot_settings(SettingsSnapshot *dst,GraphState const *s)
{
    *dst=(SettingsSnapshot){
        .param_a=s->param_a,.param_b=s->param_b,.param_c=s->param_c,.param_t=s->param_t,
        .period_x=s->period_x,.volume_x=s->volume_x,.volume_y=s->volume_y,
        .volume_z=s->volume_z,.volume_radius=s->volume_radius,
        .julia_re=s->fractal.julia_re,.julia_im=s->fractal.julia_im,
        .escape_radius=s->fractal.escape_radius,.accuracy2=s->accuracy2,
        .preview_quality=s->preview_quality,.fast_ray_step=s->fast_ray_step,.fast_ray_q=s->fast_ray_q
    };
}
static Parsed parsed;
static Program programs[CURVE_COUNT], explicit_programs[CURVE_COUNT], scratch_program, fractal_program;
static Program ray_zero_programs[SURFACE_COUNT];
static uint8_t ray_zero_valid[SURFACE_COUNT];
static Relation explicit_relations[CURVE_COUNT];
static uint8_t explicit_valid[CURVE_COUNT];
static char notice[100];
static const int palette[3]={C_RGB(28,6,5),C_RGB(2,10,26),C_RGB(3,21,9)};
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
    if(right){int w,h;dsize(right,NULL,&w,&h);(void)h;dtext(DWIDTH-w-7,3,C_WHITE,right);}
}
static void footer(char const *a,char const *b,char const *c,char const *d,char const *e,char const *f)
{
    char const *names[]={a,b,c,d,e,f};
    drect(0,203,DWIDTH-1,DHEIGHT-1,NAVY);
    for(int i=0;i<6;i++) {
        int x=2+i*65;
        drect_border(x,206,x+62,220,C_BLACK,1,C_WHITE);
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
static int editor_group_numerator(char *text,EditorMeta *meta,int *target,int fraction_node);
static void editor_erase_range(char *text,EditorMeta *meta,int start,int end)
{
    int len=(int)strlen(text);
    editor_meta_delete(meta,len,start,end);
    memmove(text+start,text+end,(size_t)(len-end+1));
}
static void delete_text(char *text,int *cursor,EditorMeta *meta)
{
    int len=(int)strlen(text);if(*cursor<=0)return;
    int end=*cursor,start=end-1;
    if(expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) && layout_measure(&parsed)){
        /* A hidden numerator opener can share the same visual position with
           a surrounding operator. Retry at its left boundary without walking
           through an invisible source character or losing an operand. */
retry_visible:
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
        /* At the beginning of a SUM/PROD term DEL must not become stuck on
           the invisible comma. An empty term removes the whole template. For
           a nonempty term, remove the operator and limits but keep its term
           in the original parentheses, preserving all term content/grouping. */
        int aggregate=-1,part=layout_sum_field(&parsed,end,&aggregate);
        if(part==3 && aggregate>=0){
            ExprNode const *n=&parsed.nodes[aggregate],*body=&parsed.nodes[n->extra];
            if(end==body->start && start==end){
                if(body->kind==N_HOLE){start=n->start;end=n->end;}
                else {
                    int open=n->op_pos>=0?n->op_pos:-n->op_pos-2;
                    if(open>=n->start && open<body->start){
                        int at=n->start;
                        editor_erase_range(text,meta,open+1,body->start);
                        editor_erase_range(text,meta,at,open);
                        *cursor=at+1;return;
                    }
                }
            }
        }
        /* Protect function template delimiters too. Backspace from an empty
           unary function removes the function, not just its opening '('. */
        for(int i=0;i<parsed.count;i++){
            ExprNode const *n=&parsed.nodes[i];
            if(n->kind!=N_FUNC || n->op_pos<0 || n->left<0)continue;
            ExprNode const *a=&parsed.nodes[n->left];
            if(end==a->start && start==n->op_pos){
                int other_empty=n->right<0 || parsed.nodes[n->right].kind==N_HOLE;
                if(a->kind==N_HOLE && other_empty){start=n->start;end=n->end;}
                else if(n->right<0){
                    /* Remove a unary function name but keep its parentheses
                       and argument. Thus 3*sin(1+2) becomes 3*(1+2), not
                       3*1+2; an existing argument is never silently lost. */
                    int at=n->start,open=n->op_pos;
                    editor_erase_range(text,meta,at,open);
                    *cursor=end-(open-at);return;
                }else{
                    /* Two arguments cannot be unwrapped into a valid scalar
                       without inventing an operator. Move to the last
                       nonempty argument; repeated DEL can then remove both. */
                    ExprNode const *b=&parsed.nodes[n->right];
                    *cursor=b->kind==N_HOLE?a->end:b->end;return;
                }
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
                    else {
                        /* Backspace at the beginning of a numerator must
                           reach the preceding visible item, not stop forever
                           on the invisible grouping parenthesis. */
                        end=n->start;
                        if(end<=0){*cursor=0;return;}
                        goto retry_visible;
                    }
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
            }else if(n->kind==N_DIV && n->left>=0){
                /* A nonempty denominator is not deleted at its left edge.
                   Move to the numerator (like UP) instead of getting stuck. */
                ExprNode const *a=&parsed.nodes[n->left];
                int target=(a->kind==N_GROUP && a->left>=0)?parsed.nodes[a->left].end:a->end;
                if(editor_group_numerator(text,meta,&target,i))*cursor=target;
                return;
            }else return;
            break;
        }
    }
    if(start<0 || start>=end || end>len)return;
    editor_erase_range(text,meta,start,end);*cursor=start;
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
        if(cursor<0 || c.found){
            layout_draw(&parsed,cursor,ox,oy,INK,ACCENT,0);
            dwindow_set(old);
            return;
        }
    }
    {
        int ox=14,w=0,h=0;
        if(cursor>=0){
            char prefix[EXPR_MAX];int n=cursor,len=(int)strlen(text);
            if(n>len)n=len;
            if(n>=EXPR_MAX)n=EXPR_MAX-1;
            memcpy(prefix,text,(size_t)n);prefix[n]=0;dsize(prefix,NULL,&w,&h);
            /* Keep even unparsed, incomplete and over-complex source visible
               while the user moves or types; the window clips both sides. */
            if(ox+w>DWIDTH-22)ox=DWIDTH-22-w;
            if(ox+w<12)ox=12-w;
        }
        dtext(ox,66,INK,text);
        if(cursor>=0)dline(ox+w,66,ox+w,66+h-1,ACCENT);
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
    /* OPTN can open a second editor. Keeping the 3 KiB ownership metadata
       and 768-byte text buffer on each 16 KiB SH stack would leave too little
       room for deep formula layout. Allocate one temporary session per editor;
       on failure the original destination remains untouched. */
    typedef struct { char text[EXPR_MAX],error[90]; EditorMeta meta; } EditorSession;
    EditorSession *session=malloc(sizeof *session);
    if(!session){snprintf(notice,sizeof notice,"Editor unavailable: not enough memory.");return 0;}
    char *text=session->text,*error=session->error;
    EditorMeta *meta=&session->meta;
    error[0]=0;snprintf(text,sizeof session->text,"%s",destination);
    editor_group_divisors(text);
    editor_meta_init(meta,text);
    int cursor=(int)strlen(text),page=0;
    for(;;) {
        dclear(PALE);header(title,alpha_mode==INPUT_LOWER_LOCK?"ALPHA LOCK ON":
            alpha_mode==INPUT_UPPER_LOCK?"UPPER LOCK ON":alpha_mode==INPUT_ALPHA_ONCE?"ALPHA NEXT":"123 INPUT");
        dtext(10,22,MUTED,"LEFT/RIGHT: cursor   UP/DOWN: fields");
        formula_area(text,cursor,meta);
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
            dtext(10,151,MUTED,settings_active?
                "F5: 123 / abc / ABC   |   EXIT: cancel":
                "F5: 123 / abc / ABC   |   OPTN: settings");
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
        if(k==KEY_EXIT){free(session);return 0;}
        if(k==KEY_UP || k==KEY_DOWN){
            if(expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) && layout_measure(&parsed)){
                int direction=k==KEY_DOWN?1:-1;
                int fraction=layout_fraction_field(&parsed,cursor,NULL);
                int sum=layout_sum_field(&parsed,cursor,NULL);
                int next;
                if(ev.shift && sum>=0)next=layout_sum_step(&parsed,cursor,direction);
                else if(fraction>=0)next=layout_fraction_step(&parsed,cursor,direction);
                else next=layout_sum_step(&parsed,cursor,direction);
                int node=-1,part=layout_fraction_field(&parsed,next,&node);
                if(next!=cursor && part==0 && !editor_group_numerator(text,meta,&next,node))
                    snprintf(error,sizeof session->error,"Cannot group numerator: limit reached.");
                else {cursor=next;error[0]=0;}
            }
            continue;
        }
        if(k==KEY_LEFT || k==KEY_RIGHT){
            if(expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) && layout_measure(&parsed)){
                int op=-1,field=layout_sum_field(&parsed,cursor,&op);
                int next;
                if(ev.shift && field>=0){
                    ExprNode const *n=&parsed.nodes[op];
                    next=k==KEY_LEFT?n->index_end:parsed.nodes[n->extra].end;
                }else next=layout_cursor_step(&parsed,cursor,k==KEY_LEFT?-1:1);
                int old_node=-1,new_node=-1;
                int old_part=layout_fraction_field(&parsed,cursor,&old_node);
                int new_part=layout_fraction_field(&parsed,next,&new_node);
                if(next>cursor && !editor_meta_can_cross(meta,text,cursor,next))
                    snprintf(error,sizeof session->error,"Close the inner group with ).");
                else if(next!=cursor && new_part==0 && (new_node!=old_node || old_part!=0) &&
                        !editor_group_numerator(text,meta,&next,new_node))
                    snprintf(error,sizeof session->error,"Cannot group numerator: limit reached.");
                else {cursor=next;error[0]=0;}
            }
            else if(k==KEY_LEFT && cursor>0)cursor--;
            else if(k==KEY_RIGHT && cursor<(int)strlen(text))cursor++;
            continue;
        }
        if(k==KEY_OPTN){if(!settings_active)settings();continue;}
        if(k==KEY_DEL){
            if(ev.shift){text[0]=0;cursor=0;editor_meta_init(meta,text);}
            else delete_text(text,&cursor,meta);
            error[0]=0;continue;
        }
        if(k==KEY_F5){
            if(ev.shift)page=(page+1)%9;
            else input_cycle_alpha(&alpha_mode);
            continue;
        }
        if(k==KEY_EXE || k==KEY_F6) {
            if(!compile_formula(text,&scratch_program,error,sizeof session->error))continue;
            if(scratch_program.variables & ~allowed_vars(mode)) {
                snprintf(error,sizeof session->error,"Unsupported free variable in this mode.");continue;
            }
            if((mode==0||mode==3||mode==4) && scratch_program.has_equal){
                snprintf(error,sizeof session->error,"Enter an expression without a comparison.");continue;
            }
            strcpy(destination,text);free(session);return 1;
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
            if(expr_parse_editor(&parsed,text,meta->mate,meta->comma_owner) && layout_measure(&parsed))
                field=layout_sum_field(&parsed,cursor,&node);
            if(node>=0 && field==0){
                ExprNode const *n=&parsed.nodes[node],*lo=&parsed.nodes[n->left],*hi=&parsed.nodes[n->right],*body=&parsed.nodes[n->extra];
                /* Typing summ() creates exactly one ')' at the end of the
                   empty template, then moves to its expression field. */
                if(n->op_pos<0 && cursor==lo->start && lo->kind==N_HOLE && hi->kind==N_HOLE &&
                   body->kind==N_HOLE && n->index_start==n->index_end){
                    int end=body->end,term=body->start;
                    if(!insert_text(text,&end,")",0,meta))snprintf(error,sizeof session->error,"Expression limit reached.");
                    else {cursor=term;error[0]=0;}
                    continue;
                }
            }
            if(editor_meta_skip(meta,text,cursor,')')){cursor++;error[0]=0;continue;}
        }else if(one[0]==',' && editor_meta_skip(meta,text,cursor,',')){
            cursor++;error[0]=0;continue;
        }
        if(ins) {if(!insert_text(text,&cursor,ins,back,meta))snprintf(error,sizeof session->error,"Expression limit reached.");else error[0]=0;}
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
char const *graphlab_test_notice(void){return notice;}
void graphlab_test_formula_area(char const *text,int cursor)
{
    EditorMeta meta;editor_meta_init(&meta,text);
    dclear(C_WHITE);formula_area(text,cursor,&meta);
}
#endif
/* One list event has exactly one owner. Explicit list actions (F-keys,
   PLOT, EXE, OPTN, UP/DOWN and EXIT) take priority in equations(). A first
   editor action is passed to editor_seed once; EXE/F6 are never forwarded. */
typedef enum {ENTRY_NONE,ENTRY_NEW,ENTRY_EDIT} EquationEntry;
static EquationEntry equation_entry(key_event_t e)
{
    int k=e.key;
    if(k==KEY_DEL || (e.alpha && input_alpha_letter(k)))return ENTRY_NEW;
    if((k>=KEY_1 && k<=KEY_3)||(k>=KEY_4 && k<=KEY_6)||
       (k>=KEY_7 && k<=KEY_9)||k==KEY_0)return ENTRY_NEW;
    if(k==KEY_XOT || k==KEY_SIN || k==KEY_COS || k==KEY_TAN ||
       k==KEY_LN || k==KEY_LOG)return ENTRY_NEW;
    /* These keys already have editor semantics. Preserve the existing
       expression so an operator/fraction/power does not silently replace it. */
    switch(k){
    case KEY_LEFT:case KEY_RIGHT:
    case KEY_DOT:case KEY_ADD:case KEY_SUB:case KEY_NEG:case KEY_MUL:
    case KEY_DIV:case KEY_FRAC:case KEY_LEFTP:case KEY_RIGHTP:case KEY_COMMA:
    case KEY_POWER:case KEY_SQUARE:case KEY_FD:case KEY_EXP:
        return ENTRY_EDIT;
    case KEY_UP:case KEY_DOWN:case KEY_F5:
        return e.shift?ENTRY_EDIT:ENTRY_NONE;
    default:return ENTRY_NONE;
    }
}

/* Numeric fields preserve their existing value for operators, but begin a
   fresh value for a digit, decimal point, negative sign, constant or function.
   Navigation and reserved interface keys are handled by each caller first. */
static EquationEntry numeric_entry(key_event_t e)
{
    if(e.key==KEY_NEG||e.key==KEY_DOT||e.key==KEY_FD||e.key==KEY_EXP ||
       (e.key==KEY_SQUARE && e.shift))return ENTRY_NEW;
    return equation_entry(e);
}
/* Accept EXE/F1 without forwarding the interface event; all seeded calls
   forward exactly one editor key. An aborted/invalid editor changes nothing. */
static int numeric_value(float old,char const *title,key_event_t e,EquationEntry entry,float *out)
{
    char text[EXPR_MAX],error[90];
    if(entry==ENTRY_NEW)text[0]=0;
    else snprintf(text,sizeof text,"%.7g",(double)old);
    if(!editor_seed(text,title,0,e,entry!=ENTRY_NONE) ||
       !compile_formula(text,&scratch_program,error,sizeof error))return 0;
    EvalVars zero={0};*out=expr_eval_real(&scratch_program,&zero);return 1;
}
#ifdef GL_EDITOR_TEST
int graphlab_test_numeric_entry_event(key_event_t e){return (int)numeric_entry(e);}
float graphlab_test_clip_radius(void){return app.volume_radius;}
int graphlab_test_entry_event(key_event_t e){return (int)equation_entry(e);}
int graphlab_test_seed_editor(char *destination,int mode,key_event_t e)
{return editor_seed(destination,"TEST SEEDED EDITOR",mode,e,1);}
#endif

static void examples(Equation *eq,int is3)
{
    static char const *const ex2[]={"y=sin(x)","x^2+y^2=9","y^2=x^3-x","x*y=1","y=1/x","sin(x*y)=0","(x^2+y^2-1)^3=x^2*y^3"};
    static char const *const ex3[]={"z+x^2=sqrt(y)","x^2+y^2+z^2=9","z=sin(x)*cos(y)","x^2+y^2=1","z^2=x^2+y^2","sin(x)+sin(y)+sin(z)=0"};
    char const *const *xs=is3?ex3:ex2;int count=is3?6:7,sel=0;
    for(;;){
        dclear(C_WHITE);header("EXAMPLES","REPLACES SELECTED SLOT");
        for(int i=0;i<count;i++){
            if(i==sel)drect_border(5,23+i*23,DWIDTH-6,43+i*23,C_WHITE,1,C_BLACK);
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
    /* OPTN cycles pages. All map sampling is derived from pixels, not set here. */
    enum {S_WIDTH,S_RADIUS,S_A,S_B,S_C,S_T,S_JULIA_RE,S_JULIA_IM,
          S_ESCAPE,S_PERIOD,S_ACCURACY,
          S_FAST_STEP,S_FAST_Q,S_POINT,S_MAP_DOWN,S_MAP_UP,S_COUNT};
    char const *names[]={"2D line width (px)","3D clip half-size",
        "Parameter a","Parameter b","Parameter c","Parameter t","Julia c: real","Julia c: imaginary",
        "Fractal escape radius","X period (0 = off)","2D accuracy (px)",
        "Ray start step (px)","Ray Q","Preview step (px)",
        "Extend Z below clip","Extend Z above clip"};
    float *values[]={NULL,&app.volume_radius,&app.param_a,&app.param_b,&app.param_c,&app.param_t,
        &app.fractal.julia_re,&app.fractal.julia_im,&app.fractal.escape_radius,&app.period_x,
        NULL,NULL,NULL,NULL,&app.map_extend_down,&app.map_extend_up};
    int page=0,sel=0,off=0;
    settings_active++;
    for(;;){
        int first=page==0?0:page==1?S_FAST_STEP:S_MAP_DOWN;
        int count=page==0?S_FAST_STEP:page==1?S_MAP_DOWN-S_FAST_STEP:S_COUNT-S_MAP_DOWN;
        if(sel<off)off=sel;
        if(sel>=off+7)off=sel-6;
        dclear(C_WHITE);
        if(page==2){
            header("SETTINGS: MAP 3/3","OPTN: NEXT PAGE");
            for(int i=0;i<2;i++){
                int y=22+i*23;if(i==sel)drect_border(6,y,DWIDTH-7,y+20,C_WHITE,1,C_BLACK);
                dtext(12,y+5,INK,names[S_MAP_DOWN+i]);
                dprint(218,y+5,ACCENT,"%.6g",(double)*values[S_MAP_DOWN+i]);
            }
            dprint(9,76,INK,"Z range: %.6g .. %.6g",(double)(-app.volume_radius-app.map_extend_down),
                   (double)(app.volume_radius+app.map_extend_up));
            dtext(9,99,INK,"XY/Z spacing: one map pixel (automatic).");
            dtext(9,119,INK,"F2 exact XY; arrows check whole pixel.");
            dtext(9,139,INK,"F3 next found; F4 nearest mapped.");
            if(notice[0])draw_notice();
            else dtext(9,174,MUTED,"Extra Z expands beyond the clip; 0 = off.");
        }else{
            header(page==0?"SETTINGS: GENERAL 1/3":"SETTINGS: 3D 2/3","OPTN: NEXT PAGE");
            for(int row=0;row<7 && row+off<count;row++){
                int i=first+row+off,y=22+row*23;
                if(row+off==sel)drect_border(6,y,DWIDTH-7,y+20,C_WHITE,1,C_BLACK);
                dtext(12,y+5,INK,names[i]);
                if(i==S_WIDTH)dprint(218,y+5,ACCENT,"%d px  LEFT/RIGHT",app.curve_width);
                else if(i==S_ACCURACY)dprint(218,y+5,ACCENT,"%.2g px",(double)accuracy_values[app.accuracy2]);
                else if(i==S_FAST_STEP)dprint(218,y+5,ACCENT,"%d px  (1..32)",app.fast_ray_step);
                else if(i==S_FAST_Q)dprint(218,y+5,ACCENT,"Q=%d  (4..48)",app.fast_ray_q);
                else if(i==S_POINT)dprint(218,y+5,ACCENT,"%d px  (1..32)",app.preview_ray_step);
                else dprint(218,y+5,ACCENT,"%.6g",(double)*values[i]);
            }
            int current=first+sel;
            if(notice[0])draw_notice();
            else if(current==S_RADIUS)dtext(9,188,MUTED,"World clip is fixed at origin: -R to +R.");
            else if(current==S_WIDTH)dtext(9,188,MUTED,"2D curve thickness in screen pixels.");
            else if(current>=S_A && current<=S_T)dtext(9,188,MUTED,"Values used by letters in your formulas.");
            else if(current==S_JULIA_RE || current==S_JULIA_IM)dtext(9,188,MUTED,"Julia mode only: fixed complex c.");
            else if(current==S_ESCAPE)dtext(9,188,MUTED,"Fractal: distance marking escape.");
            else if(current==S_PERIOD)dtext(9,188,MUTED,"0 = off; use only a true x period.");
            else if(current==S_ACCURACY)dtext(9,188,MUTED,"Smaller = finer 2D contour refinement.");
            else if(current==S_FAST_STEP)dtext(9,188,MUTED,"Initial step when no preview is ready.");
            else if(current==S_FAST_Q)dtext(9,188,MUTED,"One Q for preview and final refinement.");
            else if(current==S_POINT)dtext(9,188,MUTED,"Same renderer; smaller = finer and slower.");
        }
        footer("EDIT",NULL,NULL,"HELP","SAVE","BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6){settings_active--;return;}
        if(e.key==KEY_OPTN){page=(page+1)%3;sel=off=0;clear_notice();continue;}
        if(e.key==KEY_F5&&!e.shift){save_now();continue;}
        if(e.key==KEY_F4){help();continue;}
        if(e.key==KEY_UP){sel=(sel+count-1)%count;clear_notice();continue;}
        if(e.key==KEY_DOWN){sel=(sel+1)%count;clear_notice();continue;}
        int item=first+sel;
        if(item==S_WIDTH && (e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_EXE||e.key==KEY_F1)){
            int d=e.key==KEY_LEFT?-1:1;app.curve_width=(uint8_t)((app.curve_width-1+d+5)%5+1);clear_notice();
        }else if(item==S_ACCURACY && (e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_EXE||e.key==KEY_F1)){
            int d=e.key==KEY_LEFT?-1:1;app.accuracy2=(uint8_t)((app.accuracy2+d+4)%4);clear_notice();
        }else if((item==S_FAST_STEP||item==S_POINT)&&
                 (e.key==KEY_LEFT||e.key==KEY_RIGHT)){
            uint8_t *step=item==S_FAST_STEP?&app.fast_ray_step:&app.preview_ray_step;
            int next=(int)*step+(e.key==KEY_LEFT?-1:1);
            if(next>=1&&next<=32)*step=(uint8_t)next;
            clear_notice();
        }else if(item==S_FAST_Q && (e.key==KEY_LEFT||e.key==KEY_RIGHT)){
            int q=app.fast_ray_q+(e.key==KEY_LEFT?-1:1);
            if(q>=4&&q<=48)app.fast_ray_q=(uint8_t)q;
            clear_notice();
        }else if(numeric_entry(e)!=ENTRY_NONE ||
                ((e.key==KEY_EXE||e.key==KEY_F1) && (values[item]||item==S_FAST_Q||item==S_FAST_STEP||item==S_POINT))) {
            float old=values[item]?*values[item]:item==S_WIDTH?(float)app.curve_width:
                      item==S_ACCURACY?accuracy_values[app.accuracy2]:item==S_FAST_Q?(float)app.fast_ray_q:item==S_FAST_STEP?(float)app.fast_ray_step:
                      item==S_POINT?(float)app.preview_ray_step:0;
            float x;EquationEntry entry=numeric_entry(e);
            if(numeric_value(old,names[item],e,entry,&x)) {
                if(item==S_WIDTH){
                    if(!isfinite(x)||x<1||x>5||fabsf(x-roundf(x))>.00001f)
                        snprintf(notice,sizeof notice,"Width requires an integer from 1 to 5.");
                    else {app.curve_width=(uint8_t)lroundf(x);snprintf(notice,sizeof notice,"Width updated. F5 saves the settings.");}
                }else if(item==S_ACCURACY){
                    int index=-1;if(isfinite(x))for(int i=0;i<4;i++)if(fabsf(x-accuracy_values[i])<.00001f)index=i;
                    if(index<0)snprintf(notice,sizeof notice,"Choose 0.25, 0.5, 1 or 2 pixels.");
                    else {app.accuracy2=(uint8_t)index;snprintf(notice,sizeof notice,"Accuracy updated. F5 saves the settings.");}
                }else if(item==S_FAST_Q){
                    if(!isfinite(x)||x<4||x>48||fabsf(x-roundf(x))>.00001f)
                        snprintf(notice,sizeof notice,"Ray grid requires an integer Q from 4 to 48.");
                    else {app.fast_ray_q=(uint8_t)lroundf(x);
                        snprintf(notice,sizeof notice,"Ray grid updated. F5 saves the settings.");}
                }else if(item==S_FAST_STEP||item==S_POINT){
                    if(!isfinite(x)||x<1||x>32||fabsf(x-roundf(x))>.00001f)
                        snprintf(notice,sizeof notice,"Step requires an integer from 1 to 32.");
                    else {if(item==S_FAST_STEP)app.fast_ray_step=(uint8_t)lroundf(x);else app.preview_ray_step=(uint8_t)lroundf(x);
                        snprintf(notice,sizeof notice,"Step updated. F5 saves the settings.");}
                }else{
                    *values[item]=x;
                    if(!state_validate(&app)){*values[item]=old;snprintf(notice,sizeof notice,"Value is not finite or outside allowed range.");}
                    else snprintf(notice,sizeof notice,"Value updated. F5 saves the settings.");
                }
            }
        }
    }
}

static void help(void)
{
    dclear(C_WHITE);header("GRAPH LAB HELP","OFFLINE / fx-CG50");
    dtext(8,24,INK,"3D: auto when still. 2D: PLOT / EXE.");
    dtext(8,42,INK,"OPTN: settings. F5 saves in menus.");
    dtext(8,60,INK,"3D: arrows turn; 8/5 front/back; 4/6 side.");
    dtext(8,78,INK,"3D: 1/0 up/down; ALPHA slow; SHIFT fast.");
    dtext(8,96,INK,"3D: 2 map; SHIFT+VARS eye; F4 quality.");
    dtext(8,114,INK,"Editor F5: 123/abc/ABC; SHIFT+F5 symbols.");
    dtext(8,132,INK,"FRAC: UP/DOWN fields; RIGHT exits.");
    dtext(8,148,INK,"SUM: SHIFT+UP/DOWN bounds inside a fraction.");
    dtext(8,164,INK,"SUM: SHIFT+LEFT index, SHIFT+RIGHT term.");
    dtext(8,180,MUTED,"2D: +/- zoom, SHIFT+F2 line width.");
    footer(NULL,NULL,NULL,NULL,NULL,"BACK");dupdate();
    for(;;){key_event_t e=input_key();if(e.key==KEY_EXIT||e.key==KEY_EXE||e.key==KEY_F6)break;}
}

static void present3d(Camera const *cam,uint16_t const *layer,int ready,int slow,int fast,
                      int empty,int previewing,int drawn_step,int drawn_progress);
typedef struct {
    int cancel,menu,next,frame_done,frame_total,motion,command;
    int track_shift,track_alpha,motion_shift,motion_alpha,command_shift,command_alpha;
    key_event_t motion_event,command_event;
    /* Optional, time-throttled intermediate display during a final ray pass. */
    Camera const *live_cam;uint16_t const *live_layer;
    int live_slow,live_started,live_row;uint16_t live_tick;
} Progress;
/* Standard retains the previous complete screen while calculating. Dev
   deliberately displays a throttled sequence of intermediate frames. */
static int progress(int done,int total,void *context)
{
    (void)done;(void)total;Progress *p=context;key_event_t ev;
#ifdef GL_DEV
    /* About ten frames per pass, plus a bounded fallback for long work at
       the same progress value. Fast explicit 2D still has visible steps. */
    if(total!=p->frame_total||done<p->frame_done){p->frame_total=total;p->frame_done=0;}
    int stride=total/10;if(stride<1)stride=1;
    if((done>0&&done<total&&done-p->frame_done>=stride)||++p->next>=256){
        p->frame_done=done;p->next=0;dupdate();
    }
#endif
    while((ev=pollevent()).type!=KEYEV_NONE){
        if(ev.type==KEYEV_DOWN && ev.key==KEY_EXIT){p->cancel=1;return 0;}
        if(ev.type==KEYEV_DOWN && ev.key==KEY_MENU){p->menu=1;return 0;}
    }
    return 1;
}
/* Raw pollevent() events have no enriched modifiers. Track queued SHIFT /
   ALPHA transitions so a quick movement is not lost or slowed incorrectly if
   the keys were already released by the time the callback handles it. */
static void progress_modifiers(Progress *p,key_event_t ev)
{
    if(ev.key==KEY_SHIFT){if(ev.type==KEYEV_DOWN)p->track_shift=1;else if(ev.type==KEYEV_UP)p->track_shift=0;}
    if(ev.key==KEY_ALPHA){if(ev.type==KEYEV_DOWN)p->track_alpha=1;else if(ev.type==KEYEV_UP)p->track_alpha=0;}
}
static void progress_capture_motion(Progress *p,key_event_t ev)
{
    p->motion=1;p->motion_event=ev;
    p->motion_shift=p->track_shift||keydown(KEY_SHIFT)||(ev.mod&&ev.shift);
    p->motion_alpha=p->track_alpha||keydown(KEY_ALPHA)||(ev.mod&&ev.alpha);
#ifdef GL_HOST
    p->motion_shift|=ev.held_shift;p->motion_alpha|=ev.held_alpha;
#endif
}
/* These commands must remain usable when automatic refinement has already
   started; handle them in the normal 3D event loop after stopping a stage. */
static int progress_is_command(int key)
{
    return key==KEY_F2||key==KEY_F3||key==KEY_F4||key==KEY_F5||
           key==KEY_OPTN||key==KEY_VARS||key==KEY_2;
}
static void progress_capture_command(Progress *p,key_event_t ev)
{
    p->command=1;p->command_event=ev;
    p->command_shift=p->track_shift||keydown(KEY_SHIFT)||(ev.mod&&ev.shift);
    p->command_alpha=p->track_alpha||keydown(KEY_ALPHA)||(ev.mod&&ev.alpha);
#ifdef GL_HOST
    p->command_shift|=ev.held_shift;p->command_alpha|=ev.held_alpha;
#endif
}
/* During progressive refinement, camera motion cancels the incomplete stage. */
static int progressive_progress(int done,int total,void *context)
{
    Progress *p=context;key_event_t ev;
    while((ev=pollevent()).type!=KEYEV_NONE){
        progress_modifiers(p,ev);
        if(ev.type==KEYEV_DOWN&&(ev.key==KEY_EXIT||ev.key==KEY_F1)){p->cancel=1;return 0;}
        if(ev.type==KEYEV_DOWN&&ev.key==KEY_MENU){p->menu=1;return 0;}
        if((ev.type==KEYEV_DOWN||ev.type==KEYEV_HOLD)&&navigation_key(ev.key)){
            progress_capture_motion(p,ev);return 0;
        }
        if(ev.type==KEYEV_DOWN&&progress_is_command(ev.key)){
            progress_capture_command(p,ev);return 0;
        }
    }
#ifndef GL_HOST
    if(navigation_held((key_event_t){0})){
        progress_capture_motion(p,(key_event_t){.type=KEYEV_NONE});return 0;
    }
#endif
    /* The keyboard scanner's 16-bit tick runs at 128 Hz by default. Update
       at most once per ~2 s, only after another scanline has completed.
       Unsigned subtraction handles wraparound. No extra ray is computed. */
    if(p->live_cam && p->live_layer && total==PLOT_H){
        uint16_t now=ev.time;
        if(!p->live_started){p->live_started=1;p->live_tick=now;p->live_row=done;}
        else if(done>p->live_row && (uint16_t)(now-p->live_tick)>=256){
            p->live_row=done;p->live_tick=now;
            present3d(p->live_cam,p->live_layer,1,p->live_slow,0,0,2,1,done*100/PLOT_H);
        }
    }
    return 1;
}
/* While building a moving preview, catch NEW presses but do not abort merely
   because the original movement key is still held. */
static int preview_progress(int done,int total,void *context)
{
    (void)done;(void)total;Progress *p=context;key_event_t ev;
    while((ev=pollevent()).type!=KEYEV_NONE){
        progress_modifiers(p,ev);
        if(ev.type==KEYEV_DOWN&&ev.key==KEY_EXIT){p->cancel=1;return 0;}
        if(ev.type==KEYEV_DOWN&&ev.key==KEY_MENU){p->menu=1;return 0;}
        if(ev.type==KEYEV_DOWN&&navigation_key(ev.key)){
            progress_capture_motion(p,ev);return 0;
        }
    }
    return 1;
}
static void apply_progress_motion(Progress const *p,int slow_lock,int alpha_down)
{
    uint32_t held=navigation_held(p->motion_event);
    if(p->motion_event.type==KEYEV_DOWN||p->motion_event.type==KEYEV_HOLD)
        held|=navigation_key(p->motion_event.key);
    if(held)navigation_step(&app,held,slow_lock||alpha_down||p->motion_alpha,p->motion_shift);
}
#ifdef GL_PROGRESS_TEST
/* Target tests use actual gint keydev events; omitted from production. */
static Progress progress_test;
void graphlab_test_progress_init(void)
{
    memset(&progress_test,0,sizeof progress_test);
    progress_test.track_shift=keydown(KEY_SHIFT);progress_test.track_alpha=keydown(KEY_ALPHA);
}
int graphlab_test_progress_poll(int preview)
{
    return preview?preview_progress(0,1,&progress_test):progressive_progress(0,1,&progress_test);
}
int graphlab_test_progress_key(void){return progress_test.motion?progress_test.motion_event.key:0;}
int graphlab_test_progress_shift(void){return progress_test.motion_shift;}
int graphlab_test_progress_command(void){return progress_test.command?progress_test.command_event.key:0;}
int graphlab_test_progress_command_shift(void){return progress_test.command_shift;}
int graphlab_test_progress_menu(void){return progress_test.menu;}
int graphlab_test_progress_cancel(void){return progress_test.cancel;}
void graphlab_test_progress_apply(GraphState *s)
{
    app=*s;apply_progress_motion(&progress_test,0,0);*s=app;
}
/* Real progress/render code linked to fake display by the SH/QEMU harness. */
void graphlab_test_live_init(Camera const *cam,uint16_t const *layer)
{
    memset(&progress_test,0,sizeof progress_test);memset(&app,0,sizeof app);
    app.fast_ray_q=8;
    progress_test.live_cam=cam;progress_test.live_layer=layer;
}
int graphlab_test_live_poll(int done,int total,void *unused)
{
    (void)unused;return progressive_progress(done,total,&progress_test);
}
#endif
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
static int changed_common(GraphState const *a,SettingsSnapshot const *b)
{
    return a->param_a!=b->param_a||a->param_b!=b->param_b||a->param_c!=b->param_c||a->param_t!=b->param_t;
}
static int changed_3d(GraphState const *a,SettingsSnapshot const *b)
{
    return changed_common(a,b)||a->period_x!=b->period_x||
        a->volume_x!=b->volume_x||a->volume_y!=b->volume_y||a->volume_z!=b->volume_z||
        a->volume_radius!=b->volume_radius||a->fast_ray_q!=b->fast_ray_q;
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
        else {
            explicit_valid[i]=(uint8_t)expr_compile_explicit_z(&parsed,&explicit_programs[i]);
            ray_zero_valid[i]=(uint8_t)expr_compile_zero_surface(&parsed,&ray_zero_programs[i]);
        }
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
    while(x<=v->xmax && limit++<30){int xp=px(v,x);
        if(xp>=PLOT_X0&&xp<=PLOT_X1)for(int yp=PLOT_Y0;yp<=PLOT_Y1;yp+=4)dpixel(xp,yp,C_BLACK);
        x+=unit;
    }
    float y=ceilf(v->ymin/unit)*unit;limit=0;
    while(y<=v->ymax && limit++<30){int yp=py(v,y);
        if(yp>=PLOT_Y0&&yp<=PLOT_Y1)for(int xp=PLOT_X0;xp<=PLOT_X1;xp+=4)dpixel(xp,yp,C_BLACK);
        y+=unit;
    }
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
typedef struct {
    PlaneView const *view;unsigned char *mask;int dashed,phase,color;
    int x0,y0,x1,y1; /* inclusive screen clip for newly computed pixels */
} LineContext;
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
        if(a>=c->x0&&a<=c->x1&&b>=c->y0&&b<=c->y1&&
           (!c->dashed || (c->phase%9)<5)){
            size_t i=(size_t)(b-PLOT_Y0)*PLOT_W+(a-PLOT_X0);
            c->mask[i>>3]|=(unsigned char)(1u<<(i&7));
#ifdef GL_DEV
            /* Normally 2D lines live in masks; Dev paints the running trace. */
            dpixel(a,b,c->color);
#endif
        }
        c->phase++;
        if(a==d && b==e)break;
        int twice=2*err;if(twice>=dy){err+=dy;a+=sx;}if(twice<=dx){err+=dx;b+=sy;}
    }
}
typedef struct {
    int color;Relation relation;
    PlaneView const *screen,*sample,*grid;
    int x0,y0,x1,y1,x_offset;
    int grid_ix,grid_iy,grid_shift_x,grid_shift_y;
} FillContext;
static int floor_ratio(int a,int b)
{
    int q=a/b,r=a%b;return r<0?q-1:q;
}
static int fill_color(int slot)
{
    static const int colors[]={C_RGB(31,25,25),C_RGB(25,28,31),C_RGB(25,31,25)};
    return colors[slot%3];
}
static void fill_span(int x,int first,int last,void *context)
{
    FillContext *c=context;int xp=PLOT_X0+c->x_offset+x;
    int ya=PLOT_Y0+first,yb=PLOT_Y0+last;
    if(xp<c->x0||xp>c->x1)return;
    if(ya<c->y0)ya=c->y0;
    if(yb>c->y1)yb=c->y1;
    if(ya<=yb)drect(xp,ya,xp,yb,c->color);
}
static void fill_cell(int ix,int iy,int cols,int rows,float const q[4],void *context)
{
    FillContext *c=context;
    for(int k=0;k<4;k++)if(!isfinite(q[k]))return;
    int x0,x1,y0,y1;
    if(c->sample==c->screen){
        /* Preserve the original full-render rasterization. */
        x0=PLOT_X0+ix*(PLOT_W-1)/cols;x1=PLOT_X0+(ix+1)*(PLOT_W-1)/cols;
        y0=PLOT_Y0+iy*(PLOT_H-1)/rows;y1=PLOT_Y0+(iy+1)*(PLOT_H-1)/rows;
    }else{
        /* Use the original cell pixel boundaries, translated with the
           cached image, so filled implicit relations meet at the seam. */
        int gx=ix+c->grid_ix,gy=iy+c->grid_iy;
        x0=PLOT_X0+floor_ratio(gx*(PLOT_W-1),c->grid->cols)+c->grid_shift_x;
        x1=PLOT_X0+floor_ratio((gx+1)*(PLOT_W-1),c->grid->cols)+c->grid_shift_x;
        y0=PLOT_Y0+floor_ratio(gy*(PLOT_H-1),c->grid->rows)+c->grid_shift_y;
        y1=PLOT_Y0+floor_ratio((gy+1)*(PLOT_H-1),c->grid->rows)+c->grid_shift_y;
    }
    int ax=x0<c->x0?c->x0:x0,bx=x1>c->x1?c->x1:x1;
    int ay=y0<c->y0?c->y0:y0,by=y1>c->y1?c->y1:y1;
    if(ax>bx||ay>by)return;
    int t[4];for(int k=0;k<4;k++)t[k]=expr_relation_true(c->relation,q[k]);
    if(t[0]==t[1]&&t[0]==t[2]&&t[0]==t[3]){
        if(t[0])drect(ax,ay,bx,by,c->color);
        return;
    }
    for(int y=ay;y<=by;y++){
        float fy=y1==y0?0:(float)(y-y0)/(y1-y0);
        float left=q[0]*(1-fy)+q[3]*fy,right=q[1]*(1-fy)+q[2]*fy;
        if(!isfinite(left)||!isfinite(right))continue;
        for(int x=ax;x<=bx;x++){
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
            if(i==sel)drect_border(10,38+i*31,DWIDTH-11,64+i*31,C_WHITE,1,C_BLACK);
            dprint(16,45+i*31,INK,"%s = %.7g",names[i],(double)*v[i]);
        }
        dtext(12,157,MUTED,"Type value or EXE: edit coordinate.");
        footer("EDIT",NULL,NULL,NULL,NULL,"DONE");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6)return changed;
        if(e.key==KEY_UP)sel=(sel+count-1)%count;
        else if(e.key==KEY_DOWN)sel=(sel+1)%count;
        else if(e.key==KEY_F1||e.key==KEY_EXE||numeric_entry(e)!=ENTRY_NONE||e.key==KEY_F5){
            float next;EquationEntry entry=numeric_entry(e);
            if(e.key==KEY_F5&&entry==ENTRY_NONE)entry=ENTRY_EDIT;
            if(numeric_value(*v[sel],"COORDINATE",e,entry,&next)){
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
/* Reuse only a whole-pixel translation. When float center coordinates lose
   too much precision relative to the scale, a full render is safer. */
static int pan_is_pixels(float before,float after,float scale,int pixels)
{
    if(!isfinite(before)||!isfinite(after)||!isfinite(scale)||scale<=0)return 0;
    float actual=(after-before)/scale;
    return isfinite(actual)&&fabsf(actual-(float)pixels)<0.02f;
}
typedef struct {float values[PLOT_W];unsigned char known[PLOT_W];} Explicit2Cache;
static void explicit_cache_pan(Explicit2Cache *cache,int dx)
{
    if(!cache||!dx)return;
    if(dx<=-PLOT_W||dx>=PLOT_W){memset(cache->known,0,sizeof cache->known);return;}
    int left=dx>0?dx:0,source=dx<0?-dx:0,count=PLOT_W-abs(dx);
    memmove(cache->values+left,cache->values+source,(size_t)count*sizeof(float));
    memmove(cache->known+left,cache->known+source,(size_t)count);
    if(left)memset(cache->known,0,(size_t)left);
    if(left+count<PLOT_W)memset(cache->known+left+count,0,(size_t)(PLOT_W-left-count));
}
/* Build a small world-space contour grid that preserves the original grid's
   phase after the already-computed pixels have moved by shift_x/shift_y. */
static int strip_grid(PlaneView const *grid,ScrollRect r,int shift_x,int shift_y,
                      PlaneView *sub,int *out_ix,int *out_iy)
{
    if(abs(shift_x)>500000||abs(shift_y)>500000)return 0;
    int ix0=floor_ratio((r.x0-shift_x)*grid->cols,PLOT_W-1)-1;
    int ix1=floor_ratio((r.x1-shift_x)*grid->cols,PLOT_W-1)+2;
    int iy0=floor_ratio((r.y0-shift_y)*grid->rows,PLOT_H-1)-1;
    int iy1=floor_ratio((r.y1-shift_y)*grid->rows,PLOT_H-1)+2;
    int nx=ix1-ix0,ny=iy1-iy0;
    if(nx<1||nx>400||ny<1||ny>250)return 0;
    float dx=(grid->xmax-grid->xmin)/grid->cols,dy=(grid->ymax-grid->ymin)/grid->rows;
    *sub=*grid;sub->xmin=grid->xmin+ix0*dx;sub->xmax=grid->xmin+ix1*dx;
    sub->ymax=grid->ymax-iy0*dy;sub->ymin=grid->ymax-iy1*dy;
    sub->cols=nx;sub->rows=ny;
    if(!(sub->xmin<sub->xmax&&sub->ymin<sub->ymax))return 0;
    *out_ix=ix0;*out_iy=iy0;return 1;
}
/* Draw a full graph, or only the exposed strip. The caller supplies white in
   the dirty area; line masks and fill outside that clip are never touched. */
static int render2d_region(PlaneView const *screen,PlaneView const *grid,
            ScrollRect rect,int full,int shift_x,int shift_y,unsigned char *masks,
            Explicit2Cache *cache,Progress *pg)
{
    EvalVars vars=base_vars();
    for(int i=0;i<CURVE_COUNT;i++)if(app.curve[i].active){
        Relation relation=(Relation)programs[i].relation;PlotStats stats;int rc;
        int cx0=PLOT_X0+rect.x0,cy0=PLOT_Y0+rect.y0;
        int cx1=PLOT_X0+rect.x1,cy1=PLOT_Y0+rect.y1;
        LineContext line={.view=screen,.mask=masks+(size_t)i*LINE_MASK_BYTES,
            .dashed=relation==REL_LT||relation==REL_GT,.phase=0,.color=palette[i],
            .x0=cx0,.y0=cy0,.x1=cx1,.y1=cy1};
        FillContext fill={.color=fill_color(i),.relation=relation,.screen=screen,
            .sample=screen,.grid=grid,.x0=cx0,.y0=cy0,.x1=cx1,.y1=cy1,
            .grid_shift_x=shift_x,.grid_shift_y=shift_y};
        if(explicit_valid[i]){
            int first=full?0:MAXF(0,rect.x0-1),last=full?PLOT_W-1:MINF(PLOT_W-1,rect.x1+1);
            PlaneView sub=*screen;
            float step=(screen->xmax-screen->xmin)/(PLOT_W-1);
            sub.xmin=screen->xmin+first*step;sub.xmax=screen->xmin+last*step;
            fill.relation=explicit_relations[i];fill.x_offset=first;
            rc=plot_explicit_y_cached(&explicit_programs[i],explicit_relations[i],&vars,&sub,
                last-first+1,PLOT_H,draw_segment,&line,fill_span,&fill,progress,pg,&stats,
                cache?cache[i].values:NULL,cache?cache[i].known:NULL,first,cache?PLOT_W:0);
        }else{
            PlaneView sub;PlaneView const *sample=screen;int ix=0,iy=0;
            if(!full){
                if(!strip_grid(grid,rect,shift_x,shift_y,&sub,&ix,&iy))return -1;
                sample=&sub;fill.sample=sample;fill.grid_ix=ix;fill.grid_iy=iy;
            }
            int pw=full?PLOT_W:1+(int)lroundf((float)sample->cols*(PLOT_W-1)/grid->cols);
            int ph=full?PLOT_H:1+(int)lroundf((float)sample->rows*(PLOT_H-1)/grid->rows);
            ContourOptions options={accuracy_values[app.accuracy2],pw,ph,fill_cell,&fill};
            rc=plot_contour_opt(&programs[i],&vars,sample,draw_segment,&line,&options,progress,pg,&stats);
        }
        if(rc<=0)return rc;
    }
    return 1;
}
static int render2d_pan(PlaneView const *view,PlaneView const *grid,int dx,int dy,
           int shift_x,int shift_y,uint16_t *layer,unsigned char *masks,
           Explicit2Cache *cache,Progress *pg)
{
    ScrollRect r[2];int count=scroll_exposed(PLOT_W,PLOT_H,dx,dy,r);
    /* Check feasibility before mutating cached layers. */
    if(has_implicit_2d())for(int i=0;i<count;i++){
        PlaneView sub;int ix,iy;if(!strip_grid(grid,r[i],shift_x,shift_y,&sub,&ix,&iy))return -2;
    }
    scroll_pixels(layer,PLOT_W,PLOT_H,PLOT_W,dx,dy,C_WHITE);
    for(int i=0;i<CURVE_COUNT;i++){
        scroll_bits(masks+(size_t)i*LINE_MASK_BYTES,PLOT_W,PLOT_H,dx,dy);
        if(cache)explicit_cache_pan(&cache[i],dx);
    }
    dclear(C_WHITE);layer_blit(layer);int rc=1;
    for(int i=0;i<count&&rc==1;i++)rc=render2d_region(view,grid,r[i],0,shift_x,shift_y,masks,cache,pg);
    if(rc==1)layer_capture(layer);
    return rc;
}
static void plot2d(void)
{
    if(!compile_active(0))return;
    uint16_t *layer=layer_create();unsigned char *masks=calloc(CURVE_COUNT,LINE_MASK_BYTES);
    Explicit2Cache *cache=calloc(CURVE_COUNT,sizeof *cache); /* optional speedup */
    if(!layer||!masks){free(cache);free(layer);free(masks);message("2D MEMORY","Not enough memory for the plot layers.");return;}
    int ready=0,render=0,grid_shift_x=0,grid_shift_y=0;PlaneView grid={0};
    for(;;) {
        PlaneView view=plane_view();
        if(!(view.xmin<view.xmax && view.ymin<view.ymax)){
            dclear(C_WHITE);header("2D ZOOM LIMIT","FLOAT PRECISION");
            dtext(10,60,WARNING,"The selected center and scale exceed float precision.");
            dtext(10,82,INK,"Press - to zoom out, or F3 RESET.");
            dtext(10,104,MUTED,"VARS sets the center; EXIT returns to equations.");
            footer("EQS",NULL,"RESET",NULL,NULL,NULL);dupdate();key_event_t e=input_key();
            if(e.key==KEY_EXIT||e.key==KEY_F1)break;
            if(e.key==KEY_SUB)app.plane_scale=MINF(1000,app.plane_scale*1.5f);
            else if(e.key==KEY_F3){app.plane_x=app.plane_y=0;app.plane_scale=.05f;}
            else if(e.key==KEY_VARS)point_editor("2D VIEW CENTER",&app.plane_x,&app.plane_y,NULL);
            ready=0;render=0;continue;
        }
        if(render){
            calculation_begin();dclear(C_WHITE);memset(masks,0,CURVE_COUNT*LINE_MASK_BYTES);
            if(cache)memset(cache,0,CURVE_COUNT*sizeof *cache);
            grid=view;grid_shift_x=grid_shift_y=0;
            ScrollRect all={0,0,PLOT_W-1,PLOT_H-1};Progress pg={0};
            int rc=render2d_region(&view,&grid,all,1,0,0,masks,cache,&pg);
            calculation_end();
            if(pg.menu){gint_osmenu();break;}
            if(pg.cancel||rc==0)break;
            if(rc<0){message("PLOT ERROR","Not enough memory or invalid view.");break;}
            layer_capture(layer);ready=1;render=0;
        }
        present2d(&view,layer,masks,ready);key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F1)break;
        int move_px=e.shift?15:45,pan=0,dx=0,dy=0;
        float oldx=app.plane_x,oldy=app.plane_y,move=app.plane_scale*move_px;int changed=0;
        if(e.key==KEY_LEFT){app.plane_x-=move;dx=move_px;pan=changed=1;}
        else if(e.key==KEY_RIGHT){app.plane_x+=move;dx=-move_px;pan=changed=1;}
        else if(e.key==KEY_UP){app.plane_y+=move;dy=move_px;pan=changed=1;}
        else if(e.key==KEY_DOWN){app.plane_y-=move;dy=-move_px;pan=changed=1;}
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
            snapshot_settings(&settings_before,&app);settings();
            changed=changed_common(&app,&settings_before)||app.period_x!=settings_before.period_x||
                (has_implicit_2d() && app.accuracy2!=settings_before.accuracy2);
        }
        else if(e.key==KEY_F6||e.key==KEY_EXE){if(!ready)render=1;}
        app.plane_x=MAXF(-1e6f,MINF(1e6f,app.plane_x));app.plane_y=MAXF(-1e6f,MINF(1e6f,app.plane_y));
        if(changed && ready){
            int safe=pan&&pan_is_pixels(oldx,app.plane_x,app.plane_scale,-dx)&&
                          pan_is_pixels(oldy,app.plane_y,app.plane_scale,dy);
            PlaneView next=plane_view();
            safe=safe&&pan_is_pixels(view.xmin,next.xmin,app.plane_scale,-dx)&&
                       pan_is_pixels(view.xmax,next.xmax,app.plane_scale,-dx)&&
                       pan_is_pixels(view.ymin,next.ymin,app.plane_scale,dy)&&
                       pan_is_pixels(view.ymax,next.ymax,app.plane_scale,dy);
            if(safe&&next.xmin<next.xmax&&next.ymin<next.ymax){
                int next_x=grid_shift_x+dx,next_y=grid_shift_y+dy;
                Progress pg={0};calculation_begin();
                int rc=render2d_pan(&next,&grid,dx,dy,next_x,next_y,layer,masks,cache,&pg);
                calculation_end();
                if(pg.menu){gint_osmenu();break;}
                if(pg.cancel||rc==0)break;
                if(rc==-2)render=1; /* precision/grid overflow: full fallback */
                else if(rc<0){message("PLOT ERROR","Not enough memory or invalid view.");break;}
                else {grid_shift_x=next_x;grid_shift_y=next_y;}
            }else render=1;
        }
    }
    free(cache);free(masks);free(layer);
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
            if(i==axis)drect_border(8,28+i*24,DWIDTH-9,48+i*24,C_WHITE,1,C_BLACK);
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
        dtext(13,172,MUTED,"Type / EXE: exact   +/-: adjust selected axis");
        footer("NEXT",NULL,NULL,NULL,NULL,"BACK");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6||e.key==KEY_VARS)break;
        if(e.key==KEY_LEFT)axis=(axis+2)%3;else if(e.key==KEY_RIGHT)axis=(axis+1)%3;
        else if(e.key==KEY_UP||e.key==KEY_ADD)xyz[axis]+=app.volume_radius*(e.shift?.01f:.1f);
        else if(e.key==KEY_DOWN||e.key==KEY_SUB)xyz[axis]-=app.volume_radius*(e.shift?.01f:.1f);
        else if(e.key==KEY_F1){for(int j=0;j<SURFACE_COUNT;j++){slot=(slot+1)%SURFACE_COUNT;if(app.surface[slot].active)break;}}
        else if(e.key==KEY_EXE||numeric_entry(e)!=ENTRY_NONE||e.key==KEY_F5){
            float value2;EquationEntry entry=numeric_entry(e);
            if(e.key==KEY_F5&&entry==ENTRY_NONE)entry=ENTRY_EDIT;
            if(numeric_value(xyz[axis],"PROBE COORDINATE",e,entry,&value2) &&
               isfinite(value2)&&fabsf(value2)<=1e6f)xyz[axis]=value2;
        }
    }
    memcpy(gint_vram,backup,bytes);free(backup);
}
/* A cached XY projection of TWO surfaces; cursor Z is always searched afresh. */
#define MAP_X0 7
#define MAP_Y0 PLOT_Y0
#define MAP_X1 (MAP_X0+IMAP_SIDE-1)
#define MAP_Y1 (MAP_Y0+IMAP_SIDE-1)
static int map_px(IMapView const *v,float x)
{
    if(x<v->center_x-v->radius)return MAP_X0-1;
    if(x>v->center_x+v->radius)return MAP_X1+1;
    return MAP_X0+(int)lroundf((x-v->center_x+v->radius)*(IMAP_SIDE-1)/(2*v->radius));
}
static int map_py(IMapView const *v,float y)
{
    if(y>v->center_y+v->radius)return MAP_Y0-1;
    if(y<v->center_y-v->radius)return MAP_Y1+1;
    return MAP_Y1-(int)lroundf((y-v->center_y+v->radius)*(IMAP_SIDE-1)/(2*v->radius));
}
static void map_present(IMapView const *v,IMapPixel const *map,IMapRoots const *roots,
                        IMapPixelHits const *hits,int pixel_mode,float x,float y,int selected,
                        IMapPoint const *nearest,int has_near,IMapStats const *st)
{
    dclear(C_WHITE);header("3D CROSSINGS","XY PROJECTION");
    drect_border(MAP_X0,MAP_Y0,MAP_X1,MAP_Y1,C_WHITE,1,C_BLACK);
    int ox=map_px(v,0),oy=map_py(v,0);
    if(ox>MAP_X0&&ox<MAP_X1)dline(ox,MAP_Y0+1,ox,MAP_Y1-1,C_LIGHT);
    if(oy>MAP_Y0&&oy<MAP_Y1)dline(MAP_X0+1,oy,MAP_X1-1,oy,C_LIGHT);
    for(int j=0;j<IMAP_SIDE;j++)for(int i=0;i<IMAP_SIDE;i++)
        if(map[(size_t)j*IMAP_SIDE+i].z!=INT16_MIN)
            gint_vram[(size_t)(MAP_Y0+j)*DWIDTH+MAP_X0+i]=C_BLACK;
    dprint(MAP_X0+3,MAP_Y0+2,INK,"Y %.3g",(double)(v->center_y+v->radius));
    dprint(MAP_X0+3,MAP_Y1-23,INK,"Y %.3g",(double)(v->center_y-v->radius));
    dprint(MAP_X0+3,MAP_Y1-12,INK,"X %.3g",(double)(v->center_x-v->radius));
    char xmax[24];snprintf(xmax,sizeof xmax,"X %.3g",(double)(v->center_x+v->radius));
    int label_width=0;dsize(xmax,NULL,&label_width,NULL);
    dtext(MAXF(MAP_X0+3,MAP_X1-label_width-3),MAP_Y1-12,INK,xmax);
    int found=pixel_mode?hits->count:(roots->count||roots->interval);
    if(has_near&&!found){
        int nx=map_px(v,nearest->x),ny=map_py(v,nearest->y);
        drect_border(MAXF(MAP_X0,nx-2),MAXF(MAP_Y0,ny-2),MINF(MAP_X1,nx+2),MINF(MAP_Y1,ny+2),C_WHITE,1,C_BLACK);
    }
    int cx=map_px(v,x),cy=map_py(v,y);
    if(cx>=MAP_X0&&cx<=MAP_X1&&cy>=MAP_Y0&&cy<=MAP_Y1){
        dline(MAXF(MAP_X0,cx-4),cy,MINF(MAP_X1,cx+4),cy,C_BLACK);
        dline(cx,MAXF(MAP_Y0,cy-4),cx,MINF(MAP_Y1,cy+4),C_BLACK);
    }
    dline(193,21,193,198,C_LIGHT);
    dtext(201,25,INK,pixel_mode?"PIXEL CURSOR":"EXACT INPUT XY");
    dprint(201,43,INK,"X  %.6g",(double)x);dprint(201,59,INK,"Y  %.6g",(double)y);
    if(pixel_mode&&hits->count){
        IMapPoint const *p=&hits->point[selected];
        dtext(201,80,INK,"IN PIXEL: FOUND ~");
        dprint(201,98,INK,"X %.7g",(double)p->x);
        dprint(201,114,INK,"Y %.7g",(double)p->y);
        dprint(201,130,INK,"Z %.7g",(double)p->z);
        dprint(201,148,MUTED,"%d/%d%s  F3: NEXT",selected+1,hits->count,hits->overflow?"+":"");
    }else if(!pixel_mode&&roots->interval){
        dtext(201,80,INK,"MANY Z? (SAMPLED)");dtext(201,99,MUTED,"Not isolated; numerical.");
    }else if(!pixel_mode&&roots->count){
        dtext(201,80,INK,"AT XY: FOUND ~");
        dprint(201,98,INK,"Z  %.7g",(double)roots->z[selected]);
        dprint(201,116,INK,"%d / %d%s",selected+1,roots->count,roots->overflow?"+":"");
        dtext(201,134,MUTED,"F3: NEXT Z");
    }else{
        int mapped=cx>=MAP_X0&&cx<=MAP_X1&&cy>=MAP_Y0&&cy<=MAP_Y1&&
            map[(size_t)(cy-MAP_Y0)*IMAP_SIDE+(cx-MAP_X0)].z!=INT16_MIN;
        dtext(201,80,INK,pixel_mode?(mapped?"PIXEL: UNVERIFIED":"PIXEL: NOT FOUND"):
                                        "AT XY: NOT FOUND");
        if(has_near){
            dtext(201,99,INK,"NEAREST MAPPED ~");
            dprint(201,116,INK,"X %.5g  Y %.5g",(double)nearest->x,(double)nearest->y);
            dprint(201,133,INK,"Z %.6g",(double)nearest->z);
            dtext(201,150,MUTED,"F4: MOVE TO NEAR");
        }else dtext(201,99,MUTED,"NO MAPPED POINT");
    }
    if(cx<MAP_X0||cx>MAP_X1||cy<MAP_Y0||cy>MAP_Y1)dtext(201,168,MUTED,"XY OUTSIDE MAP");
    else dtext(201,168,MUTED,"ARROWS MOVE  +/- ZOOM");
    dprint(201,184,MUTED,"Z %.4g .. %.4g%s",(double)v->zmin,(double)v->zmax,st->low_memory?" *":"");
    footer("ZOOM","INPUT","NEXT","NEAR","RESET","BACK");dupdate();
}
static void map_center_pixel(IMapView const *v,int ix,int iy,float *x,float *y)
{
    ix=MAXF(0,MINF(IMAP_SIDE-1,ix));iy=MAXF(0,MINF(IMAP_SIDE-1,iy));
    float step=2*v->radius/(IMAP_SIDE-1);
    *x=v->center_x-v->radius+ix*step;*y=v->center_y+v->radius-iy*step;
}
static int map_auto_view(IMapView *v)
{
    return imap_auto_view_bounds(v,-app.volume_radius-app.map_extend_down,
                                 app.volume_radius+app.map_extend_up);
}
static void intersection_screen(void)
{
    RaySurface surface[2];Program const *height[2]={NULL,NULL};int count=0;
    for(int i=0;i<SURFACE_COUNT;i++)if(app.surface[i].active){
        if(programs[i].relation>=REL_LT){message("CROSSINGS","Use two equalities, not inequalities.");return;}
        surface[count]=(RaySurface){ray_zero_valid[i]?&ray_zero_programs[i]:&programs[i],i};
        height[count]=explicit_valid[i]?&explicit_programs[i]:NULL;count++;
    }
    if(count!=2){message("CROSSINGS","Turn on two 3D equations first.");return;}
    IMapPixel *map=malloc((size_t)IMAP_SIDE*IMAP_SIDE*sizeof *map);
    if(!map){message("MAP MEMORY","Not enough memory for the XY map.");return;}
    IMapView view={.radius=app.volume_radius};
    if(!map_auto_view(&view)){free(map);message("MAP ERROR","Z range needs too many pixel slices.");return;}
    IMapStats stats={0};IMapRoots roots={0};IMapPixelHits hits={0};IMapPoint nearest={0};
    float x=0,y=0;int selected=0,dirty=1,search=1,has_near=0,pixel_mode=1;
    for(;;){
        if(dirty){
            Progress pg={0};EvalVars vars=base_vars();calculation_begin();
            int rc=imap_build_auto(surface,&vars,&view,height,map,progress,&pg,&stats);calculation_end();
            if(pg.menu){gint_osmenu();break;}
            if(pg.cancel||rc==0)break;
            if(rc<0){message("MAP ERROR","Not enough memory or invalid range.");break;}
            dirty=0;search=1;
        }
        if(search){
            Progress pg={0};EvalVars vars=base_vars();int rc=1;
            memset(&roots,0,sizeof roots);memset(&hits,0,sizeof hits);
            int ix=map_px(&view,x)-MAP_X0,iy=map_py(&view,y)-MAP_Y0;
            calculation_begin();
            if(pixel_mode&&ix>=0&&ix<IMAP_SIDE&&iy>=0&&iy<IMAP_SIDE)
                rc=imap_pixel_search(surface,&vars,&view,ix,iy,map,progress,&pg,&hits);
            else if(!pixel_mode)rc=imap_search(surface,&vars,&view,x,y,progress,&pg,&roots);
            calculation_end();
            if(pg.menu){gint_osmenu();break;}
            if(pg.cancel||rc==0)break;
            if(rc<0){message("SEARCH ERROR","Invalid XY or Z range.");break;}
            selected=0;has_near=imap_nearest(map,&view,x,y,&nearest);search=0;
        }
        map_present(&view,map,&roots,&hits,pixel_mode,x,y,selected,&nearest,has_near,&stats);
        key_event_t e=input_key();
        if(e.key==KEY_EXIT||e.key==KEY_F6)break;
        if(e.key==KEY_LEFT||e.key==KEY_RIGHT||e.key==KEY_UP||e.key==KEY_DOWN){
            int step=e.shift?1:5,ix=map_px(&view,x)-MAP_X0,iy=map_py(&view,y)-MAP_Y0;
            ix=MAXF(0,MINF(IMAP_SIDE-1,ix));iy=MAXF(0,MINF(IMAP_SIDE-1,iy));
            if(e.key==KEY_LEFT)ix-=step;else if(e.key==KEY_RIGHT)ix+=step;
            else if(e.key==KEY_UP)iy-=step;else iy+=step;
            map_center_pixel(&view,ix,iy,&x,&y);pixel_mode=1;search=1;
        }else if(e.key==KEY_F2||e.key==KEY_VARS){
            (void)point_editor("ENTER XY FOR Z SEARCH",&x,&y,NULL);
            pixel_mode=0;search=1;
        }else if(e.key==KEY_F3){
            int n=pixel_mode?hits.count:roots.count;if(n)selected=(selected+1)%n;
        }else if(e.key==KEY_F4&&has_near){
            int ix=map_px(&view,nearest.x)-MAP_X0,iy=map_py(&view,nearest.y)-MAP_Y0;
            map_center_pixel(&view,ix,iy,&x,&y);pixel_mode=1;search=1;
        }else if(e.key==KEY_F1||e.key==KEY_ADD||e.key==KEY_SUB){
            float factor=(e.key==KEY_SUB||(e.key==KEY_F1&&e.shift))?2:.5f;
            float r=MAXF(.001f,MINF(1e4f,view.radius*factor));
            if(r!=view.radius){
                IMapView next=view;next.radius=r;next.center_x=x;next.center_y=y;
                if(!map_auto_view(&next))message("MAP ZOOM","Too many pixel slices. Zoom out.");
                else {view=next;pixel_mode=1;dirty=1;}
            }
        }else if(e.key==KEY_F5){
            view.center_x=view.center_y=0;view.radius=app.volume_radius;x=y=0;
            if(!map_auto_view(&view)){message("MAP ERROR","Z range needs too many pixel slices.");break;}
            pixel_mode=1;dirty=1;
        }else if(e.key==KEY_OPTN){
            settings();
            if(!map_auto_view(&view)){
                view.center_x=view.center_y=0;view.radius=app.volume_radius;x=y=0;pixel_mode=1;
                if(!map_auto_view(&view)){message("MAP ERROR","Z range needs too many pixel slices.");break;}
            }
            dirty=1;
        }
    }
    free(map);
}

static void present3d(Camera const *cam,uint16_t const *layer,int ready,int slow,int fast,int empty,int previewing,int drawn_step,int drawn_progress)
{
    char const *mode=previewing==3?"PREVIEW UNAVAILABLE | PLOT":
        previewing==2?"REFINING | MOVE: STOP":previewing?(slow?"PREVIEW | SLOW | AUTO":fast?"PREVIEW | FAST | AUTO":"PREVIEW | AUTO REFINE"):
        slow?"ARROWS: TURN  |  SLOW":fast?"ARROWS: TURN  |  FAST":"ARROWS: TURN";
    char quality[12];
    if(previewing==2&&drawn_step==1&&drawn_progress>=0)
        snprintf(quality,sizeof quality,"R%d/1 %d%%",app.fast_ray_q,drawn_progress>100?100:drawn_progress);
    else if(previewing&&previewing!=3)snprintf(quality,sizeof quality,"R%d/P%d",app.fast_ray_q,drawn_step);
    else snprintf(quality,sizeof quality,"R%d/1",app.fast_ray_q);
    dclear(C_WHITE);header("3D GRAPH",mode);
    if(app.axes && !app.axes_front)render3_axes(cam);
    if(ready){
        if(app.axes && !app.axes_front)layer_blit(layer);
        else for(int y=0;y<PLOT_H;y++)memcpy(gint_vram+(size_t)(PLOT_Y0+y)*DWIDTH+PLOT_X0,
                                              layer+(size_t)y*PLOT_W,(size_t)PLOT_W*sizeof *layer);
    }
    if(app.axes && app.axes_front)render3_axes(cam);
    drect(7,20,390,33,C_WHITE);
    dprint(9,22,MUTED,"Eye %.3g, %.3g, %.3g  |  clip %.3g  |  %s",
        (double)app.eye_x,(double)app.eye_y,(double)app.eye_z,(double)app.volume_radius,quality);
    if(ready && empty){
        drect(7,174,390,195,C_WHITE);
        if(previewing==2)
            dtext(9,178,WARNING,"No sampled hit at this step; refining.");
        else if(previewing&&previewing!=3&&drawn_step>1)
            dtext(9,178,WARNING,"No sampled hit; auto refining on stop.");
        else dtext(9,178,WARNING,"No visible surface / volume in clip cube.");
    }
    if(!ready){
        drect(7,173,390,198,C_WHITE);
        dtext(9,175,ACCENT,"8/5: front/back  4/6: side  1/0: up/down");
        dtext(9,187,MUTED,"2: XY map  |  Auto draws when still");
    }
    footer("EQS","RESET",app.axes?"AX ON":"AX OFF",quality,app.axes_front?"FRONT":"BACK","AUTO");dupdate();
}
/* Each stage operates on the same layer and only samples previously unknown
   screen positions. The existing candidate/occupancy grid stays reusable. */
static int render3d_stage(Camera const *cam,int step,int final_stream,RayGridCache **raygrid,
                          ScreenRefine *refine,uint16_t *layer,int *empty,Progress *pg,int interruptible)
{
    if(!*raygrid)*raygrid=calloc(1,sizeof **raygrid); /* optional cache */
    Volume vol={.x=0,.y=0,.z=0,.radius=app.volume_radius,
        .div=app.fast_ray_q,.period_x=app.period_x};
    RaySurface surfaces[SURFACE_COUNT];
    RayRenderOptions ro={.grid=*raygrid,.screen_step=(uint8_t)step,.final_stream=(uint8_t)final_stream,.layer=layer,.refine=refine};int n=0;
    for(int i=0;i<SURFACE_COUNT;i++)if(app.surface[i].active){
        surfaces[n]=(RaySurface){ray_zero_valid[i]?&ray_zero_programs[i]:&programs[i],i};
        if(explicit_valid[i])ro.height[n]=&explicit_programs[i];
        n++;
    }
    EvalVars vars=base_vars();
    pg->track_shift=keydown(KEY_SHIFT);pg->track_alpha=keydown(KEY_ALPHA);
    ProgressFn callback=interruptible==1?progressive_progress:interruptible==2?preview_progress:progress;
    calculation_begin();
    RayStats st={0};int rc=ray3d_render_ex(surfaces,n,&vars,&vol,cam,&ro,callback,pg,&st);
    /* The streamed step skips old even/even pixels. A previous hit remains. */
    *empty=final_stream?(*empty&&st.hits==0):(st.hits==0);
    calculation_end();return rc;
}
static void clear_3d_grid(RayGridCache **r)
{
    ray_grid_cache_clear(*r);free(*r);*r=NULL;
}
/* 1: complete coarse frame, 0: EXIT, -1: allocation/error, -2: MENU. */
static int preview3d_frame(uint16_t **layer,Camera const *cam,RayGridCache **r,
                           ScreenRefine *refine,int *empty,int slow_lock,int alpha_down)
{
    if(!*layer)*layer=layer_create();
    if(!*layer)return -1;
    Camera current=*cam;
    for(;;){
        if(!refine_begin(refine,app.preview_ray_step))return -1;
        Progress pg={0};int rc=render3d_stage(&current,app.preview_ray_step,0,r,refine,*layer,empty,&pg,2);
        if(pg.menu)return -2;
        if(pg.cancel)return 0;
        if(pg.motion){apply_progress_motion(&pg,slow_lock,alpha_down);current=camera3d();continue;}
        if(rc==0)return 0;
        return rc<0?-1:1;
    }
}
static void plot3d(void)
{
    if(!compile_active(1))return;
    uint16_t *layer=NULL;RayGridCache *grid=NULL;ScreenRefine refine={0};
    int ready=0,render=0,previewing=0,empty=0,slow_lock=0,alpha_chord=0,hold_parked=0,drawn_step=0,drawn_progress=-1;
    int pending=0,pending_shift=0,pending_alpha=0;key_event_t pending_event={0};
    int alpha_down=keydown(KEY_ALPHA);
    for(;;){
        Camera cam=camera3d();
        if(render){
            int interrupt=0,broken=0;
            if(!ready||!refine.seed_done){
                if(!layer)layer=layer_create();
                if(!layer){clear_3d_grid(&grid);layer=layer_create();}
                int seed=app.fast_ray_step;
                if(!layer||!refine_begin(&refine,seed)){message("3D MEMORY","Not enough memory for progressive plot.");break;}
            }
            int step=refine.seed_done?refine_next(refine.last_step):refine.seed_step;
            while(step){
                /* Stream the heavy 2 -> 1 Ray transition by completed rows.
                   The old even/even lattice is skipped without retracing. */
                int stream=step==1&&refine.seed_done&&refine.last_step==2;
                Progress pg={0};
                if(stream){
                    pg.live_cam=&cam;pg.live_layer=layer;pg.live_slow=slow_lock||alpha_down;
                    present3d(&cam,layer,1,pg.live_slow,0,0,2,1,0);
                }
                int was_empty=empty;
                int rc=render3d_stage(&cam,step,stream,&grid,&refine,layer,&empty,&pg,1);
                if(pg.menu){gint_osmenu();broken=1;break;}
                if(pg.cancel){broken=1;break;}
                if(pg.command){
                    pending=1;pending_event=pg.command_event;
                    pending_shift=pg.command_shift;pending_alpha=pg.command_alpha;
                    /* Incomplete seed has no valid image; otherwise retain the
                       last level and restart its unfinished successor later. */
                    if(!ready){refine_free(&refine);previewing=0;}
                    else {previewing=1;empty=was_empty&&empty;}
                    drawn_progress=-1;interrupt=1;break;
                }
                if(pg.motion){
                    apply_progress_motion(&pg,slow_lock,alpha_down);
                    Camera next=camera3d();int pr=preview3d_frame(&layer,&next,&grid,&refine,&empty,slow_lock,alpha_down);
                    if(pr==-2){gint_osmenu();broken=1;break;}
                    if(pr==0){broken=1;break;}
                    ready=pr>0;previewing=pr<0?3:1;drawn_progress=-1;
                    if(pr>0)drawn_step=app.preview_ray_step;
                    interrupt=1;break;
                }
                if(rc!=1){message("3D ERROR","Not enough memory or invalid view.");broken=1;break;}
                ready=1;drawn_step=step;drawn_progress=-1;previewing=step>1?2:0;
                /* Each complete refinement level becomes visible. */
                present3d(&cam,layer,1,slow_lock||alpha_down,0,empty,previewing,drawn_step,drawn_progress);
                step=refine_next(step);
            }
            if(broken)break;
            render=0;
            if(interrupt)continue;
            previewing=0; /* last stage is the full one-pixel frame */
        }
#ifdef GL_HOST
        int hud_slow=slow_lock||alpha_down,hud_fast=0;
#else
        int hud_slow=slow_lock||keydown(KEY_ALPHA),hud_fast=keydown(KEY_SHIFT);
#endif
        present3d(&cam,layer,ready,hud_slow,hud_fast,empty,previewing,drawn_step,drawn_progress);
        int idle=0,needs_auto=previewing!=3&&(!ready||previewing==1);
        int from_pending=pending;key_event_t e;
        if(pending){e=pending_event;pending=0;}
        else e=hold_parked?navigation_wait_change():
            needs_auto?navigation_wait_idle(&idle):navigation_wait();
        hold_parked=0;
        if(idle && !navigation_held(e)){render=1;continue;}
        if(e.key==KEY_ALPHA && e.type==KEYEV_DOWN){alpha_down=1;alpha_chord=navigation_held(e)!=0;continue;}
        if(e.key==KEY_ALPHA && e.type==KEYEV_UP){
            if(alpha_down && !alpha_chord)slow_lock=!slow_lock;
            alpha_down=0;alpha_chord=0;continue;
        }
#ifdef GL_HOST
        int alpha_held=alpha_down||e.held_alpha||(from_pending&&pending_alpha);
        int shift_held=e.shift||e.held_shift||(from_pending&&pending_shift);
#else
        int alpha_held=keydown(KEY_ALPHA)||(from_pending&&pending_alpha);
        int shift_held=keydown(KEY_SHIFT)||(from_pending&&pending_shift);
#endif
        uint32_t held=navigation_held(e);
        if(alpha_held && e.type==KEYEV_DOWN && e.key!=KEY_ALPHA)alpha_chord=1;
        if((e.type==KEYEV_DOWN && navigation_key(e.key)) || e.type==KEYEV_NONE || e.type==KEYEV_HOLD){
            if(held){
                if(navigation_step(&app,held,slow_lock||alpha_held,shift_held)){
                    Camera next=camera3d();int pr=preview3d_frame(&layer,&next,&grid,&refine,&empty,slow_lock,alpha_down);
                    if(pr==-2){gint_osmenu();break;}
                    if(pr==0)break;
                    previewing=pr<0?3:1;ready=pr>0;drawn_progress=-1;
                    if(pr>0)drawn_step=app.preview_ray_step;
                }else hold_parked=1;
            }
            if(held && alpha_held)alpha_chord=1;
            continue;
        }
        if(e.type!=KEYEV_DOWN)continue;
        if(e.key==KEY_EXIT||e.key==KEY_F1)break;
        int changed=0,view_only=0;
        if(e.key==KEY_MENU){gint_osmenu();}
        else if(e.key==KEY_F2){app.camera_yaw=.75f;app.camera_pitch=.55f;app.camera_zoom=1;state_default_eye(&app);changed=view_only=1;}
        else if(e.key==KEY_2 || (e.key==KEY_F3 && shift_held)){
            snapshot_settings(&settings_before,&app);clear_3d_grid(&grid);intersection_screen();
            changed=changed_3d(&app,&settings_before);
        }
        else if(e.key==KEY_F3)app.axes^=1;
        else if(e.key==KEY_F4){
            int q=app.fast_ray_q;app.fast_ray_q=(uint8_t)(q<12?12:q<20?20:q<32?32:q<48?48:4);
            changed=1;
        }
        else if(e.key==KEY_F5)app.axes_front^=1;
        else if(e.key==KEY_OPTN){snapshot_settings(&settings_before,&app);settings();changed=changed_3d(&app,&settings_before);}
        else if(e.key==KEY_F6||e.key==KEY_EXE){if(!ready||previewing)render=1;}
        else if(e.key==KEY_VARS){
            /* Auto rendering can finish before VARS is pressed. SHIFT+VARS
               keeps exact observer coordinates accessible after that point. */
            if(ready&&!shift_held)probe3d();
            else {changed=point_editor("3D OBSERVER POSITION",&app.eye_x,&app.eye_y,&app.eye_z);view_only=1;}
        }
        if(changed){
            if(view_only&&ready){
                Camera next=camera3d();int pr=preview3d_frame(&layer,&next,&grid,&refine,&empty,slow_lock,alpha_down);
                if(pr==-2){gint_osmenu();break;}
                if(pr==0)break;
                previewing=pr<0?3:1;ready=pr>0;drawn_progress=-1;if(pr>0)drawn_step=app.preview_ray_step;
            }else if(!view_only){clear_3d_grid(&grid);refine_free(&refine);ready=previewing=0;drawn_progress=-1;}
        }
    }
    free(layer);refine_free(&refine);clear_3d_grid(&grid);
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
/* Shift only the plot area in VRAM; the physical display stays on the
   previous complete frame until every newly exposed block has been computed. */
static int fractal_pan(int dx,int dy,int *phase_x,int *phase_y)
{
    FractalView const *v=&app.fractal;
    uint16_t *plot=gint_vram+(size_t)PLOT_Y0*DWIDTH+PLOT_X0;
    scroll_pixels(plot,PLOT_W,PLOT_H,DWIDTH,dx,dy,C_WHITE);
    *phase_x=(*phase_x+dx)%v->block;if(*phase_x<0)*phase_x+=v->block;
    *phase_y=(*phase_y+dy)%v->block;if(*phase_y<0)*phase_y+=v->block;
    ScrollRect exposed[2];int count=scroll_exposed(PLOT_W,PLOT_H,dx,dy,exposed);
    Progress pg={0};EvalVars vars=base_vars();int rc=1;
    calculation_begin();
    for(int i=0;i<count&&rc==1;i++){
        ScrollRect r=exposed[i];
        rc=fractal_generate_region(&fractal_program,&vars,v,
                    PLOT_X0+r.x0,PLOT_Y0+r.y0,PLOT_X0+r.x1,PLOT_Y0+r.y1,
                    *phase_x,*phase_y,fractal_block,(void *)v,progress,&pg);
    }
    calculation_end();
    if(pg.menu){gint_osmenu();return 0;}
    if(pg.cancel||rc==0)return 0;
    if(rc<0){message("FRACTAL ERROR","Invalid view or formula.");return 0;}
    dupdate();return 1;
}
static void plot_fractal(void)
{
    char error[90];
    if(!compile_formula(app.recurrence[app.fractal_slot],&fractal_program,error,sizeof error)){message("INVALID FRACTAL",error);return;}
    if(fractal_program.has_equal || (fractal_program.variables & ~allowed_vars(3))){message("INVALID FRACTAL","Use x, y, z, c, n, i and a, b, t.");return;}
    int dirty=0,ready=0,phase_x=0,phase_y=0;
    for(;;){
        if(dirty){
            phase_x=phase_y=0;
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
        int move=e.shift?15:45,pan=0,dx=0,dy=0;
        float oldx=app.fractal.cx,oldy=app.fractal.cy;
        float distance=app.fractal.units_per_pixel*move;int changed=1;
        if(e.key==KEY_LEFT){app.fractal.cx-=distance;dx=move;pan=1;}
        else if(e.key==KEY_RIGHT){app.fractal.cx+=distance;dx=-move;pan=1;}
        else if(e.key==KEY_UP){app.fractal.cy+=distance;dy=move;pan=1;}
        else if(e.key==KEY_DOWN){app.fractal.cy-=distance;dy=-move;pan=1;}
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
                snapshot_settings(&settings_before,&app);settings();
                changed=changed_common(&app,&settings_before)||
                    app.fractal.julia_re!=settings_before.julia_re||
                    app.fractal.julia_im!=settings_before.julia_im||
                    app.fractal.escape_radius!=settings_before.escape_radius;
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
        if(changed && ready){
            int safe=pan&&pan_is_pixels(oldx,app.fractal.cx,app.fractal.units_per_pixel,-dx)&&
                          pan_is_pixels(oldy,app.fractal.cy,app.fractal.units_per_pixel,dy);
            if(safe){if(!fractal_pan(dx,dy,&phase_x,&phase_y))return;}
            else dirty=1;
        }
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
            int y=24+i*38;if(i==sel)drect_border(5,y,DWIDTH-6,y+34,C_WHITE,1,C_BLACK);
            drect_border(10,y+7,17,y+14,eq[i].active?C_BLACK:C_WHITE,1,C_BLACK);
            dprint(23,y+5,INK,"%d %s",i+1,eq[i].active?"ON":"OFF");
            struct dwindow old=dwindow_set((struct dwindow){72,y+3,DWIDTH-9,y+31});
            dtext(73,y+7,INK,eq[i].formula);dwindow_set(old);
        }
        dtext(10,145,MUTED,"EXE: edit  |  Letter/digit/DEL: new");
        dtext(10,163,MUTED,"Math keys edit  |  F3 / OPTN: settings");draw_notice();
        footer("EDIT","ON/OFF","PARAM","EXAMPL","SAVE","PLOT");dupdate();key_event_t e=input_key();
        if(e.key==KEY_EXIT)return;
        /* SHIFT+UP/DOWN and SHIFT+F5 are editor actions. The corresponding
           unmodified keys remain list navigation and SAVE respectively. */
        EquationEntry entry=equation_entry(e);
        if((e.key==KEY_UP||e.key==KEY_DOWN||e.key==KEY_F5) && e.shift && entry==ENTRY_EDIT){
            if(editor_seed(eq[sel].formula,is3?"EDIT 3D EQUATION":"EDIT 2D EQUATION",is3?2:1,e,1)){
                eq[sel].active=1;clear_notice();
            }
        }
        else if(e.key==KEY_UP){sel=(sel+count-1)%count;clear_notice();}
        else if(e.key==KEY_DOWN){sel=(sel+1)%count;clear_notice();}
        else if(e.key==KEY_F1||e.key==KEY_EXE){if(editor(eq[sel].formula,is3?"EDIT 3D EQUATION":"EDIT 2D EQUATION",is3?2:1)){eq[sel].active=1;clear_notice();}}
        else if(e.key==KEY_F2){eq[sel].active^=1;clear_notice();}
        else if(e.key==KEY_F3||e.key==KEY_OPTN)settings();
        else if(e.key==KEY_F4){examples(&eq[sel],is3);clear_notice();}
        else if(e.key==KEY_F5)save_now();
        else if(e.key==KEY_F6){if(is3)plot3d();else plot2d();}
        else if(entry==ENTRY_NEW){
            char fresh[EXPR_MAX]="";
            if(editor_seed(fresh,is3?"NEW 3D EQUATION":"NEW 2D EQUATION",is3?2:1,e,1)) {
                strcpy(eq[sel].formula,fresh);eq[sel].active=1;clear_notice();
            }
        }
        else if(entry==ENTRY_EDIT){
            if(editor_seed(eq[sel].formula,is3?"EDIT 3D EQUATION":"EDIT 2D EQUATION",is3?2:1,e,1)){
                eq[sel].active=1;clear_notice();
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
#ifdef GL_DEV
        dclear(PALE);header("GRAPH LAB 1.20 DEV","CASIO fx-CG50");
#else
        dclear(PALE);header("GRAPH LAB 1.20","CASIO fx-CG50");
#endif
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
