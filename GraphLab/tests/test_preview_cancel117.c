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
#define N(a) (sizeof(a)/sizeof *(a))
static void run_cancel(int which)
{
    key_event_t keys[]={K(F2),K(F6),K(F6),K(RIGHT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_async_on_calculation((key_event_t){.key=which,.type=KEYEV_DOWN},2,3);
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==2&&host_partial_updates()==0&&host_vram_guards_ok());
    assert(host_menus_opened()==(which==KEY_MENU?1:0));
}
int main(void)
{
    char dir[]="/tmp/gl117-cancel-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.legacy_render_mode=1;s.preview_ray_step=16;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");assert(storage_save(&s)==0);
    run_cancel(KEY_EXIT);run_cancel(KEY_MENU);
    key_event_t before[]={K(F2),K(F6),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    host_keys(before,N(before));assert(graphlab_main()==0&&host_keys_used()==N(before)&&host_calculation_count()==0);
    char p[256];snprintf(p,sizeof p,"%s/GLAB7_A.DAT",dir);unlink(p);assert(rmdir(dir)==0);
    puts("Preview EXIT/MENU cancellation leaves no partial frame; camera setup before PLOT does not start a render.");
}
