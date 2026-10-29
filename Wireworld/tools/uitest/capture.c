/* Generate actual-VRAM host previews of Wireworld 1.64's bitmap UI.
 * Build with the same host stubs as ui_test.c. Captures are simulations, not
 * photographs of the calculator. Output is PPM; convert to PNG if desired.
 */
#define main ww_main
#include "../../src/main.c"
#undef main
#include <stdio.h>
#include <stdlib.h>

int host_key_down(int key) { (void)key; return 0; }
void host_putdisp(void) { }
void host_tick(void) { }
int host_os_menu(void) { return 0; }

static void capture(const char *name)
{
    FILE *f = fopen(name, "wb"); int x,y;
    if (!f) { perror(name); exit(1); }
    fprintf(f, "P6\n%d %d\n255\n", VIS_W, VIS_H);
    for(y=0;y<VIS_H;y++) for(x=0;x<VIS_W;x++) {
        unsigned c=vram[(size_t)y*VRAM_W+x];
        unsigned char rgb[3]={(unsigned char)(((c>>11)&31)*255/31),
            (unsigned char)(((c>>5)&63)*255/63),(unsigned char)((c&31)*255/31)};
        fwrite(rgb,1,3,f);
    }
    fclose(f);
}
int main(int argc, char **argv)
{
    int i, x0, y0, x1, y1; char path[512];
    const char *dir=(argc>1)?argv[1]:".";
    #define SHOT(name) do { snprintf(path,sizeof(path),"%s/%s.ppm",dir,name); capture(path); } while (0)
    vram=(unsigned short*)GetVRAMAddress();
    if(ww_init(&world,512,256)) return 2;
    field_index=FIELD_N-1; recalc_view(); load_pattern(0); render_frame();
    SHOT("start");

    cam_x=cam_y=0; cur_x=38; cur_y=18; running=1; toast_on=0;
    for(i=0;i<30;i++) ww_step(&world,0,0);
    mark_all(); render_frame(); SHOT("field");
    draw_menu(M_SPEED,0); SHOT("menu");

    wipe_screen(); mark_all(); render_frame();
    x0=8; y0=2; x1=VIS_W-9; y1=box_bottom(y0,HELP_LINES);
    draw_box(x0,y0,x1,y1);
    mini_text(x0+6,row_y(y0,0),help_pages[1][0],COL_BOXFG,COL_BOXBG);
    for(i=1;i<HELP_LINES;i++)
        mini_text(x0+6,row_y(y0,i),help_pages[1][i],COL_BOXDIM,COL_BOXBG);
    SHOT("help");

    zoom=3; recalc_view(); cam_x=cam_y=0; cur_x=35; cur_y=16;
    mark_all(); render_frame(); SHOT("zoom");
    toast("PASTE 2x3 - ROTATE 90 DEG"); render_frame(); SHOT("toast");
    ww_free(&world); return 0;
}
