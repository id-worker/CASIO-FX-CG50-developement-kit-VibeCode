/* Regression: a SUM/PROD must not trap LEFT or RIGHT inside a cycle. */
#include "../src/layout.h"
#include "host_gint.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static Parsed p;
static void check(char const *s,int direction)
{
    if(!expr_parse(&p,s,1)||!layout_measure(&p)){
        fprintf(stderr,"Failed to lay out: %s (%s)\n",s,p.message);abort();
    }
    int pos=direction<0?p.len:0,end=direction<0?0:p.len,seen[EXPR_MAX+1]={0},steps=0;
    while(pos!=end){
        if(pos<0||pos>p.len||seen[pos]||steps++>EXPR_MAX){
            fprintf(stderr,"Cursor cycle/stuck at %d (direction %d): %s\n",pos,direction,s);abort();
        }
        seen[pos]=1;
        FormulaCaret caret=layout_draw(&p,pos,0,0,C_BLACK,C_BLUE,1);
        if(!caret.found){fprintf(stderr,"Invisible cursor at %d: %s\n",pos,s);abort();}
        pos=layout_cursor_step(&p,pos,direction);
    }
}
int main(void)
{
    static char const *const prefixes[]={"0.5*","x+","2/(","(1+2)*","summ(0,1,n,","2"};
    static char const *const operators[]={"summ","prod"};
    static char const *const lower[]={"0","-2","(1+2)"};
    static char const *const upper[]={"1","inf","min(1,2)"};
    static char const *const terms[]={"n","n^2","(n+1)","sin(n)","prod(1,2,k,k)","summ(0,1,k,k+n)"};
    char s[EXPR_MAX];int structures=0;
    for(unsigned a=0;a<sizeof prefixes/sizeof *prefixes;a++)
      for(unsigned b=0;b<sizeof operators/sizeof *operators;b++)
       for(unsigned c=0;c<sizeof lower/sizeof *lower;c++)
        for(unsigned d=0;d<sizeof upper/sizeof *upper;d++)
         for(unsigned e=0;e<sizeof terms/sizeof *terms;e++){
            int n=snprintf(s,sizeof s,"%s%s(%s,%s,n,%s)%s",prefixes[a],operators[b],
                lower[c],upper[d],terms[e],a==2||a==4?")":"");
            assert(n>0 && n<(int)sizeof s);
            check(s,-1);check(s,1);structures++;
         }
    /* All four genuinely empty slots have different carets. */
    assert(expr_parse(&p,"summ(,,,)",1) && layout_measure(&p));
    FormulaCaret lo=layout_draw(&p,5,0,0,C_BLACK,C_BLUE,1);
    FormulaCaret hi=layout_draw(&p,6,0,0,C_BLACK,C_BLUE,1);
    FormulaCaret index=layout_draw(&p,7,0,0,C_BLACK,C_BLUE,1);
    FormulaCaret term=layout_draw(&p,8,0,0,C_BLACK,C_BLUE,1);
    assert(lo.found&&hi.found&&index.found&&term.found);
    assert(hi.y<term.y && term.y<lo.y && index.y==lo.y && index.x<lo.x);
    printf("Editor navigation: %d SUM/PROD structures traversed both ways; empty fields have distinct carets.\n",structures);
}
