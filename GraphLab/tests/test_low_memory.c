#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/ray.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
void *__real_malloc(size_t n);void *__real_calloc(size_t a,size_t b);
static size_t fail_size;static int remaining,failures,fail_calloc;
void *__wrap_malloc(size_t n){if(!fail_calloc&&remaining&&n==fail_size){remaining--;failures++;return NULL;}return __real_malloc(n);}
void *__wrap_calloc(size_t a,size_t b){if(fail_calloc&&remaining&&a*b==fail_size){remaining--;failures++;return NULL;}return __real_calloc(a,b);}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(a) (sizeof(a)/sizeof((a)[0]))
static void scene(GraphState *s,size_t size,int calloc_mode)
{
    assert(storage_save(s)==0);fail_size=size;fail_calloc=calloc_mode;remaining=1;failures=0;
    key_event_t keys[]={K(F2),K(F6),K(F6),K(RIGHT),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));assert(graphlab_main()==0&&host_keys_used()==N(keys));
    assert(failures==1&&host_vram_guards_ok()&&host_partial_updates()==0&&host_calculation_count()==2);
    assert(!host_text_seen("3D ERROR")&&!host_text_seen("3D MEMORY"));
    remaining=0;
}
int main(void)
{
    char dir[]="/tmp/gl-lowmem-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.render3_mode=1;strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");
    size_t cloud=(size_t)((PLOT_W+3)/4)*((PLOT_H+3)/4)*sizeof(Preview3DPoint);
    size_t depth=(size_t)((PLOT_W+1)/2)*((PLOT_H+1)/2)*sizeof(uint16_t);
    scene(&s,cloud,0);scene(&s,depth,0);scene(&s,(size_t)PLOT_W*PLOT_H*2,0);
    scene(&s,PREVIEW3D_AUX_CAPACITY*sizeof(Preview3DPoint),0); /* optional coarse cloud */
    scene(&s,sizeof(RayGridCache),1); /* optional cache descriptor */
    scene(&s,((size_t)20*20*20+7)/8,1);
    /* With no memory for a denser height lattice, use the original rolling
       planes; exact image quality may differ, but the renderer remains usable. */
    strcpy(s.surface[0].formula,"z=sin(x)*sin(y)");
    scene(&s,(size_t)(2*20+1)*(2*20+1)*sizeof(float),0);
    char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
    puts("Injected allocation failures: preview, depth, final layer, RAY grid and height-lattice fallback passed.");
}
