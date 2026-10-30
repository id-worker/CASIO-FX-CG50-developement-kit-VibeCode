#include <assert.h>
#include <stdint.h>
#include <limits.h>
#include <stdio.h>
int preview_test_span_divide(int32_t value,int span);
int main(void)
{
    static const int32_t edge[]={INT32_MIN,INT32_MIN+1,-1073676288,-32768,-2,-1,0,1,2,32768,1073676288,INT32_MAX};
    uint32_t rng=0x413f2ed1u;
    for(int d=1;d<=4096;d++){
        for(unsigned k=0;k<sizeof edge/sizeof *edge;k++)
            assert(preview_test_span_divide(edge[k],d)==edge[k]/d);
        for(int k=0;k<2048;k++){
            rng=rng*1664525u+1013904223u;
            int32_t n=(int32_t)rng;
            assert(preview_test_span_divide(n,d)==n/d);
        }
    }
    puts("Fast span division: >8 million exact signed cases including extremes passed.");
}
