#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "storage.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
int main(int argc,char **argv){assert(argc==4);char dir[]="/tmp/gl110compare-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
 GraphState s;state_defaults(&s);s.legacy_render_mode=(uint8_t)atoi(argv[1]);strcpy(s.surface[0].formula,argv[2]);assert(storage_save(&s)==0);
 key_event_t k[]={K(F2),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
 host_keys(k,sizeof k/sizeof*k);host_snapshot_on_key(3,argv[3]);assert(graphlab_main()==0);
 char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
 snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
}
