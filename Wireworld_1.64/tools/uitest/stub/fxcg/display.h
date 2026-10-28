#ifndef STUB_FXCG_DISPLAY_H
#define STUB_FXCG_DISPLAY_H
void *GetVRAMAddress(void);
void Bdisp_PutDisp_DD(void);
void Bdisp_EnableColor(int mode);
unsigned short FrameColor(int mode, unsigned short color);
void EnableStatusArea(int mode);
void Bdisp_AllClr_VRAM(void);
void Cursor_SetFlashOff(void);
int  DefineStatusAreaFlags(int mode, int flags, char *color1, char *color2);
void DisplayStatusArea(void);

#define SAF_BATTERY     0x0001
#define SAF_ALPHA_SHIFT 0x0002
#define SAF_TEXT        0x0100
#define SAF_GLYPH       0x0200
void DrawFrame(int color);
void PrintMini(int *x, int *y, const char *MB_string, int mode_flags,
               unsigned int xlimit, int P6, int P7, int color, int back_color,
               int writeflag, int P11);
#endif
