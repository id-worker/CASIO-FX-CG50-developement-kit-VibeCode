#include "../src/layout.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed p;
static void test_carets(const char *s) {
    if(!expr_parse(&p,s,1) || !layout_measure(&p)) {fprintf(stderr,"failed layout %s (%s)\n",s,p.message);abort();}
    for(int i=0;i<=(int)strlen(s);i++) {
        FormulaCaret c=layout_draw(&p,i,0,0,C_BLACK,C_BLUE,1);
        if(!c.found) {fprintf(stderr,"missing caret at %d in '%s'\n",i,s);abort();}
        assert(c.x>=0 && c.y>=0 && c.x<=p.nodes[p.root].width+10 && c.y<=p.nodes[p.root].height+10);
    }
}
int main(void) {
    const char *cases[]={"", "1/", "1/2", "x+1", "(x+1)/(y-2)","x^2+y^2=25", "y<x","y<=x^2","y>=sqrt(x)","y>", "sqrt(y)","sqrt(","-1/(2+x)","sin(x)/cos(y)","min(x,y)","z^2+c","1/(2/(3+4))","  x / ( y + 1 ) ","summ(0,inf,n,x^n/n!)","summ(0,inf,n,)","summ(0,3,k,k!)","summ(","summ(0,inf,n,summ(0,2,k,k))","prod(1,5,n,n)","prod(1,5,k,)","summ(0,3,n,prod(1,2,k,k+n))"};
    for(unsigned i=0;i<sizeof cases/sizeof *cases;i++)test_carets(cases[i]);
    /* Incomplete manual brackets/functions must not draw an untyped closer
       or a fabricated binary-function comma. */
    host_keys(NULL,0);assert(expr_parse(&p,"(x",1)&&layout_measure(&p));
    layout_draw(&p,2,0,0,C_BLACK,C_BLUE,0);assert(!host_text_seen(")"));
    host_keys(NULL,0);assert(expr_parse(&p,"min(1",1)&&layout_measure(&p));
    layout_draw(&p,5,0,0,C_BLACK,C_BLUE,0);assert(!host_text_seen(",") && !host_text_seen(")"));
    assert(expr_parse(&p,"1/2",1) && layout_measure(&p));
    FormulaCaret a=layout_draw(&p,0,10,10,C_BLACK,C_BLUE,1),b=layout_draw(&p,2,10,10,C_BLACK,C_BLUE,1);
    assert(a.y<b.y); /* The denominator and its caret are below the numerator. */
    assert(layout_fraction_field(&p,1,NULL)==0 && layout_fraction_field(&p,2,NULL)==1);
    assert(layout_fraction_step(&p,1,1)==3 && layout_fraction_step(&p,2,-1)==1);
    assert(expr_parse(&p,"1/(2/(3))",1) && layout_measure(&p));
    assert(layout_fraction_step(&p,7,-1)==4); /* inner denominator to numerator */
    assert(layout_fraction_step(&p,4,-1)==1); /* then out to outer numerator */
    assert(layout_fraction_step(&p,1,1)==8); /* outer numerator to denominator */
    assert(expr_parse(&p,"sin((1)/(2))",1) && layout_measure(&p));
    assert(layout_cursor_step(&p,6,1)==9 && layout_cursor_step(&p,9,-1)==6); /* no duplicate hidden numerator stop */
    assert(expr_parse(&p,"1/2",1) && layout_measure(&p));
    dclear(C_WHITE);layout_draw(&p,-1,10,10,C_BLACK,C_BLUE,0);
    assert(dgetpixel(12,22)==C_BLACK); /* actual horizontal fraction line */
    assert(expr_parse(&p,"sqrt(x)+inf",1) && layout_measure(&p));
    assert(layout_cursor_step(&p,0,1)==5);
    assert(layout_cursor_step(&p,5,-1)==0);
    assert(layout_cursor_step(&p,8,1)==11);
    assert(layout_cursor_step(&p,11,-1)==8);
    const char *all_functions[]={"sin(x)","cos(x)","tan(x)","asin(x)","acos(x)","atan(x)",
      "sqrt(x)","abs(x)","ln(x)","log(x)","exp(x)","floor(x)","ceil(x)","re(x)","im(x)",
      "conj(x)","min(x,y)","max(x,y)","atan2(x,y)"};
    for(unsigned i=0;i<sizeof all_functions/sizeof *all_functions;i++)test_carets(all_functions[i]);
    assert(expr_parse(&p,"x^(n)+2",1) && layout_measure(&p));
    assert(layout_cursor_step(&p,1,1)==3 && layout_cursor_step(&p,3,-1)==1);
    FormulaCaret exponent=layout_draw(&p,4,0,0,C_BLACK,C_BLUE,1);
    FormulaCaret after=layout_draw(&p,5,0,0,C_BLACK,C_BLUE,1);
    assert(exponent.y<after.y); /* visible transition when leaving the exponent */
    assert(expr_parse(&p,"1/(n!)+2",1) && layout_measure(&p));
    FormulaCaret denom=layout_draw(&p,5,0,0,C_BLACK,C_BLUE,1);
    after=layout_draw(&p,6,0,0,C_BLACK,C_BLUE,1);
    assert(denom.y!=after.y); /* leaving denominator is not an invisible stop */
    assert(expr_parse(&p,"summ(0,inf,n,x^n/n!)",1) && layout_measure(&p));
    assert(layout_cursor_step(&p,0,1)==13); /* enter at the term */
    FormulaCaret low=layout_draw(&p,5,10,10,C_BLACK,C_BLUE,1);
    FormulaCaret upper=layout_draw(&p,7,10,10,C_BLACK,C_BLUE,1);
    assert(low.y>upper.y); /* bounds have the expected mathematical placement */
    assert(expr_parse(&p,"summ(0,inf,n,)",1) && layout_measure(&p));
    assert(layout_sum_field(&p,6,NULL)==0 && layout_sum_step(&p,6,-1)==10);
    assert(layout_sum_field(&p,10,NULL)==1 && layout_delete_start(&p,10)==7);
    assert(layout_sum_step(&p,10,1)==6 && layout_sum_field(&p,12,NULL)==2);
    assert(layout_sum_field(&p,13,NULL)==3 && layout_sum_step(&p,13,-1)==10);
    assert(layout_sum_step(&p,13,1)==6);
    assert(layout_cursor_step(&p,6,1)==13 && layout_cursor_step(&p,13,-1)==0);
    assert(layout_cursor_step(&p,5,-1)==12 && layout_cursor_step(&p,12,1)==5);
    assert(layout_cursor_step(&p,13,1)==14);
    assert(layout_delete_start(&p,5)==5 && layout_delete_start(&p,7)==7 &&
           layout_delete_start(&p,11)==11 && layout_delete_start(&p,13)==13);
    assert(expr_parse(&p,"prod(1,5,n,)",1) && layout_measure(&p));
    int prodbody=(int)strlen(p.src)-1;
    assert(layout_sum_field(&p,prodbody,NULL)==3);
    assert(layout_sum_step(&p,prodbody,-1)==8 && layout_sum_step(&p,prodbody,1)==6);
    assert(layout_delete_start(&p,prodbody)==prodbody);
    /* Distinct empty slots, editable by arrows or by ordinary comma input. */
    assert(expr_parse(&p,"summ(,,,)",1) && layout_measure(&p));
    for(int k=0;k<4;k++)assert(layout_sum_field(&p,5+k,NULL)==k);
    assert(layout_sum_delimiter(&p,5,',')==6 && layout_sum_delimiter(&p,6,',')==7);
    assert(layout_sum_delimiter(&p,7,',')==8 && layout_sum_delimiter(&p,8,')')==9);
    assert(layout_sum_delimiter(&p,5,')')==8); /* literal summ() -> term */
    assert(expr_parse(&p,"prod(,,,)",1) && layout_measure(&p));
    assert(layout_sum_delimiter(&p,5,')')==8 && layout_sum_delimiter(&p,8,')')==9);
    assert(expr_parse(&p,"summ(0,min(1,2),n,(x+1))",1) && layout_measure(&p));
    char const *function_close=strchr(p.src,')'),*group_close=strrchr(p.src,')')-1;
    assert(layout_close_delimiter(&p,(int)(function_close-p.src))==(int)(function_close-p.src)+1);
    assert(layout_close_delimiter(&p,(int)(group_close-p.src))==(int)(group_close-p.src)+1);
    assert(expr_parse(&p,"summ(0,3,k,summ(0,inf,n,n))",1) && layout_measure(&p));
    char const *inner=strstr(p.src,"inf");int inner_end=(int)(inner-p.src)+3, sum_index=-1;
    assert(layout_sum_field(&p,inner_end,&sum_index)==1 && sum_index>=0);
    assert(p.nodes[sum_index].start==(int)(strstr(p.src,"summ(0,inf")-p.src));
    /* Reproduce the calculator report: LEFT must escape the entire operator
       and reach 0.5 rather than cycling through bound/index/upper. */
    char const *escape_cases[]={"0.5*summ(0,1,n,1)","0.5*prod(1,5,n,n)",
                                "0.5*summ(1,3,n,prod(1,2,k,k))"};
    for(unsigned q=0;q<sizeof escape_cases/sizeof *escape_cases;q++){
        assert(expr_parse(&p,escape_cases[q],1) && layout_measure(&p));
        int pos=(int)strlen(escape_cases[q]);
        for(int i=0;i<32 && pos>3;i++)pos=layout_cursor_step(&p,pos,-1);
        assert(pos==3); /* immediately before '*', after 0.5 */
    }
    puts("Natural fraction/sum/infinity layout, field selection, left escape and visible cursor checks passed.");
}
