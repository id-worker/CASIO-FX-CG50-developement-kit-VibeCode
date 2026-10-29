/* Reproducible host benchmark of the Wireworld engine (not calculator FPS).
 * cc -O2 -I src tools/bench_step.c src/wireworld.c src/patterns.c -o bench
 * ./bench
 */
#define _POSIX_C_SOURCE 200809L
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "wireworld.h"
#include "patterns.h"

/* The UI's hook only marks dirty cells. Define WW_BENCH_BASELINE when
   compiling this same benchmark against the unmodified v1.63 engine. */
#ifdef WW_BENCH_BASELINE
#define BENCH_STEP ww_step
#else
#define BENCH_STEP ww_step_readonly_hook
#endif

static uint32_t rng_state = 0x4ad131a7u;
static uint32_t rnd(void) { rng_state = rng_state * 1664525u + 1013904223u; return rng_state; }
static uint64_t nanos(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return (uint64_t)t.tv_sec * 1000000000u + t.tv_nsec; }
static volatile unsigned hook_total;
static void hook(int x, int y, int state, void *arg) { (void)arg; hook_total += (unsigned)(x + y + state); }

static int pattern_named(const char *name) {
    int i; for (i = 0; i < patternCount; i++) if (strstr(patterns[i].name, name)) return i;
    return 0;
}
static void setup(WWWorld *w, int type) {
    int i, n = w->w * w->h;
    rng_state = 0x4ad131a7u;
    if (type == 0) {
        ww_load_text(w, patterns[pattern_named("CLOCK")].art, 170, 80);
    } else if (type == 1) {
        memset(w->cells, WW_CONDUCTOR, (size_t)n);
        w->cells[n / 2 + w->w/2] = WW_HEAD;
        w->cells[n / 2 + w->w/2 - 1] = WW_TAIL;
    } else if (type == 2) {
        for (i=0;i<n;i++) {
            unsigned r=rnd()>>24;
            w->cells[i] = r < 25 ? WW_HEAD : r < 40 ? WW_TAIL : r < 200 ? WW_CONDUCTOR : WW_EMPTY;
        }
    } else if (type == 3) {
        /* Two active circuits straddle opposite sides of the torus. */
        ww_load_text(w, patterns[pattern_named("CLOCK")].art, 1, 1);
        ww_load_text(w, patterns[pattern_named("CLOCK")].art, w->w - 145, w->h - 70);
    } else if (type == 4) {
        memset(w->cells, WW_CONDUCTOR, (size_t)n);
    }
}
static uint64_t checksum(const WWWorld *w) {
    uint64_t h=1469598103934665603ull; int i,n=w->w*w->h;
    for (i=0;i<n;i++) h=(h^w->cells[i])*1099511628211ull;
    return h;
}
int main(void) {
    static const char *names[] = {"CLOCK centre", "dense wires + one spark", "dense random reset", "CLOCK opposite corners", "static dense wire"};
    int type;
    for(type=0;type<5;type++) {
        WWWorld w; uint64_t best=~(uint64_t)0, h=0; int rep, steps=type==2?40:180;
        if(ww_init(&w,512,256)) return 1;
        for(rep=0;rep<3;rep++) {
            int i; uint64_t elapsed=0; ww_clear(&w); setup(&w,type); hook_total=0;
            if(type==2) {
                unsigned char *original=(unsigned char*)malloc(512*256);
                memcpy(original,w.cells,512*256);
                for(i=0;i<steps;i++) {
                    uint64_t start;
                    memcpy(w.cells,original,512*256);
                    start=nanos(); BENCH_STEP(&w,hook,NULL); elapsed+=nanos()-start;
                }
                free(original);
            } else {
                uint64_t start=nanos();
                for(i=0;i<steps;i++) BENCH_STEP(&w,hook,NULL);
                elapsed=nanos()-start;
            }
            if(elapsed<best) best=elapsed;
            h=checksum(&w);
        }
        printf("%-27s %8.2f us/step  hash=%016llx hook=%u\n",names[type],
               (double)best/steps/1000.,(unsigned long long)h,hook_total);
        ww_free(&w);
    }
    return 0;
}
