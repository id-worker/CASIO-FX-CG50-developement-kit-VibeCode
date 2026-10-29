/* Actual color palette: exercise the startup failure screen with no heap. */
#define ww_init fail_ww_init
#define main ww_application_main
#include "../../src/main.c"
#undef main
#undef ww_init
#include <assert.h>
#include <stdio.h>
static int ticks,frames;
int fail_ww_init(WWWorld *w,int width,int height){(void)w;(void)width;(void)height;return -1;}
int host_key_down(int key){return key==KEY_PRGM_RETURN && ticks>=10 && ticks<20;}
void host_tick(void){ticks++;}
void host_putdisp(void){frames++;}
int host_os_menu(void){return 0;}
int main(void)
{
    vram=(unsigned short*)GetVRAMAddress();run_wireworld();
    int white[2]={0,0};
    for(int y=0;y<VIS_H;y++)for(int x=0;x<VIS_W;x++){
        unsigned short col=vram[(size_t)y*VRAM_W+x];
        assert(col==COL_BG || col==COL_CURSOR);
        if(col==COL_CURSOR){if(y>=row_y(2,0)&&y<row_y(2,0)+font_h)white[0]++;
                           if(y>=row_y(2,1)&&y<row_y(2,1)+font_h)white[1]++;}
    }
    assert(frames==1 && white[0]>20 && white[1]>20);
    puts("Wireworld low memory: both message lines visible in the real black/white palette.");
    return 0;
}
