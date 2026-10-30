#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.shift=1,.mod=1}
#define N(a) (sizeof(a)/sizeof *(a))
static void run(key_event_t const *events,size_t n)
{
    host_keys(events,n);assert(graphlab_main()==0&&host_keys_used()==n&&host_vram_guards_ok());
    assert(host_polls_outside_calculation()==0&&host_partial_updates()==0);
}
int main(void)
{
    char dir[]="/tmp/gl112-mapui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.map_xy_quality=0;s.map_z_quality=0;s.cursor_quality=2;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");
    strcpy(s.surface[1].formula,"x=0");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t path[]={K(F2),K(F6),K(2),K(F3),K(RIGHT),K(F2),K(EXE),S(DEL),K(0),K(EXE),
        K(DOWN),K(EXE),S(DEL),K(1),K(DOT),K(2),K(EXE),K(F6),K(F3),K(EXIT),K(EXIT),K(EXIT),K(F6)};
    run(path,N(path));
    fprintf(stderr,"map calculations=%d\n",host_calculation_count());
    assert(host_calculation_count()==4);
    assert(host_text_seen("3D CROSSINGS")&&host_text_seen("AT XY: FOUND ~"));
    assert(host_text_seen("1 / 2")&&host_text_seen("2 / 2"));
    assert(host_text_seen("PIXEL: NOT FOUND")&&host_text_seen("NEAREST MAPPED ~"));
    assert(host_text_seen("IN PIXEL: FOUND ~"));
    assert(host_text_seen("ENTER XY FOR Z SEARCH")&&host_text_seen("Z  -1.6")&&host_text_seen("Z  1.6"));
    /* OPTN pages cycle; point preview remains independent of automatic map. */
    state_defaults(&s);assert(storage_save(&s)==0);
    key_event_t settings[]={K(OPTN),K(OPTN),K(UP),K(LEFT),K(OPTN),K(OPTN),K(F5),K(F6),K(F6)};
    run(settings,N(settings));assert(storage_load(&s)==1);
    assert(s.preview_ray_step==31&&s.map_zmin==-3&&s.map_zmax==3);
    assert(host_text_seen("SETTINGS: 3D 2/3")&&host_text_seen("SETTINGS: MAP 3/3")&&host_calculation_count()==0);
    /* Changing preview spacing in the map does not invalidate a finished 3D frame. */
    state_defaults(&s);s.legacy_render_mode=2;s.map_xy_quality=0;s.map_z_quality=0;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");strcpy(s.surface[1].formula,"x=0");s.surface[1].active=1;
    assert(storage_save(&s)==0);
    key_event_t nested[]={K(F2),K(F6),K(F6),K(2),K(OPTN),K(OPTN),K(UP),K(LEFT),K(F6),K(EXIT),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(nested,N(nested));assert(host_calculation_count()==10);
    /* One or inequality surface must not masquerade as two surface crossing. */
    state_defaults(&s);assert(storage_save(&s)==0);
    key_event_t single[]={K(F2),K(F6),K(2),K(EXE),K(EXIT),K(EXIT),K(F6)};
    run(single,N(single));assert(host_text_seen("Turn on two 3D equations first.")&&host_calculation_count()==0);
    state_defaults(&s);strcpy(s.surface[0].formula,"z<0");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t inequ[]={K(F2),K(F6),K(2),K(EXE),K(EXIT),K(EXIT),K(F6)};
    run(inequ,N(inequ));assert(host_text_seen("Use two equalities, not inequalities."));
    /* Cancel a map while calculating; no partial frames or accidental plot. */
    state_defaults(&s);strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");strcpy(s.surface[1].formula,"x=0");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t cancel[]={K(F2),K(F6),K(2),K(EXIT),K(EXIT),K(F6)};
    host_keys(cancel,N(cancel));host_async_on_calculation((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},1,2);
    assert(graphlab_main()==0&&host_keys_used()==N(cancel)&&host_calculation_count()==1&&host_partial_updates()==0);
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Map UI: independent searches, NEXT, nearest, exact entry, settings, rejection and cancel passed.");
}
