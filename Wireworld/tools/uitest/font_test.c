/* Exercise the actual embedded bitmap renderer and UI text geometry on host. */
#define main ww_main
#include "../../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int host_key_down(int key) { (void)key; return 0; }
void host_putdisp(void) { }
void host_tick(void) { }
int host_os_menu(void) { return 0; }

static int tests, failed;
static void check(int cond, const char *name)
{
    tests++;
    if (!cond) { failed++; fprintf(stderr,"FAIL %s\n",name); }
}
static void clip_case(int sx, int sy, const char *str)
{
    static unsigned short expected[VRAM_W * VIS_H];
    const unsigned short marker=0x4321, fg=0xCDEF, bg=0x1234;
    int x=sx, i, gy, gx;
    for(i=0;i<VRAM_W*VIS_H;i++) vram[i]=expected[i]=marker;
    for(i=0;str[i];i++,x+=WW_FONT_W) {
        unsigned char c=(unsigned char)str[i];
        if(c<32 || c>126) c='?';
        for(gy=0;gy<WW_FONT_H;gy++) for(gx=0;gx<WW_FONT_W;gx++) {
            int px=x+gx, py=sy+gy;
            if(px>=0 && px<VIS_W && py>=0 && py<VIS_H)
                expected[py*VRAM_W+px]=(ww_glyphs[c-32][gy] & (0x80u>>gx))?fg:bg;
        }
    }
    mini_text(sx,sy,str,fg,bg);
    check(memcmp(expected,vram,sizeof expected)==0,"font clipping / stride / pixel masks");
}
int main(void)
{
    int c,y,p,l,maxhelp=0,maxmenu=0;
    char all[96], line[96], buf[64];
    vram=(unsigned short *)GetVRAMAddress();
    for(c=32;c<127;c++) all[c-32]=(char)c;
    all[95]=0;
    for(c=33;c<127;c++) {
        int ink=0;
        for(y=0;y<WW_FONT_H;y++) ink |= ww_glyphs[c-32][y];
        check(ink!=0,"printable character has ink");
    }
    clip_case(0,0,all);
    clip_case(-100,30,all);
    clip_case(-3,-4,"ABCDg_ ");
    clip_case(380,210,"W?g");
    clip_case(384,216,"ABC");
    clip_case(10,-12,"AB");
    clip_case(10,215,"AB");
    clip_case(10,20,"\001\177");
    for(p=0;p<HELP_PAGES;p++) for(l=0;l<HELP_LINES;l++) {
        int w=(int)strlen(help_pages[p][l])*WW_FONT_W;
        if(w>maxhelp) maxhelp=w;
        check(w <= (VIS_W-9)-(8+6)-1,"help line fits box horizontally");
    }
    field_index=FIELD_N-1;
    for(c=0;c<M_COUNT;c++) {
        int w;
        menu_line(c,buf); w=(int)strlen(buf)*WW_FONT_W;
        if(w>maxmenu) maxmenu=w;
        check(w <= (VIS_W-31)-(30+10)-1,"menu item fits box horizontally");
    }
    check(rows_that_fit()>=M_COUNT,"all menu rows fit vertically");
    check(box_bottom(2,M_COUNT)<VIS_H,"menu bottom fits on screen");
    check(row_y(2,HELP_LINES-1)+font_h<box_bottom(2,HELP_LINES),
          "last help line fits box vertically");
    world.gen=4294967295UL; cur_x=511; cur_y=255; ax=ay=0;
    tool=TOOL_COPY; anchor_on=1;
    snprintf(line,sizeof line,"G%06lu S60/9999 STOP %s* X%03d Y%03d Z8",
             world.gen,tool_label(),cur_x,cur_y);
    check(2+(int)strlen(line)*WW_FONT_W <= VIS_W,"worst-case header fits");
    printf("%d font/layout checks, %d failures; longest help %d px, menu %d px\n",
           tests,failed,maxhelp,maxmenu);
    return failed?1:0;
}
