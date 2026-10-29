#ifndef HOST_FXCG_H
#define HOST_FXCG_H

#include <stddef.h>

#define READ            0
#define READWRITE       2
#define CREATEMODE_FILE 1

void Bdisp_EnableColor(int mode);
void Bdisp_PutDisp_DD(void);
void OS_InnerWait_ms(int ms);
int  RTC_GetTicks(void);
int  RTC_Elapsed_ms(int start, int dur);

void Bfile_StrToName_ncpy(unsigned short *dest, const char *src, size_t n);
void Bfile_NameToStr_ncpy(char *dest, const unsigned short *src, size_t n);
int  Bfile_FindFirst_NON_SMEM(const char *pathname, int *FindHandle, char *foundfile, void *fileinfo);
int  Bfile_FindNext_NON_SMEM(int FindHandle, char *foundfile, char *fileinfo);
int  Bfile_FindClose(int FindHandle);
int  Bfile_OpenFile_OS(const unsigned short *filename, int mode, int null);
int  Bfile_GetFileSize_OS(int handle);
int  Bfile_ReadFile_OS(int handle, void *buf, int size, int readpos);
int  Bfile_WriteFile_OS(int handle, const void *buf, int size);
int  Bfile_CloseFile_OS(int handle);
int  Bfile_CreateEntry_OS(const unsigned short *filename, int mode, size_t *size);
int  Bfile_DeleteEntry(const unsigned short *filename);
int  Bfile_RenameEntry(const unsigned short *oldpath, const unsigned short *newpath);

int  host_key_down(int code);
int  host_step_after_frame(void);
void host_leave_to_menu(void);

#endif
