/*
 * host_stub.c -- stand-ins for the Prizm syscalls, so that src/main.c can be
 * compiled and driven on a PC.  Only used by tools/uitest/ui_test.c.
 */
#include "stub/fxcg/display.h"
#include "stub/fxcg/keyboard.h"
#include "stub/fxcg/system.h"
#include "stub/fxcg/rtc.h"
#include "stub/fxcg/file.h"
#include "stub/fxcg/heap.h"
#include <stdlib.h>

#include <stdio.h>
#include <string.h>

static unsigned short vram[384 * 216];
static long host_ms;

/* hooks implemented by the test driver */
extern int  host_key_down(int code);
extern void host_putdisp(void);
extern void host_tick(void);
extern int  host_os_menu(void);

unsigned short *host_vram(void) { return vram; }

void *GetVRAMAddress(void) { return vram; }
void Bdisp_PutDisp_DD(void) { host_putdisp(); }
void Bdisp_EnableColor(int mode) { (void)mode; }
unsigned short FrameColor(int mode, unsigned short color)
{
    (void)mode;
    return color;
}
void EnableStatusArea(int mode) { (void)mode; }
void Bdisp_AllClr_VRAM(void) { memset(vram, 0, sizeof vram); }
void Cursor_SetFlashOff(void) { }
int DefineStatusAreaFlags(int mode, int flags, char *c1, char *c2)
{ (void)mode; (void)flags; (void)c1; (void)c2; return 0; }
void DisplayStatusArea(void) { }
void DrawFrame(int color) { (void)color; }

/* rough: one 6x9 block per character, so overlapping text is detectable */
void PrintMini(int *x, int *y, const char *MB_string, int mode_flags,
               unsigned int xlimit, int P6, int P7, int color, int back_color,
               int writeflag, int P11)
{
    int len = (int)strlen(MB_string), i, j;

    (void)mode_flags; (void)xlimit; (void)P6; (void)P7;
    (void)back_color; (void)P11;
    if (writeflag) {
        for (j = 0; j < 9; j++) {
            int yy = *y + j;
            if (yy < 0 || yy >= 216) continue;
            for (i = 0; i < len * 6; i++) {
                int xx = *x + i;
                if (xx < 0 || xx >= 384) continue;
                vram[yy * 384 + xx] = (unsigned short)color;
            }
        }
    }
    *x += len * 6;
}

/* RTC ticks at 128 Hz */
int RTC_GetTicks(void) { return (int)(host_ms * 16 / 125); }

int RTC_Elapsed_ms(int start, int ms)
{
    long d = (long)RTC_GetTicks() - (long)start;
    if (d < 0) d += 0x1000000L;
    return (d * 125 / 16) >= ms;
}

void OS_InnerWait_ms(int ms)
{
    int i;
    for (i = 0; i < ms; i++) {
        host_ms++;
        host_tick();
    }
}

/* ------------------------------------------------------------------ */
/* user timers (slots 5..10 on hardware) + matrix keycode injection    */
/* ------------------------------------------------------------------ */
typedef struct {
    int state;              /* -1 free, 0 stopped, 1 running */
    void (*handler)(void);
    int elapse;
} HostTimer;

static HostTimer timers[11];
static int timers_init_done;

static void init_timers(void)
{
    int i;
    if (timers_init_done) return;
    for (i = 0; i < 11; i++) timers[i].state = -1;
    timers_init_done = 1;
}

int Timer_Install(int id, void (*handler)(void), int elapse)
{
    int i;
    init_timers();
    if (id == 0) {
        for (i = 5; i <= 10; i++)
            if (timers[i].state < 0) { id = i; break; }
        if (id == 0) return -1;
    }
    if (id < 5 || id > 10 || timers[id].state >= 0) return -1;
    timers[id].state = 0;
    timers[id].handler = handler;
    timers[id].elapse = elapse;
    return id;
}

int Timer_Start(int id)
{
    init_timers();
    if (id < 5 || id > 10 || timers[id].state < 0) return -1;
    timers[id].state = 1;
    return 0;
}

int Timer_Stop(int id)
{
    init_timers();
    if (id < 5 || id > 10 || timers[id].state < 0) return -1;
    timers[id].state = 0;
    return 0;
}

int Timer_Deinstall(int id)
{
    init_timers();
    if (id < 5 || id > 10 || timers[id].state < 0) return -2;
    timers[id].state = -1;
    timers[id].handler = NULL;
    return 0;
}

int host_timers_installed(void)
{
    int i, n = 0;
    init_timers();
    for (i = 5; i <= 10; i++)
        if (timers[i].state >= 0) n++;
    return n;
}

/* Models the real Casio OS rules (SimLo / WikiPrizm):
   - Keyboard_PutKeycode(-1, -1, key) only feeds GetKey's keycode return
     buffer; it NEVER triggers the OS Main Menu.
   - Matrix injection (X >= 0, Y >= 0) only works while GetKey / GetKeyWait_OS
     is already waiting, i.e. from inside a running timer callback. */
static int in_keywait;
static int injected_key;
static int injected_col, injected_row;

static void fire_running_timers(void)
{
    int i;
    init_timers();
    for (i = 5; i <= 10; i++) {
        if (timers[i].state == 1 && timers[i].handler)
            timers[i].handler();
    }
}

int Keyboard_PutKeycode(int X, int Y, int Keycode)
{
    if (X < 0 && Y < 0) {
        injected_key = Keycode;
        return 1;
    }
    if (!in_keywait)
        return 0;   /* ignored on hardware unless a key-wait is active */
    injected_col = X;
    injected_row = Y;
    return 1;
}

int GetKeyWait_OS(int *column, int *row, int type_of_waiting,
                  int timeout_period, int menu, unsigned short *keycode)
{
    static unsigned short saved_vram[384 * 216];

    (void)timeout_period;
    if (keycode) *keycode = 0;

    if (type_of_waiting == KEYWAIT_HALTOFF_TIMEROFF) {
        if (injected_col || injected_row) {
            if (column) *column = injected_col;
            if (row)    *row    = injected_row;
            injected_col = injected_row = 0;
            return KEYREP_KEYEVENT;
        }
        return KEYREP_NOEVENT;
    }

    in_keywait = 1;
    fire_running_timers();
    in_keywait = 0;

    if (injected_col == 4 && injected_row == 9 && menu == 0) {
        int i;
        injected_col = injected_row = 0;
        /* The OS Main Menu backs up VRAM (SaveVRAM_1) before painting itself
           and restores VRAM (LoadVRAM_1) when the same add-in is picked. */
        memcpy(saved_vram, vram, sizeof vram);
        host_os_menu();
        memcpy(vram, saved_vram, sizeof vram);
        /* Hardware rule: returning from Main Menu starts all installed user
           timers again, even stopped ones. */
        for (i = 5; i <= 10; i++)
            if (timers[i].state >= 0) timers[i].state = 1;

        if (type_of_waiting == KEYWAIT_HALTON_TIMERON)
            return KEYREP_TIMEREVENT;
    }

    if (injected_col || injected_row) {
        if (column) *column = injected_col;
        if (row)    *row    = injected_row;
        injected_col = injected_row = 0;
        return KEYREP_KEYEVENT;
    }
    return KEYREP_TIMEREVENT;
}

int GetKey(int *key)
{
    if (injected_key) {
        *key = injected_key;
        injected_key = 0;
        return 0;
    }
    *key = 0;
    return 0;
}

void Bkey_ClrAllFlags(void) { }
void EnableGetkeyToMainFunctionReturn(void) { }
int GetGetkeyToMainFunctionReturnFlag(void) { return 1; }
void Set_FKeys1(unsigned int p1, unsigned int *p2) { (void)p1; (void)p2; }
void Set_FKeys2(unsigned int p1) { (void)p1; }

/* ------------------------------------------------------------------ */
/* heap                                                                */
/* ------------------------------------------------------------------ */
void *sys_malloc(int sz) { return malloc((size_t)sz); }
void sys_free(void *p) { free(p); }
void *sys_calloc(int elements, int elementSize)
{ return calloc((size_t)elements, (size_t)elementSize); }
void *sys_realloc(void *p, int sz) { return realloc(p, (size_t)sz); }

/* ------------------------------------------------------------------ */
/* files: every path maps to one host file                             */
/* ------------------------------------------------------------------ */
#define HOST_FILE "/tmp/ww_stub_storage.bin"

static FILE *handles[4];

int Bfile_CreateEntry_OS(const unsigned short *filename, int mode, size_t *size)
{
    FILE *f;
    (void)filename; (void)mode; (void)size;
    f = fopen(HOST_FILE, "wb");
    if (!f) return -1;
    fclose(f);
    return 0;
}

int Bfile_DeleteEntry(const unsigned short *filename)
{
    (void)filename;
    return remove(HOST_FILE);
}

int Bfile_OpenFile_OS(const unsigned short *filename, int mode, int null)
{
    int i;
    FILE *f;
    (void)filename; (void)null;
    f = fopen(HOST_FILE, (mode == WRITE) ? "wb" : "rb");
    if (!f) return -1;
    for (i = 0; i < 4; i++)
        if (!handles[i]) { handles[i] = f; return 1000 + i; }
    fclose(f);
    return -1;
}

int Bfile_ReadFile_OS(int handle, void *buf, int size, int readpos)
{
    FILE *f;
    if (handle < 1000 || handle > 1003 || !handles[handle - 1000]) return -1;
    f = handles[handle - 1000];
    if (readpos >= 0) fseek(f, readpos, SEEK_SET);
    return (int)fread(buf, 1, (size_t)size, f);
}

int Bfile_WriteFile_OS(int handle, const void *buf, int size)
{
    FILE *f;
    if (handle < 1000 || handle > 1003 || !handles[handle - 1000]) return -1;
    f = handles[handle - 1000];
    return (int)fwrite(buf, 1, (size_t)size, f);
}

int Bfile_CloseFile_OS(int handle)
{
    FILE *f;
    if (handle < 1000 || handle > 1003 || !handles[handle - 1000]) return -1;
    f = handles[handle - 1000];
    handles[handle - 1000] = 0;
    fclose(f);
    return 0;
}
