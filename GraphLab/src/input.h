#ifndef GL_INPUT_H
#define GL_INPUT_H
#include "platform.h"
/* ALPHA next/locked remains available; F5 cycles number/lower/upper lock. */
enum { INPUT_NUMBER=0, INPUT_ALPHA_ONCE=1, INPUT_LOWER_LOCK=2, INPUT_UPPER_LOCK=3 };
char input_alpha_letter(int key);
void input_cycle_alpha(int *alpha_mode);
/* Applies modifiers and returns whether this event's ALPHA letter is uppercase.
   The return value is captured before a one-shot mode is consumed. */
int input_apply(key_event_t *event,int *alpha_mode,int held_shift,int held_alpha);
#endif
