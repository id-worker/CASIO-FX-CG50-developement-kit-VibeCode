/* Test the actual target navigation_wait() with gint's real keydev and a
   synthetic matrix/clock. pollevent and waitevent are stubs, not hardware. */
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
static int waits,polls;
static int configured,started,stopped,running,timer_fail,bad_timer;
static gint_call_t timer_callback;
/* Synthetic timer: checks the real target API/callback ABI, not hardware. */
int timer_configure(int which,uint64_t delay,gint_call_t cb){
    configured++;if(which!=TIMER_ANY||delay!=500000)bad_timer=1;
    if(timer_fail)return -1;timer_callback=cb;return 3;
}
void timer_start(int timer){if(timer!=3||running)bad_timer=1;started++;running=1;}
void timer_stop(int timer){if(timer!=3||!running)bad_timer=1;stopped++;running=0;}
keydev_t *keydev_std(void){return &dev;}
int keydown(int k){return keydev_keydown(&dev,k);}
key_event_t pollevent(void){polls++;return keydev_unqueue_event(&dev);}
key_event_t waitevent(volatile int *timeout){
    waits++;key_event_t ev=keydev_unqueue_event(&dev);
    if(ev.type==KEYEV_NONE&&timeout&&running)gint_call(timer_callback);
    return ev;
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
    REQUIRE(e.key==KEY_8 && e.type==KEYEV_DOWN && polls==0 && waits==1);
    REQUIRE(navigation_held(e)==NAV_FORWARD);
    e=navigation_wait();
    REQUIRE(e.type==KEYEV_NONE && polls==1 && waits==1);
    /* A new F5 event arrives while moving; the key survives polling. */
    key(KEY_F5,1);e=navigation_wait();
    REQUIRE(e.key==KEY_F5 && e.type==KEYEV_DOWN);
    key(KEY_F5,0);e=navigation_wait();REQUIRE(e.key==KEY_F5 && e.type==KEYEV_UP);
    /* A key repeat must not starve or double-count the frame. */
    key_event_t h={.type=KEYEV_HOLD,.key=KEY_8};REQUIRE(keydev_queue_event(&dev,h));
    int before=polls;e=navigation_wait();REQUIRE(e.type==KEYEV_NONE && polls==before+2);
    key(KEY_6,1);e=navigation_wait();
    REQUIRE(e.key==KEY_6 && (navigation_held(e)&(NAV_FORWARD|NAV_SIDE_RIGHT))==(NAV_FORWARD|NAV_SIDE_RIGHT));
    key(KEY_SHIFT,1);e=navigation_wait();REQUIRE(e.key==KEY_SHIFT && keydown(KEY_SHIFT));
    key(KEY_ALPHA,1);e=navigation_wait();REQUIRE(e.key==KEY_ALPHA && e.type==KEYEV_DOWN && keydown(KEY_ALPHA));
    key(KEY_ALPHA,0);e=navigation_wait();REQUIRE(e.key==KEY_ALPHA && e.type==KEYEV_UP && !keydown(KEY_ALPHA));
    key(KEY_8,0);e=navigation_wait();REQUIRE(e.key==KEY_8 && e.type==KEYEV_UP && !(navigation_held(e)&NAV_FORWARD));
    key(KEY_6,0);e=navigation_wait();REQUIRE(e.key==KEY_6 && e.type==KEYEV_UP && navigation_held(e)==0);
    /* Raw navigation events preserve a held SHIFT+F3 chord; unlike getkey,
       a tapped/released SHIFT is not a delayed modifier here. */
    key(KEY_F3,1);e=navigation_wait();REQUIRE(e.key==KEY_F3 && keydown(KEY_SHIFT));
    key(KEY_F3,0);e=navigation_wait();REQUIRE(e.key==KEY_F3 && e.type==KEYEV_UP);
    key(KEY_SHIFT,0);e=navigation_wait();REQUIRE(e.key==KEY_SHIFT && e.type==KEYEV_UP);
    key(KEY_2,1);e=navigation_wait();REQUIRE(e.key==KEY_2 && e.type==KEYEV_DOWN);
    key(KEY_2,0);e=navigation_wait();REQUIRE(e.key==KEY_2 && e.type==KEYEV_UP);
    key(KEY_3,1);e=navigation_wait();REQUIRE(e.key==KEY_3 && e.type==KEYEV_DOWN);
    key(KEY_3,0);e=navigation_wait();REQUIRE(e.key==KEY_3 && e.type==KEYEV_UP);
    /* Parked navigation blocks for a genuine key change, not a hold repeat. */
    key(KEY_8,1);e=navigation_wait();REQUIRE(e.key==KEY_8);
    REQUIRE(keydev_queue_event(&dev,h));key(KEY_8,0);before=waits;
    e=navigation_wait_change();REQUIRE(e.key==KEY_8 && e.type==KEYEV_UP && waits==before+2);
    /* No motion keys: the ordinary path also sleeps for the next event. */
    key(KEY_2,1);before=waits;e=navigation_wait();
    REQUIRE(e.key==KEY_2 && e.type==KEYEV_DOWN && waits==before+1);
    key(KEY_2,0);e=navigation_wait();REQUIRE(e.type==KEYEV_UP);
    /* The real idle wait sets its flag through GINT_CALL, then stops the
       reserved timer before returning. Key events cancel the timer instead. */
    int idle=-1;e=navigation_wait_idle(&idle);
    REQUIRE(idle==1&&e.type==KEYEV_NONE&&configured==1&&started==1&&stopped==1&&!running&&!bad_timer);
    key(KEY_F5,1);idle=-1;e=navigation_wait_idle(&idle);
    REQUIRE(!idle&&e.key==KEY_F5&&e.type==KEYEV_DOWN&&configured==2&&started==2&&stopped==2&&!running);
    key(KEY_F5,0);timer_fail=1;idle=-1;e=navigation_wait_idle(&idle);
    REQUIRE(!idle&&e.key==KEY_F5&&e.type==KEYEV_UP&&!running);
    e=navigation_wait_idle(&idle);REQUIRE(idle==1&&e.type==KEYEV_NONE&&!running);
    timer_fail=0;key(KEY_8,1);e=navigation_wait_idle(&idle);REQUIRE(!idle&&e.key==KEY_8);
    before=configured;e=navigation_wait_idle(&idle);
    REQUIRE(!idle&&e.type==KEYEV_NONE&&configured==before&&navigation_held(e)==NAV_FORWARD);
    key(KEY_8,0);e=navigation_wait_idle(&idle);REQUIRE(!idle&&e.type==KEYEV_UP&&!running);
    REQUIRE(started==stopped&&!bad_timer);
    return 0;
}
