#ifndef GL_NAVIGATION_H
#define GL_NAVIGATION_H
#include "state.h"
#include <stdint.h>
/* Simultaneous camera keys; GraphLab has no roll in this layout. */
enum {
    NAV_LEFT = 1u<<0, NAV_RIGHT = 1u<<1, NAV_UP = 1u<<2, NAV_DOWN = 1u<<3,
    NAV_FORWARD = 1u<<4, NAV_BACK = 1u<<5, NAV_SIDE_LEFT = 1u<<6, NAV_SIDE_RIGHT = 1u<<7,
    NAV_RISE = 1u<<8, NAV_FALL = 1u<<9
};
/* 0.065 radians per turn step. Movement adapts near the origin and is never
   enlarged solely by a very large clip radius. Slow/fast do not affect turn. */
int navigation_step(GraphState *state,uint32_t held,int slow,int fast);
#endif
