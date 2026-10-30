/* Exercise the real storage implementation on big-endian SH with a tiny
   simulated filesystem. This is NOT Casio OS/Bfile hardware emulation. */
#include "../src/storage.h"
#include <fcntl.h>
#include <errno.h>
#include <stdint.h>
#include <stddef.h>
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
static uint32_t crc_add(uint32_t c,unsigned char const *data,size_t len)
{
    for(size_t i=0;i<len;i++){c^=data[i];for(int b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;
}
int main(void)
{
    static GraphState s,out;state_defaults(&s);
    for(int q=4;q<=48;q++){
        if(!state_set_ray_quality(&s,q)||storage_save(&s)!=0||storage_load(&out)!=1||state_ray_quality(&out)!=q)return 1;
    }
    if(!state_set_ray_quality(&s,17)||storage_save(&s)!=0||storage_load(&out)!=1||state_ray_quality(&out)!=17)return 2;
    /* The v5 journal owns and checksums the custom Q bytes itself. */
    int found=0;for(int i=0;i<8;i++)if(strstr(files[i].name,"GLAB8_B.DAT")){found=1;}
    if(!found||storage_load(&out)!=1||state_ray_quality(&out)!=17)return 3;
    /* Convert one genuine native-endian prefix into a correctly checksummed
       1.11 record, then import it through the v5 -> v6 path on SH. */
    memset(files,0,sizeof files);state_defaults(&s);s.legacy_render_mode=0;state_set_ray_quality(&s,37);
    strcpy(s.curve[0].formula,"y=x+2");if(storage_save(&s)!=0)return 4;
    File *old=&files[0];uint32_t header[5];memcpy(header,old->bytes,sizeof header);
    header[1]=5;header[3]=(uint32_t)offsetof(GraphState,map_zmin);
    uint32_t c=0xffffffffu;c=crc_add(c,(unsigned char *)&header[1],4);
    c=crc_add(c,(unsigned char *)&header[2],4);c=crc_add(c,(unsigned char *)&header[3],4);
    c=crc_add(c,old->bytes+20,header[3]);header[4]=~c;
    memcpy(old->bytes,header,sizeof header);old->length=20+(int)header[3];
    strcpy(old->name,"/tmp/GLAB4_A.DAT");
    if(storage_load(&out)!=1||strcmp(out.curve[0].formula,"y=x+2")||
       state_ray_quality(&out)!=37||out.map_zmin!=-3||out.map_zmax!=3||out.preview_quality!=1)return 5;
    if(storage_save(&out)!=0||strcmp(old->name,"/tmp/GLAB4_A.DAT"))return 6;
    if(storage_load(&out)!=1||state_ray_quality(&out)!=37)return 7;
    /* Create a genuine big-endian v6 GLAB5 record from the shared prefix,
       import it to v7 and verify the old bytes are not modified. */
    memset(files,0,sizeof files);state_defaults(&s);s.legacy_render_mode=2;s.volume_radius=10;
    state_set_ray_quality(&s,23);strcpy(s.surface[0].formula,"z=x-y");
    if(storage_save(&s)!=0)return 8;
    old=&files[0];old->bytes[20+offsetof(GraphState,legacy_render_mode)]=1;
    memcpy(header,old->bytes,sizeof header);header[1]=6;
    header[3]=(uint32_t)offsetof(GraphState,fast_ray_step);
    c=0xffffffffu;c=crc_add(c,(unsigned char *)&header[1],4);c=crc_add(c,(unsigned char *)&header[2],4);
    c=crc_add(c,(unsigned char *)&header[3],4);c=crc_add(c,old->bytes+20,header[3]);header[4]=~c;
    memcpy(old->bytes,header,sizeof header);old->length=20+(int)header[3];strcpy(old->name,"/tmp/GLAB5_A.DAT");
    static unsigned char original[sizeof(GraphState)+64];int length=old->length;memcpy(original,old->bytes,length);
    if(storage_load(&out)!=1||out.legacy_render_mode!=2||out.volume_radius!=10||state_ray_quality(&out)!=23||
       out.fast_ray_step!=32||out.fast_ray_q!=23||out.map_extend_down!=0||out.map_extend_up!=0||
       strcmp(out.surface[0].formula,"z=x-y"))return 9;
    out.legacy_render_mode=2;out.fast_ray_step=8;out.fast_ray_q=9;out.map_extend_down=5;out.map_extend_up=7;
    if(storage_save(&out)!=0||strcmp(old->name,"/tmp/GLAB5_A.DAT")||old->length!=length||memcmp(old->bytes,original,length))return 10;
    if(storage_load(&out)!=1||out.legacy_render_mode!=2||out.fast_ray_q!=9||out.fast_ray_step!=8||
       out.map_extend_down!=5||out.map_extend_up!=7)return 11;
    /* Genuine big-endian v7 GLAB6 prefix -> v8 GLAB7, preserving every
       old byte and using a sensible default for direct-preview spacing. */
    memset(files,0,sizeof files);state_defaults(&s);s.legacy_render_mode=2;s.fast_ray_step=8;s.fast_ray_q=9;
    s.preview_quality=2;s.map_extend_down=5;s.map_extend_up=7;
    state_set_ray_quality(&s,23);strcpy(s.surface[0].formula,"z=x-y");
    if(storage_save(&s)!=0)return 12;
    old=&files[0];memcpy(header,old->bytes,sizeof header);header[1]=7;
    header[3]=(uint32_t)offsetof(GraphState,preview_ray_step);
    c=0xffffffffu;c=crc_add(c,(unsigned char *)&header[1],4);c=crc_add(c,(unsigned char *)&header[2],4);
    c=crc_add(c,(unsigned char *)&header[3],4);c=crc_add(c,old->bytes+20,header[3]);header[4]=~c;
    memcpy(old->bytes,header,sizeof header);old->length=20+(int)header[3];strcpy(old->name,"/tmp/GLAB6_A.DAT");
    length=old->length;memcpy(original,old->bytes,length);
    if(storage_load(&out)!=1||out.legacy_render_mode!=2||state_ray_quality(&out)!=23||
       out.fast_ray_step!=32||out.fast_ray_q!=9||out.preview_quality!=2||out.preview_ray_step!=32||
       out.map_extend_down!=5||out.map_extend_up!=7||strcmp(out.surface[0].formula,"z=x-y"))return 13;
    out.fast_ray_step=32;out.preview_ray_step=31;
    if(storage_save(&out)!=0||old->length!=length||memcmp(old->bytes,original,length))return 14;
    if(storage_load(&out)!=1||out.fast_ray_step!=32||out.preview_ray_step!=31)return 15;
    /* Big-endian 1.17 v8 -> 1.18 v9: old RAY Q is the new FAST Q. */
    memset(files,0,sizeof files);state_defaults(&s);s.legacy_render_mode=2;s.fast_ray_step=7;s.fast_ray_q=9;
    s.preview_ray_step=13;state_set_ray_quality(&s,12);strcpy(s.surface[0].formula,"sin(x)+sin(y)+sin(z)=0");
    if(storage_save(&s)!=0)return 16;
    old=&files[0];old->bytes[20+offsetof(GraphState,legacy_render_mode)]=1;
    memcpy(header,old->bytes,sizeof header);header[1]=8;header[3]=sizeof(GraphState);
    c=0xffffffffu;c=crc_add(c,(unsigned char *)&header[1],4);c=crc_add(c,(unsigned char *)&header[2],4);
    c=crc_add(c,(unsigned char *)&header[3],4);c=crc_add(c,old->bytes+20,header[3]);header[4]=~c;
    memcpy(old->bytes,header,sizeof header);strcpy(old->name,"/tmp/GLAB7_A.DAT");
    length=old->length;memcpy(original,old->bytes,length);
    if(storage_load(&out)!=1||out.legacy_render_mode!=2||out.fast_ray_q!=12||out.fast_ray_step!=32||
       out.preview_ray_step!=32||strcmp(out.surface[0].formula,"sin(x)+sin(y)+sin(z)=0"))return 17;
    if(storage_save(&out)!=0||old->length!=length||memcmp(old->bytes,original,length))return 18;
    if(storage_load(&out)!=1||out.legacy_render_mode!=2||out.fast_ray_q!=12)return 19;
    return 0;
}
