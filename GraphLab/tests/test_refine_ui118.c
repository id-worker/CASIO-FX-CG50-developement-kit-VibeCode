#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/ray.h"
#include "../src/navigation.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.shift=1,.mod=1}
#define IDLE {.type=KEYEV_NONE}
#define N(a) (sizeof(a)/sizeof *(a))
static Parsed parsed;static Program program;static unsigned char image[DWIDTH*DHEIGHT*3];
static void save(GraphState *s,int volume)
{
    state_defaults(s);s->axes=0;s->legacy_render_mode=2;s->fast_ray_q=8;s->preview_ray_step=32;s->fast_ray_step=32;
    strcpy(s->surface[0].formula,volume?"x^2+y^2+z^2<=1":"x^2+y^2+z^2=1");assert(storage_save(s)==0);
}
static void read_screen(void)
{
    FILE *fp=fopen("/tmp/gl118-ui.ppm","rb");assert(fp);char line[128];
    assert(fgets(line,sizeof line,fp)&&fgets(line,sizeof line,fp)&&fgets(line,sizeof line,fp));
    assert(fread(image,1,sizeof image,fp)==sizeof image);fclose(fp);unlink("/tmp/gl118-ui.ppm");
}
static void compare_final(GraphState *s,int volume)
{
    assert(expr_parse(&parsed,s->surface[0].formula,0)&&expr_compile(&parsed,&program,NULL,0));
    RaySurface surf={&program,0};EvalVars vars={0};Volume v={0,0,0,s->volume_radius,s->fast_ray_q,s->period_x};
    Camera cam={.yaw=s->camera_yaw,.pitch=s->camera_pitch,.zoom=s->camera_zoom,.eye_x=s->eye_x,.eye_y=s->eye_y,.eye_z=s->eye_z,
                .radius=s->volume_radius,.perspective=1};
    dclear(C_WHITE);
    RayStats st;assert(ray3d_render(&surf,1,&vars,&v,&cam,NULL,NULL,&st)==1&&st.hits>0);
    int colored=0;
    for(int y=34;y<174;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++){
        uint16_t c=(uint16_t)dgetpixel(x,y);size_t i=((size_t)y*DWIDTH+x)*3;
        unsigned r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
        if(image[i]!=r||image[i+1]!=g||image[i+2]!=b){fprintf(stderr,"final volume=%d xy=%d,%d\n",volume,x,y);assert(0);}
        colored+=c!=C_WHITE;
    }
    assert(colored>100);
}
static void normal(int volume)
{
    GraphState s;save(&s,volume);
    key_event_t keys[]={K(F2),K(F6),K(RIGHT),IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_snapshot_on_getkey(4,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==6&&host_partial_updates()==0&&host_vram_guards_ok());
    assert(host_text_seen("R8/P32"));
    assert(host_text_seen("R8/P16"));
    assert(host_text_seen("R8/P8"));
    assert(host_text_seen("R8/P4"));
    assert(host_text_seen("R8/P2"));
    assert(host_text_seen("R8/1 0%"));
    assert(host_text_seen("R8/1"));
    assert(navigation_step(&s,NAV_RIGHT,0,0));read_screen();compare_final(&s,volume);
}
static void interrupted(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),K(RIGHT),IDLE,IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_async_on_calculation((key_event_t){.key=KEY_UP,.type=KEYEV_DOWN},3,1);
    host_snapshot_on_getkey(5,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==9&&host_vram_guards_ok());
    assert(host_text_count("R8/P32")>=2&&host_text_seen("R8/1")&&host_partial_updates()==0);
    assert(navigation_step(&s,NAV_RIGHT,0,0));assert(navigation_step(&s,NAV_UP,0,0));
    read_screen();compare_final(&s,0);
}
static void moving_preview(int volume)
{
    GraphState s;save(&s,volume);
    key_event_t keys[]={K(F2),K(F6),K(RIGHT),IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));
    /* This event is delivered and released while the first preview computes. */
    host_async_on_calculation((key_event_t){.key=KEY_1,.type=KEYEV_DOWN,.held_shift=1},1,1);
    host_snapshot_on_getkey(4,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==7&&host_partial_updates()==0);
    assert(navigation_step(&s,NAV_RIGHT,0,0));assert(navigation_step(&s,NAV_RISE,0,1));
    read_screen();compare_final(&s,volume);
}
static void cancel_stage(int menu)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),K(F6),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_async_on_calculation((key_event_t){.key=menu?KEY_MENU:KEY_EXIT,.type=KEYEV_DOWN},4,1);
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==4&&host_partial_updates()==0);
    assert(host_text_seen("R8/P8")&&!host_text_seen("R8/P4"));
    assert(host_menus_opened()==menu&&host_vram_guards_ok());
}
static void direct_plot(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==6);
    assert(host_text_seen("R8/P32")&&host_text_seen("R8/P16")&&host_text_seen("R8/P2")&&host_text_seen("R8/1"));
}
static void automatic_start(int volume)
{
    GraphState s;save(&s,volume);
    /* F6 chooses 3D only; IDLE is a simulated timeout, not a PLOT press. */
    key_event_t keys[]={K(F2),K(F6),IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_snapshot_on_getkey(3,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==6);
    assert(host_text_seen("R8/1"));read_screen();compare_final(&s,volume);
}
static void live_and_interrupted(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),IDLE,IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_tick_per_poll(3);
    /* Interrupt during the final streamed pass, after a live update. */
    host_async_on_calculation((key_event_t){.key=KEY_UP,.type=KEYEV_DOWN},6,200);
    host_snapshot_on_getkey(4,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==12);
    assert(host_partial_updates()>0&&host_text_count("R8/1 ")>=2&&host_text_seen("R8/1"));
    assert(navigation_step(&s,NAV_UP,0,0));read_screen();compare_final(&s,0);assert(host_vram_guards_ok());
}
static void command_during_refinement(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),IDLE,IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_tick_per_poll(3);
    host_async_on_calculation((key_event_t){.key=KEY_F4,.type=KEYEV_DOWN},6,200);
    host_snapshot_on_getkey(4,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==12);
    assert(host_partial_updates()>0&&host_text_seen("R12/1"));
    s.fast_ray_q=12;read_screen();compare_final(&s,0);assert(host_vram_guards_ok());
}
static void view_command_during_refinement(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),IDLE,IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_tick_per_poll(3);
    host_async_on_calculation((key_event_t){.key=KEY_F5,.type=KEYEV_DOWN},6,200);
    host_snapshot_on_getkey(4,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_calculation_count()==7);
    assert(host_partial_updates()>0&&host_text_seen("R8/1"));
    s.axes_front^=1;read_screen();compare_final(&s,0);assert(host_vram_guards_ok());
}
static void observer_after_auto(void)
{
    GraphState s;save(&s,0);
    key_event_t keys[]={K(F2),K(F6),IDLE,S(VARS),K(EXE),S(DEL),K(3),K(EXE),K(F6),IDLE,K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,N(keys));host_snapshot_on_getkey(10,"/tmp/gl118-ui.ppm");
    assert(graphlab_main()==0&&host_keys_used()==N(keys)&&host_text_seen("3D OBSERVER POSITION"));
    s.eye_x=3;read_screen();compare_final(&s,0);assert(host_vram_guards_ok());
}
int main(void)
{
    char dir[]="/tmp/gl118-ui-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    normal(0);normal(1);interrupted();moving_preview(0);moving_preview(1);cancel_stage(0);cancel_stage(1);direct_plot();
    automatic_start(0);automatic_start(1);live_and_interrupted();command_during_refinement();view_command_during_refinement();observer_after_auto();
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);assert(rmdir(dir)==0);
    puts("UI progressive 32/16/8/4/2/1, interrupted motion and exact final frames passed for Ray equations and volumes.");
}
