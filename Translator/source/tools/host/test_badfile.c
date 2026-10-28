/* Missing/truncated dictionary must show the error path instead of reading
 * past the end. TDIC_PATH points to the intentionally bad file. */
#define HOST_TEST 1
#include "../../src/main.c"
#include <stdio.h>

int main(void)
{
    vram = GetVRAMAddress();
    if (dict_open() == 0) {
        fprintf(stderr, "FAIL: accepted invalid dictionary\n");
        Bfile_CloseFile_OS(dict_fd);
        return 1;
    }
    if (dict_fd != -1) return 2;
    view = V_NODICT;
    paint();
    host_dump("frames/missing_file.ppm");
    printf("PASS: invalid/missing dictionary rejected cleanly\n");
    return 0;
}
