/* host_fxcg.h -- minimal fx-CG50 OS stubs for HOST_TEST builds */
#ifndef HOST_FXCG_H
#define HOST_FXCG_H

#include <stddef.h>

/* display.h */
void Bdisp_EnableColor(int n);
void DrawFrame(int color);
void *GetVRAMAddress(void);
void Bdisp_PutDisp_DD(void);
void Cursor_SetFlashOff(void);
#define SAF_BATTERY      0x0001
#define SAF_ALPHA_SHIFT  0x0002
#define SAF_TEXT         0x0100
#define SAF_GLYPH        0x0200
int DefineStatusAreaFlags(int mode, int flags, char *c1, char *c2);
void EnableStatusArea(int mode);

/* keyboard.h */
#define KEYWAIT_HALTOFF_TIMEROFF 1
#define KEYWAIT_HALTON_TIMERON   0
#define KEYREP_KEYEVENT          1
int GetKeyWait_OS(int *col, int *row, int type, int timeout, int menu,
                  unsigned short *keycode);
void Bkey_ClrAllFlags(void);
int Keyboard_PutKeycode(int x, int y, int kc);
void EnableGetkeyToMainFunctionReturn(void);

/* file.h */
enum { READ = 0, READWRITE = 3 };
int Bfile_CloseFile_OS(int handle);
int Bfile_OpenFile_OS(const unsigned short *filename, int mode, int nul);
int Bfile_ReadFile_OS(int handle, void *buf, int size, int readpos);
int Bfile_SeekFile_OS(int handle, int pos);
int Bfile_GetFileSize_OS(int handle);
void Bfile_StrToName_ncpy(unsigned short *dest, const char *src, size_t n);

/* rtc.h */
int RTC_GetTicks(void);
int RTC_Elapsed_ms(int start, int duration);

/* system.h */
int Timer_Install(int id, void (*handler)(void), int elapse);
int Timer_Deinstall(int id);
int Timer_Start(int id);
int Timer_Stop(int id);
void OS_InnerWait_ms(int ms);

/* harness helpers */
extern unsigned host_bfile_reads;
void host_dump(const char *path);
int host_key_down(int code);

#endif
