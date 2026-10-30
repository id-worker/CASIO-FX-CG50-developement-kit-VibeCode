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
#define A(k) {.key=KEY_##k,.type=KEYEV_DOWN,.alpha=1,.mod=1}
#define N(a) (sizeof(a)/sizeof *(a))
static void state(char const *first,char const *second)
{
    GraphState s;state_defaults(&s);strcpy(s.curve[0].formula,first);s.curve[0].active=1;
    strcpy(s.curve[1].formula,second);s.curve[1].active=1;assert(storage_save(&s)==0);
}
static GraphState run(key_event_t *events,size_t n)
{
    host_keys(events,n);assert(graphlab_main()==0&&host_keys_used()==n&&host_vram_guards_ok());
    assert(host_polls_outside_calculation()==0&&host_partial_updates()==0);
    GraphState s;assert(storage_load(&s)==1);return s;
}
int main(void)
{
    char dir[]="/tmp/gl112-entry-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;
    state("x","x+10");
    key_event_t append[]={K(F1),K(ADD),K(2),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(append,N(append));assert(!strcmp(s.curve[0].formula,"x+2")&&host_calculation_count()==0);
    assert(host_text_seen("EDIT 2D EQUATION")&&!host_text_seen("2D GRAPH"));
    state("x","x+10");
    key_event_t power[]={K(F1),K(SQUARE),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(power,N(power));assert(!strcmp(s.curve[0].formula,"x^2")&&host_calculation_count()==0);
    state("x","x+10");
    key_event_t fraction[]={K(F1),K(FRAC),K(2),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(fraction,N(fraction));assert(!strcmp(s.curve[0].formula,"x/(2)"));
    state("x","x+10");
    key_event_t seed_arrow[]={K(F1),K(RIGHT),K(ADD),K(1),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(seed_arrow,N(seed_arrow));assert(!strcmp(s.curve[0].formula,"x+1"));
    state("x","x+10");
    key_event_t shifted_page[]={K(F1),S(F5),K(ADD),K(F1),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(shifted_page,N(shifted_page));assert(!strcmp(s.curve[0].formula,"x+a"));
    assert(host_text_count("Saved to calculator storage.")==2);
    /* EXE opens the editor but that same event cannot accept or PLOT. */
    state("x","x+10");
    key_event_t exe_once[]={K(F1),K(EXE),K(ADD),K(3),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(exe_once,N(exe_once));assert(!strcmp(s.curve[0].formula,"x+3"));
    assert(host_text_seen("EDIT 2D EQUATION")&&!host_text_seen("2D GRAPH")&&host_calculation_count()==0);
    /* PLOT stays a list action: only view selection, never an editor. */
    state("x","x+10");
    key_event_t plot_once[]={K(F1),K(F6),K(EXIT),K(EXIT),K(F6)};
    (void)run(plot_once,N(plot_once));assert(host_text_seen("2D GRAPH")&&!host_text_seen("EDIT 2D EQUATION"));
    assert(host_calculation_count()==0);
    /* DOWN and F2 belong to the list; other math keys act on selected slot. */
    state("x","x+10");
    key_event_t selection[]={K(F1),K(DOWN),K(ADD),K(2),K(EXE),K(F2),K(F5),K(EXIT),K(F6)};
    s=run(selection,N(selection));assert(!strcmp(s.curve[0].formula,"x")&&!strcmp(s.curve[1].formula,"x+10+2"));
    assert(s.curve[1].active==0);
    state("x+10","x+10");
    key_event_t alpha_new[]={K(F1),A(ADD),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(alpha_new,N(alpha_new));assert(!strcmp(s.curve[0].formula,"x"));
    state("x+10","x");
    key_event_t delete_new[]={K(F1),K(DEL),K(7),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(delete_new,N(delete_new));assert(!strcmp(s.curve[0].formula,"7"));
    state("x+10","x");
    key_event_t shift_delete_new[]={K(F1),S(DEL),K(8),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(shift_delete_new,N(shift_delete_new));assert(!strcmp(s.curve[0].formula,"8"));
    state("x+","x");
    key_event_t shifted_sqrt[]={K(F1),S(SQUARE),K(2),K(EXE),K(F5),K(EXIT),K(F6)};
    s=run(shifted_sqrt,N(shifted_sqrt));assert(!strcmp(s.curve[0].formula,"x+sqrt(2)"));
    state("summ(0,1,n,x)","x");
    key_event_t shift_navigation[]={K(F1),S(UP),K(EXIT),K(F5),K(EXIT),K(F6)};
    s=run(shift_navigation,N(shift_navigation));assert(!strcmp(s.curve[0].formula,"summ(0,1,n,x)"));
    assert(host_text_seen("EDIT 2D EQUATION"));
    /* ESC from a seeded editor must leave the existing expression untouched. */
    state("x+10","x");
    key_event_t cancel[]={K(F1),K(LEFTP),K(EXIT),K(F5),K(EXIT),K(F6)};
    s=run(cancel,N(cancel));assert(!strcmp(s.curve[0].formula,"x+10"));
    char path[256];snprintf(path,sizeof path,"%s/GLAB8_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB8_B.DAT",dir);unlink(path);rmdir(dir);
    puts("Equation entry: seeded edits, modifiers, list priority, EXE/PLOT isolation and cancel passed.");
}
