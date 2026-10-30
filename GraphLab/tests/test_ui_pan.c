#define _POSIX_C_SOURCE 200809L
#include "host_gint.h"
#include "../src/storage.h"
#include "../src/plot.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
int graphlab_main(void);
#define K(k) {.key=KEY_##k,.type=KEYEV_DOWN}
#define N(a) (sizeof(a)/sizeof((a)[0]))
static unsigned char a[DWIDTH*DHEIGHT*3],b[DWIDTH*DHEIGHT*3];
static void capture(GraphState *s,int direction,int shift,int roundtrip,unsigned char *image)
{
    assert(storage_save(s)==0);
    key_event_t pan[]={K(F1),K(F6),K(F6),K(RIGHT),K(EXIT),K(EXIT),K(F6)};
    key_event_t both[]={K(F1),K(F6),K(F6),K(RIGHT),K(LEFT),K(EXIT),K(EXIT),K(F6)};
    key_event_t plain[]={K(F1),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    if(direction){
        pan[3].key=both[3].key=direction;
        both[4].key=direction==KEY_LEFT?KEY_RIGHT:direction==KEY_RIGHT?KEY_LEFT:direction==KEY_UP?KEY_DOWN:KEY_UP;
        if(shift){pan[3].shift=pan[3].mod=1;both[3].shift=both[3].mod=1;both[4].shift=both[4].mod=1;}
    }
    key_event_t *keys=direction?(roundtrip?both:pan):plain;
    size_t count=direction?(roundtrip?N(both):N(pan)):N(plain);
    int index=direction?(roundtrip?5:4):3;
    host_keys(keys,count);host_snapshot_on_getkey((size_t)index,"/tmp/graphlab-107-ui-pan.ppm");
    assert(graphlab_main()==0&&host_keys_used()==count&&host_vram_guards_ok());
    assert(host_partial_updates()==0&&host_polls_outside_calculation()==0);
    assert(host_calculation_count()==(direction?(roundtrip?3:2):1));
    FILE *f=fopen("/tmp/graphlab-107-ui-pan.ppm","rb");assert(f);char line[100];
    assert(fgets(line,sizeof line,f)&&fgets(line,sizeof line,f)&&fgets(line,sizeof line,f));
    assert(fread(image,1,sizeof a,f)==sizeof a);fclose(f);unlink("/tmp/graphlab-107-ui-pan.ppm");
}
static int different(void)
{
    int n=0;
    /* Exclude the footer/status overlay from this graph-pixel comparison. */
    for(int y=PLOT_Y0;y<183;y++)for(int x=PLOT_X0;x<=PLOT_X1;x++){
        size_t i=((size_t)y*DWIDTH+x)*3;n+=memcmp(a+i,b+i,3)!=0;
    }
    return n;
}
int main(void)
{
    char dir[]="/tmp/gl-ui-pan-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    char const *formulas[]={"y=x+1","y<=sin(x)","x^2+y^2<=9","x^2+y^2=9","y=1/x"};
    int directions[]={KEY_LEFT,KEY_RIGHT,KEY_UP,KEY_DOWN},cases=0,max_difference=0;
    for(unsigned f=0;f<sizeof formulas/sizeof *formulas;f++)for(unsigned d=0;d<4;d++)for(int shift=0;shift<2;shift++){
        GraphState s;state_defaults(&s);s.grid=0;s.axes_front=0;strcpy(s.curve[0].formula,formulas[f]);
        int k=directions[d],step=shift?15:45;capture(&s,k,shift,0,a);GraphState end=s;
        if(k==KEY_LEFT)end.plane_x-=step*end.plane_scale;
        if(k==KEY_RIGHT)end.plane_x+=step*end.plane_scale;
        if(k==KEY_UP)end.plane_y+=step*end.plane_scale;
        if(k==KEY_DOWN)end.plane_y-=step*end.plane_scale;
        capture(&end,0,0,0,b);int diff=different();if(diff>max_difference)max_difference=diff;
        /* Implicit full renders re-anchor the sampling grid; small differences
           from a translated old grid are expected, not silently called equal. */
        assert(diff<250);
        capture(&s,k,shift,1,a);capture(&s,0,0,0,b);assert(different()==0);cases++;
    }
    char path[256];snprintf(path,sizeof path,"%s/GLAB8_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB8_B.DAT",dir);unlink(path);rmdir(dir);
    printf("2D UI strip pans: %d cases, round trips pixel-identical; full re-render max difference %d/63304 graph pixels.\n",cases,max_difference);
}
