#ifndef STUB_FXCG_FILE_H
#define STUB_FXCG_FILE_H
#include <stddef.h>
enum { CREATEMODE_FILE = 1, CREATEMODE_FOLDER = 5 };
enum { READ = 0, READ_SHARE = 1, WRITE = 2, READWRITE = 3, READWRITE_SHARE = 4 };
int Bfile_CloseFile_OS(int handle);
int Bfile_CreateEntry_OS(const unsigned short *filename, int mode, size_t *size);
int Bfile_DeleteEntry(const unsigned short *filename);
int Bfile_OpenFile_OS(const unsigned short *filename, int mode, int null);
int Bfile_ReadFile_OS(int handle, void *buf, int size, int readpos);
int Bfile_WriteFile_OS(int handle, const void *buf, int size);
#endif
