/* Raw ObjView-style camera keyboard events, isolated from formula input. */
#include "navigation_input.h"
#include "navigation.h"
#ifndef GL_HOST
#include <gint/timer.h>
#endif
#define NAVIGATION_IDLE_US 500000
uint32_t navigation_key(int key)
{
    switch(key){
    case KEY_LEFT:return NAV_LEFT;case KEY_RIGHT:return NAV_RIGHT;
    case KEY_UP:return NAV_UP;case KEY_DOWN:return NAV_DOWN;
    case KEY_8:return NAV_FORWARD;case KEY_5:return NAV_BACK;
    case KEY_4:return NAV_SIDE_LEFT;case KEY_6:return NAV_SIDE_RIGHT;
    case KEY_1:return NAV_RISE;case KEY_0:return NAV_FALL;
    default:return 0;
    }
}
uint32_t navigation_held(key_event_t event)
{
#ifdef GL_HOST
    uint32_t mask=event.held_motion;
    if(event.type==KEYEV_DOWN||event.type==KEYEV_HOLD)mask|=navigation_key(event.key);
    return mask;
#else
    (void)event;
    static int const keys[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN,KEY_8,KEY_5,
        KEY_4,KEY_6,KEY_1,KEY_0};
    uint32_t mask=0;
    for(unsigned i=0;i<sizeof keys/sizeof *keys;i++)
        if(keydown(keys[i]))mask|=navigation_key(keys[i]);
    return mask;
#endif
}
/* ObjView-style fast loop: while a motion key is down, consume pending raw
   events without adding a fixed delay after every completed frame. When no
   motion key is down, sleep until an event rather than busy-waiting. */
key_event_t navigation_wait(void)
{
#ifdef GL_HOST
    return getkey();
#else
    int moving=navigation_held((key_event_t){0})!=0;
    for(;;){
        key_event_t ev=moving?pollevent():waitevent(NULL);
        /* Motion repeats are driven by the frame loop, not by key repeat. */
        if(ev.type==KEYEV_HOLD && navigation_key(ev.key))continue;
        return ev;
    }
#endif
}
/* Start refinement after a short quiet period. Use an interrupt-driven gint
   timer with waitevent(), not a CPU-burning delay. The timer remains reserved
   until we stop it, so its callback cannot outlive this stack frame or race a
   timer that another module has just obtained. If all timers are busy, start
   immediately after draining any pending key event instead of requiring PLOT. */
key_event_t navigation_wait_idle(int *idle)
{
    if(idle)*idle=0;
#ifdef GL_HOST
    key_event_t ev=getkey();
    if(ev.type==KEYEV_NONE && !navigation_held(ev) && idle)*idle=1;
    return ev;
#else
    if(navigation_held((key_event_t){0}))return navigation_wait();
    volatile int expired=0;
    int timer=timer_configure(TIMER_ANY,NAVIGATION_IDLE_US,GINT_CALL_SET(&expired));
    if(timer<0){
        key_event_t ev=pollevent();
        if(ev.type==KEYEV_NONE && idle)*idle=1;
        return ev;
    }
    timer_start(timer);
    key_event_t ev;
    do { ev=waitevent(&expired); }
    while(ev.type==KEYEV_HOLD && navigation_key(ev.key));
    timer_stop(timer);
    if(ev.type==KEYEV_NONE && expired && idle)*idle=1;
    return ev;
#endif
}
/* If held keys cancel out, or the camera has reached its pitch limit, wait
   for a real key change instead of spinning and redrawing identical frames. */
key_event_t navigation_wait_change(void)
{
#ifdef GL_HOST
    return getkey();
#else
    for(;;){key_event_t ev=waitevent(NULL);if(ev.type!=KEYEV_HOLD)return ev;}
#endif
}
