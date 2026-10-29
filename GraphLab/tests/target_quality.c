/* Exercise the real storage implementation on big-endian SH with a tiny
   simulated filesystem. This is NOT Casio OS/Bfile hardware emulation. */
#include "../src/storage.h"
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
unsigned char target_stack[16384] __attribute__((aligned(16)));
static unsigned char arena[4*1024*1024];static unsigned used;
void *fxlibc_hal_malloc(unsigned n){n=(n+7)&~7u;if(used+n>sizeof arena)return NULL;void *p=arena+used;used+=n;return p;}
void fxlibc_hal_free(void *p){(void)p;}
void _Exit(int n){(void)n;for(;;){}}
typedef struct {char name[64];unsigned char bytes[sizeof(GraphState)+64];int length,position;} File;
static File files[8];
static File *fd_file(int fd){return fd>=3&&fd<11&&files[fd-3].name[0]?&files[fd-3]:NULL;}
int open(char const *path,int flags,...)
{
    int at=-1,free_slot=-1;
    for(int i=0;i<8;i++){
        if(!files[i].name[0]){if(free_slot<0)free_slot=i;}
        else if(!strcmp(files[i].name,path)){at=i;break;}
    }
    if(at>=0&&(flags&O_CREAT)&&(flags&O_EXCL)){errno=EEXIST;return -1;}
    if(at<0){
        if(!(flags&O_CREAT)){errno=ENOENT;return -1;}
        if(free_slot<0||strlen(path)>=sizeof files[0].name){errno=ENOSPC;return -1;}
        at=free_slot;strcpy(files[at].name,path);files[at].length=0;
    }
    if(flags&O_TRUNC)files[at].length=0;
    files[at].position=0;return at+3;
}
ssize_t read(int fd,void *p,size_t n)
{
    File *f=fd_file(fd);if(!f){errno=EBADF;return -1;}
    size_t rem=(size_t)(f->length-f->position);if(n>rem)n=rem;
    memcpy(p,f->bytes+f->position,n);f->position+=(int)n;return (ssize_t)n;
}
ssize_t write(int fd,void const *p,size_t n)
{
    File *f=fd_file(fd);if(!f||(size_t)f->position+n>sizeof f->bytes){errno=ENOSPC;return -1;}
    memcpy(f->bytes+f->position,p,n);f->position+=(int)n;if(f->length<f->position)f->length=f->position;return (ssize_t)n;
}
off_t lseek(int fd,off_t n,int whence)
{
    File *f=fd_file(fd);if(!f){errno=EBADF;return -1;}
    int at=whence==SEEK_SET?(int)n:whence==SEEK_CUR?f->position+(int)n:f->length+(int)n;
    if(at<0||at>f->length){errno=EINVAL;return -1;}f->position=at;return (off_t)at;
}
int close(int fd){return fd_file(fd)?0:-1;}
int main(void)
{
    static GraphState s,out;state_defaults(&s);
    for(int q=4;q<=48;q++){
        if(!state_set_ray_quality(&s,q)||storage_save(&s)!=0||storage_load(&out)!=1||state_ray_quality(&out)!=q)return 1;
    }
    if(!state_set_ray_quality(&s,17)||storage_save(&s)!=0||storage_load(&out)!=1||state_ray_quality(&out)!=17)return 2;
    /* The v5 journal owns and checksums the custom Q bytes itself. */
    int found=0;for(int i=0;i<8;i++)if(strstr(files[i].name,"GLAB4_B.DAT")){found=1;}
    if(!found||storage_load(&out)!=1||state_ray_quality(&out)!=17)return 3;
    return 0;
}
