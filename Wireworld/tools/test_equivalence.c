/* Compare actual new engine against the hardware-tested v1.63 engine.
 * Covers toroidal borders, variable widths, random fields, state transitions,
 * callback order and arguments, head masks, and generation/changed counters.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "wireworld.h"

extern int ww163_step(WWWorld *, WWCellHook, void *);
static uint32_t rng = 0xc2a57e31u;
static uint32_t rnd(void) { rng ^= rng<<13; rng ^= rng>>17; rng ^= rng<<5; return rng; }
typedef struct { int idx, state; } Event;
typedef struct { Event *v; int n, cap, w, h, bad; } Trace;
static void record(int x, int y, int state, void *context)
{
    Trace *t = (Trace *)context;
    if (t->n >= t->cap || x < 0 || y < 0 || x >= t->w || y >= t->h) { t->bad = 1; return; }
    t->v[t->n].idx = y * t->w + x;
    t->v[t->n].state = state;
    t->n++;
}
static void fill(unsigned char *p, int W, int H, int mode, int trial)
{
    int x, y, n = W*H;
    for (y=0;y<H;y++) for(x=0;x<W;x++) {
        unsigned r=rnd()%1000u; int v;
        if (mode==0) v=(int)(r%4);                                  /* dense random */
        else if (mode==1) v=r<8?WW_HEAD:r<16?WW_TAIL:r<140?WW_CONDUCTOR:WW_EMPTY;
        else if (mode==2) v=r<995?WW_CONDUCTOR:(r<998?WW_HEAD:WW_TAIL);
        else if (mode==3) v=(x<8 && y<8 && r<350)?(int)(r%4):(r<200?WW_CONDUCTOR:WW_EMPTY);
        else if (mode==4) v=(r&1)?WW_CONDUCTOR:WW_EMPTY;             /* no motion */
        else if (mode==5) v=r<100?WW_TAIL:r<700?WW_CONDUCTOR:WW_EMPTY;
        else if (mode==6 || mode==9 || mode==10) v=WW_CONDUCTOR;
        else if (mode==8 || mode==11) v=WW_EMPTY;
        else v=(r<10 && (x<2 || y<2 || x>=W-2 || y>=H-2)) ? WW_HEAD :
                  r<500 ? WW_CONDUCTOR : WW_EMPTY;
        p[y*W+x]=(unsigned char)v;
    }
    if (mode==6) {
        p[0]=WW_HEAD; p[W-1]=WW_TAIL;
        p[(H-1)*W]=WW_HEAD; p[n-1]=WW_TAIL;
        p[(H/2)*W+(W/2)]=WW_HEAD;
    }
    if (mode>=8) {
        /* Single small active cluster: probe byte boundaries and all seams. */
        static const int sx[]={0,1,6,7,8,9,14,15,16,17,30,31,63,64,127,255,511};
        static const int sy[]={0,1,2,7,8,15,16,31,63,127,255};
        int cx=sx[(trial/12 + (int)(rnd()%17))%17]%W;
        int cy=sy[(trial/12 + (int)(rnd()%11))%11]%H;
        int dx,dy;
        if (mode==8 || mode==11) {
            for(dy=-2;dy<=2;dy++) for(dx=-2;dx<=2;dx++) {
                int xx=(cx+dx+W*3)%W, yy=(cy+dy+H*3)%H;
                p[yy*W+xx]=WW_CONDUCTOR;
            }
        }
        p[cy*W+cx]=(mode==10 || mode==11)?WW_TAIL:WW_HEAD;
        if(mode==9) p[cy*W+(cx+W-1)%W]=WW_TAIL;
    }
}
/* Exercise the legacy API with a callback that really mutates a distant
 * cell. This must keep the v1.63 semantics, not use the read-only fast path. */
typedef struct { Trace trace; WWWorld *world; int target; } MutCtx;
static void mutate_once(int x, int y, int state, void *opaque)
{
    MutCtx *m=(MutCtx *)opaque;
    record(x,y,state,&m->trace);
    if(m->trace.n==1) m->world->cells[m->target]=WW_TAIL;
}
static int test_mutating_hook(void)
{
    WWWorld a,b; int n=512*256, ra,rb;
    Event *ea=(Event*)malloc((size_t)n*sizeof(Event));
    Event *eb=(Event*)malloc((size_t)n*sizeof(Event));
    MutCtx ca,cb;
    if(!ea || !eb || ww_init(&a,512,256) || ww_init(&b,512,256)) return 0;
    memset(a.cells,WW_CONDUCTOR,(size_t)n);
    a.cells[10*512+10]=WW_HEAD;
    memcpy(b.cells,a.cells,(size_t)n);
    ca.trace=(Trace){ea,0,n,512,256,0}; ca.world=&a; ca.target=200*512+300;
    cb.trace=(Trace){eb,0,n,512,256,0}; cb.world=&b; cb.target=200*512+300;
    ra=ww_step(&a,mutate_once,&ca); rb=ww163_step(&b,mutate_once,&cb);
    if(ra!=rb || ca.trace.bad || cb.trace.bad || ca.trace.n!=cb.trace.n ||
       memcmp(a.cells,b.cells,(size_t)n) ||
       memcmp(a.heads,b.heads,(size_t)n/8) ||
       memcmp(ea,eb,(size_t)ca.trace.n*sizeof(Event)) ||
       a.cells[ca.target]!=WW_CONDUCTOR) {
        fprintf(stderr,"FAIL mutating hook changed=%d/%d\n",ra,rb);
        ww_free(&a); ww_free(&b); free(ea); free(eb); return 0;
    }
    ww_free(&a); ww_free(&b); free(ea); free(eb); return 1;
}
static int test_position_sweep(void)
{
    enum { W=32, H=16, N=W*H };
    WWWorld a,b; Event ea[N],eb[N]; int x,y,type,step,passes=0;
    if(ww_init(&a,W,H) || ww_init(&b,W,H)) return 0;
    for(type=0;type<2;type++) for(y=0;y<H;y++) for(x=0;x<W;x++) {
        memset(a.cells,WW_CONDUCTOR,N);
        a.cells[y*W+x]=type ? WW_TAIL : WW_HEAD;
        memcpy(b.cells,a.cells,N); a.gen=b.gen=0;
        for(step=0;step<3;step++) {
            Trace ta={ea,0,N,W,H,0},tb={eb,0,N,W,H,0};
            int ra=ww_step_readonly_hook(&a,record,&ta);
            int rb=ww163_step(&b,record,&tb);
            if(ra!=rb || a.gen!=b.gen || a.changed!=b.changed || ta.bad || tb.bad ||
               ta.n!=tb.n || memcmp(a.cells,b.cells,N) ||
               memcmp(a.heads,b.heads,N/8) || memcmp(ea,eb,(size_t)ta.n*sizeof(Event))) {
                fprintf(stderr,"FAIL sweep %d,%d type=%d step=%d\n",x,y,type,step);
                ww_free(&a); ww_free(&b); return 0;
            }
        }
        passes++;
    }
    printf("%d isolated head/tail positions swept, all equal\n",passes);
    ww_free(&a); ww_free(&b); return 1;
}
int main(void)
{
    static const int dims[][2]={
        {1,1},{1,2},{2,1},{2,2},{1,9},{2,7},{3,3},{7,9},
        {8,3},{8,4},{8,9},{8,17},{16,3},{16,17},{31,17},
        {32,32},{56,29},{64,64},{128,128},{192,128},{512,256}
    };
    unsigned d; int scenarios=0, generations=0, bad=0;
    for(d=0;d<sizeof(dims)/sizeof(dims[0]);d++) {
        int W=dims[d][0], H=dims[d][1], n=W*H;
        int cases = n>50000 ? 20 : n>10000 ? 28 : 56;
        int trial;
        WWWorld a,b,u; unsigned char *unaligned_base;
        Event *ea=(Event*)malloc((size_t)n*sizeof(Event));
        Event *eb=(Event*)malloc((size_t)n*sizeof(Event));
        Event *eu=(Event*)malloc((size_t)n*sizeof(Event));
        if (!ea || !eb || !eu || ww_init(&a,W,H) || ww_init(&b,W,H) || ww_init(&u,W,H)) return 2;
        unaligned_base=(unsigned char*)malloc((size_t)n+1);
        if(!unaligned_base) return 2;
        free(u.cells); u.cells=unaligned_base+1;
        if(!((uintptr_t)u.cells&3u)) return 2;
        for(trial=0;trial<cases && !bad;trial++) {
            int step, mode=trial%12;
            fill(a.cells,W,H,mode,trial);
            memcpy(b.cells,a.cells,(size_t)n); memcpy(u.cells,a.cells,(size_t)n);
            a.gen=b.gen=u.gen=(trial%7==0)?~0ul-2:0;
            for(step=0;step<7;step++) {
                Trace ta={ea,0,n,W,H,0}, tb={eb,0,n,W,H,0}, tu={eu,0,n,W,H,0};
                /* Last step has no callback; the first six check exact events. */
                int ra,rb,ru;
                if(step<6) {
                    ra=ww_step_readonly_hook(&a,record,&ta);
                    rb=ww163_step(&b,record,&tb);
                    ru=ww_step_readonly_hook(&u,record,&tu);
                } else {
                    ra=ww_step(&a,NULL,NULL);
                    rb=ww163_step(&b,NULL,NULL);
                    ru=ww_step(&u,NULL,NULL);
                }
                if(ra!=rb || ru!=rb || a.changed!=b.changed || u.changed!=b.changed ||
                   a.gen!=b.gen || u.gen!=b.gen || ta.bad || tb.bad || tu.bad ||
                   ta.n!=tb.n || tu.n!=tb.n || (step<6 && ta.n!=ra) ||
                   memcmp(a.cells,b.cells,(size_t)n) || memcmp(u.cells,b.cells,(size_t)n) ||
                   memcmp(a.heads,b.heads,(size_t)(n+7)/8) ||
                   memcmp(u.heads,b.heads,(size_t)(n+7)/8) ||
                   memcmp(ea,eb,(size_t)ta.n*sizeof(Event)) ||
                   memcmp(eu,eb,(size_t)tu.n*sizeof(Event))) {
                    int i;
                    fprintf(stderr,"FAIL size=%dx%d trial=%d mode=%d step=%d changed=%d/%d/%d hooks=%d/%d/%d\n",
                            W,H,trial,mode,step,ra,rb,ru,ta.n,tb.n,tu.n);
                    for(i=0;i<n;i++) if(a.cells[i]!=b.cells[i]) {
                        fprintf(stderr," first difference (%d,%d)=%u/%u\n",i%W,i/W,a.cells[i],b.cells[i]); break;
                    }
                    bad=1; break;
                }
                generations++;
            }
            scenarios++;
        }
        printf("%3dx%-3d : %d scenarios, %s\n",W,H,trial,bad?"FAIL":"OK");
        u.cells=unaligned_base;
        ww_free(&a); ww_free(&b); ww_free(&u); free(ea); free(eb); free(eu);
        if(bad) break;
    }
    if(!bad && !test_mutating_hook()) bad=1;
    if(!bad && !test_position_sweep()) bad=1;
    printf("%d scenarios, %d generations, aligned + deliberately unaligned: %s\n",
           scenarios,generations,bad?"FAIL":"all equal");
    return bad?1:0;
}
