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
#define SAVE_VERSION 5u

/* Version 0.90..1.08 stored 160-byte formulas. Do not reinterpret these
   layouts using the new editor limit: they are on-disk wire formats. */
#define LEGACY_EXPR_MAX 160
typedef struct {char formula[LEGACY_EXPR_MAX];uint8_t active;} LegacyEquation;
typedef struct {
    LegacyEquation curve[CURVE_COUNT],surface[SURFACE_COUNT];
    char recurrence[FRACTAL_COUNT][LEGACY_EXPR_MAX];
    float param_a,param_b,param_c,param_t;
    float plane_x,plane_y,plane_scale;
    float volume_x,volume_y,volume_z,volume_radius;
    float camera_yaw,camera_pitch,camera_zoom;
    FractalView fractal;
    uint8_t grid,detail2,detail3,wire,axes,fractal_slot;
} GraphStateV1;
typedef struct {GraphStateV1 prefix;float eye_x,eye_y,eye_z,period_x;uint8_t axes_front;} GraphStateV2;
typedef struct {GraphStateV2 prefix;uint8_t curve_width;} GraphStateV3;
typedef struct {GraphStateV3 prefix;uint8_t render3_mode,voxel_div,accuracy2,follow_eye;} GraphStateV4;
typedef struct {
    uint32_t magic,version,sequence,payload_size,checksum;
    GraphState state;
} SaveRecord;
/* Fixed byte offsets on both host and the 32-bit SH target. */
_Static_assert(sizeof(LegacyEquation)==161,"legacy equation layout changed");
_Static_assert(offsetof(GraphStateV1,param_a)==1288,"legacy formula layout changed");
_Static_assert(sizeof(GraphStateV1)==1388,"legacy v1 layout changed");
_Static_assert(sizeof(GraphStateV2)==1408,"legacy v2 layout changed");
_Static_assert(sizeof(GraphStateV3)==1412,"legacy v3 layout changed");
_Static_assert(sizeof(GraphStateV4)==1416,"legacy v4 layout changed");
_Static_assert(offsetof(SaveRecord,state)==20,"save header layout changed");
typedef struct {char path[192];SaveRecord rec;int status,generation;} Job;
/* Separate, slot-bound authentication avoids interpreting arbitrary padding
   from an old v3/v4 save as a custom Q. Old versions simply ignore it. */
typedef struct {uint32_t magic,version,sequence,save_checksum,q,checksum;} QualityRecord;
typedef struct {char path[192];QualityRecord rec;int status;} QualityJob;
#define QUALITY_MAGIC 0x474c5138u
#ifdef GL_HOST
static char test_dir[128]="/tmp";
void storage_set_test_dir(char const *path) {snprintf(test_dir,sizeof test_dir,"%s",path);}
#endif
/* v5 has its own two-slot journal, so the old application can still use
   the unmodified v4 files. New saves are intentionally not back-propagated. */
static void path_for(char *dest,size_t size,int slot,int generation)
{
    char const *name=generation==0?(slot?"GLAB4_B.DAT":"GLAB4_A.DAT"):
                     generation==1?(slot?"GLAB3_B.DAT":"GLAB3_A.DAT"):
                     generation==2?(slot?"GLAB2_B.DAT":"GLAB2_A.DAT"):
                     generation==3?(slot?"GLAB1_B.DAT":"GLAB1_A.DAT"):
                                   (slot?"GLABC_B.DAT":"GLABC_A.DAT");
#ifdef GL_HOST
    snprintf(dest,size,"%s/%s",test_dir,name);
#else
    snprintf(dest,size,"/%s",name);
#endif
}
static void quality_path(char *dest,size_t size,int slot)
{
#ifdef GL_HOST
    snprintf(dest,size,"%s/%s",test_dir,slot?"GLQ8_B.DAT":"GLQ8_A.DAT");
#else
    snprintf(dest,size,"/%s",slot?"GLQ8_B.DAT":"GLQ8_A.DAT");
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
static uint32_t quality_checksum(QualityRecord const *r)
{
    return ~crc_add(0xffffffffu,(unsigned char const *)r,offsetof(QualityRecord,checksum));
}
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
static int custom_tag(GraphState const *s)
{
    if(!s)return 0;
    unsigned q=s->reserved_v3[1];
    unsigned check=(unsigned char)(0x5du^q^(unsigned)s->detail3);
    return s->reserved_v3[0]==0xa7u&&s->reserved_v3[2]==check&&q>=4&&q<=48?(int)q:0;
}
int state_ray_quality(GraphState const *s)
{
    static const unsigned char presets[4]={12,20,32,48};
    if(!s)return 12;
    int q=custom_tag(s);return q?q:presets[s->detail3<4?s->detail3:0];
}
int state_set_ray_quality(GraphState *s,int q)
{
    static const int presets[4]={12,20,32,48};
    if(!s||q<4||q>48)return 0;
    int nearest=0;
    for(int i=1;i<4;i++)if(abs(q-presets[i])<abs(q-presets[nearest]))nearest=i;
    s->detail3=(uint8_t)nearest;
    s->reserved_v3[0]=0xa7u;s->reserved_v3[1]=(uint8_t)q;
    s->reserved_v3[2]=(uint8_t)(0x5du^(unsigned)q^(unsigned)s->detail3);
    return 1;
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
/* Convert an exact old wire record only after its CRC has passed. The
   temporary old state lives on the heap, not gint's limited 16 KiB stack. */
static int import_old(SaveRecord *r,size_t payload,unsigned version)
{
    GraphStateV4 *old=calloc(1,sizeof *old);
    if(!old)return 0;
    memcpy(old,&r->state,payload);
    GraphStateV3 const *v3=&old->prefix;
    GraphStateV2 const *v2=&v3->prefix;
    GraphStateV1 const *v1=&v2->prefix;
    int ok=1;
    for(int i=0;i<CURVE_COUNT;i++)if(!memchr(v1->curve[i].formula,0,LEGACY_EXPR_MAX)||v1->curve[i].active>1)ok=0;
    for(int i=0;i<SURFACE_COUNT;i++)if(!memchr(v1->surface[i].formula,0,LEGACY_EXPR_MAX)||v1->surface[i].active>1)ok=0;
    for(int i=0;i<FRACTAL_COUNT;i++)if(!memchr(v1->recurrence[i],0,LEGACY_EXPR_MAX))ok=0;
    if(!ok){free(old);return 0;}
    GraphState *dst=&r->state;state_defaults(dst);
    for(int i=0;i<CURVE_COUNT;i++){
        memcpy(dst->curve[i].formula,v1->curve[i].formula,LEGACY_EXPR_MAX);
        dst->curve[i].active=v1->curve[i].active;
    }
    for(int i=0;i<SURFACE_COUNT;i++){
        memcpy(dst->surface[i].formula,v1->surface[i].formula,LEGACY_EXPR_MAX);
        dst->surface[i].active=v1->surface[i].active;
    }
    for(int i=0;i<FRACTAL_COUNT;i++)memcpy(dst->recurrence[i],v1->recurrence[i],LEGACY_EXPR_MAX);
#define COPY1(field) dst->field=v1->field
    COPY1(param_a);COPY1(param_b);COPY1(param_c);COPY1(param_t);
    COPY1(plane_x);COPY1(plane_y);COPY1(plane_scale);
    COPY1(volume_x);COPY1(volume_y);COPY1(volume_z);COPY1(volume_radius);
    COPY1(camera_yaw);COPY1(camera_pitch);COPY1(camera_zoom);COPY1(fractal);
    COPY1(grid);COPY1(detail2);COPY1(detail3);COPY1(wire);COPY1(axes);COPY1(fractal_slot);
#undef COPY1
    if(version>=2){
        dst->eye_x=v2->eye_x;dst->eye_y=v2->eye_y;dst->eye_z=v2->eye_z;
        dst->period_x=v2->period_x;dst->axes_front=v2->axes_front;
    }else {dst->period_x=0;dst->axes_front=1;state_default_eye(dst);}
    if(version>=3)dst->curve_width=v3->curve_width;
    if(version>=4){
        /* These three bytes were reserved/padding in v4. They require the
           old GLQ8 sidecar before they can be interpreted as a custom Q. */
        memcpy(dst->reserved_v3,(unsigned char const *)v3+offsetof(GraphStateV3,curve_width)+1,3);
        dst->render3_mode=old->render3_mode;dst->voxel_div=old->voxel_div;
        dst->accuracy2=old->accuracy2;dst->follow_eye=old->follow_eye;
    }
    ok=state_validate(dst);free(old);return ok;
}
/* Return status: 0 absent, 1 recognized and valid, 2 present but unknown or
 * unreadable. Unknown data is never removed or replaced. */
static int read_os(void *arg)
{
    Job *j=arg;int fd=open(j->path,O_RDONLY,0);
    if(fd<0){j->status=(errno==ENOENT)?0:2;return 0;}
    j->status=2;
    unsigned version=SAVE_VERSION-(unsigned)j->generation;
    size_t payload=version==5?sizeof(GraphState):version==4?sizeof(GraphStateV4):
                   version==3?sizeof(GraphStateV3):version==2?sizeof(GraphStateV2):sizeof(GraphStateV1);
    size_t expected=offsetof(SaveRecord,state)+payload;
    off_t size=lseek(fd,0,SEEK_END);
    if(size!=(off_t)expected || lseek(fd,0,SEEK_SET)!=0){close(fd);return 0;}
    size_t got=0;unsigned char *p=(unsigned char *)&j->rec;
    while(got<expected){ssize_t n=read(fd,p+got,expected-got);if(n<=0)break;got+=(size_t)n;}
    int close_status=close(fd);
    if(got!=expected||close_status!=0||j->rec.magic!=SAVE_MAGIC||j->rec.version!=version||
       j->rec.payload_size!=payload||j->rec.checksum!=checksum_sized(&j->rec,payload))return 0;
    if(version==SAVE_VERSION ? state_validate(&j->rec.state):import_old(&j->rec,payload,version))j->status=1;
    return 0;
}
static int quality_read_os(void *arg)
{
    QualityJob *j=arg;int fd=open(j->path,O_RDONLY,0);
    if(fd<0){j->status=errno==ENOENT?0:2;return 0;}
    j->status=2;off_t size=lseek(fd,0,SEEK_END);
    if(size!=(off_t)sizeof j->rec||lseek(fd,0,SEEK_SET)!=0){close(fd);return 0;}
    size_t got=0;unsigned char *p=(unsigned char *)&j->rec;
    while(got<sizeof j->rec){ssize_t n=read(fd,p+got,sizeof j->rec-got);if(n<=0)break;got+=(size_t)n;}
    int rc=close(fd);
    if(rc==0&&got==sizeof j->rec&&j->rec.magic==QUALITY_MAGIC&&j->rec.version==1&&
       j->rec.q>=4&&j->rec.q<=48&&j->rec.checksum==quality_checksum(&j->rec))j->status=1;
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
    memset(j,0,sizeof *j);j->generation=generation;path_for(j->path,sizeof j->path,slot,generation);
    if(os_call(read_os,j)!=0)j->status=2;
}
static void inspect_quality(QualityJob *j,int slot)
{
    memset(j,0,sizeof *j);quality_path(j->path,sizeof j->path,slot);
    if(os_call(quality_read_os,j)!=0)j->status=2;
}
static int newer(uint32_t a,uint32_t b){return (int32_t)(a-b)>0;}
int storage_load(GraphState *state)
{
    if(!state)return -1;
    Job *a=malloc(sizeof *a),*b=malloc(sizeof *b);
    if(!a||!b){free(a);free(b);return -1;}
    inspect(a,0,0);inspect(b,1,0);
    /* Only fall back when both newer files are absent: a damaged/foreign
       new journal must never be silently replaced by a stale one. */
    for(int generation=1;generation<=4&&a->status==0&&b->status==0;generation++){
        inspect(a,0,generation);inspect(b,1,generation);
    }
    Job *best=NULL;
    if(a->status==1)best=a;
    if(b->status==1 && (!best||newer(b->rec.sequence,best->rec.sequence)))best=b;
    int result=best?1:(a->status==2||b->status==2?-1:0);
    if(best){
        *state=best->rec.state;
        int q=custom_tag(state),trusted=best->rec.version==SAVE_VERSION;
        if(q&&best->rec.version==4){
            QualityJob side;inspect_quality(&side,best==a?0:1);
            trusted=side.status==1&&side.rec.sequence==best->rec.sequence&&
                    side.rec.save_checksum==best->rec.checksum&&side.rec.q==(uint32_t)q;
        }
        if(!trusted)memset(state->reserved_v3,0,sizeof state->reserved_v3);
    }
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
    int slot=(target==a?0:1);
    target->rec.magic=SAVE_MAGIC;target->rec.version=SAVE_VERSION;target->rec.sequence=seq;
    target->rec.payload_size=sizeof(GraphState);target->rec.state=*state;target->rec.checksum=checksum(&target->rec);
    int rc=os_call(write_os,target);
    if(rc==0) {
        Job *verify=malloc(sizeof *verify);
        if(!verify)rc=-1;
        else {inspect(verify,slot,0);if(verify->status!=1 || verify->rec.sequence!=seq)rc=-1;free(verify);}
    }
    free(a);free(b);return rc;
}
