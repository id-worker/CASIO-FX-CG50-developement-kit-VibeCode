#ifndef GL_NAVIGATION_INPUT_H
#define GL_NAVIGATION_INPUT_H
#include "platform.h"
#include <stdint.h>
/* Raw camera events and simultaneous physically held motion keys. */
uint32_t navigation_key(int key);
uint32_t navigation_held(key_event_t event);
key_event_t navigation_wait(void);
#endif
