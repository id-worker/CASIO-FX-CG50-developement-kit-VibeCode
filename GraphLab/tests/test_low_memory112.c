#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/ray.h"
#include "../src/intersection.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
void *__real_malloc(size_t n);
static size_t failed_size,min_key;static int remaining,failures;
void *__wrap_malloc(size_t n){
    if(remaining&&n==failed_size&&host_keys_used()>=min_key){remaining--;failures++;return NULL;}
    return __real_malloc(n);
}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(a) (sizeof(a)/sizeof *(a))
static void scene(GraphState *s,size_t size,size_t after,int unavailable,int calculations)
{
    assert(storage_save(s)==0);failed_size=size;min_key=after;remaining=1;failures=0;
    key_event_t keys[]={K(F2),K(F6),K(F6),K(RIGHT),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));assert(graphlab_main()==0&&host_keys_used()==N(keys));
    if(failures!=1||host_calculation_count()!=calculations)fprintf(stderr,"allocation size=%zu after=%zu failures=%d calculations=%d\n",size,after,failures,host_calculation_count());
    assert(failures==1&&host_vram_guards_ok()&&host_partial_updates()==0&&host_calculation_count()==calculations);
    assert(!host_text_seen("3D ERROR")&&!host_text_seen("3D MEMORY"));
    if(unavailable)assert(host_text_seen("PREVIEW UNAVAILABLE"));
    else assert(host_text_seen("PREVIEW | AUTO"));
    remaining=0;
}
int main(void)
{
    char dir[]="/tmp/gl117-low-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.legacy_render_mode=2;strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");
    /* The retained layer is retried after releasing the optional grid cache. */
    scene(&s,(size_t)PLOT_W*PLOT_H*sizeof(uint16_t),0,0,12);
    /* Ray column cache is optional and the scalar fallback still works. */
    scene(&s,(size_t)PLOT_W*2*sizeof(float),0,0,12);
    /* Optional column-cache failure on an inequality follows the same safe path. */
    s.legacy_render_mode=0;strcpy(s.surface[0].formula,"x^2+y^2+z^2<=1");
    scene(&s,(size_t)PLOT_W*2*sizeof(float),0,0,12);
    /* Failure to retain samples is explicit: no invented fallback redraw.
       After acknowledging, reopening and retrying produces the real final. */
    state_defaults(&s);s.legacy_render_mode=2;assert(storage_save(&s)==0);
    failed_size=(size_t)((PLOT_W+1)/2)*((PLOT_H+1)/2)*sizeof(uint16_t);
    min_key=3;remaining=1;failures=0;
    key_event_t retry[]={K(F2),K(F6),K(F6),K(EXE),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(retry,N(retry));assert(graphlab_main()==0&&host_keys_used()==N(retry));
    assert(failures==1&&host_text_seen("3D MEMORY")&&host_text_seen("R8/1")&&host_calculation_count()==6&&host_partial_updates()==0);
    remaining=0;
    /* An odd initial spacing uses a separate small seed buffer. */
    state_defaults(&s);s.legacy_render_mode=2;s.fast_ray_step=31;assert(storage_save(&s)==0);
    failed_size=(size_t)((PLOT_W+30)/31)*((PLOT_H+30)/31)*sizeof(uint16_t);
    min_key=3;remaining=1;failures=0;
    host_keys(retry,N(retry));assert(graphlab_main()==0&&host_keys_used()==N(retry));
    assert(failures==1&&host_text_seen("3D MEMORY")&&host_text_seen("R8/1")&&host_calculation_count()==6&&host_partial_updates()==0);
    remaining=0;
    /* A map allocation failure returns to the completed 3D view safely. */
    state_defaults(&s);s.legacy_render_mode=2;strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");
    s.surface[1].active=1;strcpy(s.surface[1].formula,"x=0");assert(storage_save(&s)==0);
    failed_size=(size_t)IMAP_SIDE*IMAP_SIDE*sizeof(IMapPixel);min_key=4;remaining=1;failures=0;
    key_event_t keys[]={K(F2),K(F6),K(F6),K(2),K(EXE),K(RIGHT),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));assert(graphlab_main()==0&&host_keys_used()==N(keys));
    assert(failures==1&&host_text_seen("MAP MEMORY")&&host_text_seen("PREVIEW")&&host_vram_guards_ok());
    remaining=0;
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Preview, optional caches and map allocation failures preserve safe recovery.");
}
