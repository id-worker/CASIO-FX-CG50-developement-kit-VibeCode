#define _POSIX_C_SOURCE 200809L
#include "../src/storage.h"
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
ssize_t __real_write(int fd,const void *data,size_t n);
static int write_budget=-1;
ssize_t __wrap_write(int fd,const void *p,size_t n)
{
    if(write_budget==0){errno=EIO;return -1;}
    if(write_budget>0&&n>(size_t)write_budget)n=(size_t)write_budget;
    ssize_t r=__real_write(fd,p,n);if(write_budget>0&&r>0)write_budget-=(int)r;return r;
}
static void path(char *p,size_t cap,char const *dir,char const *file){snprintf(p,cap,"%s/%s",dir,file);}
static void clean(char const *dir)
{
    char p[256];char const *files[]={"GLAB4_A.DAT","GLAB4_B.DAT","GLQ8_A.DAT"};
    for(size_t i=0;i<sizeof files/sizeof *files;i++){path(p,sizeof p,dir,files[i]);unlink(p);}
    assert(rmdir(dir)==0);
}
int main(void)
{
    char dir[]="/tmp/gl-quality-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s,out;state_defaults(&s);assert(state_ray_quality(&s)==20);
    assert(!state_set_ray_quality(&s,3)&&!state_set_ray_quality(&s,49));
    for(int q=4;q<=48;q++){
        assert(state_set_ray_quality(&s,q)&&state_validate(&s));
        assert(storage_save(&s)==0&&storage_load(&out)==1&&state_ray_quality(&out)==q);
    }
    /* v5 owns these bytes; corruption of the latest full record is caught by
       its main checksum and the previous complete record is recovered. */
    assert(state_set_ray_quality(&s,47)&&storage_save(&s)==0);
    assert(storage_load(&out)==1&&state_ray_quality(&out)==47);
    char p[256];path(p,sizeof p,dir,"GLAB4_B.DAT");int fd=open(p,O_RDWR);assert(fd>=0);
    assert(lseek(fd,30,SEEK_SET)==30);unsigned char byte;assert(read(fd,&byte,1)==1);
    byte^=0x40;assert(lseek(fd,30,SEEK_SET)==30&&write(fd,&byte,1)==1&&close(fd)==0);
    assert(storage_load(&out)==1&&state_ray_quality(&out)==48);
    clean(dir);

    char fail[]="/tmp/gl-quality-fail-XXXXXX";assert(mkdtemp(fail));storage_set_test_dir(fail);
    state_defaults(&s);assert(state_set_ray_quality(&s,17)&&storage_save(&s)==0);
    assert(state_set_ray_quality(&s,35));write_budget=8;assert(storage_save(&s)==-1);write_budget=-1;
    assert(storage_load(&out)==1&&state_ray_quality(&out)==17);
    path(p,sizeof p,fail,"GLAB4_B.DAT");assert(unlink(p)==0);
    assert(storage_save(&s)==0&&storage_load(&out)==1&&state_ray_quality(&out)==35);
    clean(fail);

    char foreign[]="/tmp/gl-quality-foreign-XXXXXX";assert(mkdtemp(foreign));storage_set_test_dir(foreign);
    path(p,sizeof p,foreign,"GLAB4_A.DAT");fd=open(p,O_CREAT|O_WRONLY|O_EXCL,0600);
    assert(fd>=0&&write(fd,"FOREIGN",7)==7&&close(fd)==0);
    state_defaults(&s);assert(state_set_ray_quality(&s,19));assert(storage_save(&s)==0);
    assert(state_set_ray_quality(&s,20)&&storage_save(&s)==-2);
    fd=open(p,O_RDONLY);char b[8]={0};assert(fd>=0&&read(fd,b,7)==7&&close(fd)==0&&!strcmp(b,"FOREIGN"));
    clean(foreign);
    puts("Custom Q 4..48: v5 checksum, rollback and foreign-file protection passed.");
}
