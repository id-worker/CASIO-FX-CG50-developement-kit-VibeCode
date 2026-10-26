/*
 * PyCEdit -- Python (.py) & PicoC (.c) Code Editor + Extension Switcher
 * for Casio fx-CG50, styled after PythonExtra / JustUI.
 */
#ifndef HOST_TEST
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/file.h>
#include <fxcg/rtc.h>
#include <fxcg/system.h>
#include <fxcg/heap.h>
#else
#include "host_fxcg.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

#include "font6x10.h"

/* ------------------------------------------------------------------ */
/* Display geometry & PythonExtra-inspired RGB565 palette              */
/* ------------------------------------------------------------------ */
#define VRAM_W          384
#define VIS_W           384
#define VIS_H           216

#define HDR_H           18      /* y = 0..17   PythonExtra black bar  */
#define FKEY_Y          198     /* y = 198..215 JustUI jfkeys bar     */
#define BODY_Y          18      /* y = 18..197 (180 px = 15 * 12 px)  */
#define LINE_H          12
#define EDIT_ROWS       15
#define GUTTER_W        27      /* 4 digits * 6px + padding           */
#define CODE_X          30      /* 384 - 30 - 6 = 348px -> 58 cols    */
#define CODE_COLS       58
#define FILE_ROWS       14      /* rows in FILES browser              */

#define RGB565(r,g,b)   ((unsigned short)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))

#define COL_BLACK       0x0000
#define COL_WHITE       0xFFFF
#define COL_HDR_BG      0x0000
#define COL_HDR_FG      0xFFFF
#define COL_GUTTER_BG   RGB565(241, 243, 246)
#define COL_GUTTER_FG   RGB565(130, 140, 155)
#define COL_GUTTER_CUR  RGB565(30,  60,  120)
#define COL_BORDER      RGB565(205, 212, 222)
#define COL_CURLINE     RGB565(235, 243, 255)
#define COL_SEL_BG      RGB565(180, 213, 255)
#define COL_SEL_ROW     RGB565(24,  33,  47)
#define COL_CURSOR      RGB565(0,   80,  208)

/* Syntax highlighting colors (readable on white LCD) */
#define SYN_NORMAL      RGB565(24,  28,  34)
#define SYN_KEYWORD     RGB565(0,   72,  188)
#define SYN_PREPROC     RGB565(122, 31,  162)
#define SYN_STRING      RGB565(18,  122, 46)
#define SYN_COMMENT     RGB565(112, 122, 136)
#define SYN_NUMBER      RGB565(0,   116, 132)
#define SYN_OP          RGB565(70,  80,  95)

/* Badges */
#define COL_BADGE_PY    RGB565(28,  100, 210)
#define COL_BADGE_C     RGB565(18,  130, 72)
#define COL_BADGE_DIR   RGB565(90,  100, 120)
#define COL_BADGE_MOD   RGB565(230, 140, 0)
#define COL_BADGE_ALP   RGB565(40,  150, 70)
#define COL_TOAST_BG    RGB565(20,  28,  40)
#define COL_MODAL_DIM   RGB565(225, 230, 238)

/* ------------------------------------------------------------------ */
/* Casio fx-CG50 PRGM Matrix Keycodes (col*10 + row)                   */
/* ------------------------------------------------------------------ */
#define MK_F1       79
#define MK_F2       69
#define MK_F3       59
#define MK_F4       49
#define MK_F5       39
#define MK_F6       29

#define MK_SHIFT    78
#define MK_OPTN     68
#define MK_VARS     58
#define MK_MENU     48
#define MK_LEFT     38
#define MK_UP       28

#define MK_ALPHA    77
#define MK_SQR      67
#define MK_POW      57
#define MK_EXIT     47
#define MK_DOWN     37
#define MK_RIGHT    27

#define MK_XOT      76
#define MK_LOG      66
#define MK_LN       56
#define MK_SIN      46
#define MK_COS      36
#define MK_TAN      26

#define MK_FRAC     75
#define MK_FD       65
#define MK_LPAR     55
#define MK_RPAR     45
#define MK_COMMA    35
#define MK_STORE    25

#define MK_7        74
#define MK_8        64
#define MK_9        54
#define MK_DEL      44
#define MK_ACON     10

#define MK_4        73
#define MK_5        63
#define MK_6        53
#define MK_MUL      43
#define MK_DIV      33

#define MK_1        72
#define MK_2        62
#define MK_3        52
#define MK_ADD      42
#define MK_SUB      32

#define MK_0        71
#define MK_DOT      61
#define MK_EXP      51
#define MK_NEG      41
#define MK_EXE      31

/* ------------------------------------------------------------------ */
/* Limits & static storage (fits comfortably in 128 KB static RAM)     */
/* ------------------------------------------------------------------ */
#define MAX_PATH_LEN    256
#define MAX_NAME_LEN    64
#define MAX_FILES       160
#define MAX_BUF_SIZE    49152   /* 48 KB source buffer (~1,500 lines) */
#define MAX_CLIP_SIZE   49152   /* 48 KB clipboard (supports Select All!) */
#define SMEM_ROOT       "\\\\fls0\\"
#define TEMP_SAVE_FILE  "\\\\fls0\\@PYCTMP.TMP"

typedef struct {
    unsigned short id, type;
    unsigned long fsize, dsize;
    unsigned int property;
    unsigned long address;
} fs_info_t;

typedef struct {
    char name[MAX_NAME_LEN];
    unsigned int size;
    unsigned char is_dir;
} file_entry_t;

enum { TAB_FILES = 0, TAB_EDIT = 1 };
enum { LANG_PLAIN = 0, LANG_PY = 1, LANG_C = 2 };
enum {
    ALP_OFF = 0,
    ALP_ONE_LO = 1,
    ALP_LOCK_LO = 2,
    ALP_ONE_UP = 3,
    ALP_LOCK_UP = 4
};

static unsigned short *vram;

/* File browser state */
static char cur_dir[MAX_PATH_LEN] = SMEM_ROOT;
static file_entry_t dir_items[MAX_FILES];
static int dir_count = 0;
static int dir_sel = 0;
static int dir_scroll = 0;
static int filter_code_only = 1; /* 1 = folders + .py/.c/.h/.txt, 0 = all */

/* Editor state */
static char edit_buf[MAX_BUF_SIZE];
static int edit_len = 0;
static int cursor_pos = 0;       /* byte offset 0..edit_len            */
static int target_col = 0;       /* preferred column on up/down        */
static int scroll_line = 0;      /* 0-based top visible line           */
static int scroll_col = 0;       /* 0-based horizontal scroll column   */
static char open_path[MAX_PATH_LEN] = "";
static int edit_dirty = 0;
static int edit_lang = LANG_C;
static int sel_anchor = -1;      /* -1 = no selection, 0..edit_len     */
static int select_mode = 0;      /* 1 = arrow keys extend selection    */

/* Clipboard */
static char clip_buf[MAX_CLIP_SIZE];
static int clip_len = 0;

/* UI & modifier state */
static int active_tab = TAB_FILES;
static int quit_requested = 0;
static int shift_sticky = 0;
static int shift_was_down = 0;
static int shift_used_chord = 0;
static int alpha_mode = ALP_OFF;
static int poll_last_k = 0;
static int poll_repeating = 0;
static int poll_press_tick = 0;
static int poll_rel_pending = 0;
static int poll_rel_tick = 0;
static int alpha_was_down = 0;
static int shift_rel_pending = 0;
static int shift_rel_tick = 0;
static int alpha_rel_pending = 0;
static int alpha_rel_tick = 0;

/* Toast notification */
static char toast_text[56];
static int toast_active = 0;
static int toast_tick = 0;

/* ------------------------------------------------------------------ */
/* Low-level keyboard matrix polling                                   */
/* ------------------------------------------------------------------ */
#ifndef HOST_TEST
#define KEY_PORT ((volatile unsigned short *)0xA44B0000)
static int hw_key_down(int code)
{
    int row = code % 10;
    int col = code / 10 - 1;
    int word = row >> 1;
    int bit = col + ((row & 1) << 3);
    if (code <= 0) return 0;
    return (KEY_PORT[word] & (1 << bit)) != 0;
}
#else
int host_key_down(int code);
static int hw_key_down(int code) { return host_key_down(code); }
#endif

static const unsigned char non_mod_keys[] = {
    MK_F1, MK_F2, MK_F3, MK_F4, MK_F5, MK_F6,
    MK_UP, MK_DOWN, MK_LEFT, MK_RIGHT,
    MK_EXE, MK_EXIT, MK_DEL, MK_OPTN, MK_VARS, MK_MENU,
    MK_SQR, MK_POW,
    MK_XOT, MK_LOG, MK_LN, MK_SIN, MK_COS, MK_TAN,
    MK_FRAC, MK_FD, MK_LPAR, MK_RPAR, MK_COMMA, MK_STORE,
    MK_7, MK_8, MK_9, MK_ACON,
    MK_4, MK_5, MK_6, MK_MUL, MK_DIV,
    MK_1, MK_2, MK_3, MK_ADD, MK_SUB,
    MK_0, MK_DOT, MK_EXP, MK_NEG,
    0
};

static int is_repeatable_key(int k)
{
    return k == MK_UP || k == MK_DOWN || k == MK_LEFT || k == MK_RIGHT ||
           k == MK_DEL || k == MK_DOT;
}

static int current_shift(void)
{
    return shift_sticky;
}

static void consume_one_shot_mods(void)
{
    shift_sticky = 0;
    if (alpha_mode == ALP_ONE_LO || alpha_mode == ALP_ONE_UP)
        alpha_mode = ALP_OFF;
}

static void reset_key_poller(void)
{
    poll_last_k = 0;
    poll_repeating = 0;
    poll_rel_pending = 0;
    shift_was_down = 0;
    shift_rel_pending = 0;
    alpha_was_down = 0;
    alpha_rel_pending = 0;
}

/* Polls modifiers + non-modifier keys with 32ms hardware release-debounce
   and smooth auto-repeat. Sets *mod_changed = 1 if badge state changed. */
static int poll_editor_key(int *mod_changed)
{
    int s_down = hw_key_down(MK_SHIFT);
    int a_down = hw_key_down(MK_ALPHA);
    int i, k = 0;

    if (mod_changed) *mod_changed = 0;
    (void)shift_used_chord;

    /* SHIFT rising-edge toggle with 32ms release debounce */
    if (s_down) {
        shift_rel_pending = 0;
        if (!shift_was_down) {
            shift_was_down = 1;
            shift_sticky = !shift_sticky;
            if (mod_changed) *mod_changed = 1;
        }
    } else if (shift_was_down) {
        if (!shift_rel_pending) {
            shift_rel_pending = 1;
            shift_rel_tick = RTC_GetTicks();
        } else if (RTC_Elapsed_ms(shift_rel_tick, 32)) {
            shift_was_down = 0;
            shift_rel_pending = 0;
        }
    }

    /* ALPHA rising-edge state machine with 32ms release debounce */
    if (a_down) {
        alpha_rel_pending = 0;
        if (!alpha_was_down) {
            alpha_was_down = 1;
            if (shift_sticky) {
                shift_sticky = 0;
                alpha_mode = (alpha_mode == ALP_ONE_UP) ? ALP_LOCK_UP : ALP_ONE_UP;
            } else {
                if (alpha_mode == ALP_OFF) alpha_mode = ALP_ONE_LO;
                else if (alpha_mode == ALP_ONE_LO) alpha_mode = ALP_LOCK_LO;
                else if (alpha_mode == ALP_ONE_UP) alpha_mode = ALP_LOCK_UP;
                else alpha_mode = ALP_OFF;
            }
            if (mod_changed) *mod_changed = 1;
        }
    } else if (alpha_was_down) {
        if (!alpha_rel_pending) {
            alpha_rel_pending = 1;
            alpha_rel_tick = RTC_GetTicks();
        } else if (RTC_Elapsed_ms(alpha_rel_tick, 32)) {
            alpha_was_down = 0;
            alpha_rel_pending = 0;
        }
    }

    if (poll_last_k && hw_key_down(poll_last_k)) {
        k = poll_last_k;
    } else {
        for (i = 0; non_mod_keys[i]; i++) {
            if (hw_key_down(non_mod_keys[i])) {
                k = non_mod_keys[i];
                break;
            }
        }
    }

    if (!k) {
        if (poll_last_k) {
            if (!poll_rel_pending) {
                poll_rel_pending = 1;
                poll_rel_tick = RTC_GetTicks();
            } else if (RTC_Elapsed_ms(poll_rel_tick, 32)) {
                poll_last_k = 0;
                poll_repeating = 0;
                poll_rel_pending = 0;
            }
        }
        return 0;
    }

    poll_rel_pending = 0;
    if (k != poll_last_k) {
        poll_last_k = k;
        poll_repeating = 0;
        poll_press_tick = RTC_GetTicks();
        return k;
    }
    if (!is_repeatable_key(k)) return 0;
    if (!poll_repeating) {
        if (RTC_Elapsed_ms(poll_press_tick, 330)) {
            poll_repeating = 1;
            poll_press_tick = RTC_GetTicks();
            return k;
        }
        return 0;
    }
    if (RTC_Elapsed_ms(poll_press_tick, 50)) {
        poll_press_tick = RTC_GetTicks();
        return k;
    }
    return 0;
}

static void wait_all_released(void)
{
    int guard = 0;
    while (guard++ < 1500) {
        int any = hw_key_down(MK_SHIFT) || hw_key_down(MK_ALPHA);
        int i;
        for (i = 0; !any && non_mod_keys[i]; i++)
            if (hw_key_down(non_mod_keys[i])) any = 1;
        if (!any) break;
        OS_InnerWait_ms(2);
    }
    reset_key_poller();
}

/* ------------------------------------------------------------------ */
/* PythonExtra keymap translation                                      */
/* ------------------------------------------------------------------ */
static char translate_key_char(int k, int shf, int alp)
{
    int is_alpha = (alp != ALP_OFF);
    int is_upper = (alp == ALP_ONE_UP || alp == ALP_LOCK_UP);

    if (is_alpha && !shf) {
        char c = 0;
        switch (k) {
        case MK_XOT:   c = 'a'; break;
        case MK_LOG:   c = 'b'; break;
        case MK_LN:    c = 'c'; break;
        case MK_SIN:   c = 'd'; break;
        case MK_COS:   c = 'e'; break;
        case MK_TAN:   c = 'f'; break;
        case MK_FRAC:  c = 'g'; break;
        case MK_FD:    c = 'h'; break;
        case MK_LPAR:  c = 'i'; break;
        case MK_RPAR:  c = 'j'; break;
        case MK_COMMA: c = 'k'; break;
        case MK_STORE: c = 'l'; break;
        case MK_7:     c = 'm'; break;
        case MK_8:     c = 'n'; break;
        case MK_9:     c = 'o'; break;
        case MK_4:     c = 'p'; break;
        case MK_5:     c = 'q'; break;
        case MK_6:     c = 'r'; break;
        case MK_MUL:   c = 's'; break;
        case MK_DIV:   c = 't'; break;
        case MK_1:     c = 'u'; break;
        case MK_2:     c = 'v'; break;
        case MK_3:     c = 'w'; break;
        case MK_ADD:   c = 'x'; break;
        case MK_SUB:   c = 'y'; break;
        case MK_0:     c = 'z'; break;
        case MK_DOT:   return ' ';
        case MK_EXP:   return '"';
        case MK_NEG:   return ':';
        default:       return 0;
        }
        if (c && is_upper) c = (char)(c - 'a' + 'A');
        return c;
    }

    if (shf) {
        /* PythonExtra SHIFT shortcuts + extended C/Python operators */
        switch (k) {
        case MK_MUL:   return '{';
        case MK_DIV:   return '}';
        case MK_ADD:   return '[';
        case MK_SUB:   return ']';
        case MK_0:     return ':';
        case MK_DOT:   return '=';
        case MK_COMMA: return ';';
        case MK_LPAR:  return '<';
        case MK_RPAR:  return '>';
        case MK_EXP:   return '!';
        case MK_NEG:   return '#';
        case MK_POW:   return '&';
        case MK_SQR:   return '|';
        case MK_STORE: return '\'';
        case MK_FRAC:  return '\\';
        case MK_FD:    return '%';
        default:
            /* SHIFT + letter in alpha-lock types opposite case! */
            if (is_alpha) {
                char c = translate_key_char(k, 0, is_upper ? ALP_LOCK_LO : ALP_LOCK_UP);
                return c;
            }
            return 0;
        }
    }

    /* Normal unshifted layer (matches PythonExtra map_flat) */
    switch (k) {
    case MK_0:     return '0';
    case MK_1:     return '1';
    case MK_2:     return '2';
    case MK_3:     return '3';
    case MK_4:     return '4';
    case MK_5:     return '5';
    case MK_6:     return '6';
    case MK_7:     return '7';
    case MK_8:     return '8';
    case MK_9:     return '9';
    case MK_DOT:   return '.';
    case MK_ADD:   return '+';
    case MK_SUB:   return '-';
    case MK_MUL:   return '*';
    case MK_DIV:   return '/';
    case MK_LPAR:  return '(';
    case MK_RPAR:  return ')';
    case MK_COMMA: return ',';
    case MK_STORE: return '=';
    case MK_NEG:   return '_';
    case MK_POW:   return '^';
    default:       return 0;
    }
}

/* ------------------------------------------------------------------ */
/* Drawing primitives (direct 384x216 RGB565 VRAM)                     */
/* ------------------------------------------------------------------ */
static void px_rect(int x1, int y1, int x2, int y2, unsigned short col)
{
    int x, y;
    if (x1 > x2) { int t = x1; x1 = x2; x2 = t; }
    if (y1 > y2) { int t = y1; y1 = y2; y2 = t; }
    if (x2 < 0 || y2 < 0 || x1 >= VIS_W || y1 >= VIS_H) return;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 >= VIS_W) x2 = VIS_W - 1;
    if (y2 >= VIS_H) y2 = VIS_H - 1;
    for (y = y1; y <= y2; y++) {
        unsigned short *row = vram + y * VRAM_W + x1;
        for (x = x1; x <= x2; x++) *row++ = col;
    }
}

static void px_box(int x1, int y1, int x2, int y2, unsigned short col)
{
    px_rect(x1, y1, x2, y1, col);
    px_rect(x1, y2, x2, y2, col);
    px_rect(x1, y1, x1, y2, col);
    px_rect(x2, y1, x2, y2, col);
}

static void draw_char(int x, int y, char ch, unsigned short fg)
{
    unsigned char uc = (unsigned char)ch;
    int ry, rx;
    if (uc < 32 || uc > 126) uc = '?';
    const unsigned char *g = font6x10[uc - 32];
    for (ry = 0; ry < FONT_H; ry++) {
        int py = y + ry;
        unsigned char bits;
        if (py < 0 || py >= VIS_H) continue;
        bits = g[ry];
        if (!bits) continue;
        for (rx = 0; rx < FONT_W; rx++) {
            if (bits & (0x20 >> rx)) {
                int px = x + rx;
                if (px >= 0 && px < VIS_W)
                    vram[py * VRAM_W + px] = fg;
            }
        }
    }
}

static void draw_str(int x, int y, const char *s, unsigned short fg)
{
    while (*s) {
        if (x + FONT_W > VIS_W) break;
        draw_char(x, y, *s++, fg);
        x += FONT_W;
    }
}

static void draw_str_n(int x, int y, const char *s, int max_chars, unsigned short fg)
{
    int i = 0;
    while (s[i] && i < max_chars) {
        if (x + FONT_W > VIS_W) break;
        draw_char(x, y, s[i], fg);
        x += FONT_W;
        i++;
    }
}

static void show_toast(const char *msg)
{
    strncpy(toast_text, msg, sizeof(toast_text) - 1);
    toast_text[sizeof(toast_text) - 1] = '\0';
    toast_active = 1;
    toast_tick = RTC_GetTicks();
}

/* JustUI / PythonExtra style F-key bar at y = 198..215 */
static void draw_fkeys(const char *labels[6], int active_mask)
{
    int i;
    px_rect(0, FKEY_Y - 1, VIS_W - 1, FKEY_Y - 1, COL_BORDER);
    px_rect(0, FKEY_Y, VIS_W - 1, VIS_H - 1, COL_WHITE);

    for (i = 0; i < 6; i++) {
        const char *lbl = labels[i];
        int x1 = i * 64 + 2;
        int x2 = (i + 1) * 64 - 3;
        int y1 = FKEY_Y + 2;
        int y2 = VIS_H - 2;
        int is_act = (active_mask & (1 << i)) != 0;
        int len, tx;

        if (!lbl || !lbl[0]) continue;
        len = (int)strlen(lbl);
        if (len > 9) len = 9;
        tx = x1 + ((x2 - x1 + 1) - len * FONT_W) / 2;

        if (is_act) {
            /* PythonExtra / JustUI active tab: black fill with crisp top-right bevel */
            px_rect(x1, y1, x2, y2, COL_BLACK);
            vram[y1 * VRAM_W + x2] = COL_WHITE;
            vram[y1 * VRAM_W + (x2 - 1)] = COL_WHITE;
            vram[(y1 + 1) * VRAM_W + x2] = COL_WHITE;
            draw_str_n(tx, y1 + 2, lbl, 9, COL_WHITE);
        } else {
            px_box(x1, y1, x2, y2, COL_BLACK);
            draw_str_n(tx, y1 + 2, lbl, 9, COL_BLACK);
        }
    }
}

static void draw_mod_badge(int right_x)
{
    const char *txt = NULL;
    unsigned short bg = COL_BADGE_MOD;

    if (current_shift()) {
        txt = "SHF";
        bg = COL_BADGE_MOD;
    } else if (alpha_mode == ALP_ONE_LO) {
        txt = "alp";
        bg = COL_BADGE_ALP;
    } else if (alpha_mode == ALP_LOCK_LO) {
        txt = "aLK";
        bg = COL_BADGE_ALP;
    } else if (alpha_mode == ALP_ONE_UP) {
        txt = "ALP";
        bg = COL_BADGE_PY;
    } else if (alpha_mode == ALP_LOCK_UP) {
        txt = "ALK";
        bg = COL_BADGE_PY;
    }
    if (!txt) return;

    px_rect(right_x - 24, 2, right_x, HDR_H - 3, bg);
    draw_str(right_x - 21, 4, txt, COL_WHITE);
}

static void draw_toast_overlay(void)
{
    int len, w, x1, y1;
    if (!toast_active) return;
    len = (int)strlen(toast_text);
    w = len * FONT_W + 16;
    if (w > VIS_W - 16) w = VIS_W - 16;
    x1 = (VIS_W - w) / 2;
    y1 = FKEY_Y - 22;
    px_rect(x1, y1, x1 + w - 1, y1 + 16, COL_TOAST_BG);
    px_box(x1, y1, x1 + w - 1, y1 + 16, COL_WHITE);
    draw_str_n(x1 + 8, y1 + 4, toast_text, 56, COL_WHITE);
}

/* ------------------------------------------------------------------ */
/* String & path helpers                                               */
/* ------------------------------------------------------------------ */
static int str_ends_ci(const char *s, const char *suf)
{
    size_t ls = strlen(s), lf = strlen(suf);
    size_t i;
    if (ls < lf) return 0;
    for (i = 0; i < lf; i++) {
        if (tolower((unsigned char)s[ls - lf + i]) !=
            tolower((unsigned char)suf[i]))
            return 0;
    }
    return 1;
}

static int detect_lang(const char *name)
{
    if (str_ends_ci(name, ".py")) return LANG_PY;
    if (str_ends_ci(name, ".c") || str_ends_ci(name, ".h") ||
        str_ends_ci(name, ".cpp") || str_ends_ci(name, ".hpp"))
        return LANG_C;
    return LANG_PLAIN;
}

static int is_code_visible_file(const char *name)
{
    return str_ends_ci(name, ".py") || str_ends_ci(name, ".c") ||
           str_ends_ci(name, ".h")  || str_ends_ci(name, ".txt") ||
           str_ends_ci(name, ".cfg");
}

static const char *path_basename(const char *p)
{
    const char *last = p;
    while (*p) {
        if (*p == '\\' || *p == '/') last = p + 1;
        p++;
    }
    return last;
}

static void format_dir_display(const char *dir, char *out, int out_sz)
{
    /* Convert "\\fls0\" -> "/" and "\\fls0\sub\" -> "/sub" like PythonExtra */
    const char *p = dir;
    int i = 0;
    if (strncmp(p, SMEM_ROOT, strlen(SMEM_ROOT)) == 0)
        p += strlen(SMEM_ROOT);
    out[i++] = '/';
    while (*p && i < out_sz - 1) {
        if (*p == '\\' && p[1] == '\0') break;
        out[i++] = (*p == '\\') ? '/' : *p;
        p++;
    }
    out[i] = '\0';
}

static void fmt_lnum4(char *out, int n)
{
    int i;
    if (n < 0) n = 0;
    if (n > 9999) n = n % 10000;
    out[4] = '\0';
    for (i = 3; i >= 0; i--) {
        if (n > 0 || i == 3) {
            out[i] = (char)('0' + (n % 10));
            n /= 10;
        } else {
            out[i] = ' ';
        }
    }
}

static void copy_str(char *dst, int dst_sz, const char *src)
{
    int n;
    if (dst == src || dst_sz <= 0) return;
    n = (int)strlen(src);
    if (n >= dst_sz) n = dst_sz - 1;
    memmove(dst, src, (size_t)n);
    dst[n] = '\0';
}

static void join_path(char *out, int out_sz, const char *dir, const char *name)
{
    int ld = (int)strlen(dir);
    int ln = (int)strlen(name);
    if (ld >= out_sz - 1) ld = out_sz - 1;
    memcpy(out, dir, (size_t)ld);
    if (ld + ln >= out_sz - 1) ln = (out_sz - 1) - ld;
    if (ln > 0) memcpy(out + ld, name, (size_t)ln);
    out[ld + ln] = '\0';
}

/* ------------------------------------------------------------------ */
/* Filesystem operations (safe Casio Bfile rules)                      */
/* ------------------------------------------------------------------ */
static int cmp_entries(const void *a, const void *b)
{
    const file_entry_t *ea = (const file_entry_t *)a;
    const file_entry_t *eb = (const file_entry_t *)b;
    if (ea->is_dir != eb->is_dir) return eb->is_dir - ea->is_dir;
    if (strcmp(ea->name, "..") == 0) return -1;
    if (strcmp(eb->name, "..") == 0) return 1;
    return strcmp(ea->name, eb->name);
}

static void refresh_dir(const char *select_name)
{
    unsigned short u_mask[MAX_PATH_LEN];
    unsigned short u_found[MAX_PATH_LEN];
    char mask[MAX_PATH_LEN];
    char name_buf[MAX_PATH_LEN];
    fs_info_t info;
    int fhandle = -1;
    int ret;

    dir_count = 0;

    /* Add parent ".." entry when inside a subfolder */
    if (strcmp(cur_dir, SMEM_ROOT) != 0) {
        strcpy(dir_items[dir_count].name, "..");
        dir_items[dir_count].size = 0;
        dir_items[dir_count].is_dir = 1;
        dir_count++;
    }

    join_path(mask, sizeof(mask), cur_dir, "*");
    Bfile_StrToName_ncpy(u_mask, mask, MAX_PATH_LEN);

    memset(&info, 0, sizeof(info));
    ret = Bfile_FindFirst_NON_SMEM((const char *)u_mask, &fhandle,
                                   (char *)u_found, &info);
    while (ret == 0 && dir_count < MAX_FILES) {
        Bfile_NameToStr_ncpy(name_buf, u_found, sizeof(name_buf));
        name_buf[sizeof(name_buf) - 1] = '\0';

        if (strcmp(name_buf, ".") != 0 &&
            strcmp(name_buf, "..") != 0 &&
            strcmp(name_buf, "@MainMem") != 0 &&
            strcmp(name_buf, "@PYCTMP.TMP") != 0) {
            int is_dir = (info.type == 0x0000 && info.fsize == 0);
            if (is_dir || !filter_code_only || is_code_visible_file(name_buf)) {
                copy_str(dir_items[dir_count].name, MAX_NAME_LEN, name_buf);
                dir_items[dir_count].size = (unsigned int)info.fsize;
                dir_items[dir_count].is_dir = (unsigned char)is_dir;
                dir_count++;
            }
        }
        memset(&info, 0, sizeof(info));
        ret = Bfile_FindNext_NON_SMEM(fhandle, (char *)u_found, (char *)&info);
    }
    if (fhandle >= 0) Bfile_FindClose(fhandle);

    if (dir_count > 1) {
        int i, j;
        for (i = 1; i < dir_count; i++) {
            file_entry_t tmp = dir_items[i];
            for (j = i - 1; j >= 0 && cmp_entries(&dir_items[j], &tmp) > 0; j--)
                dir_items[j + 1] = dir_items[j];
            dir_items[j + 1] = tmp;
        }
    }

    if (select_name && select_name[0]) {
        int i;
        for (i = 0; i < dir_count; i++) {
            if (strcmp(dir_items[i].name, select_name) == 0) {
                dir_sel = i;
                break;
            }
        }
    }
    if (dir_sel >= dir_count) dir_sel = dir_count > 0 ? dir_count - 1 : 0;
    if (dir_sel < 0) dir_sel = 0;
    if (dir_sel < dir_scroll) dir_scroll = dir_sel;
    if (dir_sel >= dir_scroll + FILE_ROWS)
        dir_scroll = dir_sel - FILE_ROWS + 1;
    if (dir_scroll < 0) dir_scroll = 0;
}

static int fs_file_exists(const char *fullpath)
{
    unsigned short u_path[MAX_PATH_LEN];
    int h;
    Bfile_StrToName_ncpy(u_path, fullpath, MAX_PATH_LEN);
    h = Bfile_OpenFile_OS(u_path, READ, 0);
    if (h >= 0) {
        Bfile_CloseFile_OS(h);
        return 1;
    }
    return 0;
}

static int fs_load_file(const char *fullpath)
{
    unsigned short u_path[MAX_PATH_LEN];
    int h, fsz, i, dst = 0, tabs = 0;

    Bfile_StrToName_ncpy(u_path, fullpath, MAX_PATH_LEN);
    h = Bfile_OpenFile_OS(u_path, READ, 0);
    if (h < 0) return -1;

    fsz = Bfile_GetFileSize_OS(h);
    if (fsz < 0) fsz = 0;
    if (fsz >= MAX_BUF_SIZE - 1) fsz = MAX_BUF_SIZE - 2;

    if (fsz > 0)
        Bfile_ReadFile_OS(h, edit_buf, fsz, 0);
    Bfile_CloseFile_OS(h);

    /* Pass 1 (strictly dst <= i): normalize CRLF -> LF and NUL -> space */
    for (i = 0; i < fsz; i++) {
        char c = edit_buf[i];
        if (c == '\r') {
            if (i + 1 < fsz && edit_buf[i + 1] == '\n') continue;
            c = '\n';
        } else if (c == '\0') {
            c = ' ';
        } else if (c == '\t') {
            tabs++;
        }
        edit_buf[dst++] = c;
    }

    /* Pass 2 (right-to-left so unread bytes are NEVER overwritten):
       expand tabs into 4 spaces within MAX_BUF_SIZE - 1. */
    if (tabs > 0) {
        int max_expand = (MAX_BUF_SIZE - 2 - dst) / 3;
        int expand_cnt = 0;
        int new_len, w;
        if (max_expand < 0) max_expand = 0;
        for (i = 0; i < dst; i++) {
            if (edit_buf[i] == '\t') {
                if (expand_cnt < max_expand) expand_cnt++;
                else edit_buf[i] = ' ';
            }
        }
        new_len = dst + expand_cnt * 3;
        w = new_len - 1;
        for (i = dst - 1; i >= 0; i--) {
            if (edit_buf[i] == '\t') {
                edit_buf[w--] = ' ';
                edit_buf[w--] = ' ';
                edit_buf[w--] = ' ';
                edit_buf[w--] = ' ';
            } else {
                edit_buf[w--] = edit_buf[i];
            }
        }
        dst = new_len;
    }

    edit_buf[dst] = '\0';
    edit_len = dst;
    cursor_pos = 0;
    target_col = 0;
    scroll_line = 0;
    scroll_col = 0;
    sel_anchor = -1;
    select_mode = 0;
    copy_str(open_path, sizeof(open_path), fullpath);
    edit_lang = detect_lang(open_path);
    edit_dirty = 0;
    return 0;
}

static int fs_save_file(const char *fullpath)
{
    unsigned short u_tmp[MAX_PATH_LEN];
    unsigned short u_dst[MAX_PATH_LEN];
    const char *payload = (edit_len > 0) ? edit_buf : "\n";
    size_t sz = (size_t)((edit_len > 0) ? edit_len : 1);
    int h;

    if (str_ends_ci(fullpath, ".g3a")) {
        show_toast("Cannot overwrite .g3a!");
        return -1;
    }

    Bfile_StrToName_ncpy(u_tmp, TEMP_SAVE_FILE, MAX_PATH_LEN);
    Bfile_StrToName_ncpy(u_dst, fullpath, MAX_PATH_LEN);

    /* Atomic root-temp write + rename (avoids Casio subfolder create bug) */
    Bfile_DeleteEntry(u_tmp);
    if (Bfile_CreateEntry_OS(u_tmp, CREATEMODE_FILE, &sz) >= 0) {
        h = Bfile_OpenFile_OS(u_tmp, READWRITE, 0);
        if (h >= 0) {
            Bfile_WriteFile_OS(h, payload, (int)sz);
            Bfile_CloseFile_OS(h);
            Bfile_DeleteEntry(u_dst);
            if (Bfile_RenameEntry(u_tmp, u_dst) >= 0) {
                copy_str(open_path, sizeof(open_path), fullpath);
                edit_lang = detect_lang(open_path);
                edit_dirty = 0;
                return 0;
            }
        }
        Bfile_DeleteEntry(u_tmp);
    }

    /* Fallback direct create */
    Bfile_DeleteEntry(u_dst);
    if (Bfile_CreateEntry_OS(u_dst, CREATEMODE_FILE, &sz) < 0)
        return -1;
    h = Bfile_OpenFile_OS(u_dst, READWRITE, 0);
    if (h < 0) return -1;
    Bfile_WriteFile_OS(h, payload, (int)sz);
    Bfile_CloseFile_OS(h);

    copy_str(open_path, sizeof(open_path), fullpath);
    edit_lang = detect_lang(open_path);
    edit_dirty = 0;
    return 0;
}

/* ------------------------------------------------------------------ */
/* Modal dialogs (Confirm, Text Input, Symbol Palette, Clipboard)      */
/* ------------------------------------------------------------------ */
static int modal_confirm(const char *title, const char *line1, const char *line2)
{
    const char *fk[6] = { "YES", "", "", "", "", "NO" };
    wait_all_released();

    for (;;) {
        int mc = 0, k;
        px_rect(34, 58, 349, 146, COL_WHITE);
        px_box(34, 58, 349, 146, COL_BLACK);
        px_rect(35, 59, 348, 76, COL_BLACK);
        draw_str_n(42, 63, title, 48, COL_WHITE);
        if (line1) draw_str_n(44, 86, line1, 48, SYN_NORMAL);
        if (line2) draw_str_n(44, 102, line2, 48, COL_GUTTER_FG);
        draw_str(44, 124, "EXE/F1 = Yes    EXIT/F6 = Cancel", COL_BADGE_PY);
        draw_fkeys(fk, 1);
        Bdisp_PutDisp_DD();

        k = poll_editor_key(&mc);
        if (k == MK_EXE || k == MK_F1) {
            wait_all_released();
            return 1;
        }
        if (k == MK_EXIT || k == MK_F6) {
            wait_all_released();
            return 0;
        }
        OS_InnerWait_ms(10);
    }
}

/* Returns: 1 = EXE, 2 = F1 (+.c), 3 = F2 (+.py), 0 = cancelled */
static int modal_input_name(const char *title, char *buf, int max_len, int allow_ext_btns)
{
    int cur = (int)strlen(buf);
    int saved_alpha = alpha_mode;
    alpha_mode = ALP_LOCK_LO; /* start in lowercase-lock for effortless typing */
    shift_sticky = 0;
    wait_all_released();

    for (;;) {
        const char *fk[6];
        int mc = 0, k, shf;
        char c;

        fk[0] = allow_ext_btns ? "+.c" : "OK";
        fk[1] = allow_ext_btns ? "+.py" : "";
        fk[2] = "a/A/1";
        fk[3] = "_";
        fk[4] = ".";
        fk[5] = "CANCEL";

        px_rect(24, 52, 359, 152, COL_WHITE);
        px_box(24, 52, 359, 152, COL_BLACK);
        px_rect(25, 53, 358, 70, COL_BLACK);
        draw_str_n(32, 57, title, 42, COL_WHITE);
        draw_mod_badge(354);

        px_rect(34, 84, 349, 104, COL_GUTTER_BG);
        px_box(34, 84, 349, 104, COL_BLACK);

        {
            int start = (cur > 48) ? (cur - 48) : 0;
            int cx = 40 + (cur - start) * FONT_W;
            draw_str_n(40, 89, buf + start, 49, COL_BLACK);
            px_rect(cx, 87, cx + 1, 100, COL_CURSOR);
        }

        if (allow_ext_btns)
            draw_str(34, 114, "Type name, then F1 (.c), F2 (.py) or EXE", COL_GUTTER_FG);
        else
            draw_str(34, 114, "EXE = Confirm   F3 = Mode   EXIT = Cancel", COL_GUTTER_FG);
        draw_str(34, 129, "ALPHA toggles letters/digits, DEL erases", COL_GUTTER_FG);

        draw_fkeys(fk, 0);
        Bdisp_PutDisp_DD();

        k = poll_editor_key(&mc);
        if (!k) {
            OS_InnerWait_ms(10);
            continue;
        }
        shf = current_shift();

        if (k == MK_EXIT || k == MK_F6) {
            alpha_mode = saved_alpha;
            wait_all_released();
            return 0;
        }
        if (k == MK_EXE || (!allow_ext_btns && k == MK_F1)) {
            alpha_mode = saved_alpha;
            wait_all_released();
            return 1;
        }
        if (allow_ext_btns && (k == MK_F1 || k == MK_F2)) {
            const char *ext = (k == MK_F1) ? ".c" : ".py";
            if (!buf[0]) {
                show_toast("Type a file name first");
                continue;
            }
            if (!str_ends_ci(buf, ".c") && !str_ends_ci(buf, ".py") &&
                !str_ends_ci(buf, ".h") && !str_ends_ci(buf, ".txt")) {
                if ((int)strlen(buf) + (int)strlen(ext) < max_len - 1)
                    strcat(buf, ext);
            }
            alpha_mode = saved_alpha;
            wait_all_released();
            return (k == MK_F1) ? 2 : 3;
        }
        if (k == MK_F3) {
            if (alpha_mode == ALP_LOCK_LO) alpha_mode = ALP_LOCK_UP;
            else if (alpha_mode == ALP_LOCK_UP) alpha_mode = ALP_OFF;
            else alpha_mode = ALP_LOCK_LO;
            continue;
        }
        if (k == MK_F4) c = '_';
        else if (k == MK_F5) c = '.';
        else c = translate_key_char(k, shf, alpha_mode);

        if (k == MK_LEFT) {
            if (cur > 0) cur--;
            consume_one_shot_mods();
        } else if (k == MK_RIGHT) {
            if (cur < (int)strlen(buf)) cur++;
            consume_one_shot_mods();
        } else if (k == MK_DEL) {
            int len = (int)strlen(buf);
            if (cur > 0) {
                memmove(buf + cur - 1, buf + cur, (size_t)(len - cur + 1));
                cur--;
            }
            consume_one_shot_mods();
        } else if (c && (unsigned char)c > 32 &&
                   !strchr("\\/\"*?:<>|", c)) {
            int len = (int)strlen(buf);
            if (len < max_len - 1) {
                memmove(buf + cur + 1, buf + cur, (size_t)(len - cur + 1));
                buf[cur++] = c;
            }
            consume_one_shot_mods();
        }
    }
}

/* ------------------------------------------------------------------ */
/* Editor text buffer navigation & editing primitives                  */
/* ------------------------------------------------------------------ */
static int line_start_of(int pos)
{
    while (pos > 0 && edit_buf[pos - 1] != '\n') pos--;
    return pos;
}

static int line_end_of(int pos)
{
    while (pos < edit_len && edit_buf[pos] != '\n') pos++;
    return pos;
}

static void get_cursor_line_col(int *out_line, int *out_col)
{
    int i, ln = 0, ls = 0;
    for (i = 0; i < cursor_pos && i < edit_len; i++) {
        if (edit_buf[i] == '\n') {
            ln++;
            ls = i + 1;
        }
    }
    *out_line = ln;
    *out_col = cursor_pos - ls;
}

static int total_lines(void)
{
    int i, ln = 1;
    for (i = 0; i < edit_len; i++)
        if (edit_buf[i] == '\n') ln++;
    return ln;
}

static int pos_of_line(int target_ln)
{
    int i, ln = 0;
    if (target_ln <= 0) return 0;
    for (i = 0; i < edit_len; i++) {
        if (edit_buf[i] == '\n') {
            ln++;
            if (ln == target_ln) return i + 1;
        }
    }
    return edit_len;
}

static void ensure_cursor_visible(void)
{
    int ln, col;
    get_cursor_line_col(&ln, &col);
    if (ln < scroll_line) scroll_line = ln;
    if (ln >= scroll_line + EDIT_ROWS) scroll_line = ln - EDIT_ROWS + 1;
    if (scroll_line < 0) scroll_line = 0;

    if (col < scroll_col) scroll_col = col;
    if (col >= scroll_col + CODE_COLS - 1)
        scroll_col = col - (CODE_COLS - 4);
    if (scroll_col < 0) scroll_col = 0;
}

static int has_selection(void)
{
    return sel_anchor >= 0 && sel_anchor <= edit_len && sel_anchor != cursor_pos;
}

static void get_selection_range(int *s0, int *s1)
{
    int a = sel_anchor, b = cursor_pos;
    if (a < 0) a = b;
    if (a > edit_len) a = edit_len;
    if (b < 0) b = 0;
    if (b > edit_len) b = edit_len;
    if (a <= b) { *s0 = a; *s1 = b; }
    else        { *s0 = b; *s1 = a; }
}

static void clear_selection(void)
{
    sel_anchor = -1;
    select_mode = 0;
}

static int buf_insert_n(int pos, const char *s, int n)
{
    if (n <= 0) return 1;
    if (pos < 0) pos = 0;
    if (pos > edit_len) pos = edit_len;
    if (edit_len + n >= MAX_BUF_SIZE - 1) {
        show_toast("Buffer full (48 KB)!");
        return 0;
    }
    memmove(edit_buf + pos + n, edit_buf + pos, (size_t)(edit_len - pos + 1));
    memcpy(edit_buf + pos, s, (size_t)n);
    edit_len += n;
    edit_dirty = 1;
    return 1;
}

static void buf_delete_range(int pos, int n)
{
    if (n <= 0 || pos < 0 || pos >= edit_len) return;
    if (pos + n > edit_len) n = edit_len - pos;
    memmove(edit_buf + pos, edit_buf + pos + n, (size_t)(edit_len - pos - n + 1));
    edit_len -= n;
    edit_dirty = 1;
}

static int delete_selection_if_any(void)
{
    if (has_selection()) {
        int s0, s1;
        get_selection_range(&s0, &s1);
        clear_selection();
        buf_delete_range(s0, s1 - s0);
        cursor_pos = s0;
        return 1;
    }
    clear_selection();
    return 0;
}

static void editor_select_all(void)
{
    sel_anchor = 0;
    cursor_pos = edit_len;
    select_mode = 1;
    {
        int ln, col;
        get_cursor_line_col(&ln, &col);
        target_col = col;
    }
    ensure_cursor_visible();
    show_toast(edit_len > 0 ? "Selected entire file" : "File is empty");
}

static void editor_copy_selection(int cut)
{
    int s0, s1, len;
    char msg[48];
    if (!has_selection()) {
        show_toast("Move arrows to select text first");
        return;
    }
    get_selection_range(&s0, &s1);
    len = s1 - s0;
    if (len >= MAX_CLIP_SIZE) len = MAX_CLIP_SIZE - 1;
    memcpy(clip_buf, edit_buf + s0, (size_t)len);
    clip_buf[len] = '\0';
    clip_len = len;
    if (cut) {
        clear_selection();
        buf_delete_range(s0, s1 - s0);
        cursor_pos = s0;
        {
            int ln, col;
            get_cursor_line_col(&ln, &col);
            target_col = col;
        }
        ensure_cursor_visible();
        sprintf(msg, "Cut %d chars", len);
    } else {
        clear_selection();
        sprintf(msg, "Copied %d chars", len);
    }
    show_toast(msg);
}

static void insert_text_at_cursor(const char *s)
{
    int n = (int)strlen(s);
    int sel_len = 0;
    if (has_selection()) {
        int s0, s1;
        get_selection_range(&s0, &s1);
        sel_len = s1 - s0;
    }
    if ((edit_len - sel_len) + n >= MAX_BUF_SIZE - 1) {
        show_toast("Buffer full (48 KB)!");
        return;
    }
    delete_selection_if_any();
    if (buf_insert_n(cursor_pos, s, n)) {
        cursor_pos += n;
        {
            int ln, col;
            get_cursor_line_col(&ln, &col);
            target_col = col;
        }
        ensure_cursor_visible();
    }
}

static void editor_newline_autoindent(void)
{
    int ls, indent = 0;
    char ins[40];
    int n = 0, p;

    delete_selection_if_any();
    ls = line_start_of(cursor_pos);

    while (ls + indent < cursor_pos && edit_buf[ls + indent] == ' ' && indent < 28)
        indent++;

    /* Extra +4 spaces after ':' (Python) or '{' (C) */
    p = cursor_pos - 1;
    while (p >= ls && edit_buf[p] == ' ') p--;
    if (p >= ls && (edit_buf[p] == ':' || edit_buf[p] == '{') && indent <= 28)
        indent += 4;

    ins[n++] = '\n';
    while (n - 1 < indent && n < (int)sizeof(ins) - 1)
        ins[n++] = ' ';
    ins[n] = '\0';
    insert_text_at_cursor(ins);
}

static void editor_tab(int unindent)
{
    if (has_selection()) {
        int s0, s1, first_ls, end_chk, last_ls, ls;
        get_selection_range(&s0, &s1);
        first_ls = line_start_of(s0);
        end_chk = (s1 > first_ls && edit_buf[s1 - 1] == '\n') ? (s1 - 1) : s1;
        last_ls = line_start_of(end_chk);
        ls = first_ls;
        while (ls <= last_ls && ls <= edit_len) {
            if (unindent) {
                int rm = 0;
                while (rm < 4 && ls + rm < edit_len && edit_buf[ls + rm] == ' ')
                    rm++;
                if (rm > 0) {
                    buf_delete_range(ls, rm);
                    last_ls -= rm;
                    s1 -= rm;
                }
            } else {
                int le = line_end_of(ls);
                if (le > ls || first_ls == last_ls) {
                    if (!buf_insert_n(ls, "    ", 4)) break;
                    last_ls += 4;
                    s1 += 4;
                }
            }
            {
                int le = line_end_of(ls);
                if (le >= edit_len || edit_buf[le] != '\n') break;
                ls = le + 1;
            }
        }
        if (s1 < first_ls) s1 = first_ls;
        if (s1 > edit_len) s1 = edit_len;
        sel_anchor = first_ls;
        cursor_pos = s1;
        select_mode = 1;
    } else {
        int ls = line_start_of(cursor_pos);
        clear_selection();
        if (unindent) {
            int rm = 0;
            while (rm < 4 && ls + rm < edit_len && edit_buf[ls + rm] == ' ')
                rm++;
            if (rm > 0) {
                int orig_off = cursor_pos - ls;
                buf_delete_range(ls, rm);
                orig_off -= rm;
                if (orig_off < 0) orig_off = 0;
                cursor_pos = ls + orig_off;
            }
        } else {
            int col = cursor_pos - ls;
            int add = 4 - (col % 4);
            insert_text_at_cursor(add == 1 ? " " : add == 2 ? "  " : add == 3 ? "   " : "    ");
        }
    }
    {
        int ln, col;
        get_cursor_line_col(&ln, &col);
        target_col = col;
    }
    ensure_cursor_visible();
}

static void editor_backspace(int forward_del)
{
    if (has_selection()) {
        delete_selection_if_any();
    } else {
        clear_selection();
        if (forward_del) {
            if (cursor_pos < edit_len)
                buf_delete_range(cursor_pos, 1);
        } else if (cursor_pos > 0) {
            int ls = line_start_of(cursor_pos);
            int col = cursor_pos - ls;
            int all_sp = (col > 0);
            int i;
            for (i = ls; i < cursor_pos; i++)
                if (edit_buf[i] != ' ') { all_sp = 0; break; }
            if (all_sp && col > 0) {
                /* Smart backspace unindents to previous 4-space stop */
                int rm = (col % 4 == 0) ? 4 : (col % 4);
                int del_pos = cursor_pos - rm;
                if (del_pos < 0) { rm += del_pos; del_pos = 0; }
                buf_delete_range(del_pos, rm);
                cursor_pos = del_pos;
            } else {
                int del_pos = cursor_pos - 1;
                buf_delete_range(del_pos, 1);
                cursor_pos = del_pos;
            }
        }
    }
    if (cursor_pos < 0) cursor_pos = 0;
    if (cursor_pos > edit_len) cursor_pos = edit_len;
    {
        int ln, col;
        get_cursor_line_col(&ln, &col);
        target_col = col;
    }
    ensure_cursor_visible();
}

static void editor_move_vert(int delta_lines)
{
    int ln, col, dest_ln, tot = total_lines();
    int ls, le, len;
    get_cursor_line_col(&ln, &col);
    dest_ln = ln + delta_lines;
    if (dest_ln < 0) dest_ln = 0;
    if (dest_ln >= tot) dest_ln = tot - 1;
    ls = pos_of_line(dest_ln);
    le = line_end_of(ls);
    len = le - ls;
    cursor_pos = ls + (target_col < len ? target_col : len);
    ensure_cursor_visible();
}

/* ------------------------------------------------------------------ */
/* Syntax Highlighting for Python (.py) and C / PicoC (.c, .h)         */
/* ------------------------------------------------------------------ */
static const char * const py_keywords[] = {
    "False","None","True","and","as","assert","break","class","continue",
    "def","del","elif","else","except","finally","for","from","global",
    "if","import","in","is","lambda","nonlocal","not","or","pass",
    "raise","return","try","while","with","yield", NULL
};

static const char * const c_keywords[] = {
    "NULL","auto","bool","break","case","char","const","continue",
    "default","do","double","else","enum","extern","false","float",
    "for","goto","if","int","long","register","return","short",
    "signed","sizeof","static","struct","switch","true","typedef",
    "union","unsigned","void","volatile","while", NULL
};

static int is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static int is_ident_char(char c)
{
    return is_ident_start(c) || (c >= '0' && c <= '9');
}

static int match_kw(const char *s, int len, const char * const *list)
{
    int i;
    for (i = 0; list[i]; i++) {
        if ((int)strlen(list[i]) == len && strncmp(s, list[i], (size_t)len) == 0)
            return 1;
    }
    return 0;
}

static void draw_code_line(int y, int ls, int le, int lang, int *in_block_comment)
{
    int len = le - ls;
    const char *s = edit_buf + ls;
    int i = 0;

    while (i < len) {
        unsigned short col = SYN_NORMAL;
        int tok_end = i + 1;

        if (*in_block_comment) {
            col = SYN_COMMENT;
            while (tok_end < len) {
                if (s[tok_end - 1] == '*' && s[tok_end] == '/') {
                    tok_end++;
                    *in_block_comment = 0;
                    break;
                }
                tok_end++;
            }
            if (*in_block_comment) tok_end = len;
        } else if (lang == LANG_PY && s[i] == '#') {
            col = SYN_COMMENT;
            tok_end = len;
        } else if (lang == LANG_C && i + 1 < len && s[i] == '/' && s[i + 1] == '/') {
            col = SYN_COMMENT;
            tok_end = len;
        } else if (lang == LANG_C && i + 1 < len && s[i] == '/' && s[i + 1] == '*') {
            col = SYN_COMMENT;
            tok_end = i + 2;
            *in_block_comment = 1;
            while (tok_end + 1 <= len) {
                if (tok_end < len && s[tok_end - 1] == '*' && s[tok_end] == '/' && tok_end >= i + 3) {
                    tok_end++;
                    *in_block_comment = 0;
                    break;
                }
                if (tok_end == len) break;
                tok_end++;
            }
        } else if (lang == LANG_C && s[i] == '#') {
            col = SYN_PREPROC;
            while (tok_end < len && s[tok_end] == ' ') tok_end++;
            while (tok_end < len && is_ident_char(s[tok_end])) tok_end++;
        } else if (lang == LANG_PY && s[i] == '@') {
            col = SYN_PREPROC;
            while (tok_end < len && is_ident_char(s[tok_end])) tok_end++;
        } else if (s[i] == '"' || s[i] == '\'') {
            char q = s[i];
            col = SYN_STRING;
            while (tok_end < len) {
                if (s[tok_end] == '\\' && tok_end + 1 < len) {
                    tok_end += 2;
                    continue;
                }
                if (s[tok_end] == q) {
                    tok_end++;
                    break;
                }
                tok_end++;
            }
        } else if (s[i] >= '0' && s[i] <= '9') {
            col = SYN_NUMBER;
            while (tok_end < len && (is_ident_char(s[tok_end]) || s[tok_end] == '.'))
                tok_end++;
        } else if (is_ident_start(s[i])) {
            while (tok_end < len && is_ident_char(s[tok_end])) tok_end++;
            if (lang == LANG_PY && match_kw(s + i, tok_end - i, py_keywords))
                col = SYN_KEYWORD;
            else if (lang == LANG_C && match_kw(s + i, tok_end - i, c_keywords))
                col = SYN_KEYWORD;
        } else if (s[i] != ' ') {
            col = SYN_OP;
        }

        for (; i < tok_end; i++) {
            int vis_c = i - scroll_col;
            if (vis_c >= 0 && vis_c < CODE_COLS && s[i] != ' ') {
                draw_char(CODE_X + vis_c * FONT_W, y + 1, s[i], col);
            }
        }
    }
}

/* Compute block-comment state entering scroll_line in C mode */
static int block_comment_before_line(int target_ln)
{
    int i = 0, ln = 0, in_bc = 0;
    if (edit_lang != LANG_C || target_ln <= 0) return 0;
    while (i < edit_len && ln < target_ln) {
        char c = edit_buf[i];
        if (c == '\n') {
            ln++;
            i++;
        } else if (in_bc) {
            if (c == '*' && i + 1 < edit_len && edit_buf[i + 1] == '/') {
                in_bc = 0;
                i += 2;
            } else {
                i++;
            }
        } else if (c == '/' && i + 1 < edit_len && edit_buf[i + 1] == '/') {
            while (i < edit_len && edit_buf[i] != '\n') i++;
        } else if (c == '/' && i + 1 < edit_len && edit_buf[i + 1] == '*') {
            in_bc = 1;
            i += 2;
        } else if (c == '"' || c == '\'') {
            char q = c;
            i++;
            while (i < edit_len && edit_buf[i] != '\n') {
                if (edit_buf[i] == '\\' && i + 1 < edit_len) { i += 2; continue; }
                if (edit_buf[i] == q) { i++; break; }
                i++;
            }
        } else {
            i++;
        }
    }
    return in_bc;
}

/* ------------------------------------------------------------------ */
/* Symbol & Snippet Palette (F3 in EDIT) + Line Clipboard (F6) + Help  */
/* ------------------------------------------------------------------ */
static const char * const sym_grid[24] = {
    ";", ":", "{", "}", "[", "]", "(", ")",
    "<", ">", "=", "!", "&", "|", "#", "\"",
    "'", "\\", "%", "^", "~", "?", "@", "$"
};

static const char * const c_snip_labels[8] = {
    "#include <stdio.h>",
    "#include <fxcg/display.h>",
    "#include <fxcg/keyboard.h>",
    "printf(\"...\\n\");",
    "for (i = 0; i < n; i++)",
    "while (...) { }",
    "if (...) { }",
    "main();"
};
static const char * const c_snip_texts[8] = {
    "#include <stdio.h>\n",
    "#include <fxcg/display.h>\n",
    "#include <fxcg/keyboard.h>\n",
    "printf(\"\");",
    "for (i = 0; i < 10; i++) {\n    ",
    "while (1) {\n    ",
    "if () {\n    ",
    "int main() {\n    return 0;\n}\n\nmain();\n"
};

static const char * const py_snip_labels[8] = {
    "import gint",
    "dclear + dupdate",
    "getkey() loop",
    "pollevent() loop",
    "dtext(...)",
    "drect(...)",
    "def func():",
    "for i in range():"
};
static const char * const py_snip_texts[8] = {
    "import gint\n",
    "gint.dclear(gint.C_WHITE)\ngint.dupdate()\n",
    "ev = gint.getkey()\n",
    "ev = gint.pollevent()\nwhile ev.type != gint.KEYEV_NONE:\n    ev = gint.pollevent()\n",
    "gint.dtext(8, 8, gint.C_BLACK, \"Hello\")\n",
    "gint.drect(10, 10, 60, 40, gint.C_BLUE)\n",
    "def main():\n    ",
    "for i in range(10):\n    "
};

static int key_to_digit_idx(int k)
{
    switch (k) {
    case MK_1: return 0;
    case MK_2: return 1;
    case MK_3: return 2;
    case MK_4: return 3;
    case MK_5: return 4;
    case MK_6: return 5;
    case MK_7: return 6;
    case MK_8: return 7;
    default:   return -1;
    }
}

static void open_symbol_palette(void)
{
    int page = 0; /* 0 = Symbols, 1 = C/PicoC, 2 = PythonExtra */
    int sel = 0;
    wait_all_released();

    for (;;) {
        const char *fk[6] = { "SYMS", "C/PICO", "PYEXT", "", "", "CLOSE" };
        int mc = 0, k, i;

        px_rect(18, 28, 365, 186, COL_WHITE);
        px_box(18, 28, 365, 186, COL_BLACK);
        px_rect(19, 29, 364, 46, COL_BLACK);
        draw_str(26, 33, page == 0 ? "Insert Symbol (Arrows + EXE)" :
                         page == 1 ? "C / PicoC Snippets (1-8 or EXE)" :
                                     "PythonExtra Snippets (1-8 or EXE)", COL_WHITE);

        if (page == 0) {
            /* 3 rows x 8 cols grid of symbols */
            for (i = 0; i < 24; i++) {
                int r = i / 8, c = i % 8;
                int x1 = 28 + c * 41;
                int y1 = 56 + r * 36;
                int is_s = (i == sel);
                px_rect(x1, y1, x1 + 35, y1 + 28, is_s ? COL_BLACK : COL_GUTTER_BG);
                px_box(x1, y1, x1 + 35, y1 + 28, COL_BLACK);
                draw_str(x1 + 15, y1 + 9, sym_grid[i], is_s ? COL_WHITE : COL_BLACK);
            }
            draw_str(28, 168, "Tip: SHIFT + * / + - 0 . , also type { } [ ] : = ;", COL_GUTTER_FG);
        } else {
            const char * const *lbls = (page == 1) ? c_snip_labels : py_snip_labels;
            for (i = 0; i < 8; i++) {
                int y1 = 50 + i * 16;
                int is_s = (i == sel);
                if (is_s) px_rect(24, y1, 359, y1 + 14, COL_SEL_ROW);
                draw_char(28, y1 + 2, (char)('1' + i), is_s ? COL_BADGE_MOD : COL_GUTTER_FG);
                draw_str_n(42, y1 + 2, lbls[i], 50, is_s ? COL_WHITE : SYN_NORMAL);
            }
        }

        draw_fkeys(fk, 1 << page);
        Bdisp_PutDisp_DD();

        k = poll_editor_key(&mc);
        if (!k) { OS_InnerWait_ms(10); continue; }

        if (k == MK_EXIT || k == MK_F6) { wait_all_released(); return; }
        if (k == MK_F1) { page = 0; sel = 0; continue; }
        if (k == MK_F2) { page = 1; sel = 0; continue; }
        if (k == MK_F3) { page = 2; sel = 0; continue; }

        if (page == 0) {
            if (k == MK_LEFT)  sel = (sel + 23) % 24;
            if (k == MK_RIGHT) sel = (sel + 1) % 24;
            if (k == MK_UP)    sel = (sel + 16) % 24;
            if (k == MK_DOWN)  sel = (sel + 8) % 24;
            if (k == MK_EXE) {
                insert_text_at_cursor(sym_grid[sel]);
                wait_all_released();
                return;
            }
        } else {
            const char * const *txts = (page == 1) ? c_snip_texts : py_snip_texts;
            int didx = key_to_digit_idx(k);
            if (k == MK_UP)   sel = (sel + 7) % 8;
            if (k == MK_DOWN) sel = (sel + 1) % 8;
            if (didx >= 0) {
                sel = didx;
                insert_text_at_cursor(txts[sel]);
                wait_all_released();
                return;
            }
            if (k == MK_EXE) {
                insert_text_at_cursor(txts[sel]);
                wait_all_released();
                return;
            }
        }
    }
}

static void clip_copy_line(int cut)
{
    int ls = line_start_of(cursor_pos);
    int le = line_end_of(cursor_pos);
    int len = le - ls;
    if (le < edit_len && edit_buf[le] == '\n') len++; /* include newline */
    if (len >= MAX_CLIP_SIZE) len = MAX_CLIP_SIZE - 1;
    memcpy(clip_buf, edit_buf + ls, (size_t)len);
    clip_buf[len] = '\0';
    clip_len = len;
    clear_selection();
    if (cut) {
        buf_delete_range(ls, len);
        cursor_pos = ls;
        if (cursor_pos > edit_len) cursor_pos = edit_len;
        { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
        ensure_cursor_visible();
        show_toast("Line cut to clipboard");
    } else {
        show_toast("Line copied");
    }
}

static void clip_dup_line(void)
{
    int ls = line_start_of(cursor_pos);
    int le = line_end_of(cursor_pos);
    int len = le - ls;
    if (edit_len + len + 1 >= MAX_BUF_SIZE - 1) {
        show_toast("Buffer full (48 KB)!");
        return;
    }
    clear_selection();
    memmove(edit_buf + le + len + 1, edit_buf + le, (size_t)(edit_len - le + 1));
    edit_buf[le] = '\n';
    memcpy(edit_buf + le + 1, edit_buf + ls, (size_t)len);
    edit_len += len + 1;
    edit_dirty = 1;
    cursor_pos = le + 1;
    { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
    ensure_cursor_visible();
    show_toast("Line duplicated");
}

static void open_clip_menu(void)
{
    const char *fk[6] = { "SEL", "ALL", "LINE", "PASTE", "DUP", "DELLN" };
    wait_all_released();

    for (;;) {
        int mc = 0, k;
        px_rect(26, 38, 357, 174, COL_WHITE);
        px_box(26, 38, 357, 174, COL_BLACK);
        px_rect(27, 39, 356, 56, COL_BLACK);
        draw_str(36, 43, "Select & Clipboard (VARS = quick SEL)", COL_WHITE);
        draw_str(36, 62,  "F1: Start Select at cursor (move arrows)", SYN_KEYWORD);
        draw_str(36, 76,  "F2: Select ALL file   F3: Select Line", SYN_KEYWORD);
        draw_str(36, 92,  clip_len > 0 ? "F4: Paste clipboard at cursor"
                                       : "F4: Paste (clipboard is empty)", SYN_NORMAL);
        draw_str(36, 106, "F5: Duplicate line    F6: Delete line", SYN_NORMAL);
        draw_str(36, 122, "1: Copy line   2: Cut line   EXIT: Close", COL_GUTTER_FG);
        draw_str(36, 138, "In [SEL]: F1=Copy F2=Cut F4=Indent DEL=Erase", COL_BADGE_C);
        draw_str(36, 152, "Shortcut: VARS = Select, SHIFT+VARS = All", COL_GUTTER_FG);
        draw_fkeys(fk, 0);
        Bdisp_PutDisp_DD();

        k = poll_editor_key(&mc);
        if (!k) { OS_InnerWait_ms(10); continue; }
        if (k == MK_EXIT) { wait_all_released(); return; }
        if (k == MK_F1) {
            sel_anchor = cursor_pos;
            select_mode = 1;
            show_toast("Select mode: move arrows");
            wait_all_released();
            return;
        }
        if (k == MK_F2) {
            editor_select_all();
            wait_all_released();
            return;
        }
        if (k == MK_F3) {
            int ls = line_start_of(cursor_pos);
            int le = line_end_of(cursor_pos);
            if (le < edit_len && edit_buf[le] == '\n') le++;
            sel_anchor = ls;
            cursor_pos = le;
            select_mode = 1;
            { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
            ensure_cursor_visible();
            show_toast("Line selected");
            wait_all_released();
            return;
        }
        if (k == MK_F4) {
            if (clip_len > 0) {
                insert_text_at_cursor(clip_buf);
                show_toast("Pasted");
            } else {
                show_toast("Clipboard empty");
            }
            wait_all_released();
            return;
        }
        if (k == MK_F5) { clip_dup_line(); wait_all_released(); return; }
        if (k == MK_F6) {
            int ls = line_start_of(cursor_pos);
            int le = line_end_of(cursor_pos);
            int len = (le < edit_len && edit_buf[le] == '\n') ? (le - ls + 1) : (le - ls);
            clear_selection();
            buf_delete_range(ls, len);
            cursor_pos = ls;
            if (cursor_pos > edit_len) cursor_pos = edit_len;
            { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
            ensure_cursor_visible();
            show_toast("Line deleted");
            wait_all_released();
            return;
        }
        if (k == MK_1) { clip_copy_line(0); wait_all_released(); return; }
        if (k == MK_2) { clip_copy_line(1); wait_all_released(); return; }
    }
}

static void open_help_card(void)
{
    const char *fk[6] = { "", "", "", "", "", "CLOSE" };
    wait_all_released();

    for (;;) {
        int mc = 0, k;
        px_rect(14, 24, 369, 188, COL_WHITE);
        px_box(14, 24, 369, 188, COL_BLACK);
        px_rect(15, 25, 368, 42, COL_BLACK);
        draw_str(24, 29, "PyCEdit -- PythonExtra Keymap Cheat-Sheet", COL_WHITE);

        draw_str(24, 48,  "ALPHA x1 = [alp] 1 char   ALPHA x2 = [aLK] lock", SYN_KEYWORD);
        draw_str(24, 62,  "SHIFT+ALPHA = [ALP] upper  F5 = cycle num/aLK/ALK", SYN_KEYWORD);
        draw_str(24, 78,  "VARS = Select [SEL]   SHIFT+VARS = Select All!", COL_BADGE_C);
        draw_str(24, 92,  "SHIFT + * / + -   ->   {  }  [  ]", SYN_NORMAL);
        draw_str(24, 106, "SHIFT + 0 . ,     ->   :  =  ;", SYN_NORMAL);
        draw_str(24, 120, "SHIFT + ( ) EXP   ->   <  >  !", SYN_NORMAL);
        draw_str(24, 134, "SHIFT + (-) ->    ->   #  '   (^=&  x^2=|)", SYN_NORMAL);
        draw_str(24, 148, "Normal: ( ) , = [->]   _ [(-)]   ^ [^]", SYN_NORMAL);
        draw_str(24, 164, "SHIFT+Arrows = PageUp/Down & Home/End", COL_GUTTER_FG);
        draw_str(24, 176, "FILES tab: F4 switches .py <-> .c in 1 press!", COL_BADGE_PY);

        draw_fkeys(fk, 0);
        Bdisp_PutDisp_DD();

        k = poll_editor_key(&mc);
        if (k == MK_EXIT || k == MK_EXE || k == MK_F6 || k == MK_OPTN) {
            wait_all_released();
            return;
        }
        OS_InnerWait_ms(10);
    }
}

/* ------------------------------------------------------------------ */
/* Rendering: FILES tab & EDIT tab                                     */
/* ------------------------------------------------------------------ */
static void render_files_tab(void)
{
    const char *fk[6] = { "/FILES", "EDIT", "NEW", ".py<>c", "REN", "DEL" };
    char dir_disp[120];
    char hdr[160];
    char rinfo[40];
    int i;

    px_rect(0, 0, VIS_W - 1, HDR_H - 1, COL_HDR_BG);
    format_dir_display(cur_dir, dir_disp, sizeof(dir_disp));
    sprintf(hdr, "PyCEdit (%s)", dir_disp);
    draw_str_n(6, 4, hdr, 40, COL_HDR_FG);

    sprintf(rinfo, "[%s] %d", filter_code_only ? "py/c" : "ALL", dir_count);
    draw_str(VIS_W - 6 - (int)strlen(rinfo) * FONT_W, 4, rinfo, COL_GUTTER_FG);

    px_rect(0, BODY_Y, VIS_W - 1, FKEY_Y - 2, COL_WHITE);

    if (dir_count == 0) {
        draw_str(24, 86, "No .py / .c files in this folder.", COL_GUTTER_FG);
        draw_str(24, 102, "Press F3 (NEW) to create one, or OPTN for ALL.", COL_GUTTER_FG);
    } else {
        for (i = 0; i < FILE_ROWS && (dir_scroll + i) < dir_count; i++) {
            int idx = dir_scroll + i;
            file_entry_t *e = &dir_items[idx];
            int y = BODY_Y + 4 + i * LINE_H;
            int sel = (idx == dir_sel);
            unsigned short fg = sel ? COL_WHITE : SYN_NORMAL;

            if (sel) px_rect(4, y - 1, VIS_W - 9, y + LINE_H - 2, COL_SEL_ROW);

            if (e->is_dir) {
                char dname[96];
                sprintf(dname, "[%s]", e->name);
                draw_str_n(10, y, dname, 42, sel ? COL_WHITE : SYN_KEYWORD);
                draw_str(VIS_W - 44, y, "<DIR>", sel ? COL_GUTTER_BG : COL_GUTTER_FG);
            } else {
                int lang = detect_lang(e->name);
                unsigned short badge_col = (lang == LANG_PY) ? COL_BADGE_PY :
                                           (lang == LANG_C)  ? COL_BADGE_C  : COL_BADGE_DIR;
                const char *btxt = (lang == LANG_PY) ? "PY" :
                                   (lang == LANG_C)  ? "C " : "TX";
                char sz_str[32];

                px_rect(8, y, 22, y + 9, badge_col);
                draw_str(10, y, btxt, COL_WHITE);
                draw_str_n(28, y, e->name, 42, fg);

                if (e->size < 1024)
                    sprintf(sz_str, "%u B", e->size);
                else
                    sprintf(sz_str, "%u.%u K",
                            e->size / 1024, (e->size % 1024) * 10 / 1024);
                draw_str(VIS_W - 12 - (int)strlen(sz_str) * FONT_W, y, sz_str,
                         sel ? COL_GUTTER_BG : COL_GUTTER_FG);
            }
        }

        /* Thin scrollbar on right */
        if (dir_count > FILE_ROWS) {
            int track_y1 = BODY_Y + 4, track_h = FILE_ROWS * LINE_H;
            int bar_h = track_h * FILE_ROWS / dir_count;
            int bar_y;
            if (bar_h < 10) bar_h = 10;
            bar_y = track_y1 + (track_h - bar_h) * dir_scroll / (dir_count - FILE_ROWS);
            px_rect(VIS_W - 5, track_y1, VIS_W - 3, track_y1 + track_h - 1, COL_GUTTER_BG);
            px_rect(VIS_W - 5, bar_y, VIS_W - 3, bar_y + bar_h - 1, COL_GUTTER_FG);
        }
    }

    draw_fkeys(fk, 1 << 0);
    draw_toast_overlay();
}

static void render_edit_tab(void)
{
    int sel_on = (select_mode || has_selection());
    int has_sel = has_selection();
    int shf = current_shift();
    const char *fk_norm[6] = {
        "FILES", shf ? "SV-AS" : "SAVE", "SYM", shf ? "<TAB" : "TAB", "a/A", "CLIP"
    };
    const char *fk_sel[6] = {
        "COPY", "CUT", "PASTE", shf ? "<TAB" : "TAB>", "ALL", "DONE"
    };
    int cur_ln, cur_col;
    int s0 = 0, s1 = 0;
    int in_bc, row, pos;
    char hdr_left[80];
    char pos_str[32];
    const char *bname = open_path[0] ? path_basename(open_path) : "untitled.c";

    if (has_sel) get_selection_range(&s0, &s1);
    get_cursor_line_col(&cur_ln, &cur_col);

    /* PythonExtra top black bar */
    px_rect(0, 0, VIS_W - 1, HDR_H - 1, COL_HDR_BG);
    join_path(hdr_left, sizeof(hdr_left), bname, edit_dirty ? " *" : "");
    draw_str_n(6, 4, hdr_left, 32, COL_HDR_FG);

    if (has_sel)
        sprintf(pos_str, "%dc L%d", s1 - s0, cur_ln + 1);
    else
        sprintf(pos_str, "L%d:C%d", cur_ln + 1, cur_col + 1);
    {
        int rx = VIS_W - 6;
        if (shf || alpha_mode != ALP_OFF) {
            draw_mod_badge(rx);
            rx -= 30;
        }
        if (sel_on) {
            px_rect(rx - 24, 2, rx, HDR_H - 3, COL_BADGE_PY);
            draw_str(rx - 21, 4, "SEL", COL_WHITE);
            rx -= 30;
        }
        draw_str(rx - (int)strlen(pos_str) * FONT_W, 4, pos_str, COL_GUTTER_BG);
    }

    /* Gutter & editor workspace */
    px_rect(0, BODY_Y, GUTTER_W - 1, FKEY_Y - 2, COL_GUTTER_BG);
    px_rect(GUTTER_W, BODY_Y, GUTTER_W, FKEY_Y - 2, COL_BORDER);
    px_rect(GUTTER_W + 1, BODY_Y, VIS_W - 1, FKEY_Y - 2, COL_WHITE);

    in_bc = block_comment_before_line(scroll_line);
    pos = pos_of_line(scroll_line);

    for (row = 0; row < EDIT_ROWS; row++) {
        int ln = scroll_line + row;
        int y = BODY_Y + row * LINE_H;

        if (pos <= edit_len && (ln == 0 || pos > 0 || edit_len >= 0)) {
            int ls, le;
            char lnum[8];
            if (ln >= total_lines()) break;

            ls = pos;
            le = line_end_of(ls);

            if (ln == cur_ln && !has_sel) {
                px_rect(GUTTER_W + 1, y, VIS_W - 1, y + LINE_H - 1, COL_CURLINE);
            }

            if (has_sel) {
                int hi_end = (le < edit_len && edit_buf[le] == '\n') ? (le + 1) : le;
                int ov0 = (s0 > ls) ? s0 : ls;
                int ov1 = (s1 < hi_end) ? s1 : hi_end;
                if (ov0 < ov1) {
                    int c0 = (ov0 - ls) - scroll_col;
                    int c1 = (ov1 - ls) - scroll_col;
                    if (c0 < 0) c0 = 0;
                    if (c1 > CODE_COLS) c1 = CODE_COLS;
                    if (c0 < c1) {
                        px_rect(CODE_X + c0 * FONT_W, y,
                                CODE_X + c1 * FONT_W - 1, y + LINE_H - 1,
                                COL_SEL_BG);
                    }
                }
            }

            fmt_lnum4(lnum, ln + 1);
            draw_str(1, y + 1, lnum, (ln == cur_ln) ? COL_GUTTER_CUR : COL_GUTTER_FG);

            draw_code_line(y, ls, le, edit_lang, &in_bc);

            if (ln == cur_ln) {
                int vis_c = cur_col - scroll_col;
                if (vis_c >= 0 && vis_c < CODE_COLS) {
                    int cx = CODE_X + vis_c * FONT_W;
                    px_rect(cx, y + 1, cx + 1, y + LINE_H - 2, COL_CURSOR);
                }
            }

            pos = (le < edit_len && edit_buf[le] == '\n') ? (le + 1) : (edit_len + 1);
        }
    }

    if (sel_on)
        draw_fkeys(fk_sel, has_sel ? (1 << 0) : 0);
    else
        draw_fkeys(fk_norm, edit_dirty ? (1 << 1) : 0);
    draw_toast_overlay();
}

/* ------------------------------------------------------------------ */
/* Actions in FILES tab (.py <-> .c switcher, New, Rename, Delete)     */
/* ------------------------------------------------------------------ */
static void action_toggle_py_c(void)
{
    file_entry_t *e;
    char old_full[MAX_PATH_LEN];
    char new_name[MAX_NAME_LEN];
    char new_full[MAX_PATH_LEN];
    unsigned short u_old[MAX_PATH_LEN];
    unsigned short u_new[MAX_PATH_LEN];
    size_t nlen;

    if (dir_count <= 0) return;
    e = &dir_items[dir_sel];
    if (e->is_dir) {
        show_toast("Select a .py or .c file");
        return;
    }

    nlen = strlen(e->name);
    if (str_ends_ci(e->name, ".py")) {
        if (nlen - 3 + 2 >= sizeof(new_name)) return;
        memcpy(new_name, e->name, nlen - 3);
        strcpy(new_name + nlen - 3, ".c");
    } else if (str_ends_ci(e->name, ".c")) {
        if (nlen - 2 + 3 >= sizeof(new_name)) return;
        memcpy(new_name, e->name, nlen - 2);
        strcpy(new_name + nlen - 2, ".py");
    } else {
        show_toast("Only .py and .c toggle with F4");
        return;
    }

    join_path(old_full, sizeof(old_full), cur_dir, e->name);
    join_path(new_full, sizeof(new_full), cur_dir, new_name);

    if (fs_file_exists(new_full)) {
        char msg[96];
        join_path(msg, sizeof(msg), new_name, " already exists!");
        if (!modal_confirm("Overwrite File?", msg, "Replace with renamed file?"))
            return;
        Bfile_StrToName_ncpy(u_new, new_full, MAX_PATH_LEN);
        Bfile_DeleteEntry(u_new);
    }

    Bfile_StrToName_ncpy(u_old, old_full, MAX_PATH_LEN);
    Bfile_StrToName_ncpy(u_new, new_full, MAX_PATH_LEN);

    if (Bfile_RenameEntry(u_old, u_new) >= 0) {
        char ok_msg[96];
        if (strcmp(open_path, old_full) == 0) {
            copy_str(open_path, sizeof(open_path), new_full);
            edit_lang = detect_lang(open_path);
        }
        refresh_dir(new_name);
        join_path(ok_msg, sizeof(ok_msg), "Renamed -> ", new_name);
        show_toast(ok_msg);
    } else {
        show_toast("Rename failed!");
    }
}

static int guard_unsaved_before_replace(void)
{
    if (!edit_dirty) return 1;
    if (modal_confirm("Unsaved Changes",
                      "Discard current unsaved edits?",
                      "Press EXIT to go back and F2 SAVE")) {
        return 1;
    }
    return 0;
}

static void action_new_file(void)
{
    char name[MAX_NAME_LEN] = "";
    char full[MAX_PATH_LEN];
    int r;

    if (!guard_unsaved_before_replace()) return;
    r = modal_input_name("New File (.c / .py)", name, sizeof(name), 1);
    if (!r || !name[0]) return;

    if (!strchr(name, '.')) {
        if (strlen(name) + 2 < sizeof(name) - 1)
            strcat(name, ".c");
    }
    if (str_ends_ci(name, ".g3a")) {
        show_toast(".g3a cannot be created");
        return;
    }

    join_path(full, sizeof(full), cur_dir, name);
    if (fs_file_exists(full)) {
        if (!modal_confirm("File Exists", name, "Open existing file instead?"))
            return;
        if (fs_load_file(full) == 0)
            active_tab = TAB_EDIT;
        return;
    }

    /* Pre-fill helpful minimal header or clean empty file */
    edit_buf[0] = '\0';
    edit_len = 0;
    cursor_pos = 0;
    target_col = 0;
    scroll_line = 0;
    scroll_col = 0;
    clear_selection();
    copy_str(open_path, sizeof(open_path), full);
    edit_lang = detect_lang(open_path);

    if (fs_save_file(full) == 0) {
        refresh_dir(name);
        active_tab = TAB_EDIT;
        show_toast("Created new file");
    } else {
        show_toast("Create failed!");
    }
}

static void action_rename_file(void)
{
    file_entry_t *e;
    char new_name[MAX_NAME_LEN];
    char old_full[MAX_PATH_LEN];
    char new_full[MAX_PATH_LEN];
    unsigned short u_old[MAX_PATH_LEN], u_new[MAX_PATH_LEN];

    if (dir_count <= 0) return;
    e = &dir_items[dir_sel];
    if (e->is_dir) {
        show_toast("Select a file to rename");
        return;
    }
    copy_str(new_name, sizeof(new_name), e->name);

    if (!modal_input_name("Rename File", new_name, sizeof(new_name), 1))
        return;
    if (!new_name[0] || strcmp(new_name, e->name) == 0) return;

    join_path(old_full, sizeof(old_full), cur_dir, e->name);
    join_path(new_full, sizeof(new_full), cur_dir, new_name);

    if (fs_file_exists(new_full)) {
        if (!modal_confirm("Overwrite?", new_name, "Target already exists. Replace?"))
            return;
        Bfile_StrToName_ncpy(u_new, new_full, MAX_PATH_LEN);
        Bfile_DeleteEntry(u_new);
    }

    Bfile_StrToName_ncpy(u_old, old_full, MAX_PATH_LEN);
    Bfile_StrToName_ncpy(u_new, new_full, MAX_PATH_LEN);
    if (Bfile_RenameEntry(u_old, u_new) >= 0) {
        if (strcmp(open_path, old_full) == 0) {
            copy_str(open_path, sizeof(open_path), new_full);
            edit_lang = detect_lang(open_path);
        }
        refresh_dir(new_name);
        show_toast("Renamed");
    } else {
        show_toast("Rename failed!");
    }
}

static void action_delete_file(void)
{
    file_entry_t *e;
    char full[MAX_PATH_LEN];
    unsigned short u_path[MAX_PATH_LEN];

    if (dir_count <= 0) return;
    e = &dir_items[dir_sel];
    if (e->is_dir) {
        show_toast("Cannot delete folder");
        return;
    }
    if (str_ends_ci(e->name, ".g3a")) {
        show_toast("Cannot delete .g3a add-in");
        return;
    }
    if (!modal_confirm("Delete File?", e->name, "This cannot be undone."))
        return;

    join_path(full, sizeof(full), cur_dir, e->name);
    Bfile_StrToName_ncpy(u_path, full, MAX_PATH_LEN);
    if (Bfile_DeleteEntry(u_path) >= 0) {
        if (strcmp(open_path, full) == 0) {
            open_path[0] = '\0';
            edit_buf[0] = '\0';
            edit_len = 0;
            cursor_pos = 0;
            target_col = 0;
            scroll_line = 0;
            scroll_col = 0;
            clear_selection();
            edit_dirty = 0;
        }
        refresh_dir(NULL);
        show_toast("Deleted");
    } else {
        show_toast("Delete failed!");
    }
}

static void action_open_selected(void)
{
    file_entry_t *e;
    if (dir_count <= 0) return;
    e = &dir_items[dir_sel];

    if (e->is_dir) {
        if (strcmp(e->name, "..") == 0) {
            size_t len = strlen(cur_dir);
            if (len > strlen(SMEM_ROOT)) {
                cur_dir[len - 1] = '\0';
                char *slash = strrchr(cur_dir, '\\');
                if (slash) slash[1] = '\0';
                else strcpy(cur_dir, SMEM_ROOT);
            }
        } else {
            if (strlen(cur_dir) + strlen(e->name) + 2 < sizeof(cur_dir)) {
                strcat(cur_dir, e->name);
                strcat(cur_dir, "\\");
            }
        }
        dir_sel = 0;
        dir_scroll = 0;
        refresh_dir(NULL);
    } else {
        char full[MAX_PATH_LEN];
        if (str_ends_ci(e->name, ".g3a")) {
            show_toast("Cannot edit binary .g3a");
            return;
        }
        join_path(full, sizeof(full), cur_dir, e->name);
        if (strcmp(full, open_path) == 0) {
            active_tab = TAB_EDIT;
            return;
        }
        if (!guard_unsaved_before_replace()) return;
        if (fs_load_file(full) == 0) {
            active_tab = TAB_EDIT;
        } else {
            show_toast("Failed to open file!");
        }
    }
}

static void action_save_current(int save_as)
{
    if (!open_path[0] || save_as) {
        char name[MAX_NAME_LEN] = "";
        char full[MAX_PATH_LEN];
        if (open_path[0])
            copy_str(name, sizeof(name), path_basename(open_path));
        if (!modal_input_name("Save File As", name, sizeof(name), 1) || !name[0])
            return;
        if (!strchr(name, '.') && strlen(name) + 2 < sizeof(name) - 1)
            strcat(name, ".c");
        join_path(full, sizeof(full), cur_dir, name);
        if (strcmp(full, open_path) != 0 && fs_file_exists(full)) {
            if (!modal_confirm("Overwrite?", name, "Replace existing file?"))
                return;
        }
        if (fs_save_file(full) == 0) {
            refresh_dir(name);
            show_toast("Saved!");
        } else {
            show_toast("Save error!");
        }
    } else {
        if (fs_save_file(open_path) == 0) {
            refresh_dir(path_basename(open_path));
            show_toast("Saved!");
        } else {
            show_toast("Save error!");
        }
    }
}

/* ------------------------------------------------------------------ */
/* Key dispatchers                                                     */
/* ------------------------------------------------------------------ */
static void handle_files_key(int k)
{
    int shf = current_shift();
    consume_one_shot_mods();

    switch (k) {
    case MK_UP:
        if (dir_count > 0) {
            dir_sel = shf ? (dir_sel - FILE_ROWS) : (dir_sel - 1);
            if (dir_sel < 0) dir_sel = 0;
            if (dir_sel < dir_scroll) dir_scroll = dir_sel;
        }
        break;
    case MK_DOWN:
        if (dir_count > 0) {
            dir_sel = shf ? (dir_sel + FILE_ROWS) : (dir_sel + 1);
            if (dir_sel >= dir_count) dir_sel = dir_count - 1;
            if (dir_sel >= dir_scroll + FILE_ROWS)
                dir_scroll = dir_sel - FILE_ROWS + 1;
        }
        break;
    case MK_EXE:
        action_open_selected();
        break;
    case MK_EXIT:
        if (strcmp(cur_dir, SMEM_ROOT) != 0) {
            size_t len = strlen(cur_dir);
            cur_dir[len - 1] = '\0';
            {
                char *slash = strrchr(cur_dir, '\\');
                if (slash) slash[1] = '\0';
                else strcpy(cur_dir, SMEM_ROOT);
            }
            dir_sel = 0;
            dir_scroll = 0;
            refresh_dir(NULL);
        }
        break;
    case MK_F1:
        refresh_dir(NULL);
        break;
    case MK_F2:
        if (dir_count > 0 && !dir_items[dir_sel].is_dir &&
            !str_ends_ci(dir_items[dir_sel].name, ".g3a")) {
            action_open_selected();
        } else {
            active_tab = TAB_EDIT;
        }
        break;
    case MK_F3:
        action_new_file();
        break;
    case MK_F4:
        action_toggle_py_c();
        break;
    case MK_F5:
        action_rename_file();
        break;
    case MK_F6:
        action_delete_file();
        break;
    case MK_OPTN:
        filter_code_only = !filter_code_only;
        dir_sel = 0;
        dir_scroll = 0;
        refresh_dir(NULL);
        show_toast(filter_code_only ? "Filter: .py / .c / .txt" : "Filter: ALL files");
        break;
    case MK_VARS:
        open_help_card();
        break;
    case MK_MENU:
        if (!edit_dirty || modal_confirm("Leave to Main Menu?",
                                         "Unsaved changes in editor!",
                                         "Leave without saving?"))
            quit_requested = 1;
        break;
    default:
        break;
    }
}

static void handle_edit_key(int k)
{
    int shf = current_shift();
    int sel_on = (select_mode || has_selection());
    char ch;

    if (k == MK_VARS) {
        consume_one_shot_mods();
        if (shf) {
            editor_select_all();
        } else if (sel_on) {
            clear_selection();
            show_toast("Selection cleared");
        } else {
            sel_anchor = cursor_pos;
            select_mode = 1;
            show_toast("Select mode: move arrows");
        }
        return;
    }
    if (k == MK_OPTN) {
        consume_one_shot_mods();
        open_help_card();
        return;
    }

    if (sel_on) {
        if (k == MK_F1) {
            consume_one_shot_mods();
            editor_copy_selection(0);
            return;
        }
        if (k == MK_F2) {
            consume_one_shot_mods();
            editor_copy_selection(1);
            return;
        }
        if (k == MK_F3) {
            consume_one_shot_mods();
            if (clip_len > 0) {
                insert_text_at_cursor(clip_buf);
                show_toast("Pasted");
            } else {
                show_toast("Clipboard empty");
            }
            return;
        }
        if (k == MK_F4) {
            consume_one_shot_mods();
            editor_tab(shf);
            return;
        }
        if (k == MK_F5) {
            consume_one_shot_mods();
            if (shf) {
                cursor_pos = (cursor_pos > 0) ? 0 : edit_len;
                { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
                ensure_cursor_visible();
            } else {
                editor_select_all();
            }
            return;
        }
        if (k == MK_F6 || k == MK_EXIT) {
            consume_one_shot_mods();
            clear_selection();
            return;
        }
    } else {
        if (k == MK_F1) {
            consume_one_shot_mods();
            active_tab = TAB_FILES;
            refresh_dir(open_path[0] ? path_basename(open_path) : NULL);
            return;
        }
        if (k == MK_F2) {
            consume_one_shot_mods();
            action_save_current(shf);
            return;
        }
        if (k == MK_F3) {
            consume_one_shot_mods();
            open_symbol_palette();
            return;
        }
        if (k == MK_F4) {
            consume_one_shot_mods();
            editor_tab(shf);
            return;
        }
        if (k == MK_F5) {
            consume_one_shot_mods();
            if (shf) {
                /* Jump to top/bottom quick toggle */
                cursor_pos = (cursor_pos > 0) ? 0 : edit_len;
                target_col = 0;
                ensure_cursor_visible();
                show_toast(cursor_pos == 0 ? "Top of file" : "End of file");
            } else {
                if (alpha_mode == ALP_LOCK_LO) alpha_mode = ALP_LOCK_UP;
                else if (alpha_mode == ALP_LOCK_UP) alpha_mode = ALP_OFF;
                else alpha_mode = ALP_LOCK_LO;
            }
            return;
        }
        if (k == MK_F6) {
            consume_one_shot_mods();
            open_clip_menu();
            return;
        }
        if (k == MK_EXIT) {
            consume_one_shot_mods();
            active_tab = TAB_FILES;
            refresh_dir(open_path[0] ? path_basename(open_path) : NULL);
            return;
        }
    }

    if (k == MK_MENU) {
        consume_one_shot_mods();
        if (!edit_dirty || modal_confirm("Leave to Main Menu?",
                                         "Unsaved changes in editor!",
                                         "Leave without saving?"))
            quit_requested = 1;
        return;
    }

    /* Cursor navigation (extends selection when select_mode == 1) */
    if (k == MK_LEFT) {
        consume_one_shot_mods();
        if (!select_mode && has_selection()) {
            int s0, s1;
            get_selection_range(&s0, &s1);
            clear_selection();
            cursor_pos = s0;
        } else if (shf) {
            int ls = line_start_of(cursor_pos);
            int ind = ls;
            while (ind < edit_len && edit_buf[ind] == ' ') ind++;
            cursor_pos = (cursor_pos == ind) ? ls : ind;
        } else if (cursor_pos > 0) {
            cursor_pos--;
        }
        { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
        ensure_cursor_visible();
        return;
    }
    if (k == MK_RIGHT) {
        consume_one_shot_mods();
        if (!select_mode && has_selection()) {
            int s0, s1;
            get_selection_range(&s0, &s1);
            clear_selection();
            cursor_pos = s1;
        } else if (shf) {
            cursor_pos = line_end_of(cursor_pos);
        } else if (cursor_pos < edit_len) {
            cursor_pos++;
        }
        { int l, c; get_cursor_line_col(&l, &c); target_col = c; }
        ensure_cursor_visible();
        return;
    }
    if (k == MK_UP) {
        consume_one_shot_mods();
        if (!select_mode) clear_selection();
        editor_move_vert(shf ? -14 : -1);
        return;
    }
    if (k == MK_DOWN) {
        consume_one_shot_mods();
        if (!select_mode) clear_selection();
        editor_move_vert(shf ? 14 : 1);
        return;
    }
    if (k == MK_DEL) {
        consume_one_shot_mods();
        editor_backspace(shf);
        return;
    }
    if (k == MK_EXE) {
        consume_one_shot_mods();
        editor_newline_autoindent();
        return;
    }

    /* Printable character via PythonExtra keymap */
    ch = translate_key_char(k, shf, alpha_mode);
    if (ch) {
        char s[2] = { ch, '\0' };
        consume_one_shot_mods();
        insert_text_at_cursor(s);
    }
}

/* ------------------------------------------------------------------ */
/* Hardware-verified clean MENU exit (from Wireworld v1.51)            */
/* ------------------------------------------------------------------ */
#ifndef HOST_TEST
static unsigned int set_menu_return_flag(unsigned int enabled)
{
    int addr, addr2;
    addr = *(unsigned char *)0x80020071;
    addr = (addr + 1) * 4 + 0x80020070;
    addr = *(unsigned int *)addr;
    if (addr < (int)0x80020070 || addr >= (int)0x81000000) return 0x101;
    addr += 0x1E99 * 4;
    if (addr < (int)0x80020070 || addr >= (int)0x81000000) return 0x103;
    addr = *(unsigned int *)addr;
    if (addr < (int)0x80020070 || addr >= (int)0x81000000) return 0x105;
    if ((*(unsigned char *)addr & 0xF0) == 0xD0) {
        addr2 = *(unsigned char *)(addr + 1);
        addr2 = (((addr2 + 1) * 4) + addr) & ~3;
        if (addr2 < (int)0x80020070 || addr2 >= (int)0x81000000) return 0x107;
        addr = *(unsigned int *)addr2;
        if ((addr & 0xFF000000) != 0x88000000 &&
            (addr & 0xFF000000) != 0x8C000000) return 0x109;
        *(unsigned char *)addr = enabled ? 0 : 3;
    }
    return 0;
}

static void drain_os_keys(void)
{
    int col = 0, row = 0, guard = 0;
    unsigned short keycode = 0;
    while (guard++ < 64) {
        int ev = GetKeyWait_OS(&col, &row, KEYWAIT_HALTOFF_TIMEROFF, 0, 1, &keycode);
        if (ev != KEYREP_KEYEVENT) break;
    }
}

static volatile int menu_timer_id = -1;
static volatile int menu_timer_fired = 0;

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

static os_menu_fn_t find_os_menu_function(void)
{
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
    return 0;
}

static void leave_to_menu(void)
{
    os_menu_fn_t os_menu = find_os_menu_function();

    wait_all_released();

    EnableStatusArea(0);
    DefineStatusAreaFlags(3, SAF_BATTERY | SAF_TEXT | SAF_GLYPH | SAF_ALPHA_SHIFT, 0, 0);
    Cursor_SetFlashOff();
    Bkey_ClrAllFlags();
    EnableGetkeyToMainFunctionReturn();
    set_menu_return_flag(1);
    DrawFrame(COL_WHITE);
    drain_os_keys();

    if (os_menu) {
        os_menu();
    } else {
        int col = 0, row = 0, tries;
        unsigned short keycode = 0;

        for (tries = 0; tries < 3; tries++) {
            menu_timer_fired = 0;
            menu_timer_id = Timer_Install(0, menu_timer_cb, 25);
            if (menu_timer_id > 0) Timer_Start(menu_timer_id);

            GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMERON, 1, 0, &keycode);

            if (menu_timer_id > 0) {
                Timer_Stop(menu_timer_id);
                Timer_Deinstall(menu_timer_id);
                menu_timer_id = -1;
            }
            if (menu_timer_fired) break;
        }

        if (!menu_timer_fired) {
            px_rect(0, 0, VIS_W - 1, VIS_H - 1, COL_BLACK);
            draw_str(60, 96, "PRESS MENU KEY TO LEAVE", COL_WHITE);
            Bdisp_PutDisp_DD();
            GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMERON, 3600, 0, &keycode);
        }
    }

    Bdisp_EnableColor(1);
    EnableStatusArea(3);
    DrawFrame(COL_BLACK);
    wait_all_released();
    drain_os_keys();
}
#else
void host_leave_to_menu(void);
static void leave_to_menu(void) { host_leave_to_menu(); }
#endif

/* ------------------------------------------------------------------ */
/* Main session loop                                                   */
/* ------------------------------------------------------------------ */
static void run_pycedit_session(void)
{
    int dirty_ui = 1;

    quit_requested = 0;
    shift_sticky = 0;
    alpha_mode = ALP_OFF;
    toast_active = 0;

    refresh_dir(NULL);

    while (!quit_requested) {
        int mod_changed = 0;
        int k = poll_editor_key(&mod_changed);

        if (mod_changed) dirty_ui = 1;

        if (k) {
            if (active_tab == TAB_FILES) handle_files_key(k);
            else handle_edit_key(k);
            dirty_ui = 1;
        }

        if (toast_active && RTC_Elapsed_ms(toast_tick, 1800)) {
            toast_active = 0;
            dirty_ui = 1;
        }

        if (dirty_ui) {
            if (active_tab == TAB_FILES) render_files_tab();
            else render_edit_tab();
            Bdisp_PutDisp_DD();
            dirty_ui = 0;
        }

#ifdef HOST_TEST
        if (host_step_after_frame()) break;
#endif
        OS_InnerWait_ms(10);
    }
}

#ifndef HOST_TEST
int main(void)
{
    Bdisp_EnableColor(1);
    EnableStatusArea(3);
    DrawFrame(COL_BLACK);

    vram = (unsigned short *)GetVRAMAddress();
    strcpy(cur_dir, SMEM_ROOT);
    edit_buf[0] = '\0';
    edit_len = 0;

    for (;;) {
        run_pycedit_session();
        leave_to_menu();
    }
}
#endif
