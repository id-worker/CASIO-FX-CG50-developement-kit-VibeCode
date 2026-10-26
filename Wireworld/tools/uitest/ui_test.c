/*
 * ui_test.c -- drives src/main.c on a PC against the stubs in host_stub.c.
 *
 * It presses keys on a script and checks the picture after every step:
 *
 *   - the cursor leaves no trail behind (it is exactly one ring of pixels),
 *     and it stays visible while a key is held down;
 *   - choosing a material does not put anything on the field;
 *   - full screen really covers the whole screen;
 *   - every zoom level draws the cursor;
 *   - the seam of the world (where it wraps) is marked;
 *   - the line and rectangle tools draw exactly the cells they should and
 *     leave no preview pixels behind;
 *   - saving and loading restores the field byte for byte;
 *   - the field size, the speed and the grid can be changed from the menu;
 *   - the help box and the menu fit on the screen.
 *
 * Build:  see tools/uitest/run.sh
 */
#define main ww_main          /* the real entry point lives in this file */
#include "../../src/main.c"
#undef main

/* the metrics main.c measured from the stub font */
#define HDR_H hdr_h
#define ROW_H row_h

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ------------------------------------------------------------------ */
/* hooks used by the stubs                                             */
/* ------------------------------------------------------------------ */
typedef void (*Check)(const char *name);

typedef struct {
    int key;
    int hold;        /* ms the key stays down   */
    Check check;     /* called after release    */
    const char *name;
} Step;

static int script_key;
static const Step *script;
static int script_len, script_i, script_ms, script_phase;
static unsigned short snap[384 * 216];
static long frames;

/* every frame drawn while an arrow key is held down is watched: the cursor
   must be visible in each of them, and it must never be more than one cell */
static long hold_frames, hold_no_cursor, hold_max_cursor;

static int count_px(int y0, int y1, unsigned short col);

int host_key_down(int code) { return script_key == code; }

static int is_arrow_key(int k)
{
    return k == KEY_PRGM_UP || k == KEY_PRGM_DOWN ||
           k == KEY_PRGM_LEFT || k == KEY_PRGM_RIGHT;
}

void host_putdisp(void)
{
    frames++;
    memcpy(snap, vram, sizeof snap);
    /* only the plain "move around" steps: inside the menu the field is not
       being repainted at all, so of course there is no cursor there */
    if (is_arrow_key(script_key) && !toast_on && script_i < script_len &&
        strncmp(script[script_i].name, "hold", 4) == 0) {
        int n = count_px(grid_top, 215, COL_CURSOR);
        hold_frames++;
        if (n == 0) hold_no_cursor++;
        if (n > hold_max_cursor) hold_max_cursor = n;
    }
}

/* ------------------------------------------------------------------ */
/* script                                                              */
/* ------------------------------------------------------------------ */
extern int host_timers_installed(void);

/* after the restart the add-in has to be left again the way a user does
   it: EXIT, then EXE on the confirmation */
static int resumed;

static void chk_restarted(const char *name);
static void chk_stepped_after_restart(const char *name);

static const Step after_steps_1[] = {
    { 0,               500, chk_restarted,             "restarted #1" },
    { KEY_PRGM_F2,     120, chk_stepped_after_restart, "step after restart" },
    { KEY_PRGM_EXIT,   120, NULL,                      "quit #2" },
    { KEY_PRGM_RETURN, 120, NULL,                      "confirm #2" }
};

static const Step after_steps_2[] = {
    { 0,               500, chk_restarted, "restarted #2" },
    { KEY_PRGM_EXIT,   120, NULL,          "quit #3" },
    { KEY_PRGM_RETURN, 120, NULL,          "confirm #3" }
};

void host_tick(void)
{
    if (script_i >= script_len) {
        if (resumed == 0) {            /* nothing left: leave via MENU */
            script_key = KEY_PRGM_MENU;
            return;
        }
        if (resumed == 1 && script != after_steps_1) {
            script = after_steps_1;
            script_len = (int)(sizeof(after_steps_1) / sizeof(after_steps_1[0]));
            script_i = script_ms = script_phase = 0;
        } else if (resumed == 2 && script != after_steps_2) {
            script = after_steps_2;
            script_len = (int)(sizeof(after_steps_2) / sizeof(after_steps_2[0]));
            script_i = script_ms = script_phase = 0;
        } else {
            script_key = 0;            /* waiting for the OS to take us */
            return;
        }
    }
    if (script_phase == 0) {
        script_key = script[script_i].key;
        if (++script_ms >= script[script_i].hold) {
            script_key = 0;
            script_ms = 0;
            script_phase = 1;
        }
    } else {
        if (++script_ms >= 40) {       /* released long enough to re-arm */
            script_ms = 0;
            script_phase = 0;
            if (script[script_i].check) script[script_i].check(script[script_i].name);
            script_i++;
        }
    }
}

/* ------------------------------------------------------------------ */
/* assertions                                                          */
/* ------------------------------------------------------------------ */
static int failures, checks;
static const char *cur_check = "";

static void ok(int cond, const char *what, const char *fmt, ...)
{
    va_list ap;
    checks++;
    if (cond) return;
    failures++;
    printf("FAIL [%s]: ", what);
    va_start(ap, fmt);
    vprintf(fmt, ap);
    va_end(ap);
    printf("\n");
}

static int count_px(int y0, int y1, unsigned short col)
{
    int n = 0, x, y;
    for (y = y0; y <= y1; y++)
        for (x = 0; x < 384; x++)
            if (snap[y * 384 + x] == col) n++;
    return n;
}

static int count_px_col(int x0, int y0, int y1, unsigned short col)
{
    int n = 0, y;
    for (y = y0; y <= y1; y++)
        if (snap[y * 384 + x0] == col) n++;
    return n;
}

static int count_px_row(int y0, int x0, int x1, unsigned short col)
{
    int n = 0, x;
    for (x = x0; x <= x1; x++)
        if (snap[y0 * 384 + x] == col) n++;
    return n;
}

static int count_cells(int state)
{
    int n = 0, i, total = world.w * world.h;
    for (i = 0; i < total; i++)
        if (world.cells[i] == state) n++;
    return n;
}

/* the cursor is a ring around the cell */
static int expected_cursor_px(void)
{
    if (cell_px >= 6) return 8 * cell_px;      /* 2 px thick   */
    return 4 * cell_px + 4;                    /* 1 px thick   */
}

/* ------------------------------------------------------------------ */
/* the checks                                                          */
/* ------------------------------------------------------------------ */
static int field_index_at_start = -1;
static int speed_at_start = -1;

static void chk_start(const char *name)
{
    cur_check = name; (void)cur_check;
    field_index_at_start = field_index;
    speed_at_start = speed;
    ok(cell_px == 2, name, "cell_px %d", cell_px);
    ok(hdr_on == 1, name, "header off at start");
    ok(grid_top == HDR_H, name, "grid_top %d", grid_top);
    ok(view_w == 192, name, "view_w %d", view_w);
    ok(view_h == (VIS_H - HDR_H) / 2, name, "view_h %d", view_h);
    ok(count_px(0, HDR_H - 1, COL_HDRBG) > 1000, name,
       "no header background");
    ok(count_px(HDR_H, 215, COL_HDRBG) == 0, name, "header spills into the grid");
    printf("  start: field %dx%d, view %dx%d, cell %d px, HDR %d px\n",
           world.w, world.h, view_w, view_h, cell_px, HDR_H);
}

static void chk_trail(const char *name)
{
    int white = count_px(grid_top, 215, COL_CURSOR);
    cur_check = name; (void)cur_check;
    ok(white == expected_cursor_px(), name,
       "cursor covers %d px, expected %d (trail or invisible cursor)",
       white, expected_cursor_px());
}

/* --- running the simulation --- */
static unsigned long gen_before;

static void chk_running(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(running == 1, name, "the simulation is not running");
    ok(world.gen > 5, name, "only %lu generations in 2.5 s", world.gen);
    ok(ips_measured > 0, name, "the speed was not measured");
    printf("  running: %lu generations, %d/s measured\n", world.gen,
           ips_measured);
}

static void chk_paused(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(running == 0, name, "did not pause");
}

static void chk_snap_gen(const char *name)
{
    cur_check = name; (void)cur_check;
    gen_before = world.gen;
}

static void chk_step(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(world.gen == gen_before + 1, name, "one step gave %lu generations",
       world.gen - gen_before);
    ok(running == 0, name, "F2 started the simulation");
}

/* --- shapes and painting --- */
static unsigned char *snap_cells;
static int snap_len;
static unsigned char *expect;
static int cur_x_snap, cur_y_snap, ax_snap, ay_snap;

static void take_snapshot(void)
{
    snap_len = world.w * world.h;
    free(snap_cells);
    free(expect);
    snap_cells = (unsigned char *)malloc((size_t)snap_len);
    expect = (unsigned char *)calloc((size_t)snap_len, 1);
    memcpy(snap_cells, world.cells, (size_t)snap_len);
    cur_x_snap = cur_x; cur_y_snap = cur_y;
    ax_snap = ax; ay_snap = ay;
}

static void chk_snap_field(const char *name)
{
    cur_check = name; (void)cur_check;
    take_snapshot();
}

/* pressing a material key must not change the field */
static void chk_no_paint(const char *name)
{
    int i, bad = 0;
    cur_check = name; (void)cur_check;
    for (i = 0; i < snap_len; i++)
        if (world.cells[i] != snap_cells[i]) bad++;
    ok(bad == 0, name, "choosing a material changed %d cells", bad);
}

static void chk_fullscreen(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(hdr_on == 0, name, "header still on");
    ok(grid_top == 0, name, "grid_top %d", grid_top);
    ok(view_h == 216 / cell_px, name, "view_h %d", view_h);
    ok(count_px(0, 215, COL_HDRBG) == 0, name, "header colour still on screen");
    ok(count_px(0, 215, COL_CURSOR) == expected_cursor_px(), name,
       "cursor pixels %d", count_px(0, 215, COL_CURSOR));
    printf("  full screen: view %dx%d cells\n", view_w, view_h);
}

static void chk_zoom(const char *name, int want)
{
    cur_check = name; (void)cur_check;
    ok(cell_px == want, name, "cell_px %d, expected %d", cell_px, want);
    ok(view_w == VIS_W / want, name, "view_w %d", view_w);
    ok(count_px(grid_top, 215, COL_CURSOR) == expected_cursor_px(), name,
       "cursor pixels %d, expected %d", count_px(grid_top, 215, COL_CURSOR),
       expected_cursor_px());
}

static void chk_zoom3(const char *name) { chk_zoom(name, 3); }
static void chk_zoom4(const char *name) { chk_zoom(name, 4); }
static void chk_zoom2(const char *name) { chk_zoom(name, 2); }
static void chk_zoom6(const char *name) { chk_zoom(name, 6); }
static void chk_zoom8(const char *name) { chk_zoom(name, 8); }
static void chk_zoom1(const char *name) { chk_zoom(name, 1); }

static void chk_header_on(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(hdr_on == 1, name, "header did not come back");
    ok(grid_top == HDR_H, name, "grid_top %d", grid_top);
    ok(count_px(HDR_H, 215, COL_HDRBG) == 0, name, "header colour in the grid");
}

/* the seam of the world: camera to (0,0) and look for the thin line */
static void chk_seam(const char *name)
{
    int vert, horiz;
    cur_check = name; (void)cur_check;
    cam_x = cam_y = 0;
    mark_all();
    render_frame();
    vert = count_px_col(0, grid_top, 215, COL_EDGE);
    horiz = count_px_row(grid_top, 0, 383, COL_EDGE);
    ok(vert == view_h * cell_px, name,
       "seam column has %d px, expected %d", vert, view_h * cell_px);
    ok(horiz == view_w * cell_px, name,
       "seam row has %d px, expected %d", horiz, view_w * cell_px);
    printf("  seam: %d vertical + %d horizontal pixels\n", vert, horiz);
}

static void expect_cell(int x, int y) { expect[y * world.w + x] = 1; }

static void expect_line(int x0, int y0, int x1, int y1)
{
    int dx = abs(x1 - x0), dy = abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1, sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy, e2;
    for (;;) {
        expect_cell(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        e2 = 2 * err;
        if (e2 > -dy) { err -= dy; x0 += sx; }
        if (e2 < dx)  { err += dx; y0 += sy; }
    }
}

static void expect_rect(int x0, int y0, int x1, int y1)
{
    int x, y;
    int xa = (x0 < x1) ? x0 : x1, xb = (x0 < x1) ? x1 : x0;
    int ya = (y0 < y1) ? y0 : y1, yb = (y0 < y1) ? y1 : y0;
    for (y = ya; y <= yb; y++)
        for (x = xa; x <= xb; x++)
            if (rect_fill || y == ya || y == yb || x == xa || x == xb)
                expect_cell(x, y);
}

static void chk_shape(const char *name)
{
    int i, diff = 0, missing = 0, extra = 0;
    cur_check = name; (void)cur_check;
    for (i = 0; i < snap_len; i++) {
        if (world.cells[i] != snap_cells[i]) {
            diff++;
            if (!expect[i]) extra++;
            else if (world.cells[i] != mat_state[mat]) extra++;
        } else if (expect[i] && snap_cells[i] != mat_state[mat]) {
            missing++;
        }
    }
    ok(diff > 0, name, "nothing was drawn");
    ok(extra == 0, name, "%d cells changed that should not have", extra);
    ok(missing == 0, name, "%d cells of the shape are missing", missing);
    printf("  %s: %d cells drawn from (%d,%d) to (%d,%d)\n", name, diff,
           ax_snap, ay_snap, cur_x_snap, cur_y_snap);
}

static void chk_no_preview(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(count_px(0, 215, COL_PREVIEW) == 0, name, "%d preview pixels left",
       count_px(0, 215, COL_PREVIEW));
}

static void chk_preview(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(anchor_on == 1, name, "no anchor");
    ok(count_px(0, 215, COL_PREVIEW) > 0, name, "the shape is not shown");
}

static void chk_anchor(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(anchor_on == 1, name, "EXE did not set the start point");
    ok(tool == TOOL_LINE, name, "tool %d", tool);
    take_snapshot();
}

static void chk_rect_anchor(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(anchor_on == 1, name, "EXE did not set the start point");
    ok(tool == TOOL_RECT, name, "tool %d", tool);
    take_snapshot();
}

static void chk_line_done(const char *name)
{
    cur_x_snap = cur_x; cur_y_snap = cur_y;
    expect_line(ax_snap, ay_snap, cur_x, cur_y);
    chk_shape(name);
}

static void chk_rect_done(const char *name)
{
    cur_x_snap = cur_x; cur_y_snap = cur_y;
    expect_rect(ax_snap, ay_snap, cur_x, cur_y);
    chk_shape(name);
}

/* --- files --- */
static void chk_saved(const char *name)
{
    FILE *f = fopen("/tmp/ww_stub_storage.bin", "rb");
    long size = 0;
    int rowbytes = (world.w + 3) / 4;
    cur_check = name; (void)cur_check;
    take_snapshot();                       /* remember the field for chk_loaded */
    ok(f != NULL, name, "no file was written");
    if (f) {
        fseek(f, 0, SEEK_END);
        size = ftell(f);
        fclose(f);
    }
    ok(size == 10 + (long)rowbytes * world.h, name, "file is %ld bytes", size);
    printf("  saved %ld bytes (%dx%d)\n", size, world.w, world.h);
}

static void chk_cleared(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(count_cells(WW_EMPTY) == world.w * world.h, name, "field not empty");
}

static void chk_loaded(const char *name)
{
    int i, bad = 0;
    cur_check = name; (void)cur_check;
    for (i = 0; i < snap_len; i++)
        if (world.cells[i] != snap_cells[i]) bad++;
    ok(bad == 0, name, "%d cells differ from what was saved", bad);
}

/* --- settings --- */

static void chk_field_resized(const char *name)
{
    /* the list was opened on the current size and we pressed UP once */
    int want = field_index_at_start > 0 ? field_index_at_start - 1 : 0;
    cur_check = name; (void)cur_check;
    ok(world.w == field_list[want][0] && world.h == field_list[want][1], name,
       "field is %dx%d, expected %dx%d", world.w, world.h,
       field_list[want][0], field_list[want][1]);
    printf("  field resized to %dx%d\n", world.w, world.h);
}

static void chk_speed(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(speed == (speed_at_start + 1) % SPEED_COUNT, name,
       "speed index %d, expected %d", speed,
       (speed_at_start + 1) % SPEED_COUNT);
    printf("  speed: %d -> %s generations per second\n", speed,
           speed_ips[speed] ? "some" : "MAX");
}

static void chk_grid_lines(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(grid_mode == GRID_LINES, name, "grid_mode %d", grid_mode);
}

static void chk_menu_closed(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(count_px(0, 215, COL_BOXBG) == 0, name, "the menu is still on screen");
}

/* the help box must fit on the screen and must not eat the field */
static void chk_help_open(const char *name)
{
    int box = count_px(0, 215, COL_BOXBG);
    int border_top = count_px(2, 2, COL_BOXFG);
    int bottom = box_bottom(2, HELP_LINES);
    int border_bot = count_px(bottom, bottom, COL_BOXFG);
    int below = count_px(bottom + 3, 215, COL_BOXFG)
              + count_px(bottom + 3, 215, COL_BOXBG);
    cur_check = name; (void)cur_check;
    ok(box > 5000, name, "help box is only %d px", box);
    ok(border_top > 100, name, "no top border");
    ok(border_bot > 100, name, "no bottom border");
    ok(below == 0, name, "the help box runs off the screen");
    ok(bottom < VIS_H - 1, name, "help box bottom at %d", bottom);
    ok(row_y(2, HELP_LINES - 1) + font_h < VIS_H, name,
       "the last help line is below the screen");
}

/* the restarted program must be a fresh one: a new field of the biggest
   size, generation 0 and none of the OS menu left on the screen */
static void chk_restarted(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(world.w == 512 && world.h == 256, name,
       "field is %dx%d after the restart", world.w, world.h);
    ok(world.gen == 0, name, "generation %lu after the restart", world.gen);
    ok(zoom == 1 && hdr_on == 1 && speed == 4 && grid_mode == GRID_DOTS, name,
       "globals were not reset on restart");
    ok(host_timers_installed() == 0, name,
       "%d user timers still installed after restart", host_timers_installed());
    ok(count_px(0, 215, 0xFFFF) < 20000, name,
       "the white OS menu is still on screen (%d px)",
       count_px(0, 215, 0xFFFF));
}

static void chk_stepped_after_restart(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(world.gen == 1, name, "expected gen 1 after F2, got %lu", world.gen);
}

static void centre_cursor(void)
{
    cur_x = wrap(cam_x + view_w / 2, world.w);
    cur_y = wrap(cam_y + view_h / 2, world.h);
    mark_all();
    render_frame();
}

static void chk_help_closed(const char *name)
{
    cur_check = name; (void)cur_check;
    centre_cursor();
    ok(count_px(0, 215, COL_BOXBG) == 0, name, "help box did not close");
    ok(count_px(grid_top, 215, COL_CURSOR) == expected_cursor_px(), name,
       "cursor pixels %d", count_px(grid_top, 215, COL_CURSOR));
}

/* Sets up an asymmetric 2x3 L-shape at (100, 80):
     (100,80)=HEAD   (101,80)=EMPTY
     (100,81)=WIRE   (101,81)=EMPTY
     (100,82)=WIRE   (101,82)=TAIL
   and positions cursor at (100, 80) to drive Copy/Cut/Paste/Rotate/Flip via keys. */
static void chk_clip_setup(const char *name)
{
    cur_check = name; (void)cur_check;
    ww_clear(&world);
    ww_set(&world, 100, 80, WW_HEAD);
    ww_set(&world, 100, 81, WW_CONDUCTOR);
    ww_set(&world, 100, 82, WW_CONDUCTOR);
    ww_set(&world, 101, 82, WW_TAIL);
    /* background wire at (111, 80) to test transparent vs box paste */
    ww_set(&world, 111, 80, WW_CONDUCTOR);
    ww_set(&world, 121, 80, WW_CONDUCTOR);
    cur_x = 100; cur_y = 80;
    recentre();
    mark_all();
    render_frame();
    ok(ww_get(&world, 100, 80) == WW_HEAD, name, "setup head");
}

static void chk_copy_outline(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(tool == TOOL_COPY && anchor_on == 1, name, "not anchored in copy");
    ok(count_px(grid_top, 215, COL_COPY_BOX) > 0, name, "no green copy outline");
}

static void chk_copied(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(clip_w == 2 && clip_h == 3, name, "clip %dx%d != 2x3", clip_w, clip_h);
    ok(anchor_on == 0, name, "anchor still on");
    ok(count_px(grid_top, 215, COL_COPY_BOX) == 0, name, "copy outline left behind");
    /* source untouched after copy */
    ok(ww_get(&world, 100, 80) == WW_HEAD && ww_get(&world, 101, 82) == WW_TAIL,
       name, "copy modified source");
    /* move cursor to (110, 80) for transparent paste test */
    cur_x = 110; cur_y = 80;
    mark_all();
    render_frame();
}

static void chk_pasted_transp(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(ww_get(&world, 110, 80) == WW_HEAD, name, "pasted head missing");
    ok(ww_get(&world, 110, 81) == WW_CONDUCTOR, name, "pasted wire missing");
    ok(ww_get(&world, 111, 82) == WW_TAIL, name, "pasted tail missing");
    /* (111, 80) had existing wire under an empty clipboard cell -> kept! */
    ok(ww_get(&world, 111, 80) == WW_CONDUCTOR, name,
       "transparent paste erased underlying wire");
    /* move to (120, 80) to test box-overwrite paste */
    cur_x = 120; cur_y = 80;
    mark_all();
    render_frame();
}

static void chk_pasted_box(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(paste_opaque == 1, name, "paste_opaque not toggled");
    ok(ww_get(&world, 120, 80) == WW_HEAD, name, "box paste head missing");
    /* (121, 80) had existing wire under empty clipboard cell -> overwritten! */
    ok(ww_get(&world, 121, 80) == WW_EMPTY, name,
       "overwrite box paste did not clear underlying wire");
    /* move to (130, 80) to test 90 CW rotation + horizontal flip */
    cur_x = 130; cur_y = 80;
    mark_all();
    render_frame();
}

static void chk_pasted_rot90(const char *name)
{
    /* 2x3 L-shape rotated 90 CW becomes 3x2:
         row 0: WIRE(130,80) WIRE(131,80) HEAD(132,80)
         row 1: TAIL(130,81) EMPTY        EMPTY */
    cur_check = name; (void)cur_check;
    ok(clip_rot == 1, name, "expected rot=1, got %d", clip_rot);
    ok(ww_get(&world, 130, 80) == WW_CONDUCTOR &&
       ww_get(&world, 131, 80) == WW_CONDUCTOR &&
       ww_get(&world, 132, 80) == WW_HEAD &&
       ww_get(&world, 130, 81) == WW_TAIL &&
       ww_get(&world, 132, 81) == WW_EMPTY,
       name, "90 CW rotated cells mismatch");
    cur_x = 140; cur_y = 80;
    mark_all();
    render_frame();
}

static void chk_pasted_fliph(const char *name)
{
    /* Horizontal mirror of the 90 CW 3x2 stamp:
         row 0: HEAD(140,80) WIRE(141,80) WIRE(142,80)
         row 1: EMPTY        EMPTY        TAIL(142,81) */
    cur_check = name; (void)cur_check;
    ok(ww_get(&world, 140, 80) == WW_HEAD &&
       ww_get(&world, 141, 80) == WW_CONDUCTOR &&
       ww_get(&world, 142, 80) == WW_CONDUCTOR &&
       ww_get(&world, 142, 81) == WW_TAIL &&
       ww_get(&world, 140, 81) == WW_EMPTY,
       name, "horizontal flip cells mismatch");
    /* move back to original (100, 80) to test CUT */
    cur_x = 100; cur_y = 80;
    mark_all();
    render_frame();
}

static void chk_cut_done(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(count_px(grid_top, 215, COL_CUT_BOX) == 0, name, "cut outline left behind");
    ok(ww_get(&world, 100, 80) == WW_EMPTY &&
       ww_get(&world, 100, 81) == WW_EMPTY &&
       ww_get(&world, 100, 82) == WW_EMPTY &&
       ww_get(&world, 101, 82) == WW_EMPTY,
       name, "cut did not clear source cells");
    ok(clip_w == 2 && clip_h == 3 && clip_rot == 0 && clip_flip == 0,
       name, "cut clipboard state wrong");
}

static void chk_paste_exited(const char *name)
{
    cur_check = name; (void)cur_check;
    ok(tool == TOOL_PEN, name, "EXIT from paste did not return to PEN (tool=%d)", tool);
    ok(count_px(grid_top, 215, COL_PREVIEW) == 0, name, "paste preview left behind");
    printf("  clipboard: copy, cut, paste (transp+box), 90 CW rotate & flip verified\n");
}

static void chk_menu_back_clean(const char *name)
{
    cur_check = name; (void)cur_check;
    /* Menu closed via EXE on 'BACK TO THE FIELD' at zoom 6 (rows 212..215 remainder):
       zero menu pixels anywhere on screen, including rows 212..215, and EXE did not paint! */
    ok(count_px(0, 215, COL_BOXBG) == 0, name,
       "menu COL_BOXBG left on screen (%d px)", count_px(0, 215, COL_BOXBG));
    ok(count_px(0, 215, COL_BOXSEL) == 0, name,
       "menu COL_BOXSEL left on screen (%d px)", count_px(0, 215, COL_BOXSEL));
    ok(count_px(212, 215, COL_BOXFG) == 0, name,
       "menu border left in bottom remainder strip (%d px)",
       count_px(212, 215, COL_BOXFG));
    ok(ww_get(&world, cur_x, cur_y) == WW_EMPTY, name,
       "closing menu with EXE accidentally painted under cursor");
    printf("  menu close: zero leftover pixels after BACK TO THE FIELD & remainder strip\n");
}

static void chk_final(const char *name)
{
    cur_check = name; (void)cur_check;
    centre_cursor();
    ok(count_px(grid_top, 215, COL_CURSOR) == expected_cursor_px(), name,
       "cursor pixels %d", count_px(grid_top, 215, COL_CURSOR));
    ok(count_px(0, 215, COL_PREVIEW) == 0, name, "preview left over");
}

/* ------------------------------------------------------------------ */
#define S(k, h, c, n) { k, h, c, n }
static const Step steps[] = {
    S(0,                2100, chk_start,        "start"),
    S(KEY_PRGM_RIGHT,    900, chk_trail,        "hold right"),
    S(KEY_PRGM_RIGHT,    900, chk_trail,        "hold right again"),
    S(KEY_PRGM_LEFT,     600, chk_trail,        "hold left"),
    S(KEY_PRGM_UP,       600, chk_trail,        "hold up"),

    S(KEY_PRGM_F1,       120, NULL,             "run"),
    S(0,                2500, chk_running,      "running"),
    S(KEY_PRGM_F1,       120, NULL,             "pause"),
    S(0,                 300, chk_paused,       "paused"),
    S(0,                 200, chk_snap_gen,     "gen"),
    S(KEY_PRGM_F2,       120, chk_step,         "single step"),

    S(0,                 300, chk_snap_field,   "snapshot"),
    S(KEY_PRGM_1,        120, chk_no_paint,     "choose wire"),
    S(KEY_PRGM_3,        120, chk_no_paint,     "choose tail"),
    S(KEY_PRGM_4,        120, chk_no_paint,     "choose pen"),

    S(KEY_PRGM_8,        120, NULL,             "full screen"),
    S(0,                1800, chk_fullscreen,   "full screen"),

    S(KEY_PRGM_9,        120, NULL,             "zoom in"),
    S(0,                 250, chk_zoom3,        "zoom 3"),
    S(KEY_PRGM_9,        120, NULL,             "zoom in"),
    S(0,                 250, chk_zoom4,        "zoom 4"),
    S(KEY_PRGM_9,        120, NULL,             "zoom in"),
    S(0,                 250, chk_zoom6,        "zoom 6"),
    S(KEY_PRGM_9,        120, NULL,             "zoom in"),
    S(0,                 250, chk_zoom8,        "zoom 8"),
    S(KEY_PRGM_7,        120, NULL,             "zoom out"),
    S(KEY_PRGM_7,        120, NULL,             "zoom out"),
    S(KEY_PRGM_7,        120, NULL,             "zoom out"),
    S(KEY_PRGM_7,        120, NULL,             "zoom out"),
    S(0,                 250, chk_zoom2,        "zoom 2"),
    S(KEY_PRGM_7,        120, NULL,             "zoom out"),
    S(0,                 250, chk_zoom1,        "zoom 1"),
    S(KEY_PRGM_9,        120, NULL,             "zoom in"),
    S(0,                 250, chk_zoom2,        "zoom 2"),
    S(KEY_PRGM_8,        120, NULL,             "status line on"),
    S(0,                1800, chk_header_on,    "status line on"),

    S(0,                 250, chk_seam,         "seam"),

    S(KEY_PRGM_5,        120, NULL,             "line tool"),
    S(KEY_PRGM_RETURN,   120, chk_anchor,       "line start"),
    S(KEY_PRGM_RIGHT,    900, chk_preview,      "line preview"),
    S(KEY_PRGM_RETURN,   120, chk_line_done,    "line drawn"),
    S(0,                 250, chk_no_preview,   "no preview"),
    S(KEY_PRGM_6,        120, NULL,             "rect tool"),
    S(KEY_PRGM_RETURN,   120, chk_rect_anchor,  "rect start"),
    S(KEY_PRGM_RIGHT,    600, NULL,             "rect drag"),
    S(KEY_PRGM_DOWN,     400, NULL,             "rect drag"),
    S(KEY_PRGM_RETURN,   120, chk_rect_done,    "rect drawn"),
    S(0,                 250, chk_no_preview,   "no preview"),
    S(KEY_PRGM_4,        120, NULL,             "pen tool"),

    S(KEY_PRGM_OPTN,     120, NULL,             "menu"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_RETURN,   300, chk_saved,        "save"),
    S(0,                1800, NULL,             "wait"),
    S(KEY_PRGM_F3,       200, chk_cleared,      "clear"),
    S(KEY_PRGM_OPTN,     120, NULL,             "menu"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_RETURN,   300, chk_loaded,       "load"),
    S(0,                1800, NULL,             "wait"),

    S(KEY_PRGM_OPTN,     120, NULL,             "menu"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_RETURN,   200, NULL,             "size list"),
    S(KEY_PRGM_UP,       120, NULL,             "pick smaller"),
    S(KEY_PRGM_RETURN,   300, chk_field_resized, "field size"),
    S(KEY_PRGM_EXIT,     120, NULL,             "menu closed"),
    S(0,                1800, chk_menu_closed,  "menu gone"),

    S(KEY_PRGM_OPTN,     120, NULL,             "menu"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_RETURN,   200, NULL,             "speed list"),
    S(KEY_PRGM_DOWN,     120, NULL,             "faster"),
    S(KEY_PRGM_RETURN,   200, chk_speed,        "speed"),
    S(KEY_PRGM_EXIT,     120, chk_menu_closed,  "menu closed"),

    S(KEY_PRGM_OPTN,     120, NULL,             "menu"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_DOWN,     120, NULL,             "menu down"),
    S(KEY_PRGM_RETURN,   200, NULL,             "grid list"),
    S(KEY_PRGM_DOWN,     120, NULL,             "next grid"),
    S(KEY_PRGM_RETURN,   120, chk_grid_lines,   "grid lines"),
    S(KEY_PRGM_EXIT,     120, chk_menu_closed,  "menu closed"),

    S(KEY_PRGM_F6,       120, NULL,             "help"),
    S(KEY_PRGM_RIGHT,    120, chk_help_open,    "help page 2"),
    S(KEY_PRGM_EXIT,     120, chk_help_closed,  "help closed"),

    /* Copy / Paste / Box-Paste / Rotate / Flip / Cut / Exit-to-Pen */
    S(0,                 120, chk_clip_setup,   "clip setup"),
    S(KEY_PRGM_MUL,      120, NULL,             "copy tool"),
    S(KEY_PRGM_RETURN,   120, NULL,             "copy start"),
    S(KEY_PRGM_RIGHT,    120, NULL,             "copy right 1"),
    S(KEY_PRGM_DOWN,     120, NULL,             "copy down 1"),
    S(KEY_PRGM_DOWN,     120, chk_copy_outline, "copy down 2"),
    S(KEY_PRGM_RETURN,   120, NULL,             "copy commit"),
    S(0,                 200, chk_copied,       "copied 2x3"),

    S(KEY_PRGM_ADD,      120, NULL,             "paste tool"),
    S(KEY_PRGM_RETURN,   120, chk_pasted_transp,"paste transp"),

    S(KEY_PRGM_ADD,      120, NULL,             "toggle box paste"),
    S(KEY_PRGM_RETURN,   120, chk_pasted_box,   "paste box"),
    S(KEY_PRGM_ADD,      120, NULL,             "toggle back transp"),

    S(KEY_PRGM_SUB,      120, NULL,             "rotate 90 CW"),
    S(KEY_PRGM_RETURN,   120, chk_pasted_rot90, "paste rot 90"),

    S(KEY_PRGM_DOT,      120, NULL,             "flip horiz"),
    S(KEY_PRGM_RETURN,   120, chk_pasted_fliph, "paste flip H"),

    S(KEY_PRGM_DIV,      120, NULL,             "cut tool"),
    S(KEY_PRGM_RETURN,   120, NULL,             "cut start"),
    S(KEY_PRGM_RIGHT,    120, NULL,             "cut right 1"),
    S(KEY_PRGM_DOWN,     120, NULL,             "cut down 1"),
    S(KEY_PRGM_DOWN,     120, NULL,             "cut down 2"),
    S(KEY_PRGM_RETURN,   120, NULL,             "cut commit"),
    S(0,                 200, chk_cut_done,     "cut verified"),

    S(KEY_PRGM_ADD,      120, NULL,             "paste again"),
    S(KEY_PRGM_EXIT,     120, NULL,             "exit paste to pen"),
    S(0,                 1800, chk_paste_exited,"paste exited"),

    /* Test closing OPTN menu via EXE on 'BACK TO THE FIELD' at zoom=6px */
    S(KEY_PRGM_9,        120, NULL,             "zoom 3"),
    S(KEY_PRGM_9,        120, NULL,             "zoom 4"),
    S(KEY_PRGM_9,        120, NULL,             "zoom 6"),
    S(KEY_PRGM_OPTN,     120, NULL,             "open menu at zoom 6"),
    S(KEY_PRGM_RETURN,   120, chk_menu_back_clean, "close via BACK TO THE FIELD"),
    S(KEY_PRGM_7,        120, NULL,             "zoom 4"),
    S(KEY_PRGM_7,        120, NULL,             "zoom 3"),
    S(KEY_PRGM_7,        120, NULL,             "zoom 2"),

    S(0,                 250, chk_final,        "final")
};
#undef S

static void report(void);

/* models the OS main menu.  On the first two exits the user picks Wireworld
   again, so the add-in has to come back and start a clean session; on the
   third exit it is left for good. */
int host_os_menu(void)
{
    static int visits;
    int i;

    visits++;
    ok(count_px(0, 215, COL_BOXFG) > 0, "os menu entry",
       "VRAM flashed black before entering OS menu (%d white px)",
       count_px(0, 215, COL_BOXFG));
    ok(host_timers_installed() == 0, "os menu entry",
       "%d user timers left installed when entering OS menu",
       host_timers_installed());

    if (visits <= 2) {
        resumed = visits;
        for (i = 0; i < 384 * 216; i++) vram[i] = 0xFFFF;   /* the menu */
        printf("  OS menu opened (#%d), Wireworld picked again -> restarts\n",
               visits);
        return 1;
    }
    printf("  the add-in was left for good (#%d)\n", visits);
    report();
    exit(failures ? 1 : 0);
}

int main(void)
{
    script = steps;
    script_len = (int)(sizeof(steps) / sizeof(steps[0]));

    ww_main();

    /* the add-in must never leave main: the OS then refuses to start it
       again until another add-in has been run */
    checks++;
    failures++;
    printf("FAIL [exit]: main() returned\n");
    report();
    return 1;
}

static void report(void)
{
    cur_check = "cursor while moving";
    checks++;
    if (hold_frames < 20) {
        failures++;
        printf("FAIL [cursor while moving]: only %ld frames watched\n",
               hold_frames);
    } else if (hold_no_cursor) {
        failures++;
        printf("FAIL [cursor while moving]: the cursor vanished in %ld of "
               "%ld frames\n", hold_no_cursor, hold_frames);
    } else if (hold_max_cursor != expected_cursor_px()) {
        failures++;
        printf("FAIL [cursor while moving]: cursor grew to %ld px (one cell "
               "is %d px)\n", hold_max_cursor, expected_cursor_px());
    }
    printf("  cursor: visible in all %ld frames drawn while moving\n",
           hold_frames);

    printf("\n%d checks, %d failures, %ld frames drawn\n",
           checks, failures, frames);
}
