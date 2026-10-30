#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(a) (sizeof(a)/sizeof *(a))
int main(void)
{
    char dir[]="/tmp/gl116-fastui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    key_event_t path[]={K(OPTN),K(OPTN), /* first item is Ray start step */
        K(3),K(2),K(EXE), /* Ray start step 32 */
        K(DOWN),K(RIGHT), /* independent Ray Q 8 -> 9 */
        K(DOWN),K(3),K(1),K(EXE), /* Preview step 31 */
        K(OPTN),K(5),K(EXE),K(DOWN),K(7),K(EXE), /* -3-5 .. +3+7 */
        K(OPTN),K(F5),K(EXIT),
        K(F2),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(path,N(path));assert(graphlab_main()==0&&host_keys_used()==N(path)&&host_vram_guards_ok());
    assert(host_text_seen("GRAPH LAB 1.20"));
    assert(host_text_seen("SETTINGS: GENERAL 1/3"));
    assert(host_text_seen("SETTINGS: 3D 2/3"));
    assert(host_text_seen("SETTINGS: MAP 3/3"));
    assert(host_text_seen("Ray Q"));assert(host_text_seen("R9/1"));
    GraphState saved;assert(storage_load(&saved)==1);
    assert(saved.legacy_render_mode==2&&saved.fast_ray_step==32&&saved.fast_ray_q==9);
    assert(state_ray_quality(&saved)==20&&saved.preview_ray_step==31);
    assert(saved.map_extend_down==5&&saved.map_extend_up==7);
    assert(host_text_seen("Z range: -8 .. 10"));
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Settings OPTN cycle, independent Ray step/Q, Preview and render passed on host.");
}
