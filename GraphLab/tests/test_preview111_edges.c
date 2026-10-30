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
static int builds,divs[16],results[16];
int __wrap_preview_mesh_build(PreviewMesh *m,RaySurface const *s,int n,EvalVars const *v,Volume const *vol,ProgressFn f,void *ctx)
{
    assert(builds<16);int at=builds++;divs[at]=vol->div;
    return results[at]=__real_preview_mesh_build(m,s,n,v,vol,f,ctx);
}
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(k) (sizeof(k)/sizeof *(k))
static void run(key_event_t *k,size_t n){(void)k;assert(graphlab_main()==0&&host_keys_used()==n&&host_vram_guards_ok());}
int main(void)
{
    char dir[]="/tmp/gl111-edges-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.legacy_render_mode=1;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");assert(storage_save(&s)==0);
    /* User selects M12 even before drawing. */
    key_event_t early[]={K(F2),K(F6),K(3),K(F6),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    builds=0;host_keys(early,N(early));run(early,N(early));
    assert(builds==1&&divs[0]==12&&host_text_seen("M12")&&host_text_seen("MESH PREVIEW"));
    /* Preview may not exceed the final chosen Q, including custom minimum. */
    assert(state_set_ray_quality(&s,4)&&storage_save(&s)==0);
    key_event_t cap[]={K(F2),K(F6),K(F6),K(3),K(3),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    builds=0;host_keys(cap,N(cap));run(cap,N(cap));
    assert(builds==1&&divs[0]==4&&host_text_seen("Q4")&&host_text_seen("M20"));
    /* The same cap applies to VOXEL quality V6. */
    state_defaults(&s);s.legacy_render_mode=0;s.legacy_grid_div=6;
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");assert(storage_save(&s)==0);
    builds=0;host_keys(cap,N(cap));run(cap,N(cap));
    assert(builds==1&&divs[0]==6&&host_text_seen("V6")&&host_text_seen("M20"));
    /* Empty model is safe to move and rebuild. */
    state_defaults(&s);s.legacy_render_mode=1;strcpy(s.surface[0].formula,"x^2+y^2+z^2=500");assert(storage_save(&s)==0);
    key_event_t empty[]={K(F2),K(F6),K(F6),K(RIGHT),K(3),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    builds=0;host_keys(empty,N(empty));run(empty,N(empty));
    assert(builds==2&&results[0]<0&&results[1]<0&&host_text_seen("No visible"));
    /* Inequality remains a volume and uses saved points at every M setting. */
    strcpy(s.surface[0].formula,"x^2+y^2+z^2<=4");assert(storage_save(&s)==0);
    key_event_t ineq[]={K(F2),K(F6),K(F6),K(3),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    builds=0;host_keys(ineq,N(ineq));run(ineq,N(ineq));
    assert(builds==2&&results[0]<0&&results[1]<0&&host_text_seen("POINT PREVIEW")&&host_text_seen("M12"));
    /* EXIT while changing M stops only the rebuild; captured points remain. */
    strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");assert(storage_save(&s)==0);
    key_event_t cancel[]={K(F2),K(F6),K(F6),K(3),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    builds=0;host_keys(cancel,N(cancel));host_async_on_calculation((key_event_t)K(EXIT),2,0);run(cancel,N(cancel));
    assert(builds==2&&results[1]==0&&host_text_seen("POINT PREVIEW")&&host_menus_opened()==0);
    /* MENU during the rebuild returns through the normal OS-menu route. */
    key_event_t menu[]={K(F2),K(F6),K(F6),K(3),K(EXIT),K(F6)};
    builds=0;host_keys(menu,N(menu));host_async_on_calculation((key_event_t)K(MENU),2,0);run(menu,N(menu));
    assert(builds==2&&results[1]==0&&host_menus_opened()==1);
    char path[256];snprintf(path,sizeof path,"%s/GLAB4_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB4_B.DAT",dir);unlink(path);rmdir(dir);
    puts("Preview edges: early quality, Q4/V6 caps, empty/inequality, EXIT and MENU during rebuild passed.");
}
