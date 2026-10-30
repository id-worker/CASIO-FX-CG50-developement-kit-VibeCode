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
int main(void)
{
    char dir[]="/tmp/gl116-pixelui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.volume_radius=10;
    strcpy(s.surface[0].formula,"sin(x)+sin(y)+sin(z)=0");
    strcpy(s.surface[1].formula,"z=x-y");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t path[]={K(F2),K(F6),K(2),
       K(F2),K(EXE),S(DEL),K(NEG),K(3),K(DOT),K(4),K(EXE),
       K(DOWN),K(EXE),S(DEL),K(NEG),K(1),K(DOT),K(1),K(EXE),K(F6),
       K(F4),K(F3),K(EXIT),K(EXIT),K(EXIT),K(F6)};
    host_keys(path,N(path));assert(graphlab_main()==0&&host_keys_used()==N(path)&&host_vram_guards_ok());
    assert(host_text_seen("PIXEL CURSOR"));
    assert(host_text_seen("EXACT INPUT XY"));
    assert(host_text_seen("AT XY: NOT FOUND"));
    assert(host_text_seen("IN PIXEL: FOUND ~"));
    assert(host_text_seen("ENTER XY FOR Z SEARCH"));
    /* Extend only Z above the clipping cube; an intersection at z=5 is
       reachable without increasing the XY map's default radius. */
    state_defaults(&s);s.volume_radius=3;s.map_extend_up=4;
    strcpy(s.surface[0].formula,"x=0");strcpy(s.surface[1].formula,"z=5");s.surface[1].active=1;
    assert(storage_save(&s)==0);
    key_event_t extended[]={K(F2),K(F6),K(2),K(F2),K(F6),K(EXIT),K(EXIT),K(EXIT),K(F6)};
    host_keys(extended,N(extended));assert(graphlab_main()==0&&host_keys_used()==N(extended)&&host_vram_guards_ok());
    assert(host_text_seen("AT XY: FOUND ~")&&host_text_seen("Z  5"));
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Pixel UI: exact input, NEAR to pixel, FOUND representative and NEXT passed (host only).");
}
