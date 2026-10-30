/* Synthetic fx-CG50 keyboard matrix, actual gint keydev and GraphLab progress.
   QEMU runs real SH instructions but not a calculator/OS. */
#include "../src/state.h"
#include "../src/navigation.h"
#include "../src/ray.h"
#include <gint/drivers/keydev.h>
#include <gint/hardware.h>
#include <gint/display.h>
#include <string.h>
uint8_t target_stack[16384] __attribute__((aligned(16)));
uint32_t gint[HW_KEYS];static keydev_t dev;static uint8_t scan[12];static int tick_step;
static unsigned char arena[2*1024*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return 0;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
int write(int fd,const void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int read(int fd,void *p,unsigned n){(void)fd;(void)p;(void)n;return -1;}
int close(int fd){(void)fd;return -1;}
int lseek(int fd,int n,int w){(void)fd;(void)n;(void)w;return -1;}
void _Exit(int n){(void)n;for(;;){}}
static uint16_t guarded[DWIDTH*DHEIGHT+2],layer[PLOT_W*PLOT_H];
uint16_t *gint_vram=guarded+1;static int display_updates;
void dclear(color_t c){for(int i=0;i<DWIDTH*DHEIGHT;i++)gint_vram[i]=c;}
void drect(int a,int b,int c,int d,int color){(void)a;(void)b;(void)c;(void)d;(void)color;}
void drect_border(int a,int b,int c,int d,int color,int w,int border){(void)a;(void)b;(void)c;(void)d;(void)color;(void)w;(void)border;}
void dtext(int x,int y,int color,char const *s){(void)x;(void)y;(void)color;(void)s;}
void dprint(int x,int y,int color,char const *fmt,...){(void)x;(void)y;(void)color;(void)fmt;}
void dsize(char const *s,font_t const *font,int *w,int *h){(void)s;(void)font;if(w)*w=0;if(h)*h=0;}
void dupdate(void){display_updates++;}
void render3_axes(Camera const *cam){(void)cam;}
keydev_t *keydev_std(void){return &dev;}
int keydown(int key){return keydev_keydown(&dev,key);}
key_event_t pollevent(void){dev.time+=tick_step;return keydev_unqueue_event(&dev);}
void graphlab_test_progress_init(void);int graphlab_test_progress_poll(int preview);
int graphlab_test_progress_key(void);int graphlab_test_progress_shift(void);
int graphlab_test_progress_command(void);int graphlab_test_progress_command_shift(void);
int graphlab_test_progress_menu(void);int graphlab_test_progress_cancel(void);
void graphlab_test_progress_apply(GraphState *s);
void graphlab_test_live_init(Camera const *cam,uint16_t const *layer);
int graphlab_test_live_poll(int done,int total,void *unused);
static void reset(void){keydev_init(&dev);memset(scan,0,sizeof scan);tick_step=0;gint[HWCALC]=HWCALC_FXCG50;graphlab_test_progress_init();}
static void key(int code,int down){int row=code>>4,col=7-(code&7);
    if(down)scan[row]|=(uint8_t)(1u<<col);else scan[row]&=(uint8_t)~(1u<<col);
    keydev_process_state(&dev,scan);keydev_tick(&dev,8000);}
#define REQUIRE(c) do {if(!(c))return __LINE__;}while(0)
int main(void)
{
    static GraphState s,expected;
    for(int i=0;i<1024;i++)target_stack[i]=0xa5;
    guarded[0]=guarded[DWIDTH*DHEIGHT+1]=0x5aa5;
    for(int preview=0;preview<=1;preview++){
        reset();key(KEY_SHIFT,1);key(KEY_1,1);key(KEY_1,0);key(KEY_SHIFT,0);
        REQUIRE(!keydown(KEY_SHIFT)&&!keydown(KEY_1));
        REQUIRE(graphlab_test_progress_poll(preview)==0);
        REQUIRE(graphlab_test_progress_key()==KEY_1&&graphlab_test_progress_shift());
        memset(&s,0,sizeof s);s.eye_y=6;s.volume_radius=3;s.camera_zoom=1;expected=s;
        REQUIRE(navigation_step(&expected,NAV_RISE,0,1));graphlab_test_progress_apply(&s);
        REQUIRE(s.eye_z==expected.eye_z&&s.eye_z>0);
    }
    /* A held key without an event stops refinement but not moving preview. */
    reset();key(KEY_8,1);(void)pollevent();
    REQUIRE(graphlab_test_progress_poll(1)==1);
    REQUIRE(graphlab_test_progress_poll(0)==0);
    reset();key(KEY_MENU,1);key(KEY_MENU,0);REQUIRE(!graphlab_test_progress_poll(0)&&graphlab_test_progress_menu());
    reset();key(KEY_EXIT,1);key(KEY_EXIT,0);REQUIRE(!graphlab_test_progress_poll(1)&&graphlab_test_progress_cancel());
    /* Raw, already-released SHIFT+F3 is delivered rather than lost when it
       interrupts an automatically running refinement. */
    reset();key(KEY_SHIFT,1);key(KEY_F3,1);key(KEY_F3,0);key(KEY_SHIFT,0);
    REQUIRE(!graphlab_test_progress_poll(0));
    REQUIRE(graphlab_test_progress_command()==KEY_F3&&graphlab_test_progress_command_shift());
    reset();key(KEY_F1,1);REQUIRE(!graphlab_test_progress_poll(0)&&graphlab_test_progress_cancel());
    /* Exercise real row-wise ray rendering -> real progress -> real present3d
       with a fake LCD, synthetic time and a guarded 16 KiB SH stack. */
    reset();tick_step=3;used=0;display_updates=0;
    static Parsed parsed;static Program p;static EvalVars vars;static RayGridCache grid;static ScreenRefine refine;
    REQUIRE(expr_parse(&parsed,"x^2+y^2+z^2=1",0)&&expr_compile(&parsed,&p,0,0));
    Volume v={0,0,0,3,6,0};Camera cam={.yaw=.4f,.pitch=.3f,.zoom=1,.radius=3,.eye_y=6,.perspective=1};
    RaySurface surface={&p,0};RayStats st;REQUIRE(refine_begin(&refine,2));
    RayRenderOptions op={.grid=&grid,.screen_step=2,.layer=layer,.refine=&refine};
    REQUIRE(ray3d_render_ex(&surface,1,&vars,&v,&cam,&op,0,0,&st)==1);
    op.screen_step=1;op.final_stream=1;graphlab_test_live_init(&cam,layer);
    REQUIRE(ray3d_render_ex(&surface,1,&vars,&v,&cam,&op,graphlab_test_live_poll,0,&st)==1);
    REQUIRE(display_updates>0&&refine.last_step==1&&st.rays==52303);
    dclear(C_WHITE);RayRenderOptions direct={.grid=&grid,.screen_step=1};
    REQUIRE(ray3d_render_ex(&surface,1,&vars,&v,&cam,&direct,0,0,&st)==1);
    for(int y=0;y<PLOT_H;y++)for(int x=0;x<PLOT_W;x++)
        REQUIRE(layer[y*PLOT_W+x]==gint_vram[(y+PLOT_Y0)*DWIDTH+x+PLOT_X0]);
    REQUIRE(guarded[0]==0x5aa5&&guarded[DWIDTH*DHEIGHT+1]==0x5aa5);
    for(int i=0;i<1024;i++)REQUIRE(target_stack[i]==0xa5);
    return 0;
}
