/* Regression for DEL, function tokens and fraction exit. Runs the actual
   editor on host with scripted keys; this is NOT Casio OS/hardware. */
#define _POSIX_C_SOURCE 200809L
#include "../src/state.h"
#include "../src/storage.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int graphlab_test_editor(char *destination,int mode);
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.shift=1}
#define N(a) (sizeof(a)/sizeof((a)[0]))
#define SUM_PAGE S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5)
static char states[160][EXPR_MAX];
static int cursors[160],trace_n;
void graphlab_editor_trace(char const *s,int pos,char const *error)
{
    (void)error;
    if(trace_n>=160){fprintf(stderr,"editor trace overflow\n");abort();}
    snprintf(states[trace_n],EXPR_MAX,"%s",s);cursors[trace_n++]=pos;
}
static void run_initial(char const *name,int mode,char const *initial,
    key_event_t const *keys,size_t count,char const *want)
{
    char dest[EXPR_MAX];snprintf(dest,sizeof dest,"%s",initial);
    trace_n=0;host_keys(keys,count);int ok=graphlab_test_editor(dest,mode);
    if(!ok || strcmp(dest,want)){
        fprintf(stderr,"%s (mode %d): expected %s; got %d %s\n",name,mode,want,ok,dest);
        for(int j=0;j<trace_n;j++)fprintf(stderr,"%.*s|%s\n",cursors[j],states[j],states[j]+cursors[j]);
        abort();
    }
    assert(host_vram_guards_ok());
}
static void erase_all(int mode,char const *formula)
{
    key_event_t keys[130];
    for(int i=0;i<128;i++)keys[i]=(key_event_t)K(DEL);
    keys[128]=(key_event_t)K(7);keys[129]=(key_event_t)K(EXE);
    run_initial("repeated DEL",mode,formula,keys,N(keys),"7");
    if(strcmp(states[128],"") || cursors[128]!=0){
        fprintf(stderr,"DEL stuck on '%s': '%s' @%d\n",formula,states[128],cursors[128]);abort();
    }
}
int main(void)
{
    static char const *const expressions[]={
        "x","sin(x)","cos(x)","tan(x)","asin(x)","acos(x)","atan(x)",
        "ln(x)","log(x)","sqrt(x)","abs(x)","exp(x)","floor(x)","ceil(x)",
        "min(x,2)","max(x,2)","atan2(x,2)","re(x)","im(x)","conj(x)",
        "pi","e","inf","(x)","((x))","sin((x))","x^(2)","x^((1)/(2))",
        "(1)/(2)","(x+1)/(sin(x))","1+((x)/(2))","((1)/(2))/((3)/(4))",
        "1/(min(2,3))","min((1)/(2),(3)/(4))","summ(1,3,n,n)","prod(1,3,n,n)",
        "summ(0,inf,n,((x)/(n)))","summ(1,3,n,summ(1,3,k,k))",
        "sin(summ(1,3,n,n))","summ(1,3,n,sin(n))","(summ(1,3,n,n))/(2)",
        "(2)/(summ(1,3,n,n))","summ(1,3,n,(1)/(min(n,2)))",
        "x<=1","x>=1","x^2+y^2=1","1+2*3","1/(x-1)","(1/(x-1))"
    };
    for(int mode=1;mode<=2;mode++)for(size_t i=0;i<N(expressions);i++)erase_all(mode,expressions[i]);
    static char const *const calculator[]={"1/2+sqrt(9)","sin((1)/(2))","min(1,2)",
        "summ(1,3,n,((1)/(n)))","prod(1,4,n,n)","(1)/(summ(1,3,n,n))"};
    for(size_t i=0;i<N(calculator);i++)erase_all(4,calculator[i]);

    key_event_t unary[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(EXE)};
    run_initial("unwrap unary and preserve grouping",1,"2*sin(x+1)",unary,N(unary),"2*(x+1)");
    assert(!strcmp(states[5],"2*(x+1)") && cursors[5]==3);
    key_event_t sum_unwrap[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(EXE)};
    run_initial("unwrap sum preserving term",1,"2*summ(1,3,n,x+1)",sum_unwrap,N(sum_unwrap),"2*(x+1)");
    key_event_t product_unwrap[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(EXE)};
    run_initial("unwrap product preserving term",1,"2*prod(1,3,n,x+1)",product_unwrap,N(product_unwrap),"2*(x+1)");
    key_event_t binary[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(7),K(EXE)};
    run_initial("remove binary function without losing either argument silently",4,"min(1,2)",binary,N(binary),"7");
    assert(!strcmp(states[5],"min(1,2)") && cursors[5]==7);
    key_event_t left_denom[]={K(LEFT),K(LEFT),K(DEL),K(ADD),K(3),K(EXE)};
    run_initial("DEL at denominator start moves to grouped numerator",4,"1/(2)",left_denom,N(left_denom),"(1+3)/(2)");
    key_event_t frac_func[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(EXE)};
    run_initial("DEL at fraction numerator can remove surrounding function",4,"sin((1)/(2))",frac_func,N(frac_func),"((1)/(2))");
    key_event_t function_left[]={K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(EXE)};
    run_initial("arrow skips letters of function token",4,"sin((1)/(2))",function_left,N(function_left),"sin((1)/(2))");
    assert(cursors[5]==5 && cursors[6]==0);
    key_event_t sum_exit[]={SUM_PAGE,K(F1),K(LEFTP),K(1),K(FRAC),K(2),K(RIGHT),K(RIGHTP),
        K(RIGHT),K(ADD),K(1),K(EXE)};
    run_initial("leave sum with fraction in manual parentheses",4,"",sum_exit,N(sum_exit),"summ(0,inf,n,(1/(2)))+1");
    key_event_t finite_sum[]={SUM_PAGE,K(F1),K(1),K(FRAC),K(2),S(UP),K(DEL),K(3),S(RIGHT),K(RIGHT),K(ADD),K(1),K(EXE)};
    run_initial("finite sum with fraction: edit limit, return and exit",4,"",finite_sum,N(finite_sum),"summ(0,3,n,1/(2))+1");
    key_event_t clear[]={S(DEL),K(3),K(EXE)};
    run_initial("SHIFT+DEL clears a nested sum",4,"summ(1,3,n,((1)/(n)))",clear,N(clear),"3");

    /* Real application paths, not merely a direct call to the editor. */
    char dir[]="/tmp/gl-editor-audit-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.curve[0].active=1;
    strcpy(s.curve[0].formula,"summ(1,3,n,((x)/(n)))");assert(storage_save(&s)==0);
    key_event_t ui2d[]={K(F1),K(EXE),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),
        K(XOT),K(EXE),K(F5),K(F6),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    host_keys(ui2d,N(ui2d));trace_n=0;assert(graphlab_main()==0 && host_keys_used()==N(ui2d));
    assert(storage_load(&s)==1 && !strcmp(s.curve[0].formula,"x"));assert(host_calculation_count()==1);
    key_event_t uicalc[]={K(VARS),K(EXE),S(DEL),K(SIN),K(1),K(FRAC),K(2),K(RIGHT),K(RIGHT),
        K(ADD),K(1),K(EXE),K(F6),K(F6)};
    host_keys(uicalc,N(uicalc));trace_n=0;assert(graphlab_main()==0 && host_keys_used()==N(uicalc));
    assert(host_text_seen("Result: 1.4794255"));assert(host_vram_guards_ok());
    key_event_t uicalc_sum[]={K(VARS),K(EXE),S(DEL),SUM_PAGE,K(F1),K(1),K(FRAC),K(2),
        S(UP),K(DEL),K(3),S(RIGHT),K(RIGHT),K(ADD),K(1),K(EXE),K(EXE),
        K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(7),K(EXE),K(F6),K(F6)};
    host_keys(uicalc_sum,N(uicalc_sum));trace_n=0;assert(graphlab_main()==0 && host_keys_used()==N(uicalc_sum));
    assert(host_text_seen("Result: 3") && host_text_seen("Result: 7") && host_vram_guards_ok());
    printf("Editor DEL: %zu formulas in 2D and 3D, %zu calculator formulas, SUM/PROD, fractions, functions and app UI passed.\n",N(expressions),N(calculator));
    return 0;
}
