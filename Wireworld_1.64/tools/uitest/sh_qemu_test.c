/*
 * Freestanding big-endian SH4A engine test for qemu-sh4eb. This is a Linux
 * test harness, not the Casio add-in. It compares the real v1.64 and v1.63
 * engines on the target ISA/endianness. It needs no SH Linux C library.
 */
#include <stddef.h>
#include <stdint.h>
#include "wireworld.h"
extern int ww163_step(WWWorld *, WWCellHook, void *);

enum { MAX_CELLS=512*256, MAX_MASK=MAX_CELLS/8 };
static unsigned char ac[MAX_CELLS] __attribute__((aligned(4)));
static unsigned char bc[MAX_CELLS] __attribute__((aligned(4)));
static unsigned char ah[MAX_MASK], bh[MAX_MASK];
volatile int qemu_data_segment = 1; /* keep a page-aligned RW ELF segment */

void *memset(void *dst, int val, size_t n)
{ unsigned char *p=(unsigned char *)dst; while(n--) *p++=(unsigned char)val; return dst; }
void *memcpy(void *dst, const void *src, size_t n)
{ unsigned char *p=dst; const unsigned char *q=src; while(n--) *p++=*q++; return dst; }
int memcmp(const void *a, const void *b, size_t n)
{ const unsigned char *p=a,*q=b; while(n--) { if(*p!=*q) return (int)*p-(int)*q; p++; q++; } return 0; }
static uint32_t rng=0x6ba11237u;
static uint32_t rnd(void) { rng^=rng<<13; rng^=rng>>17; rng^=rng<<5; return rng; }
typedef struct { uint32_t hash; int n; } Events;
static void hook(int x,int y,int state,void *arg)
{
    Events *e=(Events*)arg;
    e->hash=(e->hash^(uint32_t)x)*16777619u;
    e->hash=(e->hash^(uint32_t)y)*16777619u;
    e->hash=(e->hash^(uint32_t)state)*16777619u;
    e->n++;
}
static void fill(int w,int h,int mode,int trial)
{
    int n=w*h,i;
    for(i=0;i<n;i++) {
        unsigned r=rnd()%1000u;
        if(mode==0) ac[i]=(unsigned char)(r%4);
        else if(mode==1) ac[i]=(r<8)?WW_HEAD:(r<16)?WW_TAIL:(r<120)?WW_CONDUCTOR:WW_EMPTY;
        else if(mode==2) ac[i]=WW_CONDUCTOR;
        else if(mode==3) ac[i]=(r<60)?WW_TAIL:(r<700)?WW_CONDUCTOR:WW_EMPTY;
        else if(mode==4) ac[i]=WW_EMPTY;
        else ac[i]=(r<990)?WW_CONDUCTOR:(r<995)?WW_HEAD:WW_TAIL;
    }
    if(mode==2 || mode==4) {
        /* Exercise group boundaries, corners and opposing toroidal seams. */
        int x=(trial*7)%w, y=(trial*11)%h;
        ac[y*w+x]=WW_HEAD;
        ac[((y+1)%h)*w+(x+1)%w]=WW_TAIL;
        ac[(h-1)*w]=WW_HEAD;
    }
}
static int run_tests(void)
{
    static const short dims[][2]={{1,1},{1,2},{2,1},{2,2},{7,9},{8,3},
      {8,9},{16,17},{31,17},{32,32},{64,64},{128,128},{192,128},{512,256}};
    unsigned d;
    for(d=0;d<sizeof(dims)/sizeof(dims[0]);d++) {
        int w=dims[d][0],h=dims[d][1],n=w*h,trial,step;
        WWWorld a={w,h,ac,ah,0,0}, b={w,h,bc,bh,0,0};
        for(trial=0;trial<18;trial++) {
            fill(w,h,trial%6,trial);
            memcpy(bc,ac,(size_t)n);
            a.gen=b.gen=(trial%5==0)?0xfffffffdul:0;
            for(step=0;step<6;step++) {
                Events ea={2166136261u,0}, eb={2166136261u,0};
                int ra=ww_step_readonly_hook(&a,hook,&ea);
                int rb=ww163_step(&b,hook,&eb);
                if(ra!=rb || a.changed!=b.changed || a.gen!=b.gen ||
                   ea.n!=eb.n || ea.n!=ra || ea.hash!=eb.hash ||
                   memcmp(ac,bc,(size_t)n) || memcmp(ah,bh,(size_t)(n+7)/8)) return 1;
            }
        }
    }
    return 0;
}
static void say(const char *s, int len)
{
    register int r3 __asm__("r3")=4, r4 __asm__("r4")=1;
    register const char *r5 __asm__("r5")=s;
    register int r6 __asm__("r6")=len;
    __asm__ volatile("trapa #0x10" : : "r"(r3),"r"(r4),"r"(r5),"r"(r6) : "r0","memory");
}
static void quit(int n)
{
    register int r3 __asm__("r3")=1, r4 __asm__("r4")=n;
    __asm__ volatile("trapa #0x10" : : "r"(r3),"r"(r4) : "memory");
    __builtin_unreachable();
}
/* SH bare-metal toolchains prefix C symbols with '_', so this is _start. */
void start(void)
{
    static const char ok[]="SH4A big-endian: 252 scenarios / 1512 generations, all equal\n";
    static const char fail[]="SH4A big-endian: engine mismatch\n";
    int error=run_tests();
    if(error) say(fail,(int)sizeof(fail)-1); else say(ok,(int)sizeof(ok)-1);
    quit(error);
}
