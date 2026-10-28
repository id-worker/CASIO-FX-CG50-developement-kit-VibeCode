/* Hidden divisor grouping must preserve the exact strict VM program. */
#include "../src/expr.h"
#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
void graphlab_test_group_divisors(char *text);
/* Shared test build enables tracing in the editor; this test never opens it. */
void graphlab_editor_trace(char const *s,int cursor,char const *error)
{(void)s;(void)cursor;(void)error;}
static unsigned rng=0x53bac91u;
static unsigned rand32(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
static void make(char *out,size_t cap,int depth)
{
    static const char *const atom[]={"x","y","n","1","2","3.5","pi"};
    static const char *const fn[]={"sin","cos","sqrt","abs","ln","exp"};
    char a[EXPR_MAX],b[EXPR_MAX];
    if(!depth || rand32()%4==0){snprintf(out,cap,"%s",atom[rand32()%(sizeof atom/sizeof *atom)]);return;}
    make(a,sizeof a,depth-1);make(b,sizeof b,depth-1);
    switch(rand32()%7){
        case 0:snprintf(out,cap,"%s/%s",a,b);break;
        case 1:snprintf(out,cap,"(%s)+(%s)",a,b);break;
        case 2:snprintf(out,cap,"(%s)*(%s)",a,b);break;
        case 3:snprintf(out,cap,"%s(%s)",fn[rand32()%(sizeof fn/sizeof *fn)],a);break;
        case 4:snprintf(out,cap,"min(%s,%s)",a,b);break;
        case 5:snprintf(out,cap,"summ(0,3,n,%s)",a);break;
        default:snprintf(out,cap,"(%s)^2",a);break;
    }
}
int main(void)
{
    static Parsed p;static Program a,b;char error[90],s[EXPR_MAX],old[EXPR_MAX];int checked=0,changed=0;
    for(int i=0;i<50000;i++){
        make(s,sizeof s,3);snprintf(old,sizeof old,"%s",s);
        if(!expr_parse(&p,s,0) || !expr_compile(&p,&a,error,sizeof error))continue;
        graphlab_test_group_divisors(s);
        if(!expr_parse(&p,s,0) || !expr_compile(&p,&b,error,sizeof error) || memcmp(&a,&b,sizeof a)){
            fprintf(stderr,"Normalization changed VM bytecode: %s -> %s\n",old,s);return 1;
        }
        checked++;if(strcmp(s,old))changed++;
    }
    assert(checked>40000 && changed>1000);
    printf("Legacy divisor normalization: %d valid expressions, %d regrouped, identical strict VM bytecode.\n",checked,changed);
}
