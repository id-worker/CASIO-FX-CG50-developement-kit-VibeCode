/* Real target editor + gint keydev/getkey with synthetic key matrices.
   No Casio OS, physical keyboard or physical display is being emulated. */
#include "../src/state.h"
#include <gint/display.h>
#include <gint/keyboard.h>
#include <gint/drivers/keydev.h>
#include <gint/hardware.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
uint8_t target_stack[16384] __attribute__((aligned(16)));
uint32_t gint[HW_KEYS];
static unsigned char arena[256*1024];static unsigned arena_used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(arena_used+n>sizeof arena)return NULL;void *p=arena+arena_used;arena_used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int x){(void)x;__builtin_trap();for(;;){}}
static keydev_t dev;static uint8_t scan[12];
struct Event {int key,down;};
static struct Event const *script;static int script_size,script_pos;
keydev_t *keydev_std(void){return &dev;}
int keydown(int key){return keydev_keydown(&dev,key);}
void sleep(void){
    if(script_pos>=script_size)__builtin_trap();
    struct Event e=script[script_pos++];int row=e.key>>4,col=7-(e.key&7);
    if(e.down)scan[row]|=(uint8_t)(1u<<col);else scan[row]&=(uint8_t)~(1u<<col);
    keydev_process_state(&dev,scan);keydev_tick(&dev,8000);
}
void gint_osmenu(void){}
void gint_poweroff(bool force){(void)force;}
void dupdate(void){}
void dclear(color_t col){(void)col;}
void drect(int x,int y,int a,int b,int col){(void)x;(void)y;(void)a;(void)b;(void)col;}
void drect_border(int x,int y,int a,int b,int col,int w,int bc){(void)x;(void)y;(void)a;(void)b;(void)col;(void)w;(void)bc;}
void dline(int x,int y,int a,int b,int col){(void)x;(void)y;(void)a;(void)b;(void)col;}
void dtext(int x,int y,int col,char const *s){(void)x;(void)y;(void)col;(void)s;}
void dprint(int x,int y,int col,char const *s,...){(void)x;(void)y;(void)col;(void)s;}
void dsize(char const *s,font_t const *font,int *w,int *h){(void)font;if(w)*w=(int)strlen(s)*6;if(h)*h=10;}
struct dwindow dwindow={0,0,DWIDTH,DHEIGHT};
struct dwindow dwindow_set(struct dwindow n){struct dwindow old=dwindow;dwindow=n;return old;}
int state_validate(GraphState const *s){(void)s;return 1;}
int storage_save(GraphState const *s){(void)s;return 0;}
int graphlab_test_editor(char *destination,int mode);
#define TAP(k) {KEY_##k,1},{KEY_##k,0}
#define SHIFTED(k) TAP(SHIFT),TAP(k)
#define ALPHAKEY(k) TAP(ALPHA),TAP(k)
#define SEVEN_PAGES SHIFTED(F5),SHIFTED(F5),SHIFTED(F5),SHIFTED(F5),SHIFTED(F5),SHIFTED(F5),SHIFTED(F5)
#define NAME_SUMM SHIFTED(ALPHA),TAP(MUL),TAP(1),TAP(7),TAP(7),SHIFTED(ALPHA)
#define NAME_PROD SHIFTED(ALPHA),TAP(4),TAP(6),TAP(9),TAP(SIN),SHIFTED(ALPHA)
#define THREE_LEFT TAP(LEFT),TAP(LEFT),TAP(LEFT)
#define COUNT(a) ((int)(sizeof(a)/sizeof((a)[0])))
static void begin(struct Event const *e,int count){
    keydev_init(&dev);memset(scan,0,sizeof scan);gint[HWCALC]=HWCALC_FXCG50;
    script=e;script_size=count;script_pos=0;arena_used=0;
}
int main(void){
    /* Type into the term immediately; then replace both bounds and index.
       DEL at the start of an empty body must not erase its separator. */
    static const struct Event sum[]={SEVEN_PAGES,TAP(F1),TAP(DEL),TAP(XOT),
        TAP(UP),TAP(DEL),TAP(5),TAP(DOWN),TAP(DEL),TAP(1),
        SHIFTED(LEFT),TAP(DEL),ALPHAKEY(COMMA),SHIFTED(RIGHT),TAP(ADD),TAP(1),TAP(EXE)};
    char formula[EXPR_MAX]="";begin(sum,COUNT(sum));
    if(!graphlab_test_editor(formula,1))return 10;
    if(strcmp(formula,"summ(1,5,k,x+1)"))return 11;
    /* The matching product template begins in its term too. */
    static const struct Event prod[]={SEVEN_PAGES,TAP(F4),ALPHAKEY(8),
        TAP(RIGHT),TAP(DEL),ALPHAKEY(8), /* backspace after operator */
        TAP(UP),TAP(DEL),TAP(3),TAP(DOWN),TAP(DEL),TAP(2),TAP(EXE)};
    formula[0]=0;begin(prod,COUNT(prod));
    if(!graphlab_test_editor(formula,1))return 12;
    if(strcmp(formula,"prod(2,3,n,n)"))return 13;
    /* The documented x^n/n! keystrokes stay inside the sum's term. */
    static const struct Event series[]={SEVEN_PAGES,TAP(F1),TAP(XOT),TAP(POWER),
        ALPHAKEY(8),TAP(RIGHT),TAP(FRAC),ALPHAKEY(8),TAP(F3),TAP(EXE)};
    formula[0]=0;begin(series,COUNT(series));
    if(!graphlab_test_editor(formula,1))return 14;
    if(strcmp(formula,"summ(0,inf,n,x^(n)/(n!))"))return 15;
    /* Manually spell summ(), then type the term and set each empty field.
       These are real editor/keydev paths; EXIT prevents a hang on old builds. */
    static const struct Event manual_empty[]={
        SHIFTED(ALPHA),TAP(MUL),TAP(1),TAP(7),TAP(7),SHIFTED(ALPHA), /* summ */
        TAP(LEFTP),TAP(RIGHTP),TAP(XOT),TAP(UP),TAP(2),TAP(DOWN),TAP(1),
        SHIFTED(LEFT),ALPHAKEY(8),TAP(EXE),TAP(EXIT)
    };
    formula[0]=0;begin(manual_empty,COUNT(manual_empty));
    if(!graphlab_test_editor(formula,1))return 16;
    if(strcmp(formula,"summ(1,2,n,x)"))return 17;
    static const struct Event manual_product_empty[]={NAME_PROD,TAP(LEFTP),TAP(RIGHTP),ALPHAKEY(8),
        TAP(UP),TAP(3),TAP(DOWN),TAP(2),SHIFTED(LEFT),ALPHAKEY(8),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(manual_product_empty,COUNT(manual_product_empty));
    if(!graphlab_test_editor(formula,1))return 18;
    if(strcmp(formula,"prod(2,3,n,n)"))return 19;
    /* Standard raw syntax, including typing the closing parenthesis. */
    static const struct Event typed_sum[]={NAME_SUMM,TAP(LEFTP),TAP(0),TAP(COMMA),TAP(1),TAP(COMMA),
        ALPHAKEY(8),TAP(COMMA),TAP(1),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(typed_sum,COUNT(typed_sum));
    if(!graphlab_test_editor(formula,1))return 20;
    if(strcmp(formula,"summ(0,1,n,1)"))return 21;
    static const struct Event typed_prod[]={NAME_PROD,TAP(LEFTP),TAP(1),TAP(COMMA),TAP(5),TAP(COMMA),
        ALPHAKEY(8),TAP(COMMA),ALPHAKEY(8),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(typed_prod,COUNT(typed_prod));
    if(!graphlab_test_editor(formula,1))return 22;
    if(strcmp(formula,"prod(1,5,n,n)"))return 23;
    /* Typed binary function and parentheses inside bounds; the comma in min
       belongs to min, not to the surrounding sum. */
    static const struct Event typed_nested_function[]={NAME_SUMM,TAP(LEFTP),TAP(0),TAP(COMMA),
        SHIFTED(ALPHA),TAP(7),TAP(LEFTP),TAP(8),SHIFTED(ALPHA), /* min */
        TAP(LEFTP),TAP(1),TAP(COMMA),TAP(2),TAP(RIGHTP),TAP(COMMA),
        ALPHAKEY(8),TAP(COMMA),TAP(LEFTP),ALPHAKEY(8),TAP(ADD),TAP(1),TAP(RIGHTP),
        TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(typed_nested_function,COUNT(typed_nested_function));
    if(!graphlab_test_editor(formula,1))return 24;
    if(strcmp(formula,"summ(0,min(1,2),n,(n+1))"))return 25;
    /* Enter a template after an existing coefficient and traverse LEFT all
       the way back, then change 0.5 into 0.8 without damaging SUM/PROD. */
    static const struct Event left_sum[]={TAP(0),TAP(DOT),TAP(5),TAP(MUL),SEVEN_PAGES,TAP(F1),TAP(XOT),
        THREE_LEFT,TAP(DEL),TAP(8),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(left_sum,COUNT(left_sum));
    if(!graphlab_test_editor(formula,1))return 26;
    if(strcmp(formula,"0.8*summ(0,inf,n,x)"))return 27;
    static const struct Event left_prod[]={TAP(0),TAP(DOT),TAP(5),TAP(MUL),SEVEN_PAGES,TAP(F4),ALPHAKEY(8),
        THREE_LEFT,TAP(DEL),TAP(8),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(left_prod,COUNT(left_prod));
    if(!graphlab_test_editor(formula,1))return 28;
    if(strcmp(formula,"0.8*prod(1,5,n,n)"))return 29;
    /* Capital letters remain accepted by the editor and the parser. */
    static const struct Event uppercase[]={TAP(F5),TAP(F5),TAP(MUL),TAP(1),TAP(7),TAP(7),TAP(F5),
        TAP(LEFTP),TAP(RIGHTP),TAP(XOT),TAP(UP),TAP(2),TAP(DOWN),TAP(0),
        SHIFTED(LEFT),ALPHAKEY(8),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(uppercase,COUNT(uppercase));
    if(!graphlab_test_editor(formula,1))return 30;
    if(strcmp(formula,"SUMM(0,2,n,x)"))return 31;
    /* A second hand-written operator remains nested in the first term. */
    static const struct Event nested_ops[]={NAME_SUMM,TAP(LEFTP),TAP(0),TAP(COMMA),TAP(2),TAP(COMMA),
        ALPHAKEY(8),TAP(COMMA),NAME_PROD,TAP(LEFTP),TAP(1),TAP(COMMA),TAP(3),TAP(COMMA),
        ALPHAKEY(COMMA),TAP(COMMA),ALPHAKEY(COMMA),TAP(RIGHTP),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(nested_ops,COUNT(nested_ops));
    if(!graphlab_test_editor(formula,1))return 32;
    if(strcmp(formula,"summ(0,2,n,prod(1,3,k,k))"))return 33;
    /* A numeric coefficient may directly precede the manually typed name. */
    static const struct Event implicit[]={TAP(2),NAME_PROD,TAP(LEFTP),TAP(1),TAP(COMMA),TAP(2),TAP(COMMA),
        ALPHAKEY(8),TAP(COMMA),ALPHAKEY(8),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(implicit,COUNT(implicit));
    if(!graphlab_test_editor(formula,1))return 34;
    if(strcmp(formula,"2prod(1,2,n,n)"))return 35;
    /* Escaping from the upper limit itself must also reach a prior term. */
    static const struct Event upper_escape[]={TAP(0),TAP(DOT),TAP(5),TAP(MUL),SEVEN_PAGES,TAP(F1),TAP(XOT),
        TAP(UP),TAP(LEFT),TAP(LEFT),TAP(LEFT),TAP(DEL),TAP(8),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(upper_escape,COUNT(upper_escape));
    if(!graphlab_test_editor(formula,1))return 36;
    if(strcmp(formula,"0.8*summ(0,inf,n,x)"))return 37;
    static const struct Event return_term[]={TAP(0),TAP(DOT),TAP(5),TAP(MUL),SEVEN_PAGES,TAP(F1),TAP(XOT),
        THREE_LEFT,TAP(RIGHT),TAP(RIGHT),TAP(RIGHT),TAP(ADD),TAP(1),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(return_term,COUNT(return_term));
    if(!graphlab_test_editor(formula,1))return 38;
    if(strcmp(formula,"0.5*summ(0,inf,n,x+1)"))return 39;
    /* Reported regressions: physical ')' must not duplicate existing
       template/fraction closers or break a function in either direction. */
    static const struct Event frac_in_sin[]={TAP(SIN),TAP(1),TAP(FRAC),TAP(2),
        TAP(RIGHTP),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(frac_in_sin,COUNT(frac_in_sin));
    if(!graphlab_test_editor(formula,1))return 40;
    if(strcmp(formula,"sin(1/(2))"))return 41;
    static const struct Event sin_in_frac[]={TAP(1),TAP(FRAC),TAP(SIN),TAP(XOT),
        TAP(RIGHTP),TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(sin_in_frac,COUNT(sin_in_frac));
    if(!graphlab_test_editor(formula,1))return 42;
    if(strcmp(formula,"1/(sin(x))"))return 43;
    /* A fraction at an empty position starts in its numerator. */
    static const struct Event blank_frac[]={TAP(FRAC),TAP(1),TAP(DOWN),TAP(2),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(blank_frac,COUNT(blank_frac));
    if(!graphlab_test_editor(formula,1))return 44;
    if(strcmp(formula,"(1)/(2)"))return 45;
    static const struct Event fraction_fields[]={TAP(1),TAP(FRAC),TAP(2),TAP(UP),TAP(DEL),TAP(3),
        TAP(DOWN),TAP(DEL),TAP(4),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(fraction_fields,COUNT(fraction_fields));
    if(!graphlab_test_editor(formula,1))return 46;
    if(strcmp(formula,"(3)/(4)"))return 47;
    /* Fraction has UP/DOWN priority in SUM; SHIFT accesses SUM limits. */
    static const struct Event sum_frac[]={SEVEN_PAGES,TAP(F1),TAP(1),TAP(FRAC),TAP(2),
        TAP(UP),TAP(DEL),TAP(3),TAP(DOWN),TAP(DEL),TAP(4),SHIFTED(UP),TAP(DEL),TAP(5),
        SHIFTED(DOWN),TAP(DEL),TAP(1),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(sum_frac,COUNT(sum_frac));
    if(!graphlab_test_editor(formula,1))return 48;
    if(strcmp(formula,"summ(1,5,n,(3)/(4))"))return 49;
    static const struct Event frac_del[]={TAP(1),TAP(FRAC),TAP(2),TAP(RIGHT),TAP(DEL),TAP(DEL),
        TAP(ADD),TAP(3),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(frac_del,COUNT(frac_del));
    if(!graphlab_test_editor(formula,1))return 50;
    if(strcmp(formula,"1+3"))return 51;
    static const struct Event frac_empty_del[]={TAP(FRAC),TAP(DEL),TAP(3),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(frac_empty_del,COUNT(frac_empty_del));
    if(!graphlab_test_editor(formula,1))return 52;
    if(strcmp(formula,"3"))return 53;
    static const struct Event manual_group[]={SEVEN_PAGES,TAP(F1),TAP(LEFTP),TAP(XOT),
        TAP(RIGHTP),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(manual_group,COUNT(manual_group));
    if(!graphlab_test_editor(formula,1))return 54;
    if(strcmp(formula,"summ(0,inf,n,(x))"))return 55;
    static const struct Event del_function_frac[]={TAP(1),TAP(FRAC),TAP(SIN),TAP(XOT),TAP(RIGHTP),
        TAP(RIGHTP),TAP(DEL),TAP(DEL),TAP(DEL),TAP(ADD),TAP(3),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(del_function_frac,COUNT(del_function_frac));
    if(!graphlab_test_editor(formula,1))return 56;
    if(strcmp(formula,"1+3"))return 57;
    static const struct Event old_frac[]={TAP(LEFT),TAP(ADD),TAP(1),TAP(EXE),TAP(EXIT)};
    strcpy(formula,"1/x");begin(old_frac,COUNT(old_frac));
    if(!graphlab_test_editor(formula,1))return 58;
    if(strcmp(formula,"1/(x+1)"))return 59;
    static const struct Event num_plus[]={TAP(1),TAP(FRAC),TAP(2),TAP(UP),TAP(ADD),TAP(1),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(num_plus,COUNT(num_plus));
    if(!graphlab_test_editor(formula,1))return 60;
    if(strcmp(formula,"(1+1)/(2)"))return 61;
    static const struct Event num_left[]={TAP(1),TAP(FRAC),TAP(2),TAP(LEFT),TAP(LEFT),TAP(ADD),TAP(1),TAP(EXE),TAP(EXIT)};
    formula[0]=0;begin(num_left,COUNT(num_left));
    if(!graphlab_test_editor(formula,1))return 62;
    if(strcmp(formula,"(1+1)/(2)"))return 63;
    return 0;
}
