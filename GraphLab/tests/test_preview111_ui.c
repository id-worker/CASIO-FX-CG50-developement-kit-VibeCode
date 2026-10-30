#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/mesh_preview.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
int __real_preview_mesh_build(PreviewMesh *,RaySurface const *,int,EvalVars const *,Volume const *,ProgressFn,void *);
float __real_expr_eval_real(Program const *,EvalVars const *);
static int builds,divs[4],faces[4],unexpected_eval;
int __wrap_preview_mesh_build(PreviewMesh *m,RaySurface const *s,int n,EvalVars const *v,Volume const *vol,ProgressFn f,void *ctx)
{
    assert(builds<4);int at=builds++;divs[at]=vol->div;
    int rc=__real_preview_mesh_build(m,s,n,v,vol,f,ctx);faces[at]=m->triangle_count;return rc;
}
float __wrap_expr_eval_real(Program const *p,EvalVars const *v)
{
    size_t consumed=host_keys_used();
    if(consumed==4 || consumed==8)unexpected_eval++;
    return __real_expr_eval_real(p,v);
}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
int main(void)
{
    char dir[]="/tmp/gl111-preview-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.legacy_render_mode=1;assert(state_set_ray_quality(&s,20));
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");assert(storage_save(&s)==0);
    key_event_t k[]={K(F2),K(F6),K(F6),K(RIGHT),K(3),K(3),K(3),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    host_keys(k,sizeof k/sizeof*k);assert(graphlab_main()==0&&host_keys_used()==sizeof k/sizeof*k);
    assert(builds==4&&divs[0]==8&&divs[1]==12&&divs[2]==20&&divs[3]==8);
    assert(faces[0]==768&&faces[1]==1620&&faces[2]==4992&&faces[3]==768);
    assert(host_text_seen("M8")&&host_text_seen("M12")&&host_text_seen("M20")&&host_text_seen("Q20"));
    assert(host_text_seen("MESH PREVIEW")&&unexpected_eval==0&&host_calculation_count()==4&&host_vram_guards_ok());
#ifdef GL_DEV
    assert(host_partial_updates()>0);
#else
    assert(host_partial_updates()==0);
#endif
    char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
    puts("Preview UI: M8/M12/M20/M8 quality rebuilds, unchanged Q20, no VM during motion passed.");
}
