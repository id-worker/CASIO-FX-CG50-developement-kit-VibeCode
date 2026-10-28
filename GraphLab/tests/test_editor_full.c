/* Actual GraphLab editor, with scripted physical keys and every intermediate
   source/caret state observable. Not a calculator hardware emulation. */
#include "../src/state.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
int graphlab_test_editor(char *destination,int mode);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.shift=1}
#define A(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.alpha=1}
#define N(a) (sizeof(a)/sizeof((a)[0]))
#define PAGES7 S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5)
static char snapshots[256][EXPR_MAX],errors[256][90];
static int positions[256],count;
void graphlab_editor_trace(char const *s,int cursor,char const *error)
{
    if(count>=256){fprintf(stderr,"Too many trace frames\n");abort();}
    snprintf(snapshots[count],EXPR_MAX,"%s",s);snprintf(errors[count],90,"%s",error);
    positions[count++]=cursor;
}
static int saw(const char *s,int pos)
{
    for(int i=0;i<count;i++)if(!strcmp(snapshots[i],s) && (pos<0 || positions[i]==pos))return 1;
    return 0;
}
static void run_initial(const char *name,const char *initial,const key_event_t *keys,size_t n,const char *expected)
{
    char dest[EXPR_MAX];snprintf(dest,sizeof dest,"%s",initial);count=0;host_keys(keys,n);
    int ok=graphlab_test_editor(dest,1);
    if(!ok || strcmp(dest,expected)){
        fprintf(stderr,"%s: expected '%s', got %d '%s'\n",name,expected,ok,dest);
        for(int i=0;i<count;i++)fprintf(stderr,"%.*s|%s %s\n",positions[i],snapshots[i],snapshots[i]+positions[i],errors[i]);
        abort();
    }
    assert(host_vram_guards_ok());
}
static void run(const char *name,const key_event_t *keys,size_t n,const char *expected)
{
    run_initial(name,"",keys,n,expected);
}
int main(void)
{
    key_event_t manual_group[]={PAGES7,K(F1),K(LEFTP),K(XOT),K(RIGHTP),K(EXE),K(EXIT)};
    run("literal group within SUM",manual_group,N(manual_group),"summ(0,inf,n,(x))");
    assert(saw("summ(0,inf,n,()",14)); /* exactly one manually typed '(' */
    assert(saw("summ(0,inf,n,(x)",15)); /* outer SUM ')' cannot be stolen */
    key_event_t plain[]={K(LEFTP),K(LEFTP),K(1),K(ADD),K(2),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("two single parentheses",plain,N(plain),"((1+2))");
    assert(saw("(",1) && saw("((",2) && saw("((1+2)",6));
    key_event_t sin_fraction[]={K(SIN),K(1),K(FRAC),K(2),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("fraction inside sin",sin_fraction,N(sin_fraction),"sin(1/(2))");
    key_event_t fraction_sin[]={K(1),K(FRAC),K(SIN),K(XOT),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("sin inside fraction",fraction_sin,N(fraction_sin),"1/(sin(x))");
    key_event_t documented_fraction_sin[]={K(1),K(FRAC),K(SIN),K(XOT),K(RIGHTP),K(RIGHTP),K(ADD),K(1),K(EXE),K(EXIT)};
    run("documented function denominator plus outside",documented_fraction_sin,N(documented_fraction_sin),"1/(sin(x))+1");
    key_event_t nested_functions[]={K(SIN),K(1),K(FRAC),K(COS),K(2),
        K(RIGHTP),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("function in fraction in function",nested_functions,N(nested_functions),"sin(1/(cos(2)))");
    key_event_t sqrt_fraction[]={S(SQUARE),K(FRAC),K(1),K(DOWN),K(2),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("fraction inside sqrt",sqrt_fraction,N(sqrt_fraction),"sqrt((1)/(2))");
    key_event_t function_manual_group[]={K(SIN),K(LEFTP),K(XOT),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("manual group inside function",function_manual_group,N(function_manual_group),"sin((x))");
    assert(saw("sin(()",5)); /* one manual '(' and the original sin ')' */
    key_event_t after[]={K(1),K(FRAC),K(2),K(RIGHT),K(ADD),K(SIN),K(XOT),K(RIGHTP),K(EXE),K(EXIT)};
    run("function after fraction",after,N(after),"1/(2)+sin(x)");
    key_event_t empty_frac[]={K(FRAC),K(1),K(DOWN),K(2),K(EXE),K(EXIT)};
    run("fraction at empty position",empty_frac,N(empty_frac),"(1)/(2)");
    assert(saw("()/()",1)&&saw("(1)/()",5));
    key_event_t empty_in_sin[]={K(SIN),K(FRAC),K(1),K(DOWN),K(2),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("empty fraction inside function",empty_in_sin,N(empty_in_sin),"sin((1)/(2))");
    key_event_t fields[]={K(1),K(FRAC),K(2),K(UP),K(DEL),K(3),K(DOWN),K(DEL),K(4),K(EXE),K(EXIT)};
    run("fraction field edits",fields,N(fields),"(3)/(4)");
    assert(saw("(1)/(2)",2) && saw("(3)/(2)",6));
    key_event_t numerator_plus[]={K(1),K(FRAC),K(2),K(UP),K(ADD),K(1),K(EXE),K(EXIT)};
    run("plus stays in numerator after UP",numerator_plus,N(numerator_plus),"(1+1)/(2)");
    key_event_t numerator_left[]={K(1),K(FRAC),K(2),K(LEFT),K(LEFT),K(ADD),K(1),K(EXE),K(EXIT)};
    run("plus stays in numerator after LEFT",numerator_left,N(numerator_left),"(1+1)/(2)");
    key_event_t numerator_mult[]={K(2),K(MUL),K(3),K(FRAC),K(4),K(UP),K(ADD),K(1),K(EXE),K(EXIT)};
    run("whole multiplication remains numerator",numerator_mult,N(numerator_mult),"(2*3+1)/(4)");
    key_event_t numerator_outer[]={K(1),K(ADD),K(2),K(FRAC),K(3),K(UP),K(ADD),K(4),K(EXE),K(EXIT)};
    run("numerator grouping does not swallow outer addition",numerator_outer,N(numerator_outer),"1+(2+4)/(3)");
    key_event_t numerator_function[]={K(SIN),K(XOT),K(RIGHTP),K(FRAC),K(2),K(UP),K(ADD),K(1),K(EXE),K(EXIT)};
    run("function numerator stays intact",numerator_function,N(numerator_function),"(sin(x)+1)/(2)");
    key_event_t nested_sum[]={PAGES7,K(F1),K(1),K(FRAC),K(2),K(UP),K(DEL),K(3),K(DOWN),K(DEL),K(4),
        S(UP),K(DEL),K(5),S(DOWN),K(DEL),K(1),K(EXE),K(EXIT)};
    run("fraction first and SHIFT limits in SUM",nested_sum,N(nested_sum),"summ(1,5,n,(3)/(4))");
    key_event_t delete_frac[]={K(1),K(FRAC),K(2),K(RIGHT),K(DEL),K(DEL),K(ADD),K(3),K(EXE),K(EXIT)};
    run("backspace hidden fraction boundaries",delete_frac,N(delete_frac),"1+3");
    assert(saw("1/()",3) && saw("1",1));
    key_event_t delete_function_frac[]={K(1),K(FRAC),K(SIN),K(XOT),K(RIGHTP),K(RIGHTP),
        K(DEL),K(DEL),K(DEL),K(ADD),K(3),K(EXE),K(EXIT)};
    run("backspace function nested in fraction",delete_function_frac,N(delete_function_frac),"1+3");
    assert(saw("1/(sin())",7) && saw("1/()",3) && saw("1",1));
    key_event_t delete_empty[]={K(FRAC),K(DEL),K(3),K(EXE),K(EXIT)};
    run("delete blank fraction",delete_empty,N(delete_empty),"3");
    key_event_t grouped_den[]={K(1),K(FRAC),K(LEFTP),K(XOT),K(ADD),K(1),K(RIGHTP),K(RIGHT),K(ADD),K(2),K(EXE),K(EXIT)};
    run("manual grouping in denominator",grouped_den,N(grouped_den),"1/((x+1))+2");
    key_event_t min_frac[]={S(F5),S(F5),S(F5),K(F2),K(FRAC),K(1),K(DOWN),K(2),K(RIGHT),K(COMMA),
        K(SIN),K(XOT),K(RIGHTP),K(RIGHTP),K(EXE),K(EXIT)};
    run("binary function with fractional arg",min_frac,N(min_frac),"min((1)/(2),sin(x))");
    key_event_t manual_min_in_sum[]={PAGES7,K(F1),K(UP),K(DEL),
        S(ALPHA),K(7),K(LEFTP),K(8),S(ALPHA),K(LEFTP), /* min( */
        K(FRAC),K(1),K(DOWN),K(2),K(RIGHT),K(COMMA),K(3),K(RIGHTP),
        S(RIGHT),K(XOT),K(EXE),K(EXIT)};
    run("handwritten function and fraction in SUM limit",manual_min_in_sum,N(manual_min_in_sum),
        "summ(0,min((1)/(2),3),n,x)");
    key_event_t nested_frac[]={K(1),K(FRAC),K(2),K(FRAC),K(3),K(UP),K(DEL),K(4),K(DOWN),K(DEL),K(5),K(EXE),K(EXIT)};
    run("innermost nested fraction",nested_frac,N(nested_frac),"1/((4)/(5))");
    key_event_t ascend_nested[]={K(1),K(FRAC),K(2),K(FRAC),K(3),K(UP),K(UP),K(ADD),K(1),K(EXE),K(EXIT)};
    run("UP climbs nested fractions",ascend_nested,N(ascend_nested),"(1+1)/((2)/(3))");
    key_event_t accept[]={K(EXE),K(EXIT)};
    run_initial("legacy bare divisors","sin(1/2)+1/sin(1/x)",accept,N(accept),"sin(1/(2))+1/(sin(1/(x)))");
    key_event_t legacy_edit[]={K(LEFT),K(ADD),K(1),K(EXE),K(EXIT)};
    run_initial("edit denominator of legacy bare divisor","1/x",legacy_edit,N(legacy_edit),"1/(x+1)");
    key_event_t prevent_jump[]={PAGES7,K(F1),K(LEFTP),K(XOT),K(RIGHT),K(RIGHTP),K(EXE),K(EXIT)};
    run("do not cross an unclosed manual group",prevent_jump,N(prevent_jump),"summ(0,inf,n,(x))");
    int warned=0;for(int i=0;i<count;i++)if(strstr(errors[i],"Close the inner group"))warned=1;
    assert(warned);
    /* Moving into the numerator must not change evaluation by itself. */
    static const char *const formulas[]={"1/2","x/2","2*3/4","sin(x)/cos(x)",
        "(x+1)/2","1+2/3","sqrt(x)/(x+1)","summ(1,3,n,n)/2"};
    static Parsed parsed_test;static Program before,after_prog;char err[90],dest[EXPR_MAX];
    key_event_t up_existing[]={K(LEFT),K(UP),K(EXE),K(EXIT)};
    for(unsigned i=0;i<N(formulas);i++){
        assert(expr_parse(&parsed_test,formulas[i],0)&&expr_compile(&parsed_test,&before,err,sizeof err));
        snprintf(dest,sizeof dest,"%s",formulas[i]);count=0;host_keys(up_existing,N(up_existing));
        assert(graphlab_test_editor(dest,1));
        assert(expr_parse(&parsed_test,dest,0)&&expr_compile(&parsed_test,&after_prog,err,sizeof err));
        assert(!memcmp(&before,&after_prog,sizeof before));
    }
    puts("Actual editor: one-at-a-time parentheses, fractions, functions, nesting, SUM limits and DEL passed.");
}
