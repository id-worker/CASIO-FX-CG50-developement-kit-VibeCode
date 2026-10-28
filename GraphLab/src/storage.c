#include "storage.h"
#include "platform.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#define SAVE_MAGIC 0x474c4142u
#define SAVE_VERSION 4u

/* Exact old state layout from GraphLab 0.90, for lossless migration. */
typedef struct {
    Equation curve[CURVE_COUNT];Equation surface[SURFACE_COUNT];
    char recurrence[FRACTAL_COUNT][EXPR_MAX];
    float param_a,param_b,param_c,param_t;
    float plane_x,plane_y,plane_scale;
    float volume_x,volume_y,volume_z,volume_radius;
    float camera_yaw,camera_pitch,camera_zoom;
    FractalView fractal;
    uint8_t grid,detail2,detail3,wire,axes,fractal_slot;
} GraphStateV1;
/* Exact version 1.00 state, including its natural tail padding. */
typedef struct {
    GraphStateV1 prefix;
    float eye_x,eye_y,eye_z,period_x;
    uint8_t axes_front;
} GraphStateV2;
/* Exact version 1.01 state, including tail padding after curve_width. */
typedef struct { GraphStateV2 prefix; uint8_t curve_width; } GraphStateV3;
typedef struct {
    uint32_t magic,version,sequence,payload_size,checksum;
    GraphState state;
} SaveRecord;
typedef struct {
    uint32_t magic,version,sequence,payload_size,checksum;
    GraphStateV1 state;
} SaveRecordV1;
_Static_assert(offsetof(GraphState,eye_x)==sizeof(GraphStateV1),"Legacy save prefix changed");
_Static_assert(offsetof(GraphState,curve_width)==sizeof(GraphStateV2),"v1.00 save prefix changed");
_Static_assert(offsetof(GraphState,render3_mode)==sizeof(GraphStateV3),"v1.01 save prefix changed");
_Static_assert(offsetof(SaveRecord,state)==offsetof(SaveRecordV1,state),"Legacy save header changed");
typedef struct {char path[192];SaveRecord rec;int status;} Job;
#ifdef GL_HOST
static char test_dir[128]="/tmp";
void storage_set_test_dir(char const *path) {snprintf(test_dir,sizeof test_dir,"%s",path);}
#endif
/* A distinct v4 journal preserves 1.01, 1.00 and 0.90 files untouched. */
static void path_for(char *dest,size_t size,int slot,int generation)
{
    char const *name=generation==0?(slot?"GLAB3_B.DAT":"GLAB3_A.DAT"):
                     generation==1?(slot?"GLAB2_B.DAT":"GLAB2_A.DAT"):
                     generation==2?(slot?"GLAB1_B.DAT":"GLAB1_A.DAT"):
                                   (slot?"GLABC_B.DAT":"GLABC_A.DAT");
#ifdef GL_HOST
    snprintf(dest,size,"%s/%s",test_dir,name);
#else
    snprintf(dest,size,"/%s",name);
#endif
}
static uint32_t crc_add(uint32_t c,unsigned char const *data,size_t len)
{
    for(size_t i=0;i<len;i++) {c^=data[i];for(int b=0;b<8;b++)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}
    return c;
}
static uint32_t checksum_sized(SaveRecord const *r,size_t payload_size)
{
    uint32_t c=0xffffffffu;
    c=crc_add(c,(unsigned char const *)&r->version,sizeof r->version);
    c=crc_add(c,(unsigned char const *)&r->sequence,sizeof r->sequence);
    c=crc_add(c,(unsigned char const *)&r->payload_size,sizeof r->payload_size);
    c=crc_add(c,(unsigned char const *)&r->state,payload_size);
    return ~c;
}
static uint32_t checksum(SaveRecord const *r){return checksum_sized(r,sizeof r->state);}
void state_default_eye(GraphState *s)
{
    /* Increasing the fixed world boundary must not send RESET's camera far
       away from a small graph at the origin. Small boxes still fit the view. */
    float distance=2.8f*fminf(s->volume_radius,3.0f),cp=cosf(s->camera_pitch);
    s->eye_x=s->volume_x+distance*cp*sinf(s->camera_yaw);
    s->eye_y=s->volume_y+distance*cp*cosf(s->camera_yaw);
    s->eye_z=s->volume_z+distance*sinf(s->camera_pitch);
}
void state_defaults(GraphState *s)
{
    memset(s,0,sizeof *s);
    strcpy(s->curve[0].formula,"y=sin(x)");s->curve[0].active=1;
    strcpy(s->curve[1].formula,"x^2+y^2=9");
    strcpy(s->curve[2].formula,"y=x/2");
    strcpy(s->surface[0].formula,"z+x^2=sqrt(y)");s->surface[0].active=1;
    strcpy(s->surface[1].formula,"x^2+y^2+z^2=9");
    strcpy(s->recurrence[0],"z^2+c");
    strcpy(s->recurrence[1],"z^3+c");
    strcpy(s->recurrence[2],"(abs(re(z))+i*abs(im(z)))^2+c");
    s->plane_scale=.05f;
    s->volume_radius=3;
    s->camera_yaw=.75f;s->camera_pitch=.55f;s->camera_zoom=1;
    s->fractal=(FractalView){-.5f,0,.012f,-.8f,.156f,4,40,4,0};
    s->grid=1;s->detail2=1;s->detail3=1;s->axes=1;s->wire=0;s->axes_front=1;s->curve_width=1;
    s->render3_mode=0;s->voxel_div=20;s->accuracy2=1;s->follow_eye=0;
    state_default_eye(s);
}
static int good_str(char const *s){return memchr(s,0,EXPR_MAX)!=NULL;}
static int in(float x,float lo,float hi){return isfinite(x)&&x>=lo&&x<=hi;}
int state_validate(GraphState const *s)
{
    if(!s)return 0;
    for(int i=0;i<CURVE_COUNT;i++) if(!good_str(s->curve[i].formula)||s->curve[i].active>1)return 0;
    for(int i=0;i<SURFACE_COUNT;i++) if(!good_str(s->surface[i].formula)||s->surface[i].active>1)return 0;
    for(int i=0;i<FRACTAL_COUNT;i++)if(!good_str(s->recurrence[i]))return 0;
    if(s->fractal_slot>=FRACTAL_COUNT)return 0;
    if(!in(s->param_a,-1e5f,1e5f)||!in(s->param_b,-1e5f,1e5f)||!in(s->param_c,-1e5f,1e5f)||!in(s->param_t,-1e5f,1e5f))return 0;
    if(!in(s->plane_x,-1e6f,1e6f)||!in(s->plane_y,-1e6f,1e6f)||!in(s->plane_scale,.00001f,1000))return 0;
    if(!in(s->volume_x,-1e5f,1e5f)||!in(s->volume_y,-1e5f,1e5f)||!in(s->volume_z,-1e5f,1e5f)||!in(s->volume_radius,.001f,1e4f))return 0;
    if(!in(s->camera_yaw,-1000,1000)||!in(s->camera_pitch,-1.5f,1.5f)||!in(s->camera_zoom,.2f,4.0f))return 0;
    if(!in(s->eye_x,-1e6f,1e6f)||!in(s->eye_y,-1e6f,1e6f)||!in(s->eye_z,-1e6f,1e6f)||
       !in(s->period_x,0,1e5f)||s->axes_front>1||s->curve_width<1||s->curve_width>5||
       s->render3_mode>1||s->voxel_div<6||s->voxel_div>64||s->accuracy2>3||s->follow_eye>1)return 0;
    FractalView const *f=&s->fractal;
    if(!in(f->cx,-1e6f,1e6f)||!in(f->cy,-1e6f,1e6f)||!in(f->units_per_pixel,.000001f,1000)||
       !in(f->julia_re,-1e4f,1e4f)||!in(f->julia_im,-1e4f,1e4f)||!in(f->escape_radius,1,1000)||
       f->iterations<1||f->iterations>512||f->block<1||f->block>12||(f->julia!=0&&f->julia!=1))return 0;
    return s->grid<=1 && s->detail2<=2 && s->detail3<=3 && s->wire<=1 && s->axes<=1;
}
/* Return status: 0 absent, 1 recognized and valid, 2 present but unknown or
 * unreadable. Unknown data is never removed or replaced. */
static int read_os(void *arg)
{
    /* The bundled gint open() reads the optional mode argument unconditionally. */
    Job *j=arg;int fd=open(j->path,O_RDONLY,0);
    if(fd<0){j->status=(errno==ENOENT)?0:2;return 0;}
    j->status=2;
    off_t size=lseek(fd,0,SEEK_END);
    if((size!=(off_t)sizeof j->rec && size!=(off_t)sizeof(SaveRecordV1) &&
        size!=(off_t)(offsetof(SaveRecord,state)+sizeof(GraphStateV2)) &&
        size!=(off_t)(offsetof(SaveRecord,state)+sizeof(GraphStateV3))) || lseek(fd,0,SEEK_SET)!=0){close(fd);return 0;}
    size_t got=0;unsigned char *p=(unsigned char *)&j->rec;
    while(got<(size_t)size){ssize_t n=read(fd,p+got,(size_t)size-got);if(n<=0)break;got+=(size_t)n;}
    int close_status=close(fd);
    if(got!=(size_t)size || close_status!=0 || j->rec.magic!=SAVE_MAGIC)return 0;
    if(j->rec.version==SAVE_VERSION && size==(off_t)sizeof j->rec &&
       j->rec.payload_size==sizeof(GraphState) && j->rec.checksum==checksum(&j->rec) && state_validate(&j->rec.state)) {
        j->status=1;return 0;
    }
    if(j->rec.version==3 && size==(off_t)(offsetof(SaveRecord,state)+sizeof(GraphStateV3)) &&
       j->rec.payload_size==sizeof(GraphStateV3) &&
       j->rec.checksum==checksum_sized(&j->rec,sizeof(GraphStateV3))) {
        GraphState converted;state_defaults(&converted);
        memcpy(&converted,&j->rec.state,sizeof(GraphStateV3));
        if(state_validate(&converted)){j->rec.state=converted;j->status=1;}
        return 0;
    }
    if(j->rec.version==2 && size==(off_t)(offsetof(SaveRecord,state)+sizeof(GraphStateV2)) &&
       j->rec.payload_size==sizeof(GraphStateV2) &&
       j->rec.checksum==checksum_sized(&j->rec,sizeof(GraphStateV2))) {
        GraphState converted;state_defaults(&converted);
        memcpy(&converted,&j->rec.state,sizeof(GraphStateV2));
        converted.curve_width=1;
        if(state_validate(&converted)){j->rec.state=converted;j->status=1;}
        return 0;
    }
    if(j->rec.version==1 && size==(off_t)sizeof(SaveRecordV1) &&
       j->rec.payload_size==sizeof(GraphStateV1) &&
       j->rec.checksum==checksum_sized(&j->rec,sizeof(GraphStateV1))) {
        /* Prefix layout is exactly the original state. Initialize new fields
           after copying it, preserving all personal formulas and settings. */
        GraphState converted;state_defaults(&converted);
        memcpy(&converted,&j->rec.state,sizeof(GraphStateV1));
        converted.period_x=0;converted.axes_front=1;state_default_eye(&converted);
        if(state_validate(&converted)){j->rec.state=converted;j->status=1;}
    }
    return 0;
}
static int write_os(void *arg)
{
    Job *j=arg;
    int flags=O_WRONLY|O_CREAT|(j->status==0?O_EXCL:O_TRUNC);
    int fd=open(j->path,flags,0644);if(fd<0)return -1;
    size_t written=0;unsigned char const *p=(unsigned char const *)&j->rec;
    while(written<sizeof j->rec){ssize_t n=write(fd,p+written,sizeof j->rec-written);if(n<=0)break;written+=(size_t)n;}
    int rc=close(fd);return written==sizeof j->rec && rc==0?0:-1;
}
static int os_call(int (*fn)(void *),void *arg)
{
#ifdef GL_HOST
    return fn(arg);
#else
    return gint_world_switch(GINT_CALL(fn,arg));
#endif
}
static void inspect(Job *j,int slot,int generation)
{
    memset(j,0,sizeof *j);path_for(j->path,sizeof j->path,slot,generation);
    if(os_call(read_os,j)!=0)j->status=2;
}
static int newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
int storage_load(GraphState *state)
{
    if(!state)return -1;
    Job *a=malloc(sizeof *a),*b=malloc(sizeof *b);
    if(!a||!b){free(a);free(b);return -1;}
    inspect(a,0,0);inspect(b,1,0);
    /* Fall back through 1.01, 1.00 and 0.90 only when newer files are absent.
       Old files remain untouched; damaged new files are not silently bypassed. */
    if(a->status==0 && b->status==0){inspect(a,0,1);inspect(b,1,1);}
    if(a->status==0 && b->status==0){inspect(a,0,2);inspect(b,1,2);}
    if(a->status==0 && b->status==0){inspect(a,0,3);inspect(b,1,3);}
    Job *best=NULL;
    if(a->status==1)best=a;
    if(b->status==1 && (!best||newer(b->rec.sequence,best->rec.sequence)))best=b;
    int result=best?1:(a->status==2||b->status==2?-1:0);
    if(best)*state=best->rec.state;
    free(a);free(b);return result;
}
int storage_save(GraphState const *state)
{
    if(!state_validate(state))return -1;
    Job *a=malloc(sizeof *a),*b=malloc(sizeof *b);
    if(!a||!b){free(a);free(b);return -1;}
    inspect(a,0,0);inspect(b,1,0);
    Job *target=NULL;uint32_t seq=1;
    if(a->status==1)seq=a->rec.sequence+1;
    if(b->status==1 && (a->status!=1 || newer(b->rec.sequence,a->rec.sequence)))seq=b->rec.sequence+1;
    if(a->status==0)target=a;
    else if(b->status==0)target=b;
    else if(a->status==1 && b->status==1)target=newer(a->rec.sequence,b->rec.sequence)?b:a;
    if(!target){free(a);free(b);return -2;}
    target->rec.magic=SAVE_MAGIC;target->rec.version=SAVE_VERSION;target->rec.sequence=seq;
    target->rec.payload_size=sizeof(GraphState);target->rec.state=*state;target->rec.checksum=checksum(&target->rec);
    int slot=(target==a?0:1);int rc=os_call(write_os,target);
    if(rc==0) {
        Job *verify=malloc(sizeof *verify);
        if(!verify)rc=-1;
        else {inspect(verify,slot,0);if(verify->status!=1 || verify->rec.sequence!=seq)rc=-1;free(verify);}
    }
    free(a);free(b);return rc;
}
