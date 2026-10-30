#ifndef GL_HOST_GINT_H
#define GL_HOST_GINT_H
#include <stdint.h>
#include <stddef.h>
#define DWIDTH 396
#define DHEIGHT 224
#define C_WHITE 0xffff
#define C_BLACK 0
#define C_LIGHT 0xad55
#define C_DARK 0x528a
#define C_RED 0xf800
#define C_GREEN 0x07e0
#define C_BLUE 0x001f
#define C_NONE -1
#define C_RGB(r,g,b) (((r)<<11)|((g)<<6)|(b))
typedef uint16_t color_t;
struct dwindow {int left,top,right,bottom;};
extern struct dwindow dwindow;
extern uint16_t *gint_vram;
struct dwindow dwindow_set(struct dwindow r);
void dsize(char const *s, void const *font, int *w,int *h);
void dtext(int x,int y,int color,char const *s);
void dprint(int x,int y,int color,const char *fmt,...);
void dline(int x0,int y0,int x1,int y1,int color);
void dhline(int y,int color);
void dvline(int x,int color);
void drect(int x0,int y0,int x1,int y1,int color);
void drect_border(int x0,int y0,int x1,int y1,int color,int width,int border);
void dclear(color_t color);
void dupdate(void);
void dpixel(int x,int y,int color);
int dgetpixel(int x,int y);
/* Key values mirror gint's fx-CG50 keycodes for scripted UI tests. */
enum {
 KEY_F1=0x91,KEY_F2=0x92,KEY_F3=0x93,KEY_F4=0x94,KEY_F5=0x95,KEY_F6=0x96,
 KEY_SHIFT=0x81,KEY_OPTN=0x82,KEY_VARS=0x83,KEY_MENU=0x84,KEY_LEFT=0x85,KEY_UP=0x86,
 KEY_ALPHA=0x71,KEY_SQUARE=0x72,KEY_POWER=0x73,KEY_EXIT=0x74,KEY_DOWN=0x75,KEY_RIGHT=0x76,
 KEY_XOT=0x61,KEY_LOG=0x62,KEY_LN=0x63,KEY_SIN=0x64,KEY_COS=0x65,KEY_TAN=0x66,
 KEY_FRAC=0x51,KEY_FD=0x52,KEY_LEFTP=0x53,KEY_RIGHTP=0x54,KEY_COMMA=0x55,KEY_STORE=0x56,
 KEY_7=0x41,KEY_8=0x42,KEY_9=0x43,KEY_DEL=0x44,KEY_4=0x31,KEY_5=0x32,KEY_6=0x33,KEY_MUL=0x34,KEY_DIV=0x35,
 KEY_1=0x21,KEY_2=0x22,KEY_3=0x23,KEY_ADD=0x24,KEY_SUB=0x25,
 KEY_0=0x11,KEY_DOT=0x12,KEY_EXP=0x13,KEY_NEG=0x14,KEY_EXE=0x15
};
enum {KEYEV_NONE=0,KEYEV_DOWN=1,KEYEV_UP=2,KEYEV_HOLD=3,KEYEV_OSMENU=4};
typedef struct {int key,type,shift,alpha,mod,held_shift,held_alpha;uint32_t held_motion;uint16_t time;} key_event_t;
key_event_t getkey(void);
int keydown(int key);
key_event_t pollevent(void);
void gint_setrestart(int enable);
void gint_osmenu(void);
void host_keys(key_event_t const *keys,size_t count);
void host_async_key(key_event_t key,int after_polls);
/* Deliver an asynchronous key only during a selected calculation (1-based). */
void host_async_on_calculation(key_event_t key,int calculation,int after_polls);
/* Synthetic keyboard scan ticks advance only when configured by a test. */
void host_tick_per_poll(unsigned ticks);
size_t host_keys_used(void);
unsigned host_frames(void);
void host_snapshot_on_frame(unsigned frame,const char *path);
void host_snapshot_on_key(size_t consumed_keys,const char *path);
void host_snapshot_on_getkey(size_t consumed_keys,const char *path);
int host_vram_guards_ok(void);
int host_menus_opened(void);
int host_text_seen(char const *needle);
int host_text_count(char const *needle);
void host_calculation_begin(void);
void host_calculation_end(void);
int host_calculation_count(void);
int host_partial_updates(void);
int host_partial_colored_frames(void);
int host_partial_min_colored(void);
int host_partial_max_colored(void);
int host_polls_outside_calculation(void);

#endif
