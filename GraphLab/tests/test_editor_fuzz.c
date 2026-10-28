/* Random event streams through the real editor + software VRAM. */
#include "../src/state.h"
#include "host_gint.h"
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
int graphlab_test_editor(char *destination,int mode);
static unsigned rng=0x8fb34c21u,frames;
static unsigned rnd(void){rng^=rng<<13;rng^=rng>>17;rng^=rng<<5;return rng;}
void graphlab_editor_trace(char const *s,int cursor,char const *error)
{
    size_t len=strlen(s);assert(len<EXPR_MAX && cursor>=0 && cursor<=(int)len);
    assert(strlen(error)<90);frames++;
}
int main(void)
{
    static int const keys[]={KEY_0,KEY_1,KEY_2,KEY_3,KEY_4,KEY_5,KEY_7,KEY_8,KEY_DOT,
        KEY_ADD,KEY_SUB,KEY_MUL,KEY_DIV,KEY_FRAC,KEY_LEFTP,KEY_RIGHTP,KEY_COMMA,
        KEY_SIN,KEY_COS,KEY_LN,KEY_POWER,KEY_SQUARE,KEY_XOT,KEY_LEFT,KEY_RIGHT,
        KEY_UP,KEY_DOWN,KEY_DEL,KEY_F1,KEY_F2,KEY_F3,KEY_F4,KEY_F5,KEY_ALPHA};
    key_event_t events[97];
    for(int trial=0;trial<160;trial++){
        for(int i=0;i<96;i++){
            events[i]=(key_event_t){.key=keys[rnd()%(sizeof keys/sizeof *keys)],.type=KEYEV_DOWN};
            if((rnd()&7u)==0)events[i].shift=1;
        }
        events[96]=(key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN};
        char text[EXPR_MAX]="";host_keys(events,97);
        assert(!graphlab_test_editor(text,1));assert(host_vram_guards_ok());
    }
    printf("Actual editor random key streams: %u frames, bounds/VRAM checks passed.\n",frames);
}
