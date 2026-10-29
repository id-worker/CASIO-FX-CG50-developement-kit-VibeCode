/*
 * Synthetic SH/QEMU regression for the actual gint keydev.c/getkey.c.
 * It exercises the same option flags as ObjView's filename prompt with a
 * scripted queue. QEMU is not a Casio OS or a physical keyboard emulator.
 * Upstream gint source is built separately and retains its own license.
 */
#include <gint/keyboard.h>
#include <gint/drivers/keydev.h>
#include <gint/hardware.h>
#include <gint/gint.h>
#include <gint/display.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

uint32_t gint[HW_KEYS];
static keydev_t device;
volatile int qemu_data_segment=1;

static void quit(int n);
keydev_t *keydev_std(void) { return &device; }
void gint_osmenu(void) {}
void gint_poweroff(bool ask) { (void)ask; }
void dupdate(void) {}
/* The scripted queue must never be exhausted or overflow. */
void sleep(void) { quit(120); }
void abort(void) { quit(121); }
void *memset(void *dst,int c,size_t n)
{
    unsigned char *p=dst;
    while(n--)*p++=(unsigned char)c;
    return dst;
}
void *memcpy(void *dst,void const *src,size_t n)
{
    unsigned char *p=dst; unsigned char const *q=src;
    while(n--)*p++=*q++;
    return dst;
}

static void reset_device(void)
{
    keydev_init(&device);
    gint[HWCALC]=HWCALC_FXCG50;
}
static void feed(int key,int down)
{
    key_event_t e={0};
    e.type=down?KEYEV_DOWN:KEYEV_UP;
    e.key=key;
    e.row=key>>4;
    e.col=7-(key&7);
    if(!keydev_queue_event(&device,e)) __builtin_trap();
}
static int check(void)
{
    key_event_t e;
    /* With the old option ALPHA is consumed and modifies the next key. */
    reset_device();
    feed(KEY_ALPHA,1);feed(KEY_ALPHA,0);feed(KEY_XOT,1);
    e=getkey_opt(GETKEY_MOD_SHIFT|GETKEY_MOD_ALPHA,0);
    if(e.type!=KEYEV_DOWN || e.key!=KEY_XOT || !e.alpha) return 1;

    /* With the corrected option ALPHA is returned immediately as a key. */
    reset_device();
    feed(KEY_ALPHA,1);feed(KEY_ALPHA,0);feed(KEY_XOT,1);
    e=getkey_opt(GETKEY_MOD_SHIFT,0);
    if(e.type!=KEYEV_DOWN || e.key!=KEY_ALPHA || e.alpha) return 2;
    e=getkey_opt(GETKEY_MOD_SHIFT,0);
    if(e.type!=KEYEV_DOWN || e.key!=KEY_XOT || e.alpha) return 3;

    /* SHIFT remains a modifier. */
    reset_device();
    feed(KEY_SHIFT,1);feed(KEY_SHIFT,0);feed(KEY_XOT,1);
    e=getkey_opt(GETKEY_MOD_SHIFT,0);
    if(e.type!=KEYEV_DOWN || e.key!=KEY_XOT || !e.shift) return 4;

    reset_device();
    feed(KEY_F3,1);
    e=getkey_opt(GETKEY_MOD_SHIFT,0);
    if(e.type!=KEYEV_DOWN || e.key!=KEY_F3) return 5;
    return 0;
}
static void say(char const *s,int n)
{
    register int r3 __asm__("r3")=4,r4 __asm__("r4")=1;
    register char const *r5 __asm__("r5")=s;
    register int r6 __asm__("r6")=n;
    __asm__ volatile("trapa #0x10" : : "r"(r3),"r"(r4),"r"(r5),"r"(r6) : "r0","memory");
}
static void quit(int n)
{
    register int r3 __asm__("r3")=1,r4 __asm__("r4")=n;
    __asm__ volatile("trapa #0x10" : : "r"(r3),"r"(r4) : "memory");
    __builtin_unreachable();
}
/* The bare-metal cross-compiler adds the underscore: this is ELF _start. */
void start(void)
{
    static char const ok[]="SH/QEMU: actual gint modifier and raw-key tests passed\n";
    static char const bad[]="SH/QEMU: actual gint modifier test failed\n";
    int r=check();
    if(r)say(bad,(int)sizeof bad-1);else say(ok,(int)sizeof ok-1);
    quit(r);
}
