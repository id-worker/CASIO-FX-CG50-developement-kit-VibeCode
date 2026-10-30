#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.shift=1,.mod=1}
#define N(a) (sizeof(a)/sizeof *(a))
static void reset(void){GraphState s;state_defaults(&s);s.param_a=2;s.param_b=3;s.param_c=4;s.param_t=5;assert(storage_save(&s)==0);}
static GraphState run(key_event_t const *events,size_t n)
{
    host_keys(events,n);assert(graphlab_main()==0&&host_keys_used()==n&&host_vram_guards_ok());
    assert(host_polls_outside_calculation()==0&&host_partial_updates()==0);
    GraphState s;assert(storage_load(&s)==1);return s;
}
static int near(float x,float y){return fabsf(x-y)<.0001f;}
int main(void)
{
    char dir[]="/tmp/gl1121-params-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);GraphState s;
    reset();key_event_t digit[]={K(F4),K(DOWN),K(DOWN),K(7),K(DOT),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(digit,N(digit));assert(near(s.param_a,7.5)&&near(s.param_b,3)&&host_calculation_count()==0);
    assert(host_text_seen("Parameter a")&&host_text_seen("Value updated")&&host_text_seen("EXIT: cancel"));
    reset();key_event_t all_params[]={K(F4),S(DOWN),K(4),K(EXE),K(DOWN),K(7),K(EXE),K(DOWN),K(NEG),K(2),K(EXE),
        K(DOWN),K(3),K(EXE),K(DOWN),K(DOT),K(5),K(EXE),K(DOWN),K(NEG),K(DOT),K(7),K(EXE),
        K(DOWN),K(DOT),K(2),K(EXE),K(DOWN),K(1),K(6),K(EXE),K(F5),K(F6),K(F6)};
    s=run(all_params,N(all_params));assert(near(s.volume_radius,4)&&near(s.param_a,7)&&near(s.param_b,-2)&&
        near(s.param_c,3)&&near(s.param_t,.5)&&near(s.fractal.julia_re,-.7)&&near(s.fractal.julia_im,.2)&&near(s.fractal.escape_radius,16));
    reset();key_event_t constant[]={K(F4),K(DOWN),K(DOWN),K(ALPHA),K(COS),K(EXE),K(F5),K(F6),K(F6)};
    s=run(constant,N(constant));assert(near(s.param_a,2.7182818f));
    reset();key_event_t op[]={K(F4),K(DOWN),K(DOWN),K(ADD),K(3),K(EXE),K(F5),K(F6),K(F6)};
    s=run(op,N(op));assert(near(s.param_a,5)); /* 2+3, no repeated first key */
    reset();key_event_t exe[]={K(F4),K(DOWN),K(DOWN),K(EXE),K(ADD),K(3),K(EXE),K(F5),K(F6),K(F6)};
    s=run(exe,N(exe));assert(near(s.param_a,5)&&host_calculation_count()==0);
    reset();key_event_t neg[]={K(F4),K(DOWN),K(DOWN),K(NEG),K(1),K(DOT),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(neg,N(neg));assert(near(s.param_a,-1.5));
    reset();key_event_t dot[]={K(F4),K(DOWN),K(DOWN),K(DOT),K(2),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(dot,N(dot));assert(near(s.param_a,.25));
    reset();key_event_t del[]={K(F4),K(DOWN),K(DOWN),K(DEL),K(4),K(EXE),K(F5),K(F6),K(F6)};
    s=run(del,N(del));assert(near(s.param_a,4));
    reset();key_event_t sdel[]={K(F4),K(DOWN),K(DOWN),S(DEL),K(4),K(EXE),K(F5),K(F6),K(F6)};
    s=run(sdel,N(sdel));assert(near(s.param_a,4));
    reset();key_event_t cancel[]={K(F4),K(DOWN),K(DOWN),K(7),K(EXIT),K(F5),K(F6),K(F6)};
    s=run(cancel,N(cancel));assert(near(s.param_a,2));
    reset();key_event_t root[]={K(F4),K(DOWN),K(DOWN),S(SQUARE),K(9),K(EXE),K(F5),K(F6),K(F6)};
    s=run(root,N(root));assert(near(s.param_a,3));
    reset();key_event_t symbols[]={K(F4),K(DOWN),K(DOWN),S(F5),S(F5),K(F1),K(EXE),K(F5),K(F6),K(F6)};
    s=run(symbols,N(symbols));assert(near(s.param_a,2*3.14159265f));assert(host_text_count("Saved to calculator storage.")==2);
    reset();key_event_t square[]={K(F4),K(DOWN),K(DOWN),K(SQUARE),K(EXE),K(F5),K(F6),K(F6)};
    s=run(square,N(square));assert(near(s.param_a,4));
    reset();key_event_t invalid_real[]={K(F4),K(DOWN),K(DOWN),S(SQUARE),K(NEG),K(1),K(EXE),K(F5),K(F6),K(F6)};
    s=run(invalid_real,N(invalid_real));assert(near(s.param_a,2)&&host_text_seen("outside allowed range"));
    reset();key_event_t width[]={K(F4),K(RIGHT),K(EXE),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(width,N(width));assert(s.curve_width==5);
    reset();key_event_t accuracy[]={K(F4),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),K(DOWN),
      K(DOT),K(2),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(accuracy,N(accuracy));assert(s.accuracy2==0);
    reset();key_event_t ray[]={K(F4),K(OPTN),K(DOWN),K(RIGHT),K(3),K(7),K(EXE),K(F5),K(F6),K(F6)};
    s=run(ray,N(ray));assert(s.fast_ray_q==37);
    reset();key_event_t bad_ray[]={K(F4),K(OPTN),K(DOWN),K(9),K(9),K(EXE),K(F5),K(F6),K(F6)};
    s=run(bad_ray,N(bad_ray));assert(s.fast_ray_q==8&&host_text_seen("integer Q from 4 to 48"));
    reset();key_event_t start_step[]={K(F4),K(OPTN),K(3),K(1),K(EXE),K(F5),K(F6),K(F6)};
    s=run(start_step,N(start_step));assert(s.fast_ray_step==31);
    reset();key_event_t preview_step[]={K(F4),K(OPTN),K(DOWN),K(DOWN),K(5),K(EXE),K(F5),K(F6),K(F6)};
    s=run(preview_step,N(preview_step));assert(s.preview_ray_step==5);
    /* Preview and Ray settings are on 3D page; pixel map has no quality knobs. */
    reset();key_event_t adv[]={K(F4),K(OPTN),K(UP),K(8),K(EXE),
        K(UP),K(1),K(1),K(EXE),K(UP),K(2),K(EXE),K(F5),K(F6),K(F6)};
    s=run(adv,N(adv));assert(s.preview_ray_step==8&&s.fast_ray_q==11&&s.fast_ray_step==2);
    reset();key_event_t bad_advanced[]={K(F4),K(OPTN),K(UP),K(3),K(3),K(EXE),K(F5),K(F6),K(F6)};
    s=run(bad_advanced,N(bad_advanced));assert(s.preview_ray_step==32&&host_text_seen("integer from 1 to 32"));
    reset();key_event_t toggles[]={K(F4),K(OPTN),K(UP),K(RIGHT),K(LEFT),K(F5),K(F6),K(F6)};
    s=run(toggles,N(toggles));assert(s.preview_ray_step==31&&host_calculation_count()==0);
    reset();key_event_t coords[]={K(F1),K(F6),K(VARS),K(2),K(EXE),K(DOWN),K(NEG),K(3),K(EXE),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    s=run(coords,N(coords));assert(near(s.plane_x,2)&&near(s.plane_y,-3));
    reset();key_event_t fractal[]={K(F3),K(F6),K(VARS),K(2),K(EXE),K(DOWN),K(DOT),K(5),K(EXE),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    s=run(fractal,N(fractal));assert(near(s.fractal.cx,2)&&near(s.fractal.cy,.5));
    reset();key_event_t observer[]={K(F2),K(F6),K(VARS),K(1),K(EXE),K(DOWN),K(2),K(EXE),K(DOWN),K(3),K(EXE),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    s=run(observer,N(observer));assert(near(s.eye_x,1)&&near(s.eye_y,2)&&near(s.eye_z,3));
    state_defaults(&s);s.map_xy_quality=0;s.map_z_quality=0;s.cursor_quality=2;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");strcpy(s.surface[1].formula,"x=0");s.surface[1].active=1;assert(storage_save(&s)==0);
    key_event_t map[]={K(F2),K(F6),K(2),K(F2),K(0),K(EXE),K(DOWN),K(1),K(DOT),K(2),K(EXE),K(F6),K(F3),K(EXIT),K(EXIT),K(EXIT),K(F6)};
    (void)run(map,N(map));assert(host_text_seen("Z  -1.6")&&host_text_seen("Z  1.6"));
    state_defaults(&s);s.legacy_render_mode=0;s.legacy_grid_div=6;assert(storage_save(&s)==0);
    key_event_t probe[]={K(F2),K(F6),K(F6),K(VARS),K(2),K(EXE),K(ADD),K(RIGHT),K(NEG),K(3),K(EXE),K(F6),K(EXIT),K(EXIT),K(F6)};
    (void)run(probe,N(probe));assert(host_text_seen("x = 2.3")&&host_text_seen("y = -3"));
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Parameter entry: numeric settings, quality, ranges, XY/XYZ and controls passed.");
}
