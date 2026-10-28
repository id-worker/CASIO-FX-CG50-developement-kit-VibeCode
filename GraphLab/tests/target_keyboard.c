/* QEMU test of actual gint keydev/getkey with a synthetic fx-CG50 matrix.
   This is not a keyboard electrical or Casio OS emulation. */
#include "../src/input.h"
#include <gint/drivers/keydev.h>
#include <gint/hardware.h>
#include <gint/gint.h>
#include <string.h>

uint8_t target_stack[16384] __attribute__((aligned(16)));
uint32_t gint[HW_KEYS];
static keydev_t dev;
static uint8_t scan[12];
static volatile int timedout;
static int alpha_mode,event_upper;
keydev_t *keydev_std(void){return &dev;}
void sleep(void){timedout=1;}
void gint_osmenu(void){}
void gint_poweroff(bool force){(void)force;}
void dupdate(void){}

static void reset_keys(void)
{
    keydev_init(&dev);memset(scan,0,sizeof scan);
    gint[HWCALC]=HWCALC_FXCG50;alpha_mode=0;
}
static void key(int code,int down)
{
    int row=code>>4,col=7-(code&7);
    if(down)scan[row]|=(uint8_t)(1u<<col);
    else scan[row]&=(uint8_t)~(1u<<col);
    keydev_process_state(&dev,scan);
    keydev_tick(&dev,8000);
}
static key_event_t next(void)
{
    timedout=0;
    key_event_t e=getkey_opt(GETKEY_DEFAULT & ~GETKEY_MOD_ALPHA,&timedout);
    event_upper=input_apply(&e,&alpha_mode,keydev_keydown(&dev,KEY_SHIFT),keydev_keydown(&dev,KEY_ALPHA));
    return e;
}
#define REQUIRE(c) do { if(!(c))return __LINE__; } while(0)
int main(void)
{
    key_event_t e;
    reset_keys();
    /* Holding SHIFT across *separate* arrow presses stays active. */
    key(KEY_SHIFT,1);key(KEY_UP,1);e=next();
    REQUIRE(e.key==KEY_UP && e.shift);
    key(KEY_UP,0);key(KEY_DOWN,1);e=next();
    REQUIRE(e.key==KEY_DOWN && e.shift);
    key(KEY_DOWN,0);key(KEY_SHIFT,0);key(KEY_UP,1);e=next();
    REQUIRE(e.key==KEY_UP && !e.shift);

    /* Normal tap SHIFT remains a one-shot for common shifted keys. */
    reset_keys();key(KEY_SHIFT,1);key(KEY_SHIFT,0);key(KEY_SQUARE,1);e=next();
    REQUIRE(e.key==KEY_SQUARE && e.shift);
    key(KEY_SQUARE,0);key(KEY_2,1);e=next();
    REQUIRE(e.key==KEY_2 && !e.shift);

    /* One physical ALPHA -> one letter, then normal key. */
    reset_keys();key(KEY_ALPHA,1);e=next();
    REQUIRE(e.key==KEY_ALPHA && alpha_mode==1);
    key(KEY_ALPHA,0);key(KEY_XOT,1);e=next();
    REQUIRE(e.key==KEY_XOT && e.alpha && alpha_mode==0);
    key(KEY_XOT,0);key(KEY_8,1);e=next();
    REQUIRE(e.key==KEY_8 && !e.alpha);

    /* Two presses enable lock; subsequent letters stay locked; third clears. */
    reset_keys();key(KEY_ALPHA,1);e=next();key(KEY_ALPHA,0);key(KEY_ALPHA,1);e=next();
    REQUIRE(e.key==KEY_ALPHA && alpha_mode==2);
    key(KEY_ALPHA,0);key(KEY_LN,1);e=next();REQUIRE(e.alpha && alpha_mode==2);
    key(KEY_LN,0);key(KEY_8,1);e=next();REQUIRE(e.alpha && alpha_mode==2);
    key(KEY_8,0);key(KEY_ALPHA,1);e=next();REQUIRE(alpha_mode==0);
    key(KEY_ALPHA,0);key(KEY_8,1);e=next();REQUIRE(!e.alpha);

    /* Casio-style tap/release SHIFT then ALPHA also toggles the lock. */
    reset_keys();key(KEY_SHIFT,1);key(KEY_SHIFT,0);key(KEY_ALPHA,1);e=next();
    REQUIRE(e.key==KEY_ALPHA && e.shift && alpha_mode==2);
    key(KEY_ALPHA,0);key(KEY_8,1);e=next();REQUIRE(e.alpha);
    key(KEY_8,0);key(KEY_SHIFT,1);key(KEY_SHIFT,0);key(KEY_ALPHA,1);e=next();
    REQUIRE(e.key==KEY_ALPHA && e.shift && alpha_mode==0);

    /* Held SHIFT+ALPHA toggles lock, and it can also turn it off. */
    reset_keys();key(KEY_SHIFT,1);key(KEY_ALPHA,1);e=next();
    REQUIRE(e.key==KEY_ALPHA && e.shift && alpha_mode==2);
    key(KEY_ALPHA,0);key(KEY_SHIFT,0);key(KEY_8,1);e=next();REQUIRE(e.alpha);
    key(KEY_8,0);key(KEY_SHIFT,1);key(KEY_ALPHA,1);e=next();
    REQUIRE(e.shift && alpha_mode==0);

    /* F5 is handled by the editor after a real gint event. The three modes
       must survive successive presses and affect physical letter keys. */
    reset_keys();key(KEY_F5,1);e=next();REQUIRE(e.key==KEY_F5);input_cycle_alpha(&alpha_mode);
    REQUIRE(alpha_mode==INPUT_LOWER_LOCK);key(KEY_F5,0);key(KEY_8,1);e=next();
    REQUIRE(e.alpha && !event_upper);key(KEY_8,0);key(KEY_F5,1);e=next();input_cycle_alpha(&alpha_mode);
    REQUIRE(alpha_mode==INPUT_UPPER_LOCK);key(KEY_F5,0);key(KEY_8,1);e=next();
    REQUIRE(e.alpha && event_upper);key(KEY_8,0);key(KEY_F5,1);e=next();input_cycle_alpha(&alpha_mode);
    REQUIRE(alpha_mode==INPUT_NUMBER);key(KEY_F5,0);key(KEY_8,1);e=next();
    REQUIRE(!e.alpha && !event_upper);

    /* SHIFT may be tapped first or held while switching symbol pages. */
    reset_keys();key(KEY_SHIFT,1);key(KEY_SHIFT,0);key(KEY_F5,1);e=next();
    REQUIRE(e.key==KEY_F5 && e.shift);
    key(KEY_F5,0);key(KEY_F5,1);e=next();REQUIRE(e.key==KEY_F5 && !e.shift);
    reset_keys();key(KEY_SHIFT,1);key(KEY_F5,1);e=next();
    REQUIRE(e.key==KEY_F5 && e.shift);key(KEY_F5,0);key(KEY_F5,1);e=next();
    REQUIRE(e.key==KEY_F5 && e.shift);
    return 0;
}
