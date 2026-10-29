/* Host tests of the real prompt and rasterizer, not a Casio OS emulator. */
#ifndef SOURCE_FILE
#define SOURCE_FILE "../src/main.c"
#endif
#define main objview_application_main
#include SOURCE_FILE
#undef main
#include <assert.h>
void clock_set_speed(int n){(void)n;}
void cpg_get_overclock_setting(struct cpg_overclock_setting *p){(void)p;}
void cpg_set_overclock_setting(struct cpg_overclock_setting *p){(void)p;}
void cpg_set_overclock_permanent(bool p){(void)p;}
intptr_t gint_world_switch(intptr_t p){return p;}
void *kmalloc(size_t n,char const *name){(void)name;return malloc(n);}
unsigned long rtc_ticks(void){return 1;}
void clearevents(void){}
/* Model the relevant gint getkey_opt semantics: enabled modifier keys are
 * consumed and attached to the next ordinary key. In particular, the old
 * GETKEY_MOD_ALPHA option must hide KEY_ALPHA from the application. This is
 * a host model, not the actual keyboard driver or calculator OS. */
key_event_t getkey_opt(int options,void *timeout)
{
    (void)timeout;
    int shift=0,alpha=0;
    for(;;){
        key_event_t e=getkey();
        if((options&GETKEY_MOD_SHIFT) && e.key==KEY_SHIFT){shift=1;continue;}
        if((options&GETKEY_MOD_ALPHA) && e.key==KEY_ALPHA){alpha=1;continue;}
        e.shift|=shift;
        e.alpha|=alpha;
        return e;
    }
}
void dtext_opt(int x,int y,int fg,int bg,int h,int v,char const *s,int size)
{
    (void)bg;(void)size;int w,z;dsize(s,NULL,&w,&z);
    if(h==DTEXT_CENTER)x-=w/2;else if(h==DTEXT_RIGHT)x-=w;
    if(v==DTEXT_MIDDLE)y-=z/2;dtext(x,y,fg,s);
}
#define KD(k) { .key=(k), .type=KEYEV_DOWN }
static void test_filename_keys(void)
{
    char name[64];
    /* abc -> ALPHA -> ABC -> F3 -> 123 -> F3 -> abc. SHIFT is a
     * modifier in the last mode; F4/F5 remain independent shortcuts. */
    key_event_t const mixed[]={
        KD(KEY_XOT),KD(KEY_ALPHA),KD(KEY_LOG),KD(KEY_F3),KD(KEY_1),
        KD(KEY_DOT),KD(KEY_SUB),KD(KEY_F4),KD(KEY_F5),KD(KEY_F3),
        KD(KEY_LN),KD(KEY_SHIFT),KD(KEY_SIN),KD(KEY_EXE)
    };
    host_keys(mixed,sizeof mixed/sizeof *mixed);
    assert(prompt_filename(name,sizeof name,"",0)==1);
    assert(strcmp(name,"aB1.-_/cD")==0);
    assert(host_text_seen("\nabc\n") && host_text_seen("\nABC\n"));
    assert(host_text_seen("\n123\n"));
    assert(host_vram_guards_ok());

    /* ALPHA alone cycles all modes; DEL removes the last character. */
    key_event_t const alpha[]={
        KD(KEY_ALPHA),KD(KEY_XOT),KD(KEY_ALPHA),KD(KEY_9),
        KD(KEY_ALPHA),KD(KEY_LOG),KD(KEY_ALPHA),KD(KEY_F3),KD(KEY_0),
        KD(KEY_F3),KD(KEY_SHIFT),KD(KEY_LN),KD(KEY_DEL),KD(KEY_F4),KD(KEY_F1)
    };
    host_keys(alpha,sizeof alpha/sizeof *alpha);
    assert(prompt_filename(name,sizeof name,"",0)==1);
    assert(strcmp(name,"A9b0_")==0);
    assert(host_vram_guards_ok());

    /* Empty input remains on the prompt and is followed by the error text. */
    key_event_t const empty[]={KD(KEY_EXE),KD(KEY_XOT),KD(KEY_EXE)};
    host_keys(empty,sizeof empty/sizeof *empty);
    assert(prompt_filename(name,sizeof name,"",0)==1);
    assert(strcmp(name,"a")==0);
    assert(host_text_seen("Please type a file name first"));

    key_event_t const back[]={KD(KEY_F6)};
    host_keys(back,sizeof back/sizeof *back);
    assert(prompt_filename(name,sizeof name,"",1)==0);
    key_event_t const menu[]={KD(KEY_MENU)};
    host_keys(menu,sizeof menu/sizeof *menu);
    assert(prompt_filename(name,sizeof name,"",0)<0);

    /* Additional characters beyond the 56-character input limit are ignored. */
    key_event_t many[62];
    for(int i=0;i<61;i++)many[i]=(key_event_t)KD(KEY_XOT);
    many[61]=(key_event_t)KD(KEY_EXE);
    host_keys(many,sizeof many/sizeof *many);
    assert(prompt_filename(name,sizeof name,"",0)==1);
    assert(strlen(name)==56);
    for(int i=0;i<56;i++)assert(name[i]=='a');
    assert(host_vram_guards_ok());
    puts("ObjView filename controls: ALPHA/F3, SHIFT, symbols, DEL, cancel and limit passed.");
}
#undef KD

static uint64_t hash_viewport(void)
{
    uint64_t h=UINT64_C(1469598103934665603);
    for(int y=VIEW_TOP;y<=VIEW_BOT;y++)for(int x=0;x<DWIDTH;x++){
        h=(h^gint_vram[y*DWIDTH+x])*UINT64_C(1099511628211);
    }
    return h;
}
int main(int argc,char **argv)
{
    (void)argv;
    host_keys(NULL,0); /* initialize guard sentinels */
    for(int i=0;i<32;i++)gray_pal[i]=C_RGB(i,i,i);
    zbuf=calloc(DWIDTH*VIEW_H,sizeof *zbuf);assert(zbuf);
    dclear(C_WHITE);clear_viewport_and_zbuf(gray_pal[29],1);
    draw_tri_zbuf_fp(10,20,100,250,34,300,180,202,700,gray_pal[7]);
    draw_tri_zbuf_fp(260,16,110,55,190,620,390,150,850,gray_pal[20]);
    vfp3_t a={-300000,-250000,650000},b={400000,-250000,650000},c={0,420000,600000};
    render_tri_clipped_fp(a,b,c,210,1,gray_pal[12],1,0);
    vfp3_t d={-1000,9000,NEAR_FP/2},e={40000,-20000,NEAR_FP*2},f={-30000,-30000,NEAR_FP*2};
    render_tri_clipped_fp(d,e,f,210,1,gray_pal[14],1,0);
    unsigned shade_mask=0;
    for(int y=VIEW_TOP;y<=VIEW_BOT;y++)for(int x=0;x<DWIDTH;x++)
        for(int k=0;k<32;k++)if(gint_vram[y*DWIDTH+x]==gray_pal[k])shade_mask|=1u<<k;
    assert((shade_mask & (1u<<7)) && (shade_mask & (1u<<20)) && (shade_mask & (1u<<29)));
    printf("Viewport sample: %016llx\n",(unsigned long long)hash_viewport());
    assert(host_vram_guards_ok());free(zbuf);zbuf=NULL;
    if(argc==1){
        key_event_t key={.key=KEY_EXIT,.type=KEYEV_DOWN};char name[64];host_keys(&key,1);
        char const *preview=getenv("OBJVIEW_PREVIEW");if(preview)host_snapshot_on_getkey(0,preview);
        assert(prompt_filename(name,sizeof name,"Not found: /bad.obj",0)<0);
        int white=0,black=0;
        for(int i=0;i<DWIDTH*DHEIGHT;i++){
            if(gint_vram[i]==C_WHITE)white++;else if(gint_vram[i]==C_BLACK)black++;else assert(!"non-monochrome prompt");
        }
        assert(white>100 && black>100 && host_vram_guards_ok());
        draw_banner("OBJVIEW","LOADING");
        for(int i=0;i<DWIDTH*DHEIGHT;i++)assert(gint_vram[i]==C_WHITE||gint_vram[i]==C_BLACK);
        puts("ObjView prompt and banner: black/white VRAM, no guard damage.");
        test_filename_keys();
    }
}
