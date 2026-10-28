/* host_stubs.c -- implementation of the fx-CG50 OS stubs for HOST_TEST */
#define _GNU_SOURCE
#include "host_fxcg.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <sys/stat.h>

static unsigned short vram_host[384 * 216];
unsigned host_bfile_reads;

void *GetVRAMAddress(void) { return vram_host; }

void Bdisp_EnableColor(int n) { (void)n; }
void DrawFrame(int color) { (void)color; }
void Bdisp_PutDisp_DD(void) { }
void Cursor_SetFlashOff(void) { }
int DefineStatusAreaFlags(int m, int f, char *a, char *b)
{ (void)m; (void)f; (void)a; (void)b; return 0; }
void EnableStatusArea(int m) { (void)m; }
void Bkey_ClrAllFlags(void) { }
void EnableGetkeyToMainFunctionReturn(void) { }
int GetKeyWait_OS(int *c, int *r, int t, int to, int m, unsigned short *k)
{ (void)c; (void)r; (void)t; (void)to; (void)m; (void)k; return 0; }
int Keyboard_PutKeycode(int x, int y, int kc) { (void)x; (void)y; (void)kc; return 0; }
int Timer_Install(int id, void (*h)(void), int e)
{ (void)id; (void)h; (void)e; return -1; }
int Timer_Deinstall(int id) { (void)id; return 0; }
int Timer_Start(int id) { (void)id; return 0; }
int Timer_Stop(int id) { (void)id; return 0; }
void OS_InnerWait_ms(int ms) { usleep(ms * 1000); }

static int t0_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (int)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}
int RTC_GetTicks(void) { return t0_ms(); }
int RTC_Elapsed_ms(int start, int dur) { return t0_ms() - start >= dur; }

int host_key_down(int code) { (void)code; return 0; }

void Bfile_StrToName_ncpy(unsigned short *dest, const char *src, size_t n)
{
    size_t i;
    for (i = 0; i + 1 < n && src[i]; i++) dest[i] = (unsigned char)src[i];
    dest[i] = 0;
}

static void map_path(const unsigned short *u, char *out, size_t cap)
{
    char in[128];
    size_t i;
    for (i = 0; i + 1 < sizeof(in) && u[i]; i++) in[i] = (char)u[i];
    in[i] = 0;
    /* "\fls0\TDIC.DAT" -> env TDIC_PATH or dicts/TDIC.DAT */
    if (strstr(in, "TDIC.DAT")) {
        const char *p = getenv("TDIC_PATH");
        snprintf(out, cap, "%s", p ? p : "dicts/TDIC.DAT");
    } else {
        snprintf(out, cap, "%s", in);
    }
}

int Bfile_OpenFile_OS(const unsigned short *fn, int mode, int nul)
{
    char path[256];
    (void)mode; (void)nul;
    map_path(fn, path, sizeof(path));
    return open(path, O_RDONLY);
}

int Bfile_ReadFile_OS(int h, void *buf, int size, int readpos)
{
    host_bfile_reads++;
    return (int)pread(h, buf, (size_t)size, readpos);
}

int Bfile_SeekFile_OS(int h, int pos) { return (int)lseek(h, pos, SEEK_SET); }
int Bfile_GetFileSize_OS(int h) { struct stat s; return fstat(h, &s) ? -1 : (int)s.st_size; }
int Bfile_CloseFile_OS(int h) { return close(h); }

void host_dump(const char *path)
{
    FILE *f = fopen(path, "wb");
    int i;
    if (!f) return;
    fprintf(f, "P6\n384 216\n255\n");
    for (i = 0; i < 384 * 216; i++) {
        unsigned short c = vram_host[i];
        unsigned char rgb[3];
        rgb[0] = (unsigned char)((c >> 11) << 3);
        rgb[1] = (unsigned char)(((c >> 5) & 0x3F) << 2);
        rgb[2] = (unsigned char)((c & 0x1F) << 3);
        fwrite(rgb, 1, 3, f);
    }
    fclose(f);
}
