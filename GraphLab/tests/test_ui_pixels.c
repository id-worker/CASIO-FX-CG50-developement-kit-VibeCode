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
static unsigned char image[DWIDTH*DHEIGHT*3];
static void capture(GraphState *s,char const *path){
    assert(storage_save(s)==0);
    key_event_t keys[]={K(F1),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,sizeof keys/sizeof keys[0]);host_snapshot_on_getkey(3,path);
    assert(graphlab_main()==0 && host_keys_used()==sizeof keys/sizeof keys[0]);
    assert(host_calculation_count()==1 && host_partial_updates()==0 && host_vram_guards_ok());
    FILE *f=fopen(path,"rb");assert(f);char header[64];assert(fgets(header,sizeof header,f)&&!strcmp(header,"P6\n"));
    assert(fgets(header,sizeof header,f)&&!strcmp(header,"396 224\n"));
    assert(fgets(header,sizeof header,f)&&!strcmp(header,"255\n"));
    assert(fread(image,1,sizeof image,f)==sizeof image);fclose(f);unlink(path);
}
static void capture3d(GraphState *s,char const *path){
    assert(storage_save(s)==0);
    key_event_t keys[]={K(F2),K(F6),K(F6),K(EXIT),K(EXIT),K(F6)};
    host_keys(keys,sizeof keys/sizeof keys[0]);host_snapshot_on_getkey(3,path);
    assert(graphlab_main()==0&&host_keys_used()==sizeof keys/sizeof keys[0]);
    assert(host_calculation_count()==6&&host_partial_updates()==0&&host_vram_guards_ok());
    FILE *f=fopen(path,"rb");assert(f);char header[64];
    assert(fgets(header,sizeof header,f)&&fgets(header,sizeof header,f)&&fgets(header,sizeof header,f));
    assert(fread(image,1,sizeof image,f)==sizeof image);fclose(f);unlink(path);
}
static int color(int x,int y,int c){
    size_t i=((size_t)y*DWIDTH+x)*3;
    int r=((c>>11)&31)*255/31,g=((c>>5)&63)*255/63,b=(c&31)*255/31;
    return image[i]==r&&image[i+1]==g&&image[i+2]==b;
}
int main(void){
    char dir[]="/tmp/gl-pixels-XXXXXX";assert(mkdtemp(dir));storage_set_test_dir(dir);
    GraphState s;state_defaults(&s);s.grid=0;s.axes_front=0;strcpy(s.curve[0].formula,"y<x");
    capture(&s,"/tmp/gl-explicit-fill.ppm");
    assert(color(210,70,C_WHITE));assert(color(210,150,C_RGB(31,25,25)));
    strcpy(s.curve[0].formula,"x^2+y^2<=9");
    strcpy(s.curve[1].formula,"y=.5*x+1");s.curve[1].active=1;
    capture(&s,"/tmp/gl-implicit-fill.ppm");
    assert(color(210,80,C_RGB(31,25,25)));assert(color(280,80,C_WHITE));
    int blue=0;for(int y=66;y<=72;y++)for(int x=235;x<=241;x++)blue+=color(x,y,C_RGB(2,10,26));
    assert(blue>0); /* later equality boundary remains above shaded area */
    static unsigned char sphere[DWIDTH*DHEIGHT*3];
    state_defaults(&s);s.legacy_render_mode=2;strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");
    capture3d(&s,"/tmp/gl-sphere.ppm");memcpy(sphere,image,sizeof sphere);
    strcpy(s.surface[0].formula,"(x^2+y^2+z^2-1)^2=0");
    capture3d(&s,"/tmp/gl-squared-sphere.ppm");assert(!memcmp(sphere,image,sizeof sphere));
    /* Historical mode and grid bytes may be present in GLAB8; neither can
       re-enable an old engine or affect any pixel in this version. */
    s.legacy_render_mode=0;s.legacy_grid_div=64;strcpy(s.surface[0].formula,"x^2+y^2+z^2=1");
    capture3d(&s,"/tmp/gl-legacy-sphere.ppm");assert(!memcmp(sphere,image,sizeof sphere));
    strcpy(s.surface[0].formula,"(x^2+y^2+z^2-1)^2=0");
    capture3d(&s,"/tmp/gl-legacy-squared-sphere.ppm");assert(!memcmp(sphere,image,sizeof sphere));
    char path[256];snprintf(path,sizeof path,"%s/GLAB8_A.DAT",dir);unlink(path);
    snprintf(path,sizeof path,"%s/GLAB8_B.DAT",dir);unlink(path);rmdir(dir);
    puts("UI pixels: 2D inequality overlays and equivalent Ray zero-surface formula and ignored legacy mode bytes passed.");
}
