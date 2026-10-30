#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "storage.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
int graphlab_main(void);
void *__real_malloc(size_t n);
static size_t fail_projection;
void *__wrap_malloc(size_t n){
    if(fail_projection&&host_keys_used()>=4&&n==fail_projection){fail_projection=0;return NULL;}
    return __real_malloc(n);
}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
int main(int argc,char **argv){
 assert(argc==6);char const *fail=getenv("GL_FAIL_BYTES");if(fail)fail_projection=(size_t)strtoul(fail,NULL,10);char dir[]="/tmp/gl111cmp-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
 GraphState s;state_defaults(&s);s.legacy_render_mode=(uint8_t)atoi(argv[1]);s.axes_front=(uint8_t)atoi(argv[3]);
 if(atoi(argv[4]))s.eye_y=s.volume_radius*4.5f;
 strcpy(s.surface[0].formula,argv[2]);assert(storage_save(&s)==0);
 key_event_t k[]={K(F2),K(F6),K(F6),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
 host_keys(k,sizeof k/sizeof*k);host_snapshot_on_key(4,argv[5]);assert(graphlab_main()==0);
 assert(host_keys_used()==sizeof k/sizeof*k&&fail_projection==0);
 if(fail)assert(host_text_seen("POINT PREVIEW"));
 char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
 snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
}
