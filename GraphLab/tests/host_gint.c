#include "host_gint.h"
#include <stdlib.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#ifdef GL_HOST_FONT
#include GL_HOST_FONT
#endif
static uint16_t vram_storage[DWIDTH*DHEIGHT+128];
static uint16_t *vram=vram_storage+64;
uint16_t *gint_vram=vram_storage+64;
struct dwindow dwindow={0,0,DWIDTH,DHEIGHT};
struct dwindow dwindow_set(struct dwindow r) {struct dwindow o=dwindow;dwindow=r;return o;}
void dsize(char const *s,void const *font,int *w,int *h) {
    (void)font;
#ifdef GL_HOST_FONT
    int width=0;for(unsigned char const *p=(unsigned char const *)s;*p;p++){
        if(*p>=32 && *p<127){if(width)width++;width+=host_font_width[*p-32];}
    }
    if(w)*w=width;
    if(h)*h=9;
#else
    if(w)*w=(int)strlen(s)*6;
    if(h)*h=10;
#endif
}
void dpixel(int x,int y,int color) {if(x>=dwindow.left && x<dwindow.right && y>=dwindow.top && y<dwindow.bottom && x>=0 && x<DWIDTH && y>=0 && y<DHEIGHT && color>=0) vram[y*DWIDTH+x]=(uint16_t)color;}
int dgetpixel(int x,int y) {return x>=0 && x<DWIDTH && y>=0 && y<DHEIGHT?vram[y*DWIDTH+x]:-1;}
void dline(int x0,int y0,int x1,int y1,int color) {
    int dx=abs(x1-x0),sx=x0<x1?1:-1,dy=-abs(y1-y0),sy=y0<y1?1:-1,err=dx+dy;
    for(;;) {dpixel(x0,y0,color);if(x0==x1 && y0==y1)break;int e=2*err;if(e>=dy){err+=dy;x0+=sx;}if(e<=dx){err+=dx;y0+=sy;}}
}
void dhline(int y,int c){dline(0,y,DWIDTH-1,y,c);} void dvline(int x,int c){dline(x,0,x,DHEIGHT-1,c);}
void drect(int x0,int y0,int x1,int y1,int c) {for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)dpixel(x,y,c);}
void drect_border(int x0,int y0,int x1,int y1,int c,int w,int b) {drect(x0,y0,x1,y1,c);for(int i=0;i<w;i++){dline(x0+i,y0+i,x1-i,y0+i,b);dline(x0+i,y1-i,x1-i,y1-i,b);dline(x0+i,y0+i,x0+i,y1-i,b);dline(x1-i,y0+i,x1-i,y1-i,b);}}
void dclear(color_t c){for(int i=0;i<DWIDTH*DHEIGHT;i++)vram[i]=c;}
static unsigned frames;
static int in_calculation,calculation_count,polls_outside_calculation,partial_updates;
static int partial_colored,partial_min,partial_max;
static unsigned snapshot_frame;
static size_t snapshot_key=(size_t)-1;
static size_t snapshot_getkey=(size_t)-1;
static size_t script_pos;
static char snapshot_path[256];
static void snapshot(void){
    FILE *f=fopen(snapshot_path,"wb");if(!f)abort();
    fprintf(f,"P6\n%d %d\n255\n",DWIDTH,DHEIGHT);
    for(int i=0;i<DWIDTH*DHEIGHT;i++){
        uint16_t c=vram[i];unsigned char rgb[3]={(unsigned char)(((c>>11)&31)*255/31),(unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
void dupdate(void){
    frames++;
    if(in_calculation){
        partial_updates++;
        int n=0;for(int y=19;y<=199;y++)for(int x=5;x<=390;x++){
            uint16_t c=vram[y*DWIDTH+x];if(c!=C_WHITE&&c!=C_BLACK)n++;
        }
        if(n){partial_colored++;if(!partial_min||n<partial_min)partial_min=n;if(n>partial_max)partial_max=n;}
    }
    if((snapshot_frame && frames==snapshot_frame) || script_pos==snapshot_key){
        snapshot_frame=0;snapshot_key=(size_t)-1;snapshot();
    }
}
static char text_log[131072];
static size_t text_log_used;
void dtext(int x,int y,int color,char const *s) {
    size_t n=strlen(s);if(text_log_used+n+2<sizeof text_log){memcpy(text_log+text_log_used,s,n);text_log_used+=n;text_log[text_log_used++]='\n';text_log[text_log_used]=0;}
#ifdef GL_HOST_FONT
    for(unsigned char const *p=(unsigned char const *)s;*p;p++){
        if(*p<32 || *p>=127)continue;
        int index=*p-32,w=host_font_width[index];
        for(int j=0;j<11;j++)for(int i=0;i<w;i++)if(host_font_rows[index][j]&(1u<<(7-i)))dpixel(x+i,y+j,color);
        x+=w+1;
    }
#else
    (void)x;(void)y;(void)color;(void)s;
#endif
}
void dprint(int x,int y,int color,char const *fmt,...){char s[256];va_list ap;va_start(ap,fmt);vsnprintf(s,sizeof s,fmt,ap);va_end(ap);dtext(x,y,color,s);}
static key_event_t const *script;
static size_t script_count;
static key_event_t async_key;
static int async_after=-1,async_calculation=-1;
static int menus_opened;
static unsigned clock_tick,clock_per_poll;
void host_keys(key_event_t const *keys,size_t count){
    script=keys;script_count=count;script_pos=0;frames=0;in_calculation=calculation_count=polls_outside_calculation=partial_updates=0;partial_colored=partial_min=partial_max=0;async_after=-1;async_calculation=-1;snapshot_frame=0;snapshot_key=(size_t)-1;snapshot_getkey=(size_t)-1;menus_opened=0;text_log_used=0;text_log[0]=0;clock_tick=clock_per_poll=0;
    for(int i=0;i<64;i++)vram_storage[i]=vram_storage[DWIDTH*DHEIGHT+64+i]=0x5aa5;
}
void host_async_key(key_event_t key,int after_polls){async_key=key;async_after=after_polls;async_calculation=-1;}
void host_async_on_calculation(key_event_t key,int calculation,int after_polls){async_key=key;async_after=after_polls;async_calculation=calculation;}
void host_tick_per_poll(unsigned ticks){clock_per_poll=ticks;}
size_t host_keys_used(void){return script_pos;}
unsigned host_frames(void){return frames;}
void host_snapshot_on_frame(unsigned frame,const char *path){snapshot_frame=frame;snprintf(snapshot_path,sizeof snapshot_path,"%s",path);}
void host_snapshot_on_key(size_t key,const char *path){snapshot_key=key;snprintf(snapshot_path,sizeof snapshot_path,"%s",path);}
void host_snapshot_on_getkey(size_t key,const char *path){snapshot_getkey=key;snprintf(snapshot_path,sizeof snapshot_path,"%s",path);}
int host_vram_guards_ok(void){for(int i=0;i<64;i++)if(vram_storage[i]!=0x5aa5||vram_storage[DWIDTH*DHEIGHT+64+i]!=0x5aa5)return 0;return 1;}
int host_menus_opened(void){return menus_opened;}
int host_text_seen(char const *needle){return strstr(text_log,needle)!=NULL;}
int host_text_count(char const *needle){int n=0;size_t len=strlen(needle);if(!len)return 0;char const *p=text_log;while((p=strstr(p,needle))){n++;p+=len;}return n;}
void host_calculation_begin(void){if(in_calculation)abort();in_calculation=1;calculation_count++;}
void host_calculation_end(void){if(!in_calculation)abort();in_calculation=0;}
int host_calculation_count(void){return calculation_count;}
int host_partial_updates(void){return partial_updates;}
int host_partial_colored_frames(void){return partial_colored;}
int host_partial_min_colored(void){return partial_min;}
int host_partial_max_colored(void){return partial_max;}
int host_polls_outside_calculation(void){return polls_outside_calculation;}
key_event_t getkey(void){
    if(script_pos==snapshot_getkey){snapshot_getkey=(size_t)-1;snapshot();}
    if(script_pos>=script_count){fprintf(stderr,"Host key script exhausted at frame %u\n",frames);abort();}
    return script[script_pos++];
}
int keydown(int key){
    if(!script_pos)return 0;
    key_event_t e=script[script_pos-1];
    return key==KEY_SHIFT?e.held_shift:key==KEY_ALPHA?e.held_alpha:0;
}
key_event_t pollevent(void){
    if(!in_calculation)polls_outside_calculation++;
    clock_tick+=clock_per_poll;
    if(async_after>=0 && (async_calculation<0||async_calculation==calculation_count) && async_after--==0){async_after=-1;async_key.time=(uint16_t)clock_tick;return async_key;}
    return (key_event_t){.type=KEYEV_NONE,.time=(uint16_t)clock_tick};
}
void gint_setrestart(int enable){(void)enable;}
void gint_osmenu(void){menus_opened++;}
