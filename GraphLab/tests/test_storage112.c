#define _POSIX_C_SOURCE 200809L
#include "../src/storage.h"
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/stat.h>
typedef struct {char formula[160];uint8_t active;} LegacyEquation;
typedef struct {
    LegacyEquation curve[CURVE_COUNT],surface[SURFACE_COUNT];char recurrence[FRACTAL_COUNT][160];
    float param_a,param_b,param_c,param_t,plane_x,plane_y,plane_scale;
    float volume_x,volume_y,volume_z,volume_radius,camera_yaw,camera_pitch,camera_zoom;
    FractalView fractal;uint8_t grid,detail2,detail3,wire,axes,fractal_slot;
} LegacyState;
typedef struct {uint32_t magic,version,sequence,payload_size,checksum;LegacyState state;} LegacyRecord;
typedef struct {LegacyState prefix;float eye_x,eye_y,eye_z,period_x;uint8_t axes_front;} Version2State;
typedef struct {uint32_t magic,version,sequence,payload_size,checksum;Version2State state;} Version2Record;
typedef struct {Version2State prefix;uint8_t curve_width;} Version3State;
typedef struct {uint32_t magic,version,sequence,payload_size,checksum;Version3State state;} Version3Record;
static uint32_t crc_add(uint32_t c,unsigned char const *data,size_t len) {
    for(size_t i=0;i<len;i++){c^=data[i];for(int b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return c;
}
static void pack_legacy(LegacyState *dst,GraphState const *s) {
    memset(dst,0,sizeof *dst);
    for(int i=0;i<CURVE_COUNT;i++){assert(strlen(s->curve[i].formula)<160);strcpy(dst->curve[i].formula,s->curve[i].formula);dst->curve[i].active=s->curve[i].active;}
    for(int i=0;i<SURFACE_COUNT;i++){assert(strlen(s->surface[i].formula)<160);strcpy(dst->surface[i].formula,s->surface[i].formula);dst->surface[i].active=s->surface[i].active;}
    for(int i=0;i<FRACTAL_COUNT;i++){assert(strlen(s->recurrence[i])<160);strcpy(dst->recurrence[i],s->recurrence[i]);}
#define P(f) dst->f=s->f
    P(param_a);P(param_b);P(param_c);P(param_t);P(plane_x);P(plane_y);P(plane_scale);
    P(volume_x);P(volume_y);P(volume_z);P(volume_radius);P(camera_yaw);P(camera_pitch);P(camera_zoom);
    P(fractal);P(grid);P(detail2);P(detail3);P(wire);P(axes);P(fractal_slot);
#undef P
}
static void make_legacy(char const *path,GraphState const *s) {
    LegacyRecord r={.magic=0x474c4142u,.version=1,.sequence=7,.payload_size=sizeof(LegacyState)};
    pack_legacy(&r.state,s);uint32_t c=0xffffffffu;
    c=crc_add(c,(unsigned char *)&r.version,sizeof r.version);c=crc_add(c,(unsigned char *)&r.sequence,sizeof r.sequence);
    c=crc_add(c,(unsigned char *)&r.payload_size,sizeof r.payload_size);c=crc_add(c,(unsigned char *)&r.state,sizeof r.state);r.checksum=~c;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0);assert(write(fd,&r,sizeof r)==(ssize_t)sizeof r);assert(close(fd)==0);
}
static void make_version2(char const *path,GraphState const *s) {
    Version2Record r={.magic=0x474c4142u,.version=2,.sequence=9,.payload_size=sizeof(Version2State)};
    pack_legacy(&r.state.prefix,s);
    r.state.eye_x=s->eye_x;r.state.eye_y=s->eye_y;r.state.eye_z=s->eye_z;
    r.state.period_x=s->period_x;r.state.axes_front=s->axes_front;
    /* The last three v2 bytes were padding; don't rely on their values. */
    unsigned char *bytes=(unsigned char *)&r.state;
    bytes[sizeof(r.state)-1]=0x7f;
    uint32_t c=0xffffffffu;
    c=crc_add(c,(unsigned char *)&r.version,sizeof r.version);c=crc_add(c,(unsigned char *)&r.sequence,sizeof r.sequence);
    c=crc_add(c,(unsigned char *)&r.payload_size,sizeof r.payload_size);c=crc_add(c,(unsigned char *)&r.state,sizeof r.state);r.checksum=~c;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0);assert(write(fd,&r,sizeof r)==(ssize_t)sizeof r);assert(close(fd)==0);
}
static void make_version3(char const *path,GraphState const *s) {
    Version3Record r={.magic=0x474c4142u,.version=3,.sequence=11,.payload_size=sizeof(Version3State)};
    pack_legacy(&r.state.prefix.prefix,s);
    r.state.prefix.eye_x=s->eye_x;r.state.prefix.eye_y=s->eye_y;r.state.prefix.eye_z=s->eye_z;
    r.state.prefix.period_x=s->period_x;r.state.prefix.axes_front=s->axes_front;r.state.curve_width=s->curve_width;
    unsigned char *bytes=(unsigned char *)&r.state;bytes[sizeof(r.state)-1]=0x6a;
    uint32_t c=0xffffffffu;
    c=crc_add(c,(unsigned char *)&r.version,sizeof r.version);c=crc_add(c,(unsigned char *)&r.sequence,sizeof r.sequence);
    c=crc_add(c,(unsigned char *)&r.payload_size,sizeof r.payload_size);c=crc_add(c,(unsigned char *)&r.state,sizeof r.state);r.checksum=~c;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0);assert(write(fd,&r,sizeof r)==(ssize_t)sizeof r);assert(close(fd)==0);
}
int main(void) {
    char path[]="/tmp/gl-test-XXXXXX";assert(mkdtemp(path));storage_set_test_dir(path);
    GraphState st,out;state_defaults(&st);assert(state_validate(&st));assert(storage_load(&out)==0);
    assert(storage_save(&st)==0);assert(storage_load(&out)==1);assert(strcmp(out.curve[0].formula,"y=sin(x)")==0);
    strcpy(st.curve[0].formula,"x^2+y^2=1");assert(storage_save(&st)==0);assert(storage_load(&out)==1);
    assert(strcmp(out.curve[0].formula,"x^2+y^2=1")==0);
    char a[200],b[200];snprintf(a,sizeof a,"%s/GLAB8_A.DAT",path);snprintf(b,sizeof b,"%s/GLAB8_B.DAT",path);
    int fd=open(b,O_RDWR);assert(fd>=0);assert(lseek(fd,30,SEEK_SET)==30);unsigned char q=0xaa;assert(write(fd,&q,1)==1);close(fd);
    assert(storage_load(&out)==1);assert(strcmp(out.curve[0].formula,"y=sin(x)")==0);
    /* One valid save plus one damaged/foreign slot is protected. */
    assert(storage_save(&st)==-2);
    unlink(a);unlink(b);fd=open(a,O_WRONLY|O_CREAT|O_EXCL,0600);assert(fd>=0);assert(write(fd,"foreign",7)==7);close(fd);
    assert(storage_save(&st)==0); /* other slot is free */
    assert(storage_save(&st)==-2); /* never overwrite the foreign slot or only valid save */
    char data[8]={0};fd=open(a,O_RDONLY);assert(fd>=0);assert(read(fd,data,7)==7);close(fd);assert(!memcmp(data,"foreign",7));
    out=st;out.plane_scale=-1;assert(!state_validate(&out));assert(storage_save(&out)==-1);
    unlink(a);unlink(b);
    /* Genuine version-1 wire layout and CRC: preserve private formula slots. */
    state_defaults(&st);strcpy(st.recurrence[2],"z^4+c");strcpy(st.surface[1].formula,"x+y+z=4");
    char legacy[200];snprintf(legacy,sizeof legacy,"%s/GLABC_A.DAT",path);
    st.volume_x=2;st.fractal_slot=2;make_legacy(legacy,&st);
    assert(storage_load(&out)==1);assert(!strcmp(out.recurrence[2],"z^4+c"));
    assert(!strcmp(out.surface[1].formula,"x+y+z=4") && out.volume_x==2 && out.fractal_slot==2);
    assert(out.axes_front==1 && out.period_x==0 && state_validate(&out));
    assert(storage_save(&out)==0);GraphState migrated;assert(storage_load(&migrated)==1);
    assert(!strcmp(migrated.recurrence[2],"z^4+c") && migrated.eye_x==out.eye_x);
    assert(access(legacy,F_OK)==0); /* import never alters the 0.90 journal */
    strcpy(out.recurrence[2],"z^5+c");assert(storage_save(&out)==0);
    fd=open(b,O_RDWR);assert(fd>=0);assert(lseek(fd,30,SEEK_SET)==30);q=0x55;assert(write(fd,&q,1)==1);close(fd);
    assert(storage_load(&migrated)==1 && !strcmp(migrated.recurrence[2],"z^4+c"));
    /* With only a damaged new record, do not silently fall back to stale data. */
    unlink(a);assert(storage_load(&migrated)==-1);unlink(b);
    assert(storage_load(&migrated)==1 && !strcmp(migrated.recurrence[2],"z^4+c"));
    unlink(legacy);
    /* 1.00 -> 1.02: preserve formula, camera and old file byte-for-byte. */
    char old[200];snprintf(old,sizeof old,"%s/GLAB1_A.DAT",path);
    state_defaults(&st);strcpy(st.recurrence[2],"z^7+c");st.eye_x=3.25f;st.detail3=2;make_version2(old,&st);
    unsigned char original[4096],after[4096];fd=open(old,O_RDONLY);assert(fd>=0);
    ssize_t len=read(fd,original,sizeof original);assert(len>0);close(fd);
    assert(storage_load(&out)==1 && !strcmp(out.recurrence[2],"z^7+c") && out.eye_x==3.25f && out.detail3==2 && out.curve_width==1);
    out.curve_width=4;assert(storage_save(&out)==0);assert(storage_load(&migrated)==1 && migrated.curve_width==4);
    fd=open(old,O_RDONLY);assert(fd>=0);assert(read(fd,after,sizeof after)==len);close(fd);assert(!memcmp(original,after,(size_t)len));
    unlink(a);unlink(b);unlink(old);
    /* 1.01 v3 -> 1.02 v4: don't reinterpret old tail padding as settings. */
    snprintf(old,sizeof old,"%s/GLAB2_A.DAT",path);
    state_defaults(&st);strcpy(st.recurrence[0],"z^5+c");st.curve_width=5;st.eye_y=23.5f;make_version3(old,&st);
    fd=open(old,O_RDONLY);assert(fd>=0);len=read(fd,original,sizeof original);assert(len>0);close(fd);
    assert(storage_load(&out)==1 && !strcmp(out.recurrence[0],"z^5+c") && out.curve_width==5 && out.eye_y==23.5f);
    assert(out.legacy_render_mode==2 && out.legacy_grid_div==20 && out.accuracy2==1 && out.follow_eye==0);
    out.legacy_render_mode=2;out.legacy_grid_div=64;out.accuracy2=3;out.follow_eye=1;assert(storage_save(&out)==0);
    assert(storage_load(&migrated)==1 && migrated.legacy_render_mode==2 && migrated.legacy_grid_div==64 && migrated.accuracy2==3 && migrated.follow_eye==1);
    fd=open(old,O_RDONLY);assert(fd>=0);assert(read(fd,after,sizeof after)==len);close(fd);assert(!memcmp(original,after,(size_t)len));
    out.legacy_grid_div=65;assert(!state_validate(&out));out.legacy_grid_div=20;out.accuracy2=4;assert(!state_validate(&out));
    unlink(a);unlink(b);unlink(old);rmdir(path);
    puts("Save journal, v0.90/v1.00/v1.01 migration, recovery, foreign-file protection and validation passed.");
}
