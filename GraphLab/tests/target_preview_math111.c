/* Exact short-span division on actual big-endian SH instructions in QEMU.
   This is not a calculator OS or physical timing test. */
#include <stdint.h>
#include <limits.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
int preview_test_span_divide(int32_t value,int span);
int main(void)
{
    static const int32_t edge[]={INT32_MIN,INT32_MIN+1,-1073676288,-32768,-1,0,1,32768,1073676288,INT32_MAX};
    uint32_t rng=0xd5e119a7u;
    for(int d=1;d<=4096;d++){
        for(unsigned k=0;k<sizeof edge/sizeof *edge;k++)
            if(preview_test_span_divide(edge[k],d)!=edge[k]/d)return 1;
        for(int k=0;k<64;k++){
            rng=rng*1664525u+1013904223u;int32_t n=(int32_t)rng;
            if(preview_test_span_divide(n,d)!=n/d)return 2;
        }
    }
    return 0;
}
