#define _POSIX_C_SOURCE 200809L
#include "../src/storage.h"
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <sys/types.h>
ssize_t __real_write(int fd,const void *data,size_t n);
static int failure_budget=-1;
ssize_t __wrap_write(int fd,const void *data,size_t n)
{
    if(failure_budget==0){errno=EIO;return -1;}
    if(failure_budget>0 && n>(size_t)failure_budget)n=(size_t)failure_budget;
    ssize_t r=__real_write(fd,data,n);
    if(failure_budget>0 && r>0)failure_budget-=(int)r;
    return r;
}
static int fd_count(void)
{
    DIR *d=opendir("/proc/self/fd");if(!d)return -1;int n=0;while(readdir(d))n++;closedir(d);return n;
}
int main(void)
{
    char dir[]="/tmp/gl-fault-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState a,b,readback;state_defaults(&a);b=a;strcpy(b.curve[0].formula,"y=x");
    assert(storage_save(&a)==0);
    int before=fd_count();failure_budget=64;
    assert(storage_save(&b)==-1);failure_budget=-1;
    assert(storage_load(&readback)==1 && !strcmp(readback.curve[0].formula,"y=sin(x)"));
    assert(storage_save(&b)==-2); /* failed partial slot is not silently destroyed */
    int after=fd_count();if(before>=0 && after>=0)assert(before==after);
    char p[256];snprintf(p,sizeof p,"%s/GLAB3_B.DAT",dir);assert(unlink(p)==0);
    assert(storage_save(&b)==0); /* now two valid records exist */
    failure_budget=64;assert(storage_save(&a)==-1);failure_budget=-1;
    assert(storage_load(&readback)==1 && !strcmp(readback.curve[0].formula,"y=x"));
    if(before>=0 && fd_count()>=0)assert(before==fd_count());
    snprintf(p,sizeof p,"%s/GLAB3_A.DAT",dir);unlink(p);snprintf(p,sizeof p,"%s/GLAB3_B.DAT",dir);unlink(p);rmdir(dir);
    puts("Injected partial-write failures preserve the previous save and close file descriptors.");
}
