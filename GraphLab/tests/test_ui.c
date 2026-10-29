#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/navigation.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.shift=1}
#define A(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.alpha=1}
#define H(k) {.key=KEY_##k,.type=KEYEV_DOWN,.held_shift=1}
#define U(k) {.key=KEY_##k,.type=KEYEV_UP}
#define T(mask) {.type=KEYEV_NONE,.held_motion=(mask)}
#define N(a) (sizeof(a)/sizeof((a)[0]))
static void run(key_event_t const *keys,size_t count) {
    host_keys(keys,count);assert(graphlab_main()==0);assert(host_keys_used()==count);assert(host_vram_guards_ok());
    assert(host_polls_outside_calculation()==0 && host_partial_updates()==0 && !host_text_seen("Loading..."));
}
static GraphState load(void){GraphState s;assert(storage_load(&s)==1);return s;}
int main(void) {
    char dir[]="/tmp/gl-ui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    /* EXE edits; fraction groups its denominator. PLOT first selects a view,
       second PLOT builds. F5 changes layer without another calculation. */
    key_event_t curve[]={
        K(F1),K(EXE),S(DEL),K(F2),K(F4),K(1),K(FRAC),K(F1),K(ADD),K(1),K(EXE),K(F5),
        K(F6),K(RIGHT),K(SUB),K(F6),K(F2),K(F5),K(VARS),K(RIGHT),K(UP),K(F6),
        K(EXIT),K(F5),K(EXIT),K(F6)
    };
    run(curve,N(curve));GraphState st=load();
    assert(!strcmp(st.curve[0].formula,"y=1/(x+1)"));assert(st.grid==0);
    assert(st.plane_x>0 && st.plane_scale>.05f && st.axes_front==0);
    assert(host_calculation_count()==1 && host_text_seen("Choose view") && host_text_seen("zero on curve"));
    /* The first digit on a selected equation is not lost. */
    key_event_t new_digit[]={K(F1),K(7),K(ADD),K(2),K(EXE),K(F5),K(EXIT),K(F6)};
    run(new_digit,N(new_digit));st=load();assert(!strcmp(st.curve[0].formula,"7+2"));
    /* EXE preserves and edits the existing expression. */
    key_event_t edit_existing[]={K(F1),K(EXE),K(ADD),K(1),K(EXE),K(F5),K(EXIT),K(F6)};
    run(edit_existing,N(edit_existing));st=load();assert(!strcmp(st.curve[0].formula,"7+2+1"));
    /* ALPHA letter as the first key begins a new formula. */
    key_event_t new_alpha[]={K(F1),A(ADD),K(SQUARE),K(EXE),K(F5),K(EXIT),K(F6)};
    run(new_alpha,N(new_alpha));st=load();assert(!strcmp(st.curve[0].formula,"x^2"));
    key_event_t new_del[]={K(F1),K(DEL),K(3),K(EXE),K(F5),K(EXIT),K(F6)};
    run(new_del,N(new_del));st=load();assert(!strcmp(st.curve[0].formula,"3"));
    key_event_t new_shift_del[]={K(F1),S(DEL),K(4),K(EXE),K(F5),K(EXIT),K(F6)};
    run(new_shift_del,N(new_shift_del));st=load();assert(!strcmp(st.curve[0].formula,"4"));
    /* + remains inside a root and exponent until RIGHT explicitly exits. */
    key_event_t root[]={K(F1),K(DEL),S(SQUARE),K(1),K(ADD),K(2),K(RIGHT),K(ADD),K(3),K(EXE),K(F5),K(EXIT),K(F6)};
    run(root,N(root));st=load();assert(!strcmp(st.curve[0].formula,"sqrt(1+2)+3"));
    key_event_t power[]={K(F1),A(ADD),K(POWER),K(1),K(ADD),K(2),K(RIGHT),K(ADD),K(3),K(EXE),K(F5),K(EXIT),K(F6)};
    run(power,N(power));st=load();assert(!strcmp(st.curve[0].formula,"x^(1+2)+3"));
    /* SUM template, ∞, editable index, exponent and factorial. */
    key_event_t sum_template[]={
        K(F1),K(DEL),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),K(F1),
        K(DOWN),K(DEL),K(1),K(UP),K(DEL),K(5),K(DOWN),K(LEFT),K(LEFT),
        K(DEL),A(COMMA),K(RIGHT),K(RIGHT),K(RIGHT),
        K(XOT),K(POWER),A(COMMA),K(RIGHT),K(FRAC),A(COMMA),K(F3),K(EXE),K(F5),K(EXIT),K(F6)
    };
    run(sum_template,N(sum_template));st=load();
    assert(!strcmp(st.curve[0].formula,"summ(1,5,k,x^(k)/(k!))"));
    assert(host_text_seen("SUM: LOWER") && host_text_seen("SUM: UPPER") &&
           host_text_seen("SUM: INDEX") && host_text_seen("SUM: EXPRESSION"));
    /* New template starts in the visibly selected term; DEL there must not
       eat the invisible comma. Both bounds and the index remain editable. */
    key_event_t product_template[]={
        K(F1),K(DEL),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),K(F4),
        K(DEL),K(UP),K(DEL),K(3),K(DOWN),K(DEL),K(2),S(LEFT),
        K(DEL),A(COMMA),S(RIGHT),A(COMMA),K(RIGHT),K(DEL),A(COMMA),K(EXE),K(F5),K(EXIT),K(F6)
    };
    run(product_template,N(product_template));st=load();
    assert(!strcmp(st.curve[0].formula,"prod(2,3,k,k)"));
    assert(host_text_seen("PROD: LOWER") && host_text_seen("PROD: UPPER") &&
           host_text_seen("PROD: INDEX") && host_text_seen("PROD: EXPRESSION"));
    /* The physical ALPHA key locks after two presses, and remains locked. */
    key_event_t alpha_twice[]={K(VARS),K(EXE),S(DEL),K(ALPHA),K(ALPHA),K(MUL),K(LEFTP),K(8),K(ALPHA),
        K(LEFTP),K(0),K(RIGHTP),K(EXE),K(F6),K(F6)};
    run(alpha_twice,N(alpha_twice));assert(host_text_seen("Result: 0") && host_text_count("ALPHA LOCK ON")>=3);
    key_event_t alpha_shift[]={K(VARS),K(EXE),S(DEL),S(ALPHA),K(MUL),K(LEFTP),K(8),S(ALPHA),
        K(LEFTP),K(0),K(RIGHTP),K(EXE),K(F6),K(F6)};
    run(alpha_shift,N(alpha_shift));assert(host_text_seen("Result: 0") && host_text_count("ALPHA LOCK ON")>=3);
    /* Like PyCEdit, F5 cycles numbers, lower-case lock and upper-case lock.
       F4 is still an independent symbol key while letters are locked. */
    key_event_t input_cycle[]={K(F1),K(DEL),K(F5),K(ADD),K(F4),K(F5),K(ADD),K(F5),K(ADD),K(7),K(EXE),K(F5),K(EXIT),K(F6)};
    run(input_cycle,N(input_cycle));st=load();assert(!strcmp(st.curve[0].formula,"x=X+7"));
    assert(host_text_seen("ALPHA LOCK ON")&&host_text_seen("UPPER LOCK ON")&&host_text_seen("123 INPUT"));
    /* An upper-case letter typed on a selected slot starts a new formula
       without losing its case when the event is forwarded to the editor. */
    key_event_t new_upper[]={K(F1),K(EXE),K(F5),K(F5),K(EXE),K(ADD),K(ALPHA),K(SQUARE),K(EXE),K(F5),K(EXIT),K(F6)};
    run(new_upper,N(new_upper));st=load();assert(!strcmp(st.curve[0].formula,"X^2"));
    /* Physical single ALPHA is one-shot, while OPTN inside the editor opens
       settings and returns to the same unfinished formula. */
    key_event_t editor_options[]={K(F1),K(DEL),K(7),K(OPTN),K(RIGHT),K(F6),K(ADD),K(2),K(EXE),K(F5),K(EXIT),K(F6)};
    run(editor_options,N(editor_options));st=load();assert(!strcmp(st.curve[0].formula,"7+2") && st.curve_width==2);
    key_event_t physical_sum[]={K(F1),K(DEL),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),K(F1),
        K(XOT),K(POWER),K(ALPHA),K(8),K(RIGHT),K(FRAC),K(ALPHA),K(8),K(F3),
        K(EXE),K(F5),K(EXIT),K(F6)};
    run(physical_sum,N(physical_sum));st=load();assert(!strcmp(st.curve[0].formula,"summ(0,inf,n,x^(n)/(n!))"));
    /* Exercise raw ALPHA text entry, not just the menu templates. */
    key_event_t alpha_sum[]={
        K(VARS),K(EXE),S(DEL),S(ALPHA),K(MUL),K(1),K(7),K(7),S(ALPHA),K(LEFTP),K(0),K(COMMA),
        S(ALPHA),K(LEFTP),K(8),K(TAN),S(ALPHA),K(COMMA),S(ALPHA),K(8),S(ALPHA),K(COMMA),
        K(2),K(POWER),A(8),K(RIGHT),K(FRAC),A(8),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),K(F3),
        K(RIGHT),K(RIGHTP),K(EXE),K(F6),K(F6)
    };
    run(alpha_sum,N(alpha_sum));assert(host_text_seen("Result: 7.389"));
    /* Regression from the real calculator: a hand-typed empty summ() must
       put the term in the term, with independent editable empty bounds. */
    key_event_t typed_empty[]={K(F1),K(DEL),S(ALPHA),K(MUL),K(1),K(7),K(7),S(ALPHA),
        K(LEFTP),K(RIGHTP),K(XOT),K(UP),K(2),K(DOWN),K(1),S(LEFT),A(8),
        K(EXE),K(F5),K(EXIT),K(F6)};
    run(typed_empty,N(typed_empty));st=load();assert(!strcmp(st.curve[0].formula,"summ(1,2,n,x)"));
    assert(host_text_seen("SUM: UPPER")&&host_text_seen("SUM: LOWER")&&host_text_seen("SUM: EXPRESSION"));
    key_event_t return_left[]={K(F1),K(DEL),K(0),K(DOT),K(5),K(MUL),
        S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),K(F1),K(XOT),
        K(LEFT),K(LEFT),K(LEFT),K(DEL),K(8),K(EXE),K(F5),K(EXIT),K(F6)};
    run(return_left,N(return_left));st=load();assert(!strcmp(st.curve[0].formula,"0.8*summ(0,inf,n,x)"));
    /* Fractal: set view first, no recomputation for irrelevant F5/F6. */
    key_event_t fractal[]={
        K(F3),K(EXE),S(DEL),K(F3),K(POWER),K(3),K(RIGHT),K(ADD),S(F5),K(F3),K(EXE),
        K(F2),K(F3),K(F4),K(F5),K(F6),K(RIGHT),K(SUB),K(F6),K(F5),K(F6),K(OPTN),K(F6),K(ADD),
        K(EXIT),K(F5),K(EXIT),K(F6)
    };
    run(fractal,N(fractal));st=load();
    assert(!strcmp(st.recurrence[0],"z^(3)+c") && st.fractal.julia==1 && st.fractal.iterations==80 && st.fractal.block==3);
    assert(host_calculation_count()==2 && host_text_seen("FRACTAL VIEW") && host_text_seen("DETAIL 3 px"));
    key_event_t second_fractal[]={K(F3),K(RIGHT),K(EXE),S(DEL),K(F3),K(SQUARE),K(SUB),S(F5),K(F3),K(EXE),K(F5),K(EXIT),K(F6)};
    run(second_fractal,N(second_fractal));st=load();
    assert(!strcmp(st.recurrence[0],"z^(3)+c") && !strcmp(st.recurrence[1],"z^2-c") && st.fractal_slot==1);
    /* Preserve and exercise the original Q ray controls as well as voxels. */
    st=load();st.render3_mode=1;assert(storage_save(&st)==0);
    /* Exact 3D observer, plot, layer change, separate MOVE and TURN, no
       recalculation while setting a new view, then explicit PLOT and probe. */
    key_event_t surface[]={
        K(F2),K(F6),K(VARS),K(EXE),S(DEL),K(3),K(EXE),K(DOWN),K(EXE),S(DEL),K(4),K(EXE),
        K(DOWN),K(EXE),S(DEL),K(5),K(EXE),K(F6),K(F6),K(F5),K(6),K(6),
        K(1),K(1),K(UP),K(OPTN),K(F6),K(F4),K(F6),K(VARS),K(EXE),S(DEL),K(2),K(EXE),K(F6),
        K(EXIT),K(F5),K(EXIT),K(F6)
    };
    run(surface,N(surface));st=load();
    assert(st.detail3==2 && st.eye_x!=3 && st.eye_y!=4 && st.eye_z>5);
    assert(host_calculation_count()==2 && host_text_seen("3D VALUES") && host_text_seen("zero on surface"));
    assert(host_text_seen("Q20") && host_text_seen("Q32") && host_text_seen("SETTINGS"));
    /* After a completed image, movement draws only cached visible points;
       no formula calculation is started until PLOT is pressed again. */
    GraphState preview_state;state_defaults(&preview_state);preview_state.render3_mode=1;
    strcpy(preview_state.surface[0].formula,"x^2+y^2+z^2=1");assert(storage_save(&preview_state)==0);
    key_event_t preview_ray[]={K(F2),K(F6),K(F6),K(RIGHT),K(UP),K(1),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(preview_ray,N(preview_ray));assert(host_calculation_count()==2&&host_text_seen("PREVIEW"));
    strcpy(preview_state.surface[0].formula,"x^2+y^2+z^2<=1");assert(storage_save(&preview_state)==0);
    key_event_t preview_voxel[]={K(F2),K(F6),K(F6),K(RIGHT),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(preview_voxel,N(preview_voxel));assert(host_calculation_count()==2&&host_text_seen("PREVIEW")&&host_text_seen("V20*"));
    /* 3D arrows always turn; 1/0 move vertically without a mode switch.
       Held SHIFT accelerates both distinct presses without affecting turning. */
    GraphState fresh;state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t camera[]={K(F2),K(F6),K(RIGHT),K(UP),H(1),H(1),K(1),K(1),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(camera,N(camera));st=load();
    assert(st.camera_yaw<fresh.camera_yaw && st.camera_pitch<fresh.camera_pitch);
    assert(fabsf(st.eye_x-fresh.eye_x)<1e-5f && fabsf(st.eye_y-fresh.eye_y)<1e-5f);
    assert(fabsf(st.eye_z-(fresh.eye_z+8*.065f*fresh.volume_radius))<1e-4f);
    assert(host_text_seen("1/0: up/down"));
    /* ALPHA tap toggles slow, held ALPHA is momentary and must not latch.
       Timer frames simulate a held movement key, without a recalculation. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t slow_nav[]={K(F2),K(F6),K(ALPHA),U(ALPHA),K(1),K(RIGHT),K(ALPHA),U(ALPHA),K(1),
        K(ALPHA),{.key=KEY_1,.type=KEYEV_DOWN,.held_alpha=1},U(ALPHA),K(1),T(NAV_RISE),T(NAV_RISE),
        K(EXIT),K(F5),K(EXIT),K(F6)};
    run(slow_nav,N(slow_nav));st=load();
    assert(fabsf(st.camera_yaw-(fresh.camera_yaw-.065f))<1e-5f);
    assert(fabsf(st.eye_z-(fresh.eye_z+4.4f*.065f*fresh.volume_radius))<1e-4f);
    assert(host_text_seen("SLOW") && host_calculation_count()==0);
    /* Setting exact eye coordinates does not rotate the user's camera. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t eye_without_turn[]={K(F2),K(F6),K(VARS),K(EXE),S(DEL),K(3),K(EXE),K(F6),
        K(EXIT),K(F5),K(EXIT),K(F6)};
    run(eye_without_turn,N(eye_without_turn));st=load();
    assert(st.eye_x==3 && st.camera_yaw==fresh.camera_yaw && st.camera_pitch==fresh.camera_pitch);
    /* The clipping radius and centers are visible without scrolling. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t clip_first[]={K(OPTN),K(DOWN),K(EXE),S(DEL),K(1),K(0),K(EXE),K(F5),K(F6),K(F6)};
    run(clip_first,N(clip_first));st=load();
    assert(st.volume_radius==10 && host_text_seen("3D clip half-size") && host_text_seen("fixed at origin"));
    /* OPTN settings and thickness changes reuse the cached 2D plot. */
    key_event_t width[]={K(OPTN),K(RIGHT),K(RIGHT),K(F5),K(F6),K(F1),K(F6),K(F6),
        S(F2),K(OPTN),K(RIGHT),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(width,N(width));st=load();
    assert(st.curve_width==5 && host_calculation_count()==1 && host_text_seen("2D line width (px)"));
    /* Cancellation is tested during actual calculations in all three modes. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t cancel2d_pan[]={K(F1),K(F6),K(F6),K(RIGHT),K(EXIT),K(F6)};
    host_keys(cancel2d_pan,N(cancel2d_pan));host_async_on_calculation((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},2,1);
    assert(graphlab_main()==0&&host_keys_used()==N(cancel2d_pan)&&host_calculation_count()==2&&host_partial_updates()==0&&host_vram_guards_ok());
    key_event_t cancelf_pan[]={K(F3),K(F6),K(F6),K(RIGHT),K(EXIT),K(F6)};
    host_keys(cancelf_pan,N(cancelf_pan));host_async_on_calculation((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},2,1);
    assert(graphlab_main()==0&&host_keys_used()==N(cancelf_pan)&&host_calculation_count()==2&&host_partial_updates()==0&&host_vram_guards_ok());
    key_event_t cancel2d[]={K(F1),K(F6),K(F6),K(EXIT),K(F6)};
    host_keys(cancel2d,N(cancel2d));host_async_key((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},5);
    assert(graphlab_main()==0 && host_keys_used()==N(cancel2d) && host_vram_guards_ok() && host_polls_outside_calculation()==0);
    host_keys(cancel2d,N(cancel2d));host_async_key((key_event_t){.key=KEY_MENU,.type=KEYEV_DOWN},5);
    assert(graphlab_main()==0 && host_keys_used()==N(cancel2d) && host_vram_guards_ok() && host_menus_opened()==1);
    key_event_t cancel3d[]={K(F2),K(F6),K(F6),K(EXIT),K(F6)};
    host_keys(cancel3d,N(cancel3d));host_async_key((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},5);
    assert(graphlab_main()==0 && host_keys_used()==N(cancel3d) && host_vram_guards_ok() && host_polls_outside_calculation()==0);
    key_event_t cancelf[]={K(F3),K(F6),K(F6),K(EXIT),K(F6)};
    host_keys(cancelf,N(cancelf));host_async_key((key_event_t){.key=KEY_EXIT,.type=KEYEV_DOWN},5);
    assert(graphlab_main()==0 && host_keys_used()==N(cancelf) && host_vram_guards_ok() && host_polls_outside_calculation()==0);
    /* Invalid high-magnification views remain recoverable with F3 RESET. */
    st=load();st.plane_x=1000000;st.plane_scale=.00001f;assert(storage_save(&st)==0);
    key_event_t recover[]={K(F1),K(F6),K(F3),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(recover,N(recover));st=load();assert(st.plane_x==0 && fabsf(st.plane_scale-.05f)<1e-6f);
    assert(host_text_seen("FLOAT PRECISION"));
    /* DETAIL has no effect on the pixel-by-pixel explicit path, so it must
       not trigger another calculation when only explicit curves are enabled. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t explicit_detail[]={K(F1),K(F6),K(F6),K(F4),K(F4),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(explicit_detail,N(explicit_detail));st=load();
    assert(st.detail2==0 && host_calculation_count()==1);
    /* Comparisons are reachable using SHIFT+F5 and preserve the first symbol. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t inequality_input[]={K(F1),K(DEL),K(F2),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),
        K(F2),K(XOT),K(EXE),K(F5),K(F6),K(F6),K(VARS),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(inequality_input,N(inequality_input));st=load();assert(!strcmp(st.curve[0].formula,"y<=x"));
    assert(host_calculation_count()==1 && (host_text_seen("TRUE")||host_text_seen("FALSE")));
    /* Step snaps to realizable N; renderer and 2D tolerance stay independent. */
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t new_settings[]={K(OPTN),K(UP),K(EXE),S(DEL),K(1),K(EXE),
        K(UP),K(RIGHT),K(UP),K(UP),K(RIGHT),K(UP),K(RIGHT),K(F5),K(F6),K(F6)};
    run(new_settings,N(new_settings));st=load();
    assert(st.follow_eye==0 && st.voxel_div==7 && st.render3_mode==1 && st.accuracy2==2);
    assert(host_text_seen("Voxel step (world)") && host_text_seen("2D accuracy (px)"));
    /* Manual Q, invalid entry, saved custom value and stale-preview invalidation. */
    state_defaults(&fresh);fresh.render3_mode=1;strcpy(fresh.surface[0].formula,"x^2+y^2+z^2=1");
    assert(storage_save(&fresh)==0);
    key_event_t quality[]={
        K(OPTN),K(UP),K(UP),K(UP),K(EXE),S(DEL),K(1),K(7),K(EXE),K(RIGHT),
        K(EXE),S(DEL),K(4),K(9),K(EXE),K(F5),K(F6),
        K(F2),K(F6),K(F6),K(RIGHT),K(OPTN),K(UP),K(UP),K(UP),
        K(EXE),S(DEL),K(1),K(3),K(EXE),K(F6),K(F6),K(EXIT),K(F5),K(EXIT),K(F6)
    };
    run(quality,N(quality));st=load();
    assert(state_ray_quality(&st)==13&&host_calculation_count()==2);
    assert(host_text_seen("Ray grid divisions")&&host_text_seen("Q18")&&host_text_seen("Q13")&&
           host_text_seen("PREVIEW")&&host_text_seen("Ray grid requires an integer Q from 4 to 48."));
    assert(state_set_ray_quality(&st,48)&&storage_save(&st)==0);
    key_event_t quality_cycle[]={K(F2),K(F6),K(F4),K(F4),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(quality_cycle,N(quality_cycle));st=load();
    assert(state_ray_quality(&st)==12&&host_text_seen("Q48")&&host_text_seen("Q4")&&host_text_seen("Q12"));
    /* An old moving/offset 1.03 domain is normalized once on load. Moving the
       camera afterwards cannot translate it; +/- do not change field of view. */
    state_defaults(&fresh);fresh.follow_eye=1;fresh.volume_x=8;fresh.volume_y=-4;fresh.volume_z=2;
    fresh.eye_x=42;fresh.camera_zoom=2;assert(storage_save(&fresh)==0);
    key_event_t fixed_box[]={K(F2),K(F6),K(6),K(8),K(ADD),K(SUB),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(fixed_box,N(fixed_box));st=load();
    assert(st.volume_x==0 && st.volume_y==0 && st.volume_z==0 && !st.follow_eye && st.camera_zoom==1);
    assert(host_text_seen("3D cube reset to origin"));
    state_defaults(&fresh);assert(storage_save(&fresh)==0);
    key_event_t ignored_zoom[]={K(F2),K(F6),K(F6),K(ADD),K(SUB),K(EXIT),K(F5),K(EXIT),K(F6)};
    run(ignored_zoom,N(ignored_zoom));st=load();assert(st.camera_zoom==1 && host_calculation_count()==1);
    /* RAY remains available for equations, but a 3D inequality is displayed
       as a bounded voxel volume and marked with V* instead of a false Q. */
    state_defaults(&fresh);fresh.render3_mode=1;strcpy(fresh.surface[0].formula,"x^2+y^2+z^2<=4");
    assert(storage_save(&fresh)==0);
    key_event_t volume[]={K(F2),K(F6),K(F6),K(VARS),K(F6),K(EXIT),K(EXIT),K(F6)};
    run(volume,N(volume));assert(host_calculation_count()==1 && host_text_seen("V20*") && host_text_seen("TRUE"));
    char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLQ8_A.DAT",dir);unlink(path);snprintf(path,sizeof path,"%s/GLQ8_B.DAT",dir);unlink(path);assert(rmdir(dir)==0);
    puts("Scripted UI: new/edit formula, ALPHA, sums, view selection, 2D/3D/fractal silent calculation/caching, move/turn, coordinates and cancellation passed.");
}
