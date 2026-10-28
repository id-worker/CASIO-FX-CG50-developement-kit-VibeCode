/* Raw ObjView-style camera keyboard events, isolated from formula input. */
#include "navigation_input.h"
#include "navigation.h"
#ifndef GL_HOST
#include <gint/timer.h>
#endif
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
/* Low-level events keep ALPHA's release visible: a tap toggles slow mode,
   while a held chord only slows movement. A one-shot timer provides smooth
   continuous movement without busy-waiting or rebuilding the graph. */
key_event_t navigation_wait(void)
{
#ifdef GL_HOST
    return getkey();
#else
    int moving=navigation_held((key_event_t){0})!=0;
    volatile int frame=0;
    int timer=moving?timer_configure(TIMER_ANY,40000,GINT_CALL_SET_STOP(&frame)):-1;
    if(timer>=0)timer_start(timer);
    key_event_t ev;
    for(;;){
        ev=waitevent(timer>=0?&frame:NULL);
        /* The timed loop handles held keys itself. Without a timer, a gint
           repeat still permits a limited, usable fallback. */
        if(ev.type==KEYEV_HOLD && (timer>=0 || !moving || !navigation_key(ev.key)))continue;
        break;
    }
    if(timer>=0 && !frame)timer_stop(timer);
    return ev;
#endif
}
