/*
 * main.c -- Wireworld for the CASIO fx-CG50 (Prizm) add-in.
 *
 * Build: see the Makefile next to this file (needs PrizmSDK / libfxcg).
 *
 * Screen. GetVRAMAddress() returns the visible 384x216 RGB565 video buffer.
 * A compact embedded 6x11 bitmap font makes the text layout independent of
 * OS font metrics; the status line is optional and the rest is the cell grid.
 *
 * The field is much bigger than the screen (see field_list[]) and wraps around
 * in both directions, so it behaves like an endless sheet: the view follows the
 * cursor and electrons that leave one edge come back on the other.  The seam
 * where the world wraps onto itself is marked with a thin line.
 *
 * Repainting.  Everything goes through one dirty rectangle of screen cells.
 * Each frame: the rectangle covered by the previous frame's overlays (cursor,
 * shape preview, pop-up message) is marked dirty, the dirty cells are redrawn
 * from the field, then the overlays are drawn again.  Nothing is ever "erased"
 * by hand, so nothing can leave a trail behind.
 */
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/heap.h>
#include <fxcg/system.h>
#include <fxcg/rtc.h>
#include <fxcg/file.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "wireworld.h"
#include "patterns.h"
#include "font_ww.h"

/* ------------------------------------------------------------------ */
/* geometry                                                            */
/* ------------------------------------------------------------------ */
#define VRAM_W      384     /* OS video buffer == the visible area */
#define VIS_W       384
#define VIS_H       216
/* Fixed bitmap cell metrics; independent of calculator OS font versions. */
static const int font_h   = WW_FONT_H;
static const int font_off = 0;
static const int row_h    = WW_FONT_H + 3;
static const int hdr_h    = WW_FONT_H + 2;

/* ------------------------------------------------------------------ */
/* palette (RGB565)                                                    */
/* ------------------------------------------------------------------ */
#define COL_BG      0x0000  /* empty cell                        */
#define COL_GRID    0x3186  /* ruler mark every 10 cells         */
#define COL_GRID2   0x18C3  /* thin grid lines                   */
#define COL_EDGE    0xA01F  /* seam where the world wraps        */
#define COL_WIRE    0xFFE0  /* conductor (yellow)                */
#define COL_HEAD    0x03FF  /* electron head (bright blue/cyan)  */
#define COL_TAIL    0xF800  /* electron tail (red)               */
#define COL_CURSOR   0xFFFF
#define COL_PREVIEW  0xC618  /* shape / paste box being placed    */
#define COL_COPY_BOX 0x07E0  /* green frame while selecting copy  */
#define COL_CUT_BOX  0xFD20  /* orange frame while selecting cut  */
#define COL_HDRBG    0x0846  /* deep navy, status line            */
#define COL_HDRFG    0xFFFF
#define COL_HDRDIM   0xBDF7
#define COL_ACCENT   0x5DFF  /* light blue, UI accent             */
#define COL_BOXBG    0x0827  /* deep navy, menus and messages     */
#define COL_BOXEDGE  0x3BB3  /* muted blue border                 */
#define COL_BOXFG    0xFFFF
#define COL_BOXDIM   0xCE59
#define COL_BOXSEL   0x11B0  /* selected row                      */

/* ------------------------------------------------------------------ */
/* tools, materials, speeds, sizes                                     */
/* ------------------------------------------------------------------ */
enum { MAT_WIRE = 0, MAT_HEAD, MAT_TAIL, MAT_ERASE, MAT_COUNT };
enum {
    TOOL_PEN = 0, TOOL_LINE, TOOL_RECT,
    TOOL_COPY, TOOL_CUT, TOOL_PASTE, TOOL_COUNT
};
enum { GRID_OFF = 0, GRID_DOTS, GRID_LINES };

static const int mat_state[MAT_COUNT] = {
    WW_CONDUCTOR, WW_HEAD, WW_TAIL, WW_EMPTY
};
static const char *mat_name[MAT_COUNT] = { "WIRE", "HEAD", "TAIL", "ERASE" };

/* target speed in generations per second; 0 = as fast as the CPU can */
static const int speed_ips[8] = { 1, 2, 5, 10, 20, 30, 60, 0 };
#define SPEED_COUNT (int)(sizeof(speed_ips) / sizeof(speed_ips[0]))

/* pixels per cell */
static const int zooms[6] = { 1, 2, 3, 4, 6, 8 };
#define ZOOM_COUNT (int)(sizeof(zooms) / sizeof(zooms[0]))

/* world sizes offered in the menu, in cells (the area is what costs memory:
   one byte + one bit per cell, so 512x256 needs 147 kB) */
static const int field_list[][2] = {
    { 128, 128 }, { 192, 128 }, { 192, 192 }, { 256, 192 },
    { 256, 256 }, { 320, 256 }, { 384, 256 }, { 512, 256 }
};
#define FIELD_N (int)(sizeof(field_list) / sizeof(field_list[0]))

#define WW_FILE     "\\\\fls0\\Wireworld.ww"

/* ------------------------------------------------------------------ */
/* raw keyboard matrix (same trick as the SDK's starfield example)     */
/* ------------------------------------------------------------------ */
#ifndef KEY_PRGM_MUL
#define KEY_PRGM_MUL  43   /* [x]   copy region                     */
#define KEY_PRGM_DIV  33   /* [/]   cut region                      */
#define KEY_PRGM_ADD  42   /* [+]   paste (press again: box/transp) */
#define KEY_PRGM_SUB  32   /* [-]   rotate 90 CW (SHIFT+- : 90 CCW) */
#define KEY_PRGM_LPAR 55   /* [(]   rotate 90 CCW                   */
#define KEY_PRGM_RPAR 45   /* [)]   rotate 90 CW                    */
#define KEY_PRGM_DOT  61   /* [.]   flip horizontally (L/R)         */
#define KEY_PRGM_NEG  41   /* [(-)] flip vertically (U/D)           */
#endif

#define KEY_PORT ((volatile unsigned short *)0xA44B0000)

#ifdef HOST_TEST
/* unit tests on a PC (see tools/uitest): the key state comes from the driver */
int host_key_down(int code);
static int key_down(int code) { return host_key_down(code); }
#else
static int key_down(int code)
{
    int row = code % 10;
    int col = code / 10 - 1;
    int word = row >> 1;
    int bit = col + ((row & 1) << 3);
    return (KEY_PORT[word] & (1 << bit)) != 0;
}
#endif

static const unsigned char scan_order[] = {
    KEY_PRGM_UP, KEY_PRGM_DOWN, KEY_PRGM_LEFT, KEY_PRGM_RIGHT,
    KEY_PRGM_RETURN, KEY_PRGM_EXIT,
    KEY_PRGM_F1, KEY_PRGM_F2, KEY_PRGM_F3,
    KEY_PRGM_F4, KEY_PRGM_F5, KEY_PRGM_F6,
    KEY_PRGM_1, KEY_PRGM_2, KEY_PRGM_3, KEY_PRGM_0,
    KEY_PRGM_4, KEY_PRGM_5, KEY_PRGM_6,
    KEY_PRGM_7, KEY_PRGM_8, KEY_PRGM_9,
    KEY_PRGM_MUL, KEY_PRGM_DIV, KEY_PRGM_ADD, KEY_PRGM_SUB,
    KEY_PRGM_LPAR, KEY_PRGM_RPAR, KEY_PRGM_DOT, KEY_PRGM_NEG,
    KEY_PRGM_MENU, KEY_PRGM_OPTN,
    0
};

static int scan_keys(void)
{
    int i;
    for (i = 0; scan_order[i]; i++)
        if (key_down(scan_order[i])) return scan_order[i];
    return 0;
}

static int is_arrow(int k)
{
    return k == KEY_PRGM_UP || k == KEY_PRGM_DOWN ||
           k == KEY_PRGM_LEFT || k == KEY_PRGM_RIGHT;
}

/* The fx-CG RTC ticks at 128 Hz and resets at midnight, not at 2^24.
   Handle that rollover ourselves instead of stalling timers overnight.
   See https://prizm.cemetech.net/Syscalls/RTC/RTC_GetTicks/ */
#define RTC_HZ 128u
#define RTC_DAY_TICKS (86400u * RTC_HZ)
static unsigned rtc_ticks_between(int start, int end)
{
    unsigned a = (unsigned)start, b = (unsigned)end;
    return b >= a ? b - a : RTC_DAY_TICKS - a + b;
}

static int rtc_elapsed_ms(int start, unsigned ms)
{
    /* 125/16 ms per tick; both products fit in 32 bits for one day. */
    return rtc_ticks_between(start, RTC_GetTicks()) * 125u >= ms * 16u;
}

typedef struct {
    int last_tick, rate, credit, pending;
} SpeedClock;

/* One generation immediately on resume, then accumulate 128 Hz ticks. This
   avoids a blocking 1-second sleep at the slowest speed, and alternates 2/3
   ticks when targeting 60/s. Never replay an unbounded backlog after a menu. */
static int speed_clock_due(SpeedClock *sc, int active, int rate, int now)
{
    unsigned dt;
    if (!active) {
        sc->last_tick = now; sc->rate = rate; sc->credit = 0; sc->pending = 1;
        return 0;
    }
    if (sc->pending) {
        sc->last_tick = now; sc->rate = rate; sc->credit = 0; sc->pending = 0;
        return 1;
    }
    if (rate != sc->rate) {
        sc->last_tick = now; sc->rate = rate; sc->credit = 0;
        return rate == 0;
    }
    if (rate == 0) { sc->last_tick = now; return 1; }
    dt = rtc_ticks_between(sc->last_tick, now);
    sc->last_tick = now;
    if (dt > RTC_HZ) { sc->credit = 0; return 1; }
    sc->credit += (int)dt * rate;
    if (sc->credit < (int)RTC_HZ) return 0;
    sc->credit -= (int)RTC_HZ;
    if (sc->credit >= (int)RTC_HZ) sc->credit = (int)RTC_HZ - 1;
    return 1;
}

/* edge detection + auto repeat (300 ms, then about 18 keys/s).
   Only the arrows repeat: everything else acts once per press. */
static int poll_key(void)
{
    static int last = 0;
    static int repeating = 0;
    static int start = 0;
    int k = scan_keys();

    if (!k) { last = 0; repeating = 0; return 0; }
    if (k != last) {
        last = k;
        repeating = 0;
        start = RTC_GetTicks();
        return k;
    }
    if (!is_arrow(k)) return 0;
    if (!repeating) {
        if (rtc_elapsed_ms(start, 300)) {
            repeating = 1;
            start = RTC_GetTicks();
            return k;
        }
        return 0;
    }
    if (rtc_elapsed_ms(start, 55)) {
        start = RTC_GetTicks();
        return k;
    }
    return 0;
}

static void wait_release(void)
{
    int guard = 0;
    while (scan_keys() && guard++ < 2000) OS_InnerWait_ms(2);
}

static int wait_any_key(void)
{
    wait_release();
    for (;;) {
        int k;
        OS_InnerWait_ms(10);
        k = scan_keys();
        if (k) { wait_release(); return k; }
    }
}

/* ------------------------------------------------------------------ */
/* state                                                               */
/* ------------------------------------------------------------------ */
static WWWorld world;
static unsigned short *vram;

static int cell_px  = 2;        /* pixels per cell                     */
static int zoom     = 1;        /* index into zooms[]                  */
static int hdr_on   = 1;        /* status line on / off                */
static int grid_mode = GRID_DOTS;
static int field_index = 0;     /* index into field_list[]             */
static int view_w, view_h;      /* visible cells                       */
static int grid_top;            /* pixel row where the grid starts     */

static int cam_x, cam_y;        /* world cell in the top left corner   */
static int cur_x, cur_y;        /* cursor, in world cells              */
static int mat      = MAT_WIRE;
static int tool     = TOOL_PEN;
static int rect_fill = 0;       /* rectangle filled instead of hollow  */
static int anchor_on = 0;       /* start point of a line / rectangle   */
static int ax, ay;              /* ... that start point                */

static int running   = 0;
static int speed     = 4;       /* index into speed_ips[]              */
static int ips_count, ips_start, ips_measured;
static int pat_index = 0;
static int quit_now  = 0;

/* clipboard: 2 bits per cell in static RAM (32 KB covers up to 512x256,
   touching zero bytes of sys_malloc heap and surviving world clear/resize) */
#define CLIP_MAX_CELLS (512 * 256)
static unsigned char clip_buf[CLIP_MAX_CELLS / 4];
static int clip_w = 0, clip_h = 0;   /* unrotated size; 0 = empty      */
static int clip_rot = 0;             /* 0..3 -> 0 / 90 / 180 / 270 CW  */
static int clip_flip = 0;            /* 0 or 1 -> horizontal mirror    */
static int paste_opaque = 0;         /* 0 = transparent, 1 = overwrite */

/* pop-up message */
static char  toast_msg[40];
static int   toast_on = 0;
static int   toast_start = 0;

/* ------------------------------------------------------------------ */
/* world <-> screen                                                    */
/* ------------------------------------------------------------------ */
static void mark_all(void);

static int wrap(int v, int n)
{
    while (v < 0) v += n;
    while (v >= n) v -= n;
    return v;
}

static int to_screen(int world_pos, int cam, int size)
{
    int v = world_pos - cam;
    if (v < 0) v += size;
    else if (v >= size) v -= size;
    return v;
}

static int cursor_sx(void) { return to_screen(cur_x, cam_x, world.w); }
static int cursor_sy(void) { return to_screen(cur_y, cam_y, world.h); }

static int world_x(int sx) { return wrap(cam_x + sx, world.w); }
static int world_y(int sy) { return wrap(cam_y + sy, world.h); }

static void recalc_view(void)
{
    cell_px  = zooms[zoom];
    grid_top = hdr_on ? hdr_h : 0;
    view_w   = VIS_W / cell_px;
    view_h   = (VIS_H - grid_top) / cell_px;
    if (view_w < 8)  view_w = 8;
    if (view_h < 8)  view_h = 8;
}

static void clamp_view(void)
{
    if (world.w <= view_w) cam_x = 0; else cam_x = wrap(cam_x, world.w);
    if (world.h <= view_h) cam_y = 0; else cam_y = wrap(cam_y, world.h);
    cur_x = wrap(cur_x, world.w);
    cur_y = wrap(cur_y, world.h);
}

static void recentre(void)
{
    if (world.w <= view_w) cam_x = 0;
    else cam_x = wrap(cur_x - view_w / 2, world.w);
    if (world.h <= view_h) cam_y = 0;
    else cam_y = wrap(cur_y - view_h / 2, world.h);
}

static void wipe_screen(void);

static void view_changed(void)   /* zoom or full screen switched */
{
    recalc_view();
    wipe_screen();
    recentre();
    clamp_view();
    anchor_on = 0;
    mark_all();
}

/* ------------------------------------------------------------------ */
/* dirty rectangle (in screen cells)                                   */
/* ------------------------------------------------------------------ */
static int d_full, d_any, d_x0, d_y0, d_x1, d_y1;

static void mark_all(void) { d_full = 1; d_any = 1; }

static void mark_rect(int x0, int y0, int x1, int y1)
{
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= view_w) x1 = view_w - 1;
    if (y1 >= view_h) y1 = view_h - 1;
    if (x0 > x1 || y0 > y1) return;
    if (d_full) return;
    if (!d_any) {
        d_any = 1; d_x0 = x0; d_y0 = y0; d_x1 = x1; d_y1 = y1;
        return;
    }
    if (x0 < d_x0) d_x0 = x0;
    if (y0 < d_y0) d_y0 = y0;
    if (x1 > d_x1) d_x1 = x1;
    if (y1 > d_y1) d_y1 = y1;
}

/* ------------------------------------------------------------------ */
/* overlays: cursor, shape preview, pop-up message                     */
/* ------------------------------------------------------------------ */
static int ov_any, ov_x0, ov_y0, ov_x1, ov_y1;

static void ov_add(int x0, int y0, int x1, int y1)
{
    if (x0 > x1 || y0 > y1) return;
    if (!ov_any) {
        ov_any = 1; ov_x0 = x0; ov_y0 = y0; ov_x1 = x1; ov_y1 = y1;
        return;
    }
    if (x0 < ov_x0) ov_x0 = x0;
    if (y0 < ov_y0) ov_y0 = y0;
    if (x1 > ov_x1) ov_x1 = x1;
    if (y1 > ov_y1) ov_y1 = y1;
}

static void ov_add_pixels(int px0, int py0, int px1, int py1)
{
    int x0, y0, x1, y1;
    if (px1 < 0 || py1 < 0 || px0 >= VIS_W || py0 >= VIS_H) return;
    if (py1 < grid_top) return;
    x0 = px0 / cell_px - 1;
    y0 = (py0 - grid_top) / cell_px - 1;
    x1 = px1 / cell_px + 1;
    y1 = (py1 - grid_top) / cell_px + 1;
    if (py0 < grid_top) y0 = 0;
    ov_add(x0, y0, x1, y1);
}

/* ------------------------------------------------------------------ */
/* pixel helpers                                                       */
/* ------------------------------------------------------------------ */
static void px_fill(int x0, int y0, int x1, int y1, unsigned short col)
{
    int x, y;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 >= VIS_W) x1 = VIS_W - 1;
    if (y1 >= VIS_H) y1 = VIS_H - 1;
    for (y = y0; y <= y1; y++) {
        unsigned short *p = vram + (size_t)y * VRAM_W + x0;
        for (x = x0; x <= x1; x++) *p++ = col;
    }
}

/* wipes the whole screen: at some zooms the cells do not cover every
   pixel row, and whatever was there before has to go */
static void wipe_screen(void)
{
    px_fill(0, 0, VIS_W - 1, VIS_H - 1, COL_BG);
}

/* same, but never touches the status line */
static void px_fill_grid(int x0, int y0, int x1, int y1, unsigned short col)
{
    if (y0 < grid_top) y0 = grid_top;
    if (y1 < grid_top) return;
    px_fill(x0, y0, x1, y1, col);
}

/* Compact opaque text. Clip every pixel to the visible 384x216 VRAM;
   unsupported bytes are shown as '?'. Each row is packed MSB-first. */
static void mini_text(int x, int y, const char *s,
                      unsigned short fg, unsigned short bg)
{
    while (*s && x < VIS_W) {
        unsigned char c = (unsigned char)*s++;
        const unsigned char *glyph;
        int gy, gx;
        if (c < 32 || c > 126) c = '?';
        glyph = ww_glyphs[c - 32];
        for (gy = 0; gy < WW_FONT_H; gy++) {
            int py = y + gy;
            unsigned char bits = glyph[gy];
            unsigned short *row;
            if (py < 0 || py >= VIS_H) continue;
            row = vram + (size_t)py * VRAM_W;
            for (gx = 0; gx < WW_FONT_W; gx++) {
                int px = x + gx;
                if (px >= 0 && px < VIS_W)
                    row[px] = (bits & (0x80u >> gx)) ? fg : bg;
            }
        }
        x += WW_FONT_W;
    }
}

/* where the glyphs of row i of a box that starts at y0 have to be put */
static int row_y(int y0, int i) { return y0 + 4 + row_h * i - font_off; }

/* how many rows of text fit on the screen, box at y = 2 */
static int rows_that_fit(void) { return (VIS_H - 9 - font_h) / row_h + 1; }

/* ------------------------------------------------------------------ */
/* cells                                                               */
/* ------------------------------------------------------------------ */
static unsigned short cell_color(int state, int wx, int wy)
{
    switch (state) {
    case WW_HEAD:      return COL_HEAD;
    case WW_TAIL:      return COL_TAIL;
    case WW_CONDUCTOR: return COL_WIRE;
    default:           break;
    }
    /* lines only make sense from 3 pixels per cell on, smaller: ruler marks */
    if (grid_mode == GRID_DOTS || (grid_mode == GRID_LINES && cell_px < 3))
        if (wx % 10 == 0 && wy % 10 == 0) return COL_GRID;
    return COL_BG;
}

static void draw_cell(int sx, int sy)
{
    int wx = world_x(sx), wy = world_y(sy);
    int state = ww_get(&world, wx, wy);
    unsigned short col = cell_color(state, wx, wy);
    unsigned short *p = vram + (size_t)(grid_top + sy * cell_px) * VRAM_W
                        + sx * cell_px;
    int i, j;

    if (cell_px == 1 && state == WW_EMPTY && (wx == 0 || wy == 0))
        col = COL_EDGE;                      /* no room for a line */

    for (i = 0; i < cell_px; i++) {
        for (j = 0; j < cell_px; j++) p[j] = col;
        p += VRAM_W;
    }

    if (grid_mode == GRID_LINES && cell_px >= 3) {
        unsigned short gc = ((wx + 1) % 10 == 0) ? COL_GRID : COL_GRID2;
        int last = cell_px - 1;
        p = vram + (size_t)(grid_top + sy * cell_px) * VRAM_W + sx * cell_px;
        for (j = 0; j < cell_px; j++) p[last * VRAM_W + j] = gc;
        for (i = 0; i < cell_px; i++) p[i * VRAM_W + last] = gc;
    }

    /* the seam: where the world wraps onto itself */
    if (cell_px >= 2 && (wx == 0 || wy == 0)) {
        p = vram + (size_t)(grid_top + sy * cell_px) * VRAM_W + sx * cell_px;
        if (wy == 0)
            for (j = 0; j < cell_px; j++) p[j] = COL_EDGE;
        if (wx == 0)
            for (i = 0; i < cell_px; i++) p[i * VRAM_W] = COL_EDGE;
    }
}

static void flush_cells(void)
{
    int x, y;
    int gx1 = view_w * cell_px;
    int gy1 = grid_top + view_h * cell_px;
    if (!d_any) return;

    if (d_full ||
        (d_x1 - d_x0 + 1) * (d_y1 - d_y0 + 1) > (view_w * view_h) / 2) {
        for (y = 0; y < view_h; y++)
            for (x = 0; x < view_w; x++) draw_cell(x, y);
    } else {
        for (y = d_y0; y <= d_y1; y++)
            for (x = d_x0; x <= d_x1; x++) draw_cell(x, y);
    }
    /* wipe any 1..7 px remainder strips not covered by whole cells */
    if (gx1 < VIS_W) px_fill(gx1, grid_top, VIS_W - 1, VIS_H - 1, COL_BG);
    if (gy1 < VIS_H) px_fill(0, gy1, VIS_W - 1, VIS_H - 1, COL_BG);
    d_full = 0;
    d_any  = 0;
}

/* ------------------------------------------------------------------ */
/* status line and pop-up message                                      */
/* ------------------------------------------------------------------ */
static const char *tool_label(void)
{
    static char buf[36];
    if (tool == TOOL_PEN) {
        sprintf(buf, "PEN %s", mat_name[mat]);
    } else if (tool == TOOL_LINE) {
        sprintf(buf, "LINE %s", mat_name[mat]);
    } else if (tool == TOOL_RECT) {
        sprintf(buf, "RECT%s %s", rect_fill ? "F" : "", mat_name[mat]);
    } else if (tool == TOOL_COPY || tool == TOOL_CUT) {
        const char *tn = (tool == TOOL_CUT) ? "CUT" : "COPY";
        if (anchor_on)
            sprintf(buf, "%s %dx%d", tn,
                    abs(cur_x - ax) + 1, abs(cur_y - ay) + 1);
        else
            sprintf(buf, "%s", tn);
    } else {
        sprintf(buf, "PASTE%s %d%s", paste_opaque ? "B" : "",
                (clip_rot & 3) * 90, clip_flip ? "M" : "");
    }
    return buf;
}

static int header_part(int x, const char *text, unsigned short color)
{
    mini_text(x, 1, text, color, COL_HDRBG);
    return x + (int)strlen(text) * WW_FONT_W;
}

static void draw_header(void)
{
    char buf[96], sbuf[24];
    int x = 2;

    if (speed_ips[speed] == 0) strcpy(sbuf, "MAX");
    else if (running && ips_measured)
        sprintf(sbuf, "%d/%d", speed_ips[speed], ips_measured);
    else sprintf(sbuf, "%d", speed_ips[speed]);

    px_fill(0, 0, VIS_W - 1, hdr_h - 1, COL_HDRBG);
    px_fill(0, hdr_h - 1, VIS_W - 1, hdr_h - 1, COL_BOXEDGE);
    sprintf(buf, "G%06lu", world.gen);
    x = header_part(x, buf, COL_HDRFG);
    sprintf(buf, " S%s ", sbuf);
    x = header_part(x, buf, COL_HDRDIM);
    x = header_part(x, running ? "RUN" : "STOP",
                    running ? COL_ACCENT : COL_WIRE);
    sprintf(buf, " %s%s", tool_label(), anchor_on ? "*" : "");
    x = header_part(x, buf, COL_HDRFG);
    sprintf(buf, " X%03d Y%03d Z%d", cur_x, cur_y, cell_px);
    header_part(x, buf, COL_HDRDIM);
}

static void toast(const char *msg)
{
    strncpy(toast_msg, msg, sizeof(toast_msg) - 1);
    toast_msg[sizeof(toast_msg) - 1] = 0;
    toast_on = 1;
    toast_start = RTC_GetTicks();
}

static void draw_toast(void)
{
    int w = (int)strlen(toast_msg) * WW_FONT_W + 12;
    int x0 = 4, y0 = VIS_H - (font_h + 8), x1, y1 = VIS_H - 4;
    if (w > VIS_W - 8) w = VIS_W - 8;
    x1 = x0 + w;
    px_fill(x0, y0, x1, y1, COL_BOXBG);
    px_fill(x0, y0, x1, y0, COL_BOXEDGE);
    px_fill(x0, y1, x1, y1, COL_BOXEDGE);
    px_fill(x0, y0, x0, y1, COL_ACCENT);
    px_fill(x1, y0, x1, y1, COL_BOXEDGE);
    mini_text(x0 + 5, y0 + 3 - font_off, toast_msg, COL_BOXFG, COL_BOXBG);
    ov_add_pixels(x0, y0, x1, y1);
}

/* ------------------------------------------------------------------ */
/* cursor and shape preview                                            */
/* ------------------------------------------------------------------ */
/* a ring around the cell: at 1 pixel per cell a plain block would be a
   single dot, impossible to find on a 384x216 field */
static void draw_cursor_box(void)
{
    int sx = cursor_sx(), sy = cursor_sy();
    int px = sx * cell_px, py = grid_top + sy * cell_px;
    int x0 = px - 1, y0 = py - 1, x1 = px + cell_px, y1 = py + cell_px;
    int t = (cell_px >= 6) ? 2 : 1;

    if (sx < 0 || sy < 0 || sx >= view_w || sy >= view_h) return;

    px_fill_grid(x0, y0, x1, y0 + t - 1, COL_CURSOR);
    px_fill_grid(x0, y1 - t + 1, x1, y1, COL_CURSOR);
    px_fill_grid(x0, y0 + t, x0 + t - 1, y1 - t, COL_CURSOR);
    px_fill_grid(x1 - t + 1, y0 + t, x1, y1 - t, COL_CURSOR);

    ov_add(sx - 1, sy - 1, sx + 1, sy + 1);
}

static void preview_cell(int wx, int wy)
{
    int sx = to_screen(wx, cam_x, world.w);
    int sy = to_screen(wy, cam_y, world.h);
    if (sx < 0 || sy < 0 || sx >= view_w || sy >= view_h) return;
    px_fill(sx * cell_px, grid_top + sy * cell_px,
            sx * cell_px + cell_px - 1, grid_top + sy * cell_px + cell_px - 1,
            COL_PREVIEW);
    ov_add(sx, sy, sx, sy);
}

/* ------------------------------------------------------------------ */
/* shapes: line and rectangle                                          */
/* ------------------------------------------------------------------ */
/* a world cell changed: repaint it if it is on screen */
static void touch_world(int wx, int wy)
{
    int sx = to_screen(wx, cam_x, world.w);
    int sy = to_screen(wy, cam_y, world.h);
    if (sx < 0 || sy < 0 || sx >= view_w || sy >= view_h) return;
    mark_rect(sx, sy, sx, sy);
}

static void do_line(int x0, int y0, int x1, int y1, int commit)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy, e2;

    for (;;) {
        if (commit) { ww_set(&world, x0, y0, mat_state[mat]); touch_world(x0, y0); }
        else        { preview_cell(x0, y0); }
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void do_rect(int x0, int y0, int x1, int y1, int commit)
{
    int x, y;
    int xa = (x0 < x1) ? x0 : x1, xb = (x0 < x1) ? x1 : x0;
    int ya = (y0 < y1) ? y0 : y1, yb = (y0 < y1) ? y1 : y0;

    for (y = ya; y <= yb; y++) {
        for (x = xa; x <= xb; x++) {
            int edge = (y == ya || y == yb || x == xa || x == xb);
            if (!edge && !rect_fill) continue;
            if (commit) { ww_set(&world, x, y, mat_state[mat]); touch_world(x, y); }
            else        { preview_cell(x, y); }
        }
    }
}

static void do_shape(int commit)
{
    if (tool == TOOL_LINE)      do_line(ax, ay, cur_x, cur_y, commit);
    else if (tool == TOOL_RECT) do_rect(ax, ay, cur_x, cur_y, commit);
}

/* ------------------------------------------------------------------ */
/* clipboard: copy, cut, paste, rotate (0/90/180/270) and mirror       */
/* ------------------------------------------------------------------ */
static int clip_get_raw(int x, int y)
{
    int idx = y * clip_w + x;
    return (clip_buf[idx >> 2] >> ((idx & 3) * 2)) & 3;
}

static void clip_set_raw(int x, int y, int state)
{
    int idx = y * clip_w + x;
    int sh = (idx & 3) * 2;
    clip_buf[idx >> 2] = (unsigned char)((clip_buf[idx >> 2] & ~(3 << sh)) |
                                         ((state & 3) << sh));
}

static void clip_dims(int *rw, int *rh)
{
    if (clip_rot & 1) { *rw = clip_h; *rh = clip_w; }
    else              { *rw = clip_w; *rh = clip_h; }
}

/* Screen transform = R^clip_rot(F^clip_flip(source)), inverted per cell */
static int clip_sample(int rx, int ry)
{
    int sx, sy;
    switch (clip_rot & 3) {
    case 0:  sx = rx;              sy = ry;              break;
    case 1:  sx = ry;              sy = clip_h - 1 - rx; break;
    case 2:  sx = clip_w - 1 - rx; sy = clip_h - 1 - ry; break;
    default: sx = clip_w - 1 - ry; sy = rx;              break;
    }
    if (clip_flip) sx = clip_w - 1 - sx;
    return clip_get_raw(sx, sy);
}

/* Clamps strictly to pixel area covered by grid cells [0..view_w-1] x
   [0..view_h-1], so flush_cells() always repaints every outline pixel. */
static void px_fill_cells(int x0, int y0, int x1, int y1, unsigned short col)
{
    int xmax = view_w * cell_px - 1;
    int ymax = grid_top + view_h * cell_px - 1;
    if (xmax >= VIS_W) xmax = VIS_W - 1;
    if (ymax >= VIS_H) ymax = VIS_H - 1;
    if (x0 < 0) x0 = 0;
    if (y0 < grid_top) y0 = grid_top;
    if (x1 > xmax) x1 = xmax;
    if (y1 > ymax) y1 = ymax;
    if (x0 > x1 || y0 > y1) return;
    px_fill(x0, y0, x1, y1, col);
}

/* 1 px outer frame around world rectangle [x0..x0+w-1] x [y0..y0+h-1];
   sits just outside the cells like draw_cursor_box, hiding nothing inside. */
static void draw_box_outline(int x0, int y0, int w, int h, unsigned short col)
{
    int dx, dy;
    int xmax = view_w * cell_px - 1;
    int ymax = grid_top + view_h * cell_px - 1;
    if (xmax >= VIS_W) xmax = VIS_W - 1;
    if (ymax >= VIS_H) ymax = VIS_H - 1;

    for (dx = 0; dx < w; dx++) {
        int wx = wrap(x0 + dx, world.w);
        int sx = to_screen(wx, cam_x, world.w);
        int sy_t, sy_b;
        if (sx < 0 || sx >= view_w) continue;

        sy_t = to_screen(wrap(y0, world.h), cam_y, world.h);
        if (sy_t >= 0 && sy_t < view_h) {
            int py = grid_top + sy_t * cell_px - 1;
            int px0 = sx * cell_px - (dx == 0 ? 1 : 0);
            int px1 = (sx + 1) * cell_px - 1 + (dx == w - 1 ? 1 : 0);
            if (py < grid_top) py = grid_top;
            px_fill_cells(px0, py, px1, py, col);
            ov_add(sx - 1, sy_t - 1, sx + 1, sy_t + 1);
        }

        sy_b = to_screen(wrap(y0 + h - 1, world.h), cam_y, world.h);
        if (sy_b >= 0 && sy_b < view_h) {
            int py = grid_top + (sy_b + 1) * cell_px;
            int px0 = sx * cell_px - (dx == 0 ? 1 : 0);
            int px1 = (sx + 1) * cell_px - 1 + (dx == w - 1 ? 1 : 0);
            if (py > ymax) py = ymax;
            px_fill_cells(px0, py, px1, py, col);
            ov_add(sx - 1, sy_b - 1, sx + 1, sy_b + 1);
        }
    }

    for (dy = 0; dy < h; dy++) {
        int wy = wrap(y0 + dy, world.h);
        int sy = to_screen(wy, cam_y, world.h);
        int sx_l, sx_r;
        if (sy < 0 || sy >= view_h) continue;

        sx_l = to_screen(wrap(x0, world.w), cam_x, world.w);
        if (sx_l >= 0 && sx_l < view_w) {
            int px = sx_l * cell_px - 1;
            int py0 = grid_top + sy * cell_px;
            if (px < 0) px = 0;
            px_fill_cells(px, py0, px, py0 + cell_px - 1, col);
            ov_add(sx_l - 1, sy - 1, sx_l + 1, sy + 1);
        }

        sx_r = to_screen(wrap(x0 + w - 1, world.w), cam_x, world.w);
        if (sx_r >= 0 && sx_r < view_w) {
            int px = (sx_r + 1) * cell_px;
            int py0 = grid_top + sy * cell_px;
            if (px > xmax) px = xmax;
            px_fill_cells(px, py0, px, py0 + cell_px - 1, col);
            ov_add(sx_r - 1, sy - 1, sx_r + 1, sy + 1);
        }
    }
}

static void do_clip_region(int x0, int y0, int x1, int y1,
                           int is_cut, int commit)
{
    int xa = (x0 < x1) ? x0 : x1, xb = (x0 < x1) ? x1 : x0;
    int ya = (y0 < y1) ? y0 : y1, yb = (y0 < y1) ? y1 : y0;
    int w = xb - xa + 1, h = yb - ya + 1;
    int dx, dy;
    char buf[48];

    if (!commit) {
        draw_box_outline(xa, ya, w, h, is_cut ? COL_CUT_BOX : COL_COPY_BOX);
        return;
    }

    if ((size_t)w * (size_t)h > CLIP_MAX_CELLS) {
        toast("REGION TOO LARGE");
        return;
    }

    clip_w = w;
    clip_h = h;
    clip_rot = 0;
    clip_flip = 0;
    memset(clip_buf, 0, ((size_t)w * (size_t)h + 3) / 4);

    for (dy = 0; dy < h; dy++) {
        for (dx = 0; dx < w; dx++) {
            int st = ww_get(&world, xa + dx, ya + dy);
            if (st != WW_EMPTY) clip_set_raw(dx, dy, st);
            if (is_cut && st != WW_EMPTY) {
                ww_set(&world, xa + dx, ya + dy, WW_EMPTY);
                touch_world(xa + dx, ya + dy);
            }
        }
    }

    sprintf(buf, "%s %dx%d (+ PASTE)", is_cut ? "CUT" : "COPIED", w, h);
    toast(buf);
}

static void draw_paste_preview(void)
{
    int rw, rh, lim_w, lim_h, rx, ry;
    if (clip_w <= 0 || clip_h <= 0) return;

    clip_dims(&rw, &rh);
    lim_w = rw < world.w ? rw : world.w;
    lim_h = rh < world.h ? rh : world.h;

    for (ry = 0; ry < lim_h; ry++) {
        int wy = wrap(cur_y + ry, world.h);
        int sy = to_screen(wy, cam_y, world.h);
        if (sy < 0 || sy >= view_h) continue;
        for (rx = 0; rx < lim_w; rx++) {
            int wx = wrap(cur_x + rx, world.w);
            int sx = to_screen(wx, cam_x, world.w);
            int st;
            unsigned short col;
            if (sx < 0 || sx >= view_w) continue;
            st = clip_sample(rx, ry);
            if (st == WW_EMPTY) {
                if (!paste_opaque) continue;
                col = COL_BG;
            } else {
                col = cell_color(st, wx, wy);
            }
            px_fill(sx * cell_px, grid_top + sy * cell_px,
                    sx * cell_px + cell_px - 1,
                    grid_top + sy * cell_px + cell_px - 1, col);
            ov_add(sx, sy, sx, sy);
        }
    }
    draw_box_outline(cur_x, cur_y, lim_w, lim_h, COL_PREVIEW);
}

static void commit_paste(void)
{
    int rw, rh, lim_w, lim_h, rx, ry;
    if (clip_w <= 0 || clip_h <= 0) {
        toast("CLIPBOARD IS EMPTY");
        return;
    }
    clip_dims(&rw, &rh);
    lim_w = rw < world.w ? rw : world.w;
    lim_h = rh < world.h ? rh : world.h;

    for (ry = 0; ry < lim_h; ry++) {
        int wy = wrap(cur_y + ry, world.h);
        for (rx = 0; rx < lim_w; rx++) {
            int st = clip_sample(rx, ry);
            int wx;
            if (st == WW_EMPTY && !paste_opaque) continue;
            wx = wrap(cur_x + rx, world.w);
            ww_set(&world, wx, wy, st);
            touch_world(wx, wy);
        }
    }
}

static void start_copy_tool(int is_cut)
{
    tool = is_cut ? TOOL_CUT : TOOL_COPY;
    anchor_on = 0;
    mark_all();
    toast(is_cut ? "CUT: EXE, MOVE, EXE" : "COPY: EXE, MOVE, EXE");
}

static void start_paste_tool(int allow_toggle)
{
    if (clip_w <= 0 || clip_h <= 0) {
        toast("CLIPBOARD IS EMPTY");
        return;
    }
    if (tool == TOOL_PASTE && allow_toggle) {
        paste_opaque = !paste_opaque;
        mark_all();
        toast(paste_opaque ? "PASTE: OVERWRITE BOX" : "PASTE: TRANSPARENT");
        return;
    }
    tool = TOOL_PASTE;
    anchor_on = 0;
    mark_all();
    toast("PASTE: EXE PUT, - ROTATE");
}

static void rotate_clipboard(int dir)
{
    char buf[32];
    if (clip_w <= 0 || clip_h <= 0) {
        toast("CLIPBOARD IS EMPTY");
        return;
    }
    clip_rot = (clip_rot + (dir > 0 ? 1 : 3)) & 3;
    if (tool != TOOL_PASTE) {
        tool = TOOL_PASTE;
        anchor_on = 0;
    }
    mark_all();
    sprintf(buf, "ROTATE %d DEG", clip_rot * 90);
    toast(buf);
}

static void flip_clipboard(int vertical)
{
    if (clip_w <= 0 || clip_h <= 0) {
        toast("CLIPBOARD IS EMPTY");
        return;
    }
    if (vertical)
        clip_rot = (2 - clip_rot + 4) & 3;
    else
        clip_rot = (4 - clip_rot) & 3;
    clip_flip = !clip_flip;
    if (tool != TOOL_PASTE) {
        tool = TOOL_PASTE;
        anchor_on = 0;
    }
    mark_all();
    toast(vertical ? "FLIP VERTICAL" : "FLIP HORIZONTAL");
}

/* ------------------------------------------------------------------ */
/* one frame                                                           */
/* ------------------------------------------------------------------ */
static int last_frame_draw;

static void render_frame(void)
{
    if (ov_any) { mark_rect(ov_x0, ov_y0, ov_x1, ov_y1); ov_any = 0; }

    flush_cells();
    if (hdr_on) draw_header();

    if (tool == TOOL_PASTE && clip_w > 0)
        draw_paste_preview();
    else if (anchor_on && (tool == TOOL_COPY || tool == TOOL_CUT))
        do_clip_region(ax, ay, cur_x, cur_y, tool == TOOL_CUT, 0);
    else if (anchor_on && tool != TOOL_PEN)
        do_shape(0);

    draw_cursor_box();
    if (toast_on) draw_toast();

    Bdisp_PutDisp_DD();

    /* FrameColor only remembers the colour, DrawFrame actually paints the
       border of the 396x224 panel around our 384x216 area.  The OS likes to
       take its white back, so it is repainted now and then. */
    if (rtc_elapsed_ms(last_frame_draw, 250)) {
        last_frame_draw = RTC_GetTicks();
        DrawFrame(COL_BG);
    }
}

/* ------------------------------------------------------------------ */
/* simulation                                                          */
/* ------------------------------------------------------------------ */
static void on_change(int x, int y, int state, void *ctx)
{
    (void)state;
    (void)ctx;
    touch_world(x, y);
}

static void step_frame(void)
{
    int changed = ww_step_readonly_hook(&world, on_change, NULL);
    if (changed == 0) {
        running = 0;
        toast("NOTHING MOVES - PRESS F1");
    }
}

/* ------------------------------------------------------------------ */
/* editing                                                             */
/* ------------------------------------------------------------------ */
static void follow_cursor(void)
{
    const int margin = 4;
    int sx = cursor_sx(), sy = cursor_sy();
    int moved = 0;

    if (world.w > view_w) {
        if (sx < margin) {
            cam_x = wrap(cam_x - (margin - sx), world.w);
            moved = 1;
        } else if (sx > view_w - 1 - margin) {
            cam_x = wrap(cam_x + (sx - (view_w - 1 - margin)), world.w);
            moved = 1;
        }
    }
    if (world.h > view_h) {
        if (sy < margin) {
            cam_y = wrap(cam_y - (margin - sy), world.h);
            moved = 1;
        } else if (sy > view_h - 1 - margin) {
            cam_y = wrap(cam_y + (sy - (view_h - 1 - margin)), world.h);
            moved = 1;
        }
    }
    if (moved) mark_all();
}

static void move_cursor(int dx, int dy)
{
    cur_x = wrap(cur_x + dx, world.w);
    cur_y = wrap(cur_y + dy, world.h);

    if (key_down(KEY_PRGM_1) || key_down(KEY_PRGM_2) ||
        key_down(KEY_PRGM_3) || key_down(KEY_PRGM_0)) {
        ww_set(&world, cur_x, cur_y, mat_state[mat]);
        touch_world(cur_x, cur_y);
    }
    follow_cursor();
}

static void paint_here(void)          /* pen: one cell */
{
    ww_set(&world, cur_x, cur_y, mat_state[mat]);
    touch_world(cur_x, cur_y);
}

static void act(void)                 /* EXE */
{
    if (tool == TOOL_PEN) {
        paint_here();
        return;
    }
    if (tool == TOOL_PASTE) {
        commit_paste();
        return;
    }
    if (!anchor_on) {
        anchor_on = 1;
        ax = cur_x;
        ay = cur_y;
        return;
    }
    if (tool == TOOL_COPY || tool == TOOL_CUT)
        do_clip_region(ax, ay, cur_x, cur_y, tool == TOOL_CUT, 1);
    else
        do_shape(1);
    anchor_on = 0;
}

static void clear_field(void)
{
    ww_clear(&world);
    running = 0;
    anchor_on = 0;
    mark_all();
}

/* Place the camera on the actual demo rather than the centre of an empty
   large world. At tight zooms, favour the electron heads so the moving
   part of a circuit stays in view. The cells themselves stay at (2,2). */
static void focus_pattern(void)
{
    int x, y, x0 = world.w, y0 = world.h, x1 = -1, y1 = -1;
    int hx0 = world.w, hy0 = world.h, hx1 = -1, hy1 = -1;
    for (y = 0; y < world.h; y++) for (x = 0; x < world.w; x++) {
        int c = world.cells[y * world.w + x];
        if (c == WW_EMPTY) continue;
        if (x < x0) x0 = x;
        if (x > x1) x1 = x;
        if (y < y0) y0 = y;
        if (y > y1) y1 = y;
        if (c == WW_HEAD) {
            if (x < hx0) hx0 = x;
            if (x > hx1) hx1 = x;
            if (y < hy0) hy0 = y;
            if (y > hy1) hy1 = y;
        }
    }
    if (x1 < 0) { cur_x = world.w / 2; cur_y = world.h / 2; }
    else {
        if (hx1 >= 0 && x1 - x0 + 1 > view_w) { x0 = hx0; x1 = hx1; }
        if (hy1 >= 0 && y1 - y0 + 1 > view_h) { y0 = hy0; y1 = hy1; }
        cur_x = (x0 + x1) / 2;
        cur_y = (y0 + y1) / 2;
    }
    recentre();
    clamp_view();
}

static void load_pattern(int idx)
{
    pat_index = idx;
    ww_clear(&world);
    ww_load_text(&world, patterns[idx].art, 2, 2);
    cam_x = cam_y = 0;
    running = 0;
    anchor_on = 0;
    focus_pattern();
    mark_all();
    toast(patterns[idx].title);
}

/* a new world of a different size; the old contents are kept where they fit */
static unsigned char field_failed[FIELD_N];

static void resize_field(int idx)
{
    WWWorld tmp;
    int x, y, cw, ch;
    char buf[32];

    if (idx == field_index) return;

    /* First try to get the new field while keeping the old one: that way
       the contents survive.  Growing the field usually fails there (both
       fields have to fit at once), so then the old one is freed first and
       the world starts empty. */
    if (ww_init(&tmp, field_list[idx][0], field_list[idx][1]) != 0) {
        int w0 = world.w, h0 = world.h;

        ww_free(&world);
        if (ww_init(&tmp, field_list[idx][0], field_list[idx][1]) != 0) {
            field_failed[idx] = 1;
            if (ww_init(&world, w0, h0) == 0) {
                sprintf(buf, "NO MEMORY FOR %dx%d", field_list[idx][0],
                        field_list[idx][1]);
                toast(buf);
                return;
            }
            return;                 /* nothing left to do, field is gone */
        }
        world = tmp;
        world.gen = 0;
        field_index = idx;
        running = 0;
        anchor_on = 0;
        cur_x = cur_y = 0;          /* the seam is the proof it changed */
        wipe_screen();
        clamp_view();
        recentre();
        mark_all();
        sprintf(buf, "FIELD %dx%d, CLEARED", world.w, world.h);
        toast(buf);
        return;
    }
    cw = world.w < tmp.w ? world.w : tmp.w;
    ch = world.h < tmp.h ? world.h : tmp.h;
    for (y = 0; y < ch; y++)
        for (x = 0; x < cw; x++)
            ww_set(&tmp, x, y, ww_get(&world, x, y));
    tmp.gen = world.gen;

    ww_free(&world);
    world = tmp;
    field_index = idx;
    running = 0;
    anchor_on = 0;
    cur_x = cur_y = 0;              /* the seam is the proof it changed */
    wipe_screen();
    clamp_view();
    recentre();
    mark_all();
    sprintf(buf, "FIELD %dx%d CELLS", world.w, world.h);
    toast(buf);
}

/* ------------------------------------------------------------------ */
/* save / load                                                         */
/* ------------------------------------------------------------------ */
static void path_from_ascii(unsigned short *dst, const char *src)
{
    int i = 0;
    while (src[i] && i < 63) { dst[i] = (unsigned char)src[i]; i++; }
    dst[i] = 0;
}

/* 4 cells per byte, 2 bits each; 10 byte header "WWLD", version, w, h */
static int save_file(void)
{
    unsigned short path[64];
    static unsigned char buf[512];
    size_t size;
    int h, ret, x, y, rowbytes;

    if (!world.cells) return -1;
    rowbytes = (world.w + 3) / 4;
    if (rowbytes > (int)sizeof(buf)) return -1;

    path_from_ascii(path, WW_FILE);
    size = 10 + (size_t)rowbytes * (size_t)world.h;

    ret = Bfile_CreateEntry_OS(path, CREATEMODE_FILE, &size);
    if (ret < 0) {
        Bfile_DeleteEntry(path);
        ret = Bfile_CreateEntry_OS(path, CREATEMODE_FILE, &size);
    }
    if (ret < 0) return -1;

    h = Bfile_OpenFile_OS(path, WRITE, 0);
    if (h < 0) return -1;

    buf[0] = 'W'; buf[1] = 'W'; buf[2] = 'L'; buf[3] = 'D';
    buf[4] = 1;   buf[5] = 0;
    buf[6] = (unsigned char)(world.w & 0xFF);
    buf[7] = (unsigned char)((world.w >> 8) & 0xFF);
    buf[8] = (unsigned char)(world.h & 0xFF);
    buf[9] = (unsigned char)((world.h >> 8) & 0xFF);
    if (Bfile_WriteFile_OS(h, buf, 10) != 10) {
        Bfile_CloseFile_OS(h);
        return -1;
    }

    for (y = 0; y < world.h; y++) {
        memset(buf, 0, (size_t)rowbytes);
        for (x = 0; x < world.w; x++) {
            int v = ww_get(&world, x, y) & 3;
            buf[x >> 2] |= (unsigned char)(v << ((x & 3) * 2));
        }
        if (Bfile_WriteFile_OS(h, buf, rowbytes) != rowbytes) {
            Bfile_CloseFile_OS(h);
            return -1;
        }
    }
    Bfile_CloseFile_OS(h);
    return 0;
}

static int load_file(void)
{
    unsigned short path[64];
    static unsigned char buf[512];
    int h, x, y, w2, h2, rowbytes, i;
    int shrunk = 0, ox = 0, oy = 0, focus_x = -1, focus_y = -1;

    path_from_ascii(path, WW_FILE);
    h = Bfile_OpenFile_OS(path, READ, 0);
    if (h < 0) return -1;
    if (Bfile_ReadFile_OS(h, buf, 10, 0) != 10) { Bfile_CloseFile_OS(h); return -2; }
    if (buf[0] != 'W' || buf[1] != 'W' || buf[2] != 'L' || buf[3] != 'D' ||
        buf[4] != 1) { Bfile_CloseFile_OS(h); return -2; }

    w2 = buf[6] | (buf[7] << 8);
    h2 = buf[8] | (buf[9] << 8);
    if (w2 < 16 || h2 < 16 || w2 > 1024 || h2 > 1024) {
        Bfile_CloseFile_OS(h);
        return -2;
    }
    rowbytes = (w2 + 3) / 4;
    if (rowbytes > (int)sizeof(buf)) { Bfile_CloseFile_OS(h); return -2; }

    if (w2 != world.w || h2 != world.h) {
        int w0 = world.w, h0 = world.h;
        /* Close Bfile first so its heap descriptor does not split the ~156 KB
           OS heap, and free the old world before allocating w2 x h2 so large
           saved worlds (320x256..512x256) load cleanly after a cold boot. */
        Bfile_CloseFile_OS(h);
        ww_free(&world);
        if (ww_init(&world, w2, h2) == 0) {
            for (i = 0; i < FIELD_N; i++) {
                if (field_list[i][0] == w2 && field_list[i][1] == h2) {
                    field_index = i;
                    field_failed[i] = 0;
                }
            }
        } else {
            /* If the OS heap after reboot is slightly smaller than when the
               file was saved (e.g. saved 512x256 = 144 KB, now ~128-140 KB
               free), fall back to the largest field size that fits so the
               saved circuit still loads instead of failing with -3. */
            int ok = 0;
            for (i = FIELD_N - 1; i >= 0; i--) {
                if (ww_init(&world, field_list[i][0], field_list[i][1]) == 0) {
                    field_index = i;
                    field_failed[i] = 0;
                    shrunk = (world.w < w2 || world.h < h2);
                    ok = 1;
                    break;
                }
            }
            if (!ok) {
                if (w0 > 0 && h0 > 0) ww_init(&world, w0, h0);
                return -3;
            }
        }
        view_changed();

        if (shrunk) {
            int min_x = w2, max_x = -1, min_y = h2, max_y = -1;
            h = Bfile_OpenFile_OS(path, READ, 0);
            if (h < 0) return -1;
            if (Bfile_ReadFile_OS(h, buf, 10, 0) != 10) {
                Bfile_CloseFile_OS(h);
                return -2;
            }
            for (y = 0; y < h2; y++) {
                if (Bfile_ReadFile_OS(h, buf, rowbytes, -1) != rowbytes) {
                    Bfile_CloseFile_OS(h);
                    return -2;
                }
                for (x = 0; x < w2; x++) {
                    if ((buf[x >> 2] >> ((x & 3) * 2)) & 3) {
                        if (x < min_x) min_x = x;
                        if (x > max_x) max_x = x;
                        if (y < min_y) min_y = y;
                        if (y > max_y) max_y = y;
                    }
                }
            }
            Bfile_CloseFile_OS(h);
            if (max_x >= 0) {
                if (max_x >= world.w) {
                    ox = (min_x + max_x) / 2 - world.w / 2;
                    if (min_x - ox < 0) ox = min_x;
                }
                if (max_y >= world.h) {
                    oy = (min_y + max_y) / 2 - world.h / 2;
                    if (min_y - oy < 0) oy = min_y;
                }
                focus_x = wrap((min_x + max_x) / 2 - ox, world.w);
                focus_y = wrap((min_y + max_y) / 2 - oy, world.h);
            }
        }

        h = Bfile_OpenFile_OS(path, READ, 0);
        if (h < 0) return -1;
        if (Bfile_ReadFile_OS(h, buf, 10, 0) != 10) {
            Bfile_CloseFile_OS(h);
            return -2;
        }
    } else {
        ww_clear(&world);
    }

    for (y = 0; y < h2; y++) {
        if (Bfile_ReadFile_OS(h, buf, rowbytes, -1) != rowbytes) {
            Bfile_CloseFile_OS(h);
            ww_clear(&world);
            return -2;
        }
        for (x = 0; x < w2; x++) {
            int c = (buf[x >> 2] >> ((x & 3) * 2)) & 3;
            if (!c) continue;
            if (!shrunk) {
                ww_set(&world, x, y, c);
            } else {
                int dx = x - ox, dy = y - oy;
                if (dx >= 0 && dx < world.w && dy >= 0 && dy < world.h)
                    ww_set(&world, dx, dy, c);
            }
        }
    }
    Bfile_CloseFile_OS(h);

    world.gen = 0;
    world.changed = 0;
    running = 0;
    anchor_on = 0;
    cam_x = cam_y = 0;
    cur_x = (focus_x >= 0) ? focus_x : world.w / 2;
    cur_y = (focus_y >= 0) ? focus_y : world.h / 2;
    recentre();
    clamp_view();
    mark_all();
    return shrunk ? 1 : 0;
}

/* ------------------------------------------------------------------ */
/* text overlays: help and the option menu                             */
/* ------------------------------------------------------------------ */
#define HELP_LINES 10            /* lines of text on one help page */
#define HELP_PAGES (int)(sizeof(help_pages) / sizeof(help_pages[0]))

static const char *help_pages[][HELP_LINES] = {
    {
        "WIREWORLD FOR CG50  1/6  <->",
        "",
        "ARROWS       move (SHIFT: x10)",
        "EXE          draw / start-end",
        "0 ERASE  1 WIRE  2 HEAD  3 TAIL",
        "  hold 0..3 while moving = draw",
        "4 PEN   5 LINE   6 RECT (6=fill)",
        "* COPY  / CUT    + PASTE  - ROT",
        "  EXE corners; + again = box",
        "  . flip L/R   (-) flip U/D"
    },
    {
        "CLIPBOARD           2/6  <->",
        "",
        "* (or SHIFT+8)  COPY rectangle:",
        "   EXE corner 1, move, EXE",
        "/               CUT rectangle",
        "+ (or SHIFT+9)  PASTE at cursor",
        "   + again: transp / overwrite",
        "- or )          rotate 90 CW",
        "SHIFT+- or (    rotate 90 CCW",
        ". / (-)  flip   EXIT  back to PEN"
    },
    {
        "KEYS                3/6  <->",
        "",
        "F1   run / pause",
        "F2   one generation",
        "F3   clear the field",
        "F4   next demo circuit",
        "F5   speed",
        "F6   this help",
        "OPTN settings: file, size, speed,",
        "     grid, copy/paste, zoom, quit"
    },
    {
        "VIEW                4/6  <->",
        "",
        "7  zoom out   9  zoom in",
        "   1, 2, 3, 4, 6 or 8 pixels",
        "   per cell",
        "8  full screen: hides the status",
        "   line, the field then covers",
        "   all 384x216 pixels",
        "",
        "The view follows the cursor, so",
    },
    {
        "SETTINGS            5/6  <->",
        "",
        "OPTN opens the settings.  A line",
        "ending in > opens a list: press",
        "EXE, choose with the arrows and",
        "press EXE again.",
        "",
        "WORLD SIZE 128x128 ... 512x256",
        "cells, as much memory as there",
        "is.  SAVE / LOAD keeps the world",
    },
    {
        "RULES               6/6  <->",
        "",
        "empty      -> empty",
        "head       -> tail",
        "tail       -> conductor",
        "conductor  -> head when 1 or 2",
        "             of the 8 neighbour",
        "             cells are heads",
        "yellow wire, blue head, red tail,",
        "white cursor, magenta seam"
    }
};


static void draw_box(int x0, int y0, int x1, int y1)
{
    px_fill(x0, y0, x1, y1, COL_BOXBG);
    px_fill(x0, y0, x1, y0, COL_BOXEDGE);
    px_fill(x0, y1, x1, y1, COL_BOXEDGE);
    px_fill(x0, y0, x0, y1, COL_BOXEDGE);
    px_fill(x1, y0, x1, y1, COL_BOXEDGE);
    px_fill(x0 + 1, y0, x0 + 42, y0, COL_ACCENT);
}

static void draw_choice(int x0, int x1, int y, const char *text, int selected)
{
    if (selected) {
        px_fill(x0 + 4, y - 1, x1 - 4, y + font_h, COL_BOXSEL);
        px_fill(x0 + 4, y - 1, x0 + 5, y + font_h, COL_ACCENT);
    }
    mini_text(x0 + 10, y, text, selected ? COL_BOXFG : COL_BOXDIM,
              selected ? COL_BOXSEL : COL_BOXBG);
}

static int box_bottom(int y0, int rows)
{
    return y0 + 4 + row_h * (rows - 1) + font_h + 2;
}

static void show_help(void)
{
    int page = 0, top = 0;

    for (;;) {
        int i, k, rows, maxrows = rows_that_fit();
        int x0 = 8, y0 = 2, x1 = VIS_W - 9, y1;

        if (maxrows > HELP_LINES) maxrows = HELP_LINES;
        if (top > HELP_LINES - maxrows) top = HELP_LINES - maxrows;
        if (top < 0) top = 0;
        rows = HELP_LINES - top;
        if (rows > maxrows) rows = maxrows;
        y1 = box_bottom(y0, rows);

        draw_box(x0, y0, x1, y1);
        /* the title of the page stays put, the rest of it scrolls */
        mini_text(x0 + 6, row_y(y0, 0), help_pages[page][0],
                  COL_BOXFG, COL_BOXBG);
        for (i = 1; i < rows; i++)
            mini_text(x0 + 6, row_y(y0, i), help_pages[page][top + i],
                      COL_BOXDIM, COL_BOXBG);
        Bdisp_PutDisp_DD();

        wait_release();
        k = wait_any_key();
        if (k == KEY_PRGM_LEFT) {
            if (page > 0) page--;
            top = 0;
        } else if (k == KEY_PRGM_RIGHT) {
            if (page < HELP_PAGES - 1) page++;
            top = 0;
        } else if (k == KEY_PRGM_UP) {
            if (top > 0) top--;
        } else if (k == KEY_PRGM_DOWN) {
            if (top < HELP_LINES - rows) top++;
        } else break;
    }
    wipe_screen();
    mark_all();
}

enum {
    M_BACK, M_HELP, M_SAVE, M_LOAD, M_FIELD, M_SPEED, M_ZOOM,
    M_GRID, M_FULL, M_CLEAR, M_DEMO,
    M_COPY, M_CUT, M_PASTE, M_QUIT, M_COUNT
};

static const char *grid_name(void)
{
    if (grid_mode == GRID_OFF)  return "OFF";
    if (grid_mode == GRID_DOTS) return "DOTS";
    return "LINES";
}

static void menu_line(int i, char *out)
{
    char num[16];
    switch (i) {
    case M_BACK:  strcpy(out, "BACK TO THE FIELD"); break;
    case M_HELP:  strcpy(out, "HELP"); break;
    case M_SAVE:  strcpy(out, "SAVE TO FILE"); break;
    case M_LOAD:  strcpy(out, "LOAD FROM FILE"); break;
    case M_FIELD: sprintf(out, "WORLD SIZE: %dx%d >",
                          field_list[field_index][0],
                          field_list[field_index][1]); break;
    case M_SPEED:
        if (speed_ips[speed] == 0) strcpy(num, "MAX");
        else sprintf(num, "%d/S", speed_ips[speed]);
        sprintf(out, "SPEED: %s >", num);
        break;
    case M_ZOOM:  sprintf(out, "ZOOM: %d PX PER CELL >", cell_px); break;
    case M_GRID:  sprintf(out, "GRID: %s >", grid_name()); break;
    case M_FULL:  sprintf(out, "FULL SCREEN: %s", hdr_on ? "OFF" : "ON"); break;
    case M_CLEAR: strcpy(out, "CLEAR THE FIELD"); break;
    case M_DEMO:  sprintf(out, "DEMO: %s >", patterns[pat_index].name); break;
    case M_COPY:  strcpy(out, "COPY REGION (*)"); break;
    case M_CUT:   strcpy(out, "CUT REGION (/)"); break;
    case M_PASTE:
        if (clip_w > 0) sprintf(out, "PASTE %dx%d (+)", clip_w, clip_h);
        else            strcpy(out, "PASTE (+, EMPTY)");
        break;
    case M_QUIT:  strcpy(out, "QUIT"); break;
    default:      out[0] = 0; break;
    }
}

/* A full-width highlighted row with a small accent at the left. */
static void draw_menu(int sel, int top)
{
    int i, rows;
    char buf[40];
    int x0 = 30, y0 = 2, x1 = VIS_W - 31, y1;

    rows = M_COUNT - top;
    if (rows > rows_that_fit()) rows = rows_that_fit();
    y1 = box_bottom(y0, rows);

    draw_box(x0, y0, x1, y1);
    for (i = 0; i < rows; i++) {
        int item = top + i;
        menu_line(item, buf);
        draw_choice(x0, x1, row_y(y0, i), buf, item == sel);
    }
    Bdisp_PutDisp_DD();
}

typedef void (*Label)(int index, char *out);

static void field_label(int i, char *out)
{
    sprintf(out, "%d x %d   %uK CELLS", field_list[i][0], field_list[i][1],
            (unsigned)((field_list[i][0] * field_list[i][1] + 1023) / 1024));
}

static void speed_label(int i, char *out)
{
    if (speed_ips[i] == 0)       sprintf(out, "AS FAST AS POSSIBLE");
    else if (speed_ips[i] == 1)  sprintf(out, "1 PER SECOND");
    else                         sprintf(out, "%d PER SECOND", speed_ips[i]);
}

static void zoom_label(int i, char *out)
{
    if (zooms[i] == 1) sprintf(out, "1 PIXEL PER CELL");
    else               sprintf(out, "%d PIXELS PER CELL", zooms[i]);
}

static void grid_label(int i, char *out)
{
    static const char *n[3] = { "OFF", "DOTS EVERY 10 CELLS", "LINES" };
    sprintf(out, "%s", n[i]);
}

static void demo_label(int i, char *out)
{
    sprintf(out, "%s", patterns[i].name);
}

/* lets the user choose one value out of a list; returns the index or -1 */
static int pick_list(const char *title, int count, int current, Label label)
{
    int sel = (current >= 0 && current < count) ? current : 0;
    int top = 0;

    wait_release();
    for (;;) {
        char buf[40];
        int rows, i, k, maxrows = rows_that_fit();
        int x0 = 18, y0 = 2, x1 = VIS_W - 19, y1;

        rows = count + 1;
        if (rows > maxrows) rows = maxrows;
        if (sel < top) top = sel;
        if (sel >= top + rows - 1) top = sel - rows + 2;
        if (top > count - rows + 1) top = count - rows + 1;
        if (top < 0) top = 0;
        y1 = box_bottom(y0, rows);

        draw_box(x0, y0, x1, y1);
        mini_text(x0 + 10, row_y(y0, 0), title, COL_ACCENT, COL_BOXBG);
        for (i = 1; i < rows; i++) {
            int item = top + i - 1;
            if (item >= count) break;
            label(item, buf);
            draw_choice(x0, x1, row_y(y0, i), buf, item == sel);
        }
        Bdisp_PutDisp_DD();

        k = poll_key();
        if (k == KEY_PRGM_UP)         { if (sel > 0) sel--; }
        else if (k == KEY_PRGM_DOWN)  { if (sel < count - 1) sel++; }
        else if (k == KEY_PRGM_RETURN) return sel;
        else if (k == KEY_PRGM_EXIT)   return -1;
        OS_InnerWait_ms(10);
    }
}

static int confirm_quit(void)
{
    const char *lines[3] = { "QUIT WIREWORLD?", "",
                             "EXE = quit      other key = stay" };
    int i;
    int x0 = 40, y0 = 60, x1 = VIS_W - 41, y1 = box_bottom(y0, 3);

    draw_box(x0, y0, x1, y1);
    for (i = 0; i < 3; i++)
        mini_text(x0 + 6, row_y(y0, i), lines[i],
                  i == 0 ? COL_BOXFG : COL_BOXDIM, COL_BOXBG);
    Bdisp_PutDisp_DD();
    return wait_any_key() == KEY_PRGM_RETURN;
}

static void repaint_backdrop_for_menu(void)
{
    wipe_screen();
    mark_all();
    flush_cells();
    if (hdr_on) draw_header();
}

static void open_menu(void)
{
    int sel = 0, top = 0;

    draw_menu(sel, top);
    wait_release();

    for (;;) {
        int k = poll_key();
        int redraw = 0;
        int sub_closed = 0;

        if (k == KEY_PRGM_UP) {
            if (sel > 0) { sel--; if (sel < top) top = sel; redraw = 1; }
        } else if (k == KEY_PRGM_DOWN) {
            if (sel < M_COUNT - 1) {
                sel++;
                if (sel >= top + rows_that_fit()) top = sel - rows_that_fit() + 1;
                redraw = 1;
            }
        } else if (k == KEY_PRGM_EXIT) {
            break;
        } else if (k == KEY_PRGM_RETURN) {
            int done = 1;
            switch (sel) {
            case M_BACK:
                break;
            case M_HELP:
                repaint_backdrop_for_menu();
                show_help();
                break;
            case M_SAVE:
                toast(save_file() == 0 ? "SAVED WIREWORLD.WW"
                                       : "SAVE FAILED");
                break;
            case M_LOAD: {
                int r = load_file();
                if (r == 0) toast("LOADED WIREWORLD.WW");
                else if (r == 1) {
                    char mbuf[40];
                    sprintf(mbuf, "LOADED (FIT %dx%d)", world.w, world.h);
                    toast(mbuf);
                }
                else if (r == -1) toast("NO SAVED FILE");
                else if (r == -3) toast("NOT ENOUGH MEMORY");
                else toast("FILE IS DAMAGED");
                break;
            }
            case M_FIELD: {
                int i = pick_list("WORLD SIZE, CELLS", FIELD_N, field_index,
                                  field_label);
                if (i >= 0) resize_field(i);
                done = 0; sub_closed = 1; redraw = 1;
                break;
            }
            case M_SPEED: {
                int i = pick_list("SPEED", SPEED_COUNT, speed, speed_label);
                if (i >= 0) {
                    speed = i;
                    ips_count = 0;
                    ips_measured = 0;
                    ips_start = RTC_GetTicks();
                }
                done = 0; sub_closed = 1; redraw = 1;
                break;
            }
            case M_ZOOM: {
                int i = pick_list("ZOOM", ZOOM_COUNT, zoom, zoom_label);
                if (i >= 0 && i != zoom) { zoom = i; view_changed(); }
                done = 0; sub_closed = 1; redraw = 1;
                break;
            }
            case M_GRID: {
                int i = pick_list("GRID", 3, grid_mode, grid_label);
                if (i >= 0) { grid_mode = i; mark_all(); }
                done = 0; sub_closed = 1; redraw = 1;
                break;
            }
            case M_FULL:
                hdr_on = !hdr_on;
                view_changed();
                done = 0; sub_closed = 1; redraw = 1;
                break;
            case M_CLEAR:
                clear_field();
                break;
            case M_DEMO: {
                int i = pick_list("DEMO CIRCUIT", patternCount, pat_index,
                                  demo_label);
                if (i >= 0) {
                    load_pattern(i);
                } else {
                    done = 0; sub_closed = 1; redraw = 1;
                }
                break;
            }
            case M_COPY:
                start_copy_tool(0);
                break;
            case M_CUT:
                start_copy_tool(1);
                break;
            case M_PASTE:
                start_paste_tool(0);
                break;
            case M_QUIT:
                if (confirm_quit()) { quit_now = 1; break; }
                done = 0; sub_closed = 1; redraw = 1;
                break;
            default:
                done = 0;
                break;
            }
            if (done) break;
        }
        if (redraw) {
            if (sub_closed) repaint_backdrop_for_menu();
            draw_menu(sel, top);
        }
        OS_InnerWait_ms(10);
    }
    wait_release();
    wipe_screen();
    mark_all();
}

/* ------------------------------------------------------------------ */
/* diagnostics: hold F5 right after start.  If the VRAM row stride is  */
/* wrong, the diagonal and the horizontal rules break into steps.      */
/* ------------------------------------------------------------------ */
/* how much memory the OS would give us right now, in KB.  Everything is
   handed back at once, so this only looks, it does not take. */
static int free_heap_kb(void)
{
    void *blocks[192];
    int n = 0;

    while (n < 192) {
        void *p = sys_malloc(4096);
        if (!p) break;
        blocks[n++] = p;
    }
    {
        int free_kb = n * 4;
        while (n > 0) sys_free(blocks[--n]);
        return free_kb;
    }
}

static void diagnostics(void)
{
    int x, y;
    char buf[48];

    px_fill(0, 0, VIS_W - 1, VIS_H - 1, COL_BG);

    /* 1 px frame: proves the visible area is 384x216 */
    px_fill(0, 0, VIS_W - 1, 0, COL_CURSOR);
    px_fill(0, VIS_H - 1, VIS_W - 1, VIS_H - 1, COL_CURSOR);
    px_fill(0, 0, 0, VIS_H - 1, COL_CURSOR);
    px_fill(VIS_W - 1, 0, VIS_W - 1, VIS_H - 1, COL_CURSOR);

    /* rules every 16 pixels */
    for (x = 16; x < VIS_W; x += 16) px_fill(x, 1, x, VIS_H - 2, COL_WIRE);
    for (y = 16; y < VIS_H; y += 16) px_fill(1, y, VIS_W - 2, y, COL_WIRE);

    /* a corner to corner diagonal: with a bad stride it turns into steps */
    for (x = 0; x < VIS_W; x++) {
        y = x * (VIS_H - 1) / (VIS_W - 1);
        px_fill(x, y, x, y + 1, COL_HEAD);
    }

    sprintf(buf, "DIAG V1.64  STRIDE %d  %dx%d", VRAM_W, VIS_W, VIS_H);
    mini_text(6, row_y(2, 0), buf, COL_CURSOR, COL_BG);
    mini_text(6, row_y(2, 1), "FRAME, RULES AND DIAGONAL MUST BE STRAIGHT",
              COL_TAIL, COL_BG);
    mini_text(6, row_y(2, 2), "BANDS OR STEPS => WRONG ROW STRIDE",
              COL_TAIL, COL_BG);
    sprintf(buf, "CELL %d px   VIEW %dx%d cells", cell_px, view_w, view_h);
    mini_text(6, row_y(2, 3), buf, COL_WIRE, COL_BG);
    sprintf(buf, "FONT %d px   ROW %d px   MENU %d", font_h, row_h,
            GetGetkeyToMainFunctionReturnFlag());
    mini_text(6, row_y(2, 4), buf, COL_WIRE, COL_BG);
    sprintf(buf, "HEAP %d KB FREE   FIELD %dx%d", free_heap_kb(),
            world.w, world.h);
    mini_text(6, row_y(2, 5), buf, COL_WIRE, COL_BG);
    mini_text(6, row_y(2, 6), "ANY KEY = CONTINUE", COL_BOXDIM, COL_BG);
    Bdisp_PutDisp_DD();

    wait_any_key();
    mark_all();
}

/* ------------------------------------------------------------------ */
/* quitting                                                            */
/* ------------------------------------------------------------------ */
/* Hand the calculator back to the OS the way it expects: inject a MENU
   key press and let GetKey deal with it.  GetKey then performs the whole
   "leave the add-in" sequence, so the main menu is not left thinking the
   add-in is still running (which made it swallow the first tap on the
   icon: it only started again after some other add-in had been run). */
/* Legacy direct flag routine, adapted from Simon Lothar's published code and
   the fx-CG50 fork of Utilities. The documented OS enable syscall is also
   called when leaving; this legacy routine is retained for compatibility.
   Its licensing requires review before publication: THIRD_PARTY_NOTICES.md. */
static unsigned int set_menu_return_flag(unsigned int enabled)
{
#ifndef HOST_TEST
    int addr, addr2;

    addr = *(unsigned char *)0x80020071;      /* displacement in the ROM */
    addr++;
    addr *= 4;
    addr += 0x80020070;                       /* the syscall table */
    addr = *(int *)addr;
    if (addr < (int)0x80020070) return 0x101;
    if (addr >= (int)0x81000000) return 0x102;

    addr += 0x1E99 * 4;                       /* GetGetkeyToMainFunctionReturnFlag */
    if (addr < (int)0x80020070) return 0x103;
    if (addr >= (int)0x81000000) return 0x104;

    addr = *(int *)addr;
    if (addr < (int)0x80020070) return 0x105;
    if (addr >= (int)0x81000000) return 0x106;

    switch (*(unsigned char *)addr) {
    case 0xD0: case 0xD1: case 0xD2: case 0xD3: case 0xD4:
    case 0xD5: case 0xD6: case 0xD7: case 0xD8:   /* MOV.L @(disp,PC),Rn */
        addr2 = *(unsigned char *)(addr + 1);
        addr2++;
        addr2 *= 4;
        addr2 += addr;
        addr2 &= ~3;
        if (addr2 < (int)0x80020070) return 0x107;
        if (addr2 >= (int)0x81000000) return 0x108;
        addr = *(int *)addr2;
        if ((addr & 0xFF000000) != 0x88000000 &&
            (addr & 0xFF000000) != 0x8C000000) return 0x109;
        if (enabled) *(unsigned char *)addr = 0;
        else *(unsigned char *)addr = 3;
        break;
    default:
        addr = 0x100;
    }
    return (unsigned int)addr;
#else
    (void)enabled;                 /* there is no ROM to look at on a PC */
    return 0;
#endif
}

/* Drains any matrix codes the OS keyboard interrupt queued while we were
   polling KEY_PORT directly (otherwise the buffer fills up and blocks
   Keyboard_PutKeycode or wakes GetKeyWait_OS early). */
static void drain_os_keys(void)
{
    int col = 0, row = 0;
    unsigned short keycode = 0;

    while (GetKeyWait_OS(&col, &row, KEYWAIT_HALTOFF_TIMEROFF, 0, 1, &keycode)
           == KEYREP_KEYEVENT)
        ;
}

static volatile int menu_timer_id = -1;
static volatile int menu_timer_fired;

/* Matrix-code injection (0x04, 0x09 = MENU) only triggers the OS Main Menu
   when GetKey / GetKeyWait_OS is already waiting, so it has to happen from a
   timer callback that stops and uninstalls itself on the first tick. */
static void menu_timer_cb(void)
{
    Keyboard_PutKeycode(0x04, 0x09, 0);
    if (menu_timer_id > 0) {
        Timer_Stop(menu_timer_id);
        Timer_Deinstall(menu_timer_id);
        menu_timer_id = -1;
    }
    menu_timer_fired = 1;
}

typedef void (*os_menu_fn_t)(void);

/* Locate Casio OS's internal SaveAndOpenMainMenu() inside syscall 0x1E58
   (SwitchToMainMenu), same method used by gint's gint_osmenu_native().
   Calling it directly opens the Main Menu in 0 ms with zero black flash. */
static os_menu_fn_t find_os_menu_function(void)
{
#ifndef HOST_TEST
    unsigned int tbl = *(unsigned int *)0x8002007c;
    const unsigned short *insns;
    int i;

    if (tbl < 0x80020070u || tbl >= 0x81000000u - 0x1e58u * 4u)
        return 0;
    insns = *(const unsigned short **)(tbl + 0x1e58u * 4u);
    if ((unsigned int)insns < 0x80020070u || (unsigned int)insns >= 0x81000000u)
        return 0;

    for (i = 0; i < 150; i++) {
        int rX, rY, disp;
        if ((insns[i] & 0xf000) != 0xd000) continue;
        rX = (insns[i] >> 8) & 0x0f;
        if ((insns[i + 1] & 0xf0ff) != 0xe003) continue;
        rY = (insns[i + 1] >> 8) & 0x0f;
        if ((insns[i + 2] & 0xf000) != 0xb000) continue;
        disp = (int)(insns[i + 2] & 0x0fff);
        if (disp & 0x0800) disp -= 0x1000;
        if (insns[i + 3] != (unsigned short)(0x2000 + (rX << 8) + (rY << 4)))
            continue;
        if ((insns[i + 4] & 0xf000) != 0xa000) continue;
        if (insns[i + 5] != 0x0009) continue;

        return (os_menu_fn_t)((unsigned int)&insns[i + 2] + 4u +
                              (unsigned int)(disp * 2));
    }
#endif
    return 0;
}

/* Hands the screen back to the OS, opens its Main Menu instantly via
   SaveAndOpenMainMenu() (without painting an intermediate black screen or
   waiting on a timer), and returns immediately if the user re-enters. */
static void leave_to_menu(void)
{
    os_menu_fn_t os_menu = find_os_menu_function();

    ww_free(&world);
    wait_release();

    EnableStatusArea(0);
    DefineStatusAreaFlags(3, SAF_BATTERY | SAF_TEXT | SAF_GLYPH | SAF_ALPHA_SHIFT,
                          0, 0);
    Cursor_SetFlashOff();
    Bkey_ClrAllFlags();
    EnableGetkeyToMainFunctionReturn();
    set_menu_return_flag(1);
    DrawFrame(0xFFFF);
    drain_os_keys();

    if (os_menu) {
        os_menu();
    } else {
        int col = 0, row = 0, tries;
        unsigned short keycode = 0;

        for (tries = 0; tries < 3; tries++) {
            menu_timer_fired = 0;
            menu_timer_id = Timer_Install(0, menu_timer_cb, 25);
            if (menu_timer_id > 0)
                Timer_Start(menu_timer_id);

            GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMERON, 1, 0, &keycode);

            if (menu_timer_id > 0) {
                Timer_Stop(menu_timer_id);
                Timer_Deinstall(menu_timer_id);
                menu_timer_id = -1;
            }
            if (menu_timer_fired)
                break;
        }

        if (!menu_timer_fired) {
            px_fill(0, 0, VIS_W - 1, VIS_H - 1, COL_BG);
            mini_text(24, 80 - font_off, "PRESS THE MENU KEY TO LEAVE",
                      COL_BOXFG, COL_BG);
            Bdisp_PutDisp_DD();
            GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMERON, 3600, 0, &keycode);
        }
    }

    /* We only reach here if the user picked Wireworld again in the Main Menu:
       reclaim the full-screen canvas without pushing a black frame to DD. */
    Bdisp_EnableColor(1);
    EnableStatusArea(3);
    DrawFrame(COL_BG);
    wipe_screen();
    wait_release();
    drain_os_keys();
}

/* ------------------------------------------------------------------ */
/* keys                                                                */
/* ------------------------------------------------------------------ */
static void handle_key(int k)
{
    int fast = key_down(KEY_PRGM_SHIFT) ? 10 : 1;

    switch (k) {
    case KEY_PRGM_UP:    move_cursor(0, -fast);   break;
    case KEY_PRGM_DOWN:  move_cursor(0, fast);    break;
    case KEY_PRGM_LEFT:  move_cursor(-fast, 0);   break;
    case KEY_PRGM_RIGHT: move_cursor(fast, 0);    break;

    case KEY_PRGM_RETURN: act();                  break;

    /* choosing a material must not put anything on the field: EXE does that.
       If we were in Copy/Cut/Paste, picking a material returns to PEN. */
    case KEY_PRGM_1:
        mat = MAT_WIRE;
        if (tool >= TOOL_COPY) { tool = TOOL_PEN; anchor_on = 0; mark_all(); }
        toast("WIRE");
        break;
    case KEY_PRGM_2:
        mat = MAT_HEAD;
        if (tool >= TOOL_COPY) { tool = TOOL_PEN; anchor_on = 0; mark_all(); }
        toast("HEAD");
        break;
    case KEY_PRGM_3:
        mat = MAT_TAIL;
        if (tool >= TOOL_COPY) { tool = TOOL_PEN; anchor_on = 0; mark_all(); }
        toast("TAIL");
        break;
    case KEY_PRGM_0:
        mat = MAT_ERASE;
        if (tool >= TOOL_COPY) { tool = TOOL_PEN; anchor_on = 0; mark_all(); }
        toast("ERASE");
        break;

    case KEY_PRGM_4:
        tool = TOOL_PEN; anchor_on = 0; mark_all(); toast("PEN"); break;
    case KEY_PRGM_5:
        tool = TOOL_LINE; anchor_on = 0; mark_all();
        toast("LINE: EXE, MOVE, EXE"); break;
    case KEY_PRGM_6:
        if (tool == TOOL_RECT) rect_fill = !rect_fill;
        tool = TOOL_RECT;
        anchor_on = 0;
        mark_all();
        toast(rect_fill ? "RECTANGLE, FILLED" : "RECTANGLE, HOLLOW");
        break;

    case KEY_PRGM_MUL:
        start_copy_tool(0);
        break;
    case KEY_PRGM_DIV:
        start_copy_tool(1);
        break;
    case KEY_PRGM_ADD:
        start_paste_tool(1);
        break;
    case KEY_PRGM_SUB:
        rotate_clipboard(key_down(KEY_PRGM_SHIFT) ? -1 : 1);
        break;
    case KEY_PRGM_RPAR:
        rotate_clipboard(1);
        break;
    case KEY_PRGM_LPAR:
        rotate_clipboard(-1);
        break;
    case KEY_PRGM_DOT:
        flip_clipboard(0);
        break;
    case KEY_PRGM_NEG:
        flip_clipboard(1);
        break;

    case KEY_PRGM_7:
        if (zoom > 0) { zoom--; view_changed(); }
        break;
    case KEY_PRGM_9:
        if (key_down(KEY_PRGM_SHIFT)) { start_paste_tool(1); break; }
        if (zoom < ZOOM_COUNT - 1) { zoom++; view_changed(); }
        break;
    case KEY_PRGM_8:
        if (key_down(KEY_PRGM_SHIFT)) { start_copy_tool(0); break; }
        hdr_on = !hdr_on;
        view_changed();
        toast(hdr_on ? "STATUS LINE ON" : "FULL SCREEN");
        break;

    case KEY_PRGM_F1: running = !running; break;
    case KEY_PRGM_F2: running = 0; step_frame(); break;
    case KEY_PRGM_F3: clear_field(); break;
    case KEY_PRGM_F4: load_pattern((pat_index + 1) % patternCount); break;
    case KEY_PRGM_F5: speed = (speed + 1) % SPEED_COUNT; break;
    case KEY_PRGM_F6: show_help(); break;
    case KEY_PRGM_OPTN: open_menu(); break;
    case KEY_PRGM_MENU: quit_now = 1; break;
    default: break;
    }
}

/* ------------------------------------------------------------------ */
/* main                                                                */
/* ------------------------------------------------------------------ */
/* one run of the program: comes back when the user asks to leave */
static void run_wireworld(void)
{
    int i, last_render = 0, render_pending = 0;
    SpeedClock clock = { 0, 0, 0, 1 };

    zoom         = 1;
    hdr_on       = 1;
    grid_mode    = GRID_DOTS;
    mat          = MAT_WIRE;
    tool         = TOOL_PEN;
    rect_fill    = 0;
    anchor_on    = 0;
    clip_w       = 0;
    clip_h       = 0;
    clip_rot     = 0;
    clip_flip    = 0;
    paste_opaque = 0;
    running      = 0;
    speed        = 4;
    ips_count    = 0;
    ips_start    = 0;
    ips_measured = 0;
    pat_index    = 0;
    quit_now     = 0;
    toast_on     = 0;
    d_full       = 0;
    d_any        = 0;
    ov_any       = 0;
    memset(field_failed, 0, sizeof(field_failed));
    recalc_view();

    /* the biggest world that fits (one byte + one bit per cell) */
    for (i = FIELD_N - 1; i >= 0; i--)
        if (ww_init(&world, field_list[i][0], field_list[i][1]) == 0) {
            field_index = i;
            break;
        }

    if (i < 0) {                       /* not even the smallest one fits */
        px_fill(0, 0, VIS_W - 1, VIS_H - 1, COL_BG);
        mini_text(4, row_y(2, 0), "NOT ENOUGH MEMORY FOR THE FIELD",
                  COL_HDRFG, COL_BG);
        mini_text(4, row_y(2, 1), "PRESS ANY KEY", COL_BOXDIM, COL_BG);
        Bdisp_PutDisp_DD();
        wait_release();
        wait_any_key();
        return;
    }

    load_pattern(0);
    render_frame();

    /* hold F5 right at start-up to open the diagnostics screen */
    for (i = 0; i < 40; i++) {
        if (key_down(KEY_PRGM_F5)) {
            wait_release();
            diagnostics();
            mark_all();
            render_frame();
            break;
        }
        if (scan_keys()) break;
        OS_InnerWait_ms(10);
    }

    for (;;) {
        int k = poll_key();
        int need = 0, stepped = 0;

        if (k == KEY_PRGM_EXIT) {
            if (anchor_on) {
                anchor_on = 0;
                mark_all();
            } else if (tool >= TOOL_COPY) {
                tool = TOOL_PEN;
                toast("PEN");
                mark_all();
            } else if (confirm_quit()) {
                break;
            } else {
                wipe_screen();
                mark_all();
            }
            need = 1;
        } else if (k) {
            handle_key(k);
            need = 1;
        }
        if (quit_now) break;

        if (speed_clock_due(&clock, running, speed_ips[speed], RTC_GetTicks())) {
            step_frame();
            stepped = 1;
            if (hdr_on || d_any) render_pending = 1;
            if (!running) need = 1; /* automatic stop must show its toast */
            ips_count++;
            if (rtc_elapsed_ms(ips_start, 1000)) {
                ips_measured = ips_count;
                ips_count = 0;
                ips_start = RTC_GetTicks();
            }
        }

        if (toast_on && rtc_elapsed_ms(toast_start, 1600)) {
            toast_on = 0;
            need = 1;
        }

        /* ov_any is the previous frame's overlay footprint, not a request
           for a new frame. It is restored only when something actually
           changes. While running, coalesce dirty cells for up to 33 ms. */
        if (need || (!running && d_any) ||
            (running && render_pending && rtc_elapsed_ms(last_render, 33))) {
            render_frame();
            render_pending = 0;
            last_render = RTC_GetTicks();
        }

        /* Keep the physical frame black even when the image is idle, without
           pushing an unchanged 384x216 screen to the display controller. */
        if (rtc_elapsed_ms(last_frame_draw, 250)) {
            last_frame_draw = RTC_GetTicks();
            DrawFrame(COL_BG);
        }
        if (!running) OS_InnerWait_ms(10);
        else if (speed_ips[speed] && !stepped) OS_InnerWait_ms(5);
    }
}

int main(void)
{
    Bdisp_EnableColor(1);        /* 16 bit colour VRAM */
    EnableStatusArea(3);         /* no OS status bar   */
    /* DrawFrame paints the border of the 396x224 panel around the 384x216
       area.  (FrameColor only writes a colour into the OS state and is
       better left alone.) */
    DrawFrame(COL_BG);

    vram = (unsigned short *)GetVRAMAddress();

    /* Bitmap font metrics are fixed, so no OS font probe is needed. */

    /* An add-in must never return from main: the OS then keeps thinking it
       is running and refuses to start it again until another add-in has
       been run.  Leaving works the other way round - the OS draws its main
       menu over us, and picking Wireworld there brings us back, so the
       program simply runs again. */
    for (;;) {
        run_wireworld();
        leave_to_menu();
    }
}
