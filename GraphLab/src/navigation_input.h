#ifndef GL_NAVIGATION_INPUT_H
#define GL_NAVIGATION_INPUT_H
#include "platform.h"
#include <stdint.h>
/* Raw camera events and simultaneous physically held motion keys. */
uint32_t navigation_key(int key);
uint32_t navigation_held(key_event_t event);
key_event_t navigation_wait(void);
/* Wait up to half a second with no motion for an automatic refinement.
   *idle is set only when the quiet timer fires (or no timer is available).
   A physically held motion key keeps the normal responsive frame loop. */
key_event_t navigation_wait_idle(int *idle);
key_event_t navigation_wait_change(void);
#endif
