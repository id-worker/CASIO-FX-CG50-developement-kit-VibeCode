/* Test the actual target navigation_wait() with gint's real keydev and a
   synthetic matrix/clock. Timer and waitevent are stubs, not hardware. */
#include "../src/navigation_input.h"
#include "../src/navigation.h"
#include <gint/drivers/keydev.h>
#include <gint/hardware.h>
#include <gint/timer.h>
#include <string.h>
uint8_t target_stack[16384] __attribute__((aligned(16)));
uint32_t gint[HW_KEYS];
static keydev_t dev;
static uint8_t scan[12];
static int configured,started,stopped,waits,fail_timer;
keydev_t *keydev_std(void){return &dev;}
int keydown(int k){return keydev_keydown(&dev,k);}
int timer_configure(int timer,uint64_t delay,gint_call_t callback){
    (void)callback;
    if(timer!=TIMER_ANY || delay!=40000)return -1;
    configured++;return fail_timer?-1:5;
}
void timer_start(int t){if(t==5)started++;}
void timer_stop(int t){if(t==5)stopped++;}
key_event_t waitevent(volatile int *timeout){
    waits++;key_event_t e=keydev_unqueue_event(&dev);
    if(e.type==KEYEV_NONE && timeout)*timeout=1;
    return e;
}
static void key(int code,int down){
    int row=code>>4,col=7-(code&7);
    if(down)scan[row]|=(uint8_t)(1u<<col);
    else scan[row]&=(uint8_t)~(1u<<col);
    keydev_process_state(&dev,scan);keydev_tick(&dev,8000);
}
#define REQUIRE(c) do { if(!(c))return __LINE__; } while(0)
int main(void){
    keydev_init(&dev);memset(scan,0,sizeof scan);gint[HWCALC]=HWCALC_FXCG50;
    key_event_t e;
    key(KEY_8,1);e=navigation_wait();
    REQUIRE(e.key==KEY_8 && e.type==KEYEV_DOWN && configured==0);
    REQUIRE(navigation_held(e)==NAV_FORWARD);
    e=navigation_wait();
    REQUIRE(e.type==KEYEV_NONE && configured==1 && started==1 && stopped==0);
    /* A new F5 event arrives while moving: timer is cancelled, key survives. */
    key(KEY_F5,1);e=navigation_wait();
    REQUIRE(e.key==KEY_F5 && e.type==KEYEV_DOWN && stopped==1);
    key(KEY_F5,0);e=navigation_wait();REQUIRE(e.key==KEY_F5 && e.type==KEYEV_UP);
    /* A key repeat must not starve or double-count the 40ms frame. */
    key_event_t h={.type=KEYEV_HOLD,.key=KEY_8};REQUIRE(keydev_queue_event(&dev,h));
    int before=waits;e=navigation_wait();REQUIRE(e.type==KEYEV_NONE && waits==before+2);
    key(KEY_6,1);e=navigation_wait();
    REQUIRE(e.key==KEY_6 && (navigation_held(e)&(NAV_FORWARD|NAV_SIDE_RIGHT))==(NAV_FORWARD|NAV_SIDE_RIGHT));
    key(KEY_SHIFT,1);e=navigation_wait();REQUIRE(e.key==KEY_SHIFT && keydown(KEY_SHIFT));
    key(KEY_ALPHA,1);e=navigation_wait();REQUIRE(e.key==KEY_ALPHA && e.type==KEYEV_DOWN && keydown(KEY_ALPHA));
    key(KEY_ALPHA,0);e=navigation_wait();REQUIRE(e.key==KEY_ALPHA && e.type==KEYEV_UP && !keydown(KEY_ALPHA));
    key(KEY_8,0);e=navigation_wait();REQUIRE(e.key==KEY_8 && e.type==KEYEV_UP && !(navigation_held(e)&NAV_FORWARD));
    key(KEY_6,0);e=navigation_wait();REQUIRE(e.key==KEY_6 && e.type==KEYEV_UP && navigation_held(e)==0);
    key(KEY_SHIFT,0);e=navigation_wait();REQUIRE(e.key==KEY_SHIFT && e.type==KEYEV_UP);
    /* If all timers are busy, raw HOLD remains usable as a fallback. */
    key(KEY_8,1);e=navigation_wait();REQUIRE(e.key==KEY_8);
    fail_timer=1;REQUIRE(keydev_queue_event(&dev,h));e=navigation_wait();
    REQUIRE(e.key==KEY_8 && e.type==KEYEV_HOLD);
    return 0;
}
