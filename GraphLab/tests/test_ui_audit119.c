/* Scripted end-to-end UI checks for less-frequently used help/examples paths.
   Synthetic keyboard/display/filesystem; not Casio OS. */
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
#define IDLE {.type=KEYEV_NONE}
#define N(a) (sizeof(a)/sizeof((a)[0]))
static void run(key_event_t const *keys,size_t count)
{
    host_keys(keys,count);assert(graphlab_main()==0);assert(host_keys_used()==count);assert(host_vram_guards_ok());
}
int main(void)
{
    char dir[]="/tmp/gl-ui-audit-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState state;state_defaults(&state);assert(storage_save(&state)==0);
    key_event_t help[]={K(OPTN),K(F4),K(F6),K(F6),K(F6)};
    run(help,N(help));assert(host_text_seen("GRAPH LAB HELP")&&host_text_seen("SHIFT+F5 symbols"));
    state_defaults(&state);assert(storage_save(&state)==0);
    char old[EXPR_MAX];strcpy(old,state.curve[0].formula);
    key_event_t cancel_example[]={K(F1),K(F4),K(DOWN),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(cancel_example,N(cancel_example));assert(storage_load(&state)==1&&!strcmp(state.curve[0].formula,old));
    key_event_t circle[]={K(F1),K(F4),K(DOWN),K(EXE),K(F5),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(circle,N(circle));assert(storage_load(&state)==1&&!strcmp(state.curve[0].formula,"x^2+y^2=9"));
    assert(host_text_seen("EXAMPLES")&&host_calculation_count()==1);
    state_defaults(&state);state.legacy_render_mode=2;state.fast_ray_q=8;state.fast_ray_step=32;assert(storage_save(&state)==0);
    key_event_t wave[]={K(F2),K(F4),K(DOWN),K(DOWN),K(EXE),K(F5),K(F6),IDLE,K(EXIT),K(EXIT),K(F6)};
    run(wave,N(wave));assert(storage_load(&state)==1&&!strcmp(state.surface[0].formula,"z=sin(x)*cos(y)"));
    assert(host_text_seen("EXAMPLES")&&host_text_seen("R8/1")&&host_calculation_count()==6);
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);
    snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Application audit: HELP, cancel/choose 2D examples and automatically refined 3D example passed.");
}
