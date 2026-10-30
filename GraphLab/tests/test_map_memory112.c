#include "../src/intersection.h"
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
void *__real_malloc(size_t n);
static size_t fail_size;static int failures;
void *__wrap_malloc(size_t n){if(n==fail_size){failures++;return NULL;}return __real_malloc(n);}
static Parsed parsed;static Program p[2];static EvalVars vars;
static IMapPixel reference[IMAP_SIDE*IMAP_SIDE],fallback[IMAP_SIDE*IMAP_SIDE];
int main(void)
{
    assert(expr_parse(&parsed,"x^2+y^2+z^2=4",0)&&expr_compile(&parsed,&p[0],NULL,0));
    assert(expr_parse(&parsed,"x=0",0)&&expr_compile(&parsed,&p[1],NULL,0));
    RaySurface s[2]={{&p[0],0},{&p[1],1}};
    IMapView v={.radius=3,.zmin=-3,.zmax=3,.xy_div=24,.z_div=16,.cursor_steps=64};IMapStats a,b;
    assert(imap_build(s,&vars,&v,reference,NULL,NULL,&a)==1&&a.pixels>100&&!a.low_memory);
    fail_size=(size_t)(v.xy_div+1)*(v.xy_div+1)*2*sizeof(float);
    assert(imap_build(s,&vars,&v,fallback,NULL,NULL,&b)==1&&b.low_memory&&failures==1);
    assert(!memcmp(reference,fallback,sizeof reference));
    fail_size=(size_t)4*(v.xy_div+1)*2*sizeof(float);failures=0;
    assert(imap_build(s,&vars,&v,fallback,NULL,NULL,&b)==-2&&failures==1);
    puts("Map memory: verified streaming fallback matches cached plane; hard failure is safe.");
}
