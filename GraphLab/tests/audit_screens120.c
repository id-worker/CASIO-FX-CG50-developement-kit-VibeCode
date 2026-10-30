/* Generate real GraphLab host-VRAM screenshots by driving the application's
   actual UI with scripted keys. These are simulations, not calculator photos. */
#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define S(k) {.key=KEY_##k,.type=KEYEV_DOWN,.mod=1,.shift=1}
#define IDLE {.type=KEYEV_NONE}
#define SUM_PAGE S(F5),S(F5),S(F5),S(F5),S(F5),S(F5),S(F5)
#define N(a) (sizeof(a)/sizeof((a)[0]))
static void shot(key_event_t const *keys,size_t count,size_t at,char const *path,int frame)
{
    host_keys(keys,count);
    if(frame) {host_tick_per_poll(3);host_snapshot_on_frame((unsigned)at,path);}
    else host_snapshot_on_getkey(at,path);
    assert(graphlab_main()==0 && host_keys_used()==count && host_vram_guards_ok());
    FILE *fp=fopen(path,"rb");if(!fp){fprintf(stderr,"No snapshot %s\n",path);abort();}fclose(fp);
}
int main(int argc,char **argv)
{
    assert(argc==3);
    char dir[]="/tmp/gl-screens-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);
    if(!strcmp(argv[1],"sum")){
        strcpy(s.curve[0].formula,"summ(1,3,n,((x)/(n)))");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(EXIT),K(EXIT),K(F6)};shot(k,N(k),2,argv[2],0);
    }else if(!strcmp(argv[1],"sum_erased")){
        strcpy(s.curve[0].formula,"summ(1,3,n,((x)/(n)))");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(DEL),K(7),K(EXE),K(EXIT),K(F6)};
        shot(k,N(k),11,argv[2],0);
    }else if(!strcmp(argv[1],"product")){
        strcpy(s.curve[0].formula,"prod(1,4,k,(k+1)/(k))");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(EXIT),K(EXIT),K(F6)};shot(k,N(k),2,argv[2],0);
    }else if(!strcmp(argv[1],"unary_erased")){
        strcpy(s.curve[0].formula,"2*sin(x+1)");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(LEFT),K(LEFT),K(LEFT),K(LEFT),K(DEL),K(EXE),K(EXIT),K(F6)};
        shot(k,N(k),7,argv[2],0);
    }else if(!strcmp(argv[1],"fraction_edit")){
        strcpy(s.curve[0].formula,"y=(sin(x))/(1+x^2)");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(LEFT),K(LEFT),K(LEFT),K(EXIT),K(EXIT),K(F6)};shot(k,N(k),5,argv[2],0);
    }else if(!strcmp(argv[1],"series_edit")){
        strcpy(s.curve[0].formula,"summ(0,inf,n,x^(n)/(n!))");s.curve[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(EXE),K(LEFT),K(EXIT),K(EXIT),K(F6)};shot(k,N(k),3,argv[2],0);
    }else if(!strcmp(argv[1],"calc")){
        assert(storage_save(&s)==0);
        key_event_t k[]={K(VARS),K(EXE),S(DEL),K(SIN),K(1),K(FRAC),K(2),K(RIGHT),K(RIGHT),K(ADD),K(1),K(EXE),K(F6),K(F6)};
        shot(k,N(k),12,argv[2],0);assert(host_text_seen("Result: 1.4794255"));
    }else if(!strcmp(argv[1],"calc_sum")){
        assert(storage_save(&s)==0);
        key_event_t k[]={K(VARS),K(EXE),S(DEL),SUM_PAGE,K(F1),K(1),K(FRAC),K(2),
            S(UP),K(DEL),K(3),S(RIGHT),K(RIGHT),K(ADD),K(1),K(EXE),K(F6),K(F6)};
        shot(k,N(k),N(k)-2,argv[2],0);assert(host_text_seen("Result: 3"));
    }else if(!strcmp(argv[1],"2d")){
        strcpy(s.curve[0].formula,"y=sin(x)");s.curve[0].active=1;
        strcpy(s.curve[1].formula,"x^2+y^2=4");s.curve[1].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F1),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),3,argv[2],0);assert(host_calculation_count()==1);
    }else if(!strcmp(argv[1],"3d_edit")){
        strcpy(s.surface[0].formula,"z=sin((x)/(1+y^2))");s.surface[0].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F2),K(EXE),K(LEFT),K(LEFT),K(EXIT),K(EXIT),K(F6)};shot(k,N(k),4,argv[2],0);
    }else if(!strcmp(argv[1],"3d_preview")||!strcmp(argv[1],"3d_final")){
        s.legacy_render_mode=2;s.fast_ray_q=12;s.fast_ray_step=32;s.preview_ray_step=32;
        strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");s.surface[0].active=1;
        strcpy(s.surface[1].formula,"z=x-y");s.surface[1].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F2),K(F6),K(RIGHT),IDLE,K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),!strcmp(argv[1],"3d_preview")?3:4,argv[2],0);
        assert(host_text_seen("R12/P32")&&host_text_seen("R12/1"));
    }else if(!strcmp(argv[1],"3d_progress")){
        s.legacy_render_mode=2;s.fast_ray_q=8;s.fast_ray_step=32;s.preview_ray_step=32;
        strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");s.surface[0].active=1;
        strcpy(s.surface[1].formula,"z=x-y");s.surface[1].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F2),K(F6),IDLE,K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),10,argv[2],1);assert(host_partial_updates()>0);
    }else if(!strcmp(argv[1],"3d_volume")){
        /* An old mode-0 save is intentionally loaded; it must still use Ray. */
        s.legacy_render_mode=0;s.legacy_grid_div=20;s.fast_ray_q=12;s.fast_ray_step=32;
        strcpy(s.surface[0].formula,"x^2+y^2+z^2<=4");s.surface[0].active=1;
        s.surface[1].active=0;assert(storage_save(&s)==0);
        key_event_t k[]={K(F2),K(F6),IDLE,K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),3,argv[2],0);assert(host_text_seen("R12/1"));
    }else if(!strcmp(argv[1],"settings3d")){
        assert(storage_save(&s)==0);key_event_t k[]={K(OPTN),K(OPTN),K(EXIT),K(F6)};
        shot(k,N(k),2,argv[2],0);assert(host_text_seen("Ray Q")&&!host_text_seen("3D renderer"));
    }else if(!strcmp(argv[1],"3d_map")||!strcmp(argv[1],"3d_map_found")){
        s.legacy_render_mode=2;strcpy(s.surface[0].formula,"x^2+y^2+z^2=4");s.surface[0].active=1;
        strcpy(s.surface[1].formula,!strcmp(argv[1],"3d_map")?"z=0":"x=0");s.surface[1].active=1;assert(storage_save(&s)==0);
        key_event_t k[]={K(F2),K(F6),K(2),K(EXIT),K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),3,argv[2],0);assert(host_text_seen("3D CROSSINGS"));
    }else if(!strcmp(argv[1],"fractal")){
        assert(storage_save(&s)==0);key_event_t k[]={K(F3),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
        shot(k,N(k),3,argv[2],0);assert(host_text_seen("FRACTAL VIEW"));
    }else return 2;
    printf("%s: saved %s\n",argv[1],argv[2]);
    char p[256];snprintf(p,sizeof p,"%s/GLAB8_A.DAT",dir);unlink(p);
    snprintf(p,sizeof p,"%s/GLAB8_B.DAT",dir);unlink(p);rmdir(dir);
    return 0;
}
