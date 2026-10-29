#ifndef OBJVIEW_HOST_STUB_H
#define OBJVIEW_HOST_STUB_H
#include "host_gint.h"
#include <stdbool.h>
#include <stdint.h>
#define GILRAM
#define KEY_ARROW KEY_STORE
#define CLOCK_SPEED_F1 1
#define CLOCK_SPEED_F4 4
#define CLOCK_SPEED_F5 5
#define GETKEY_MOD_SHIFT 1
#define GETKEY_MOD_ALPHA 2
#define DTEXT_CENTER 1
#define DTEXT_MIDDLE 1
#define DTEXT_RIGHT 2
#define DTEXT_TOP 0
#define GINT_CALL(fn,...) ((intptr_t)0)
struct cpg_overclock_setting {int dummy;};
void clock_set_speed(int);
void cpg_get_overclock_setting(struct cpg_overclock_setting *);
void cpg_set_overclock_setting(struct cpg_overclock_setting *);
void cpg_set_overclock_permanent(bool);
intptr_t gint_world_switch(intptr_t);
void *kmalloc(size_t,char const *);
unsigned long rtc_ticks(void);
void clearevents(void);
key_event_t getkey_opt(int,void *);
void dtext_opt(int,int,int,int,int,int,char const *,int);
#endif
