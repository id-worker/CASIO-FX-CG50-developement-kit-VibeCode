#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.shift=1,.mod=1}
#define N(a) (sizeof(a)/sizeof((a)[0]))
static void run(key_event_t const *keys,size_t n){
    host_keys(keys,n);assert(graphlab_main()==0&&host_keys_used()==n);
    assert(host_vram_guards_ok()&&host_polls_outside_calculation()==0);
#ifdef GL_DEV
    assert(host_partial_updates()>0);
#else
    assert(host_partial_updates()==0);
#endif
}
int main(void)
{
    char dir[]="/tmp/gl110-ui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.legacy_render_mode=1;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");
    strcpy(s.surface[1].formula,"x=0");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t keys[]={K(F2),K(F6),K(2),K(F4),K(F4),K(RIGHT),K(F1),K(F2),S(DEL),K(1),K(EXE),
        K(F3),K(F6),K(F6),K(RIGHT),K(UP),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(keys,N(keys));
    assert(host_calculation_count()==7&&host_text_seen("3D SLICE")&&host_text_seen("PLANE XY"));
    assert(host_text_seen("PLANE XZ")&&host_text_seen("CROSS 1/2")&&host_text_seen("CROSS 2/2"));
    assert(host_text_seen("FREE CURSOR")&&host_text_seen("NOT A ROOT")&&host_text_seen("AUTO: FOUND"));
    assert(host_text_seen("MESH PREVIEW"));
    /* Inequalities remain a voxel result and use the point fallback;
       their sections are boundary curves only. */
    state_defaults(&s);s.legacy_render_mode=1;strcpy(s.surface[0].formula,"x^2+y^2+z^2<=4");assert(storage_save(&s)==0);
    key_event_t volume[]={K(F2),K(F6),S(F3),K(F6),K(F6),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    run(volume,N(volume));assert(host_text_seen("BOUNDARY")&&host_text_seen("SURFACE TRACE")&&host_text_seen("POINT PREVIEW"));
    /* RAY progressive frames differ; the standard build still remains silent. */
    state_defaults(&s);s.legacy_render_mode=1;strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");assert(storage_save(&s)==0);
    key_event_t ray[]={K(F2),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(ray,N(ray));
#ifdef GL_DEV
    assert(host_partial_colored_frames()>=2&&host_partial_max_colored()>host_partial_min_colored());
#else
    assert(host_partial_colored_frames()==0);
#endif
    /* Ensure the Dev really emits multiple progressively colored 2D frames,
       while the standard build never calls dupdate during calculation. */
    state_defaults(&s);assert(storage_save(&s)==0);
    key_event_t two[]={K(F1),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(two,N(two));
#ifdef GL_DEV
    assert(host_partial_colored_frames()>=2&&host_partial_max_colored()>host_partial_min_colored());
#else
    assert(host_partial_colored_frames()==0);
#endif
    /* The same is true for fractal work. */
    key_event_t fractal[]={K(F3),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(fractal,N(fractal));
#ifdef GL_DEV
    assert(host_partial_colored_frames()>=2&&host_partial_max_colored()>host_partial_min_colored());
#else
    assert(host_partial_colored_frames()==0);
#endif
    char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
#ifdef GL_DEV
    puts("Dev UI: progressive colored 2D/3D/fractal, manual/auto sections, selection and cached mesh/voxel preview passed.");
#else
    puts("Standard UI: no partial updates; manual/auto sections, selection and cached mesh/voxel preview passed.");
#endif
}
