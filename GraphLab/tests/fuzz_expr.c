#include "../src/expr.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint32_t state=0x1234abcf;
static unsigned rng(void) { state^=state<<13;state^=state>>17;state^=state<<5;return state; }
int main(void) {
    static Parsed p; static Program pr; EvalVars v={0};
    const char chars[]="xyzabctniefgklm1234567890.+-*/^=<>(),! sincosqrtuf\n";
    char s[EXPR_MAX];
    for(int rep=0;rep<250000;rep++) {
        int n=rng()%(EXPR_MAX-1);
        for(int i=0;i<n;i++)s[i]=chars[rng()%(sizeof(chars)-1)];
        s[n]=0;
        for(int t=0;t<2;t++) {
            if(expr_parse(&p,s,t) && expr_compile(&p,&pr,NULL,0)) {
                (void)expr_eval_real(&pr,&v); (void)expr_eval_complex(&pr,&v);
            }
        }
    }
    /* Mutate realistic SUM inputs, including incomplete arguments and names. */
    char const *templates[]={"summ(0,inf,n,x^n/n!)", "summ(1,3,k,summ(0,2,n,k*n))", "sqrt(summ(0,6,i,(-1)^i/i!))"};
    for(int rep=0;rep<50000;rep++){
        char const *base=templates[rng()%(sizeof templates/sizeof *templates)];snprintf(s,sizeof s,"%s",base);
        int len=(int)strlen(s),changes=1+(rng()%4);
        while(changes--){int at=rng()%(len+1);unsigned op=rng()%3;
            if(op==0 && len>0 && at<len){memmove(s+at,s+at+1,(size_t)(len-at));len--;}
            else if(op==1 && len<EXPR_MAX-1){memmove(s+at+1,s+at,(size_t)(len-at+1));s[at]=chars[rng()%(sizeof chars-1)];len++;}
            else if(at<len)s[at]=chars[rng()%(sizeof chars-1)];
        }
        for(int t=0;t<2;t++)if(expr_parse(&p,s,t) && expr_compile(&p,&pr,NULL,0)){
            (void)expr_eval_real(&pr,&v);(void)expr_eval_complex(&pr,&v);
        }
    }
    puts("Fuzzed 250000 expressions plus 50000 mutated sums in strict and tolerant modes.");
}
