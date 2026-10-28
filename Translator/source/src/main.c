/*
 * Translator -- offline RU<->EN dictionary add-in for Casio fx-CG50.
 *
 * Data:  TDIC.DAT on \fls0\ (FreeDict/WikDict-derived; upstream
 *        declares CC BY-SA 3.0; adaptation CC BY-SA 4.0, see data notice).
 * Search: byte-ordered binary search over plain key index; article bodies
 *         live in raw-deflate blocks decoded on demand (miniz tinfl).
 * UI:     embedded 12x14 font (incl. Cyrillic), on-screen EN/RU keyboard,
 *         list/detail views, F-key bar in PyCEdit/PythonExtra style.
 */
#ifndef HOST_TEST
#include <fxcg/display.h>
#include <fxcg/keyboard.h>
#include <fxcg/file.h>
#include <fxcg/rtc.h>
#include <fxcg/system.h>
#else
#include "host_fxcg.h"
#endif

#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "font_tr.h"
#include "miniz.h"

/* ------------------------------------------------------------------ */
/* Display geometry & palette (CG50, direct VRAM, stride 384)          */
/* ------------------------------------------------------------------ */
#define VRAM_W          384
#define VIS_W           384
#define VIS_H           216

#define RGB565(r,g,b)   ((unsigned short)((((r)&0xF8)<<8)|(((g)&0xFC)<<3)|(((b)&0xF8)>>3)))

#define COL_BLACK       0x0000
#define COL_WHITE       0xFFFF
#define COL_BORDER      RGB565(205, 212, 222)
#define COL_SEL_BG      RGB565(186, 214, 255)
#define COL_ACCENT      RGB565(24,  76,  170)
#define COL_HDR_BADGE   RGB565(40,  110, 220)
#define COL_HDR_STEM    RGB565(230, 130, 20)
#define COL_KEY_BG      RGB565(236, 238, 241)
#define COL_KEY_BD      RGB565(170, 176, 186)
#define COL_KEY_LB      RGB565(40,  46,  56)
#define COL_MUTED       RGB565(120, 130, 145)
#define COL_ERR         RGB565(190, 40,  30)

#define HDR_Y           0
#define HDR_H           16
#define INP_Y           16
#define LIST_Y          34
#define ROW_H           14
#define ROWS            7
#define KB_Y            132
#define FKEY_Y          198

#define CELL_W          TR_FONT_W    /* 9 */
#define CELL_H          TR_FONT_H    /* 14 */

static unsigned short *vram;

/* ------------------------------------------------------------------ */
/* Keycodes (PRGM matrix: col*10+row) -- same table as PyCEdit         */
/* ------------------------------------------------------------------ */
#define MK_F1   79
#define MK_F2   69
#define MK_F3   59
#define MK_F4   49
#define MK_F5   39
#define MK_F6   29
#define MK_SHIFT 78
#define MK_OPTN  68
#define MK_VARS  58
#define MK_MENU  48
#define MK_LEFT  38
#define MK_UP    28
#define MK_ALPHA 77
#define MK_EXIT  47
#define MK_DOWN  37
#define MK_RIGHT 27
#define MK_DEL   44
#define MK_ACON  10
/* Physical A-Z keys, same matrix layout as PyCEdit. */
#define MK_XOT   76
#define MK_LOG   66
#define MK_LN    56
#define MK_SIN   46
#define MK_COS   36
#define MK_TAN   26
#define MK_FRAC  75
#define MK_FD    65
#define MK_LPAR  55
#define MK_RPAR  45
#define MK_COMMA 35
#define MK_STORE 25
#define MK_MUL   43
#define MK_DIV   33
#define MK_ADD   42
#define MK_SUB   32
#define MK_DOT   61
#define MK_EXP   51
#define MK_7     74
#define MK_8     64
#define MK_9     54
#define MK_4     73
#define MK_5     63
#define MK_6     53
#define MK_1     72
#define MK_2     62
#define MK_3     52
#define MK_0     71
#define MK_EXE   31

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
    MK_ALPHA, MK_SHIFT,
    MK_XOT, MK_LOG, MK_LN, MK_SIN, MK_COS, MK_TAN,
    MK_FRAC, MK_FD, MK_LPAR, MK_RPAR, MK_COMMA, MK_STORE,
    MK_MUL, MK_DIV, MK_ADD, MK_SUB, MK_DOT, MK_EXP,
    MK_7, MK_8, MK_9, MK_ACON,
    MK_4, MK_5, MK_6,
    MK_1, MK_2, MK_3,
    MK_0,
    0
};

static int key_rep_ok(int k)
{
    return k == MK_UP || k == MK_DOWN || k == MK_LEFT || k == MK_RIGHT ||
           k == MK_DEL;
}

/* returns keycode, or -1 after ~600ms idle (caller repaints for blink) */
static int poll_key(void)
{
    int held = 0, hold_t = 0;
    int start = RTC_GetTicks();
    for (;;) {
        int i, code = 0;
        for (i = 0; non_mod_keys[i]; i++)
            if (hw_key_down(non_mod_keys[i])) { code = non_mod_keys[i]; break; }
        if (!code) {
            held = 0;
            if (RTC_Elapsed_ms(start, 600)) return -1;
            OS_InnerWait_ms(10);
            continue;
        }
        OS_InnerWait_ms(20);
        if (!hw_key_down(code)) continue;
        if (code != held) { held = code; hold_t = 0; return code; }
        if (!key_rep_ok(code)) { OS_InnerWait_ms(50); continue; }
        hold_t++;
        if (hold_t < 5) { OS_InnerWait_ms(55); continue; }
        OS_InnerWait_ms(70);
        if (hw_key_down(code)) return code;
    }
}

static void wait_all_released(void)
{
    int guard = 0;
    while (guard++ < 1500) {
        int any = 0, i;
        for (i = 0; !any && non_mod_keys[i]; i++)
            if (hw_key_down(non_mod_keys[i])) any = 1;
        if (!any) break;
        OS_InnerWait_ms(2);
    }
}

/* ------------------------------------------------------------------ */
/* Text rendering (UTF-8 -> embedded glyph, monospace 9px cell)        */
/* ------------------------------------------------------------------ */
static const unsigned short *glyph_for(unsigned code)
{
    int i;
    if (code >= 0x20 && code < 0x7F) return tr_glyphs[code - 0x20];
    for (i = 95; i < TR_GLYPH_N; i++)
        if (tr_codes[i] == code) return tr_glyphs[i];
    return tr_glyphs['?' - 0x20];
}

static unsigned utf8_next(const char **p)
{
    unsigned c = (unsigned char)**p;
    int n, i;
    if (c < 0x80) { (*p)++; return c; }
    n = c >= 0xF0 ? 4 : c >= 0xE0 ? 3 : 2;
    c &= (0xFFu >> (n + 1));
    (*p)++;
    for (i = 1; i < n && **p; i++) {
        c = (c << 6) | ((unsigned char)**p & 0x3F);
        (*p)++;
    }
    return c;
}

static int utf8_count(const char *s)
{
    int n = 0;
    while (*s) { utf8_next(&s); n++; }
    return n;
}

static void draw_glyph_at(const unsigned short *g, int x, int y,
                          unsigned short fg)
{
    int ry, rx;
    for (ry = 0; ry < TR_FONT_H; ry++) {
        int py = y + ry;
        unsigned short bits;
        if (py < 0 || py >= VIS_H) continue;
        bits = g[ry];
        if (!bits) continue;
        for (rx = 0; rx < TR_FONT_W; rx++) {
            if (bits & (0x8000 >> rx)) {
                int px = x + rx;
                if (px >= 0 && px < VIS_W)
                    vram[py * VRAM_W + px] = fg;
            }
        }
    }
}

/* draws UTF-8 text starting at (x,y); stops before x_end (pixels) */
static int draw_text(const char *s, int x, int y, unsigned short fg, int x_end)
{
    while (*s && x + TR_FONT_W <= x_end) {
        unsigned code = utf8_next(&s);
        draw_glyph_at(glyph_for(code), x, y, fg);
        x += TR_FONT_W;
    }
    return x;
}

static void draw_text_c(const char *s, int x, int y, int w,
                        unsigned short fg)
{
    int tw = utf8_count(s) * TR_FONT_W;
    draw_text(s, x + (w - tw) / 2, y, fg, x + w);
}

static void draw_text_r(const char *s, int x_right, int y, unsigned short fg)
{
    int tw = utf8_count(s) * TR_FONT_W;
    draw_text(s, x_right - tw, y, fg, x_right);
}

/* ------------------------------------------------------------------ */
/* Primitives                                                          */
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

/* ------------------------------------------------------------------ */
/* Dictionary file layer                                               */
/* ------------------------------------------------------------------ */
#define DICT_PATH       "\\\\fls0\\TDIC.DAT"
#define MATCH_CAP       500
#define QUERY_MAX       30          /* glyphs */
#define KEY_BYTES       64
#define BODY_BYTES      256

static int dict_fd = -1;
static unsigned dict_file_size;

static struct {
    unsigned en_n, ru_n;
    unsigned en_keys, en_recs, en_text, en_blk;
    unsigned ru_keys, ru_recs, ru_text, ru_blk;
    unsigned size, en_rec_n, ru_rec_n, en_bn, ru_bn;
} D;

static int read_at(unsigned off, void *buf, int len)
{
    int r;
    if (dict_fd < 0 || len < 0 || off > dict_file_size ||
        (unsigned)len > dict_file_size - off) return -1;
    r = Bfile_ReadFile_OS(dict_fd, buf, len, (int)off);
    return r == len ? 0 : -1;
}

static unsigned rd_u32(const unsigned char *p)
{
    return p[0] | (p[1] << 8) | (p[2] << 16) | ((unsigned)p[3] << 24);
}
static unsigned rd_u16(const unsigned char *p)
{
    return p[0] | (p[1] << 8);
}

static int dict_open(void)
{
    unsigned short upath[64];
    unsigned char hdr[72];
    Bfile_StrToName_ncpy(upath, DICT_PATH, 64);
    int size;
    dict_fd = Bfile_OpenFile_OS(upath, READ, 0);
    if (dict_fd < 0) return -1;
    size = Bfile_GetFileSize_OS(dict_fd);
    if (size < 72) { Bfile_CloseFile_OS(dict_fd); dict_fd = -1; return -1; }
    dict_file_size = (unsigned)size;
    if (read_at(0, hdr, 72) || memcmp(hdr, "TDIC", 4) || hdr[4] != 2) {
        Bfile_CloseFile_OS(dict_fd);
        dict_fd = -1;
        return -1;
    }
    D.en_n    = rd_u32(hdr + 8);
    D.ru_n    = rd_u32(hdr + 12);
    D.en_keys = rd_u32(hdr + 16);
    D.en_recs = rd_u32(hdr + 20);
    D.en_text = rd_u32(hdr + 24);
    D.en_blk  = rd_u32(hdr + 28);
    D.ru_keys = rd_u32(hdr + 32);
    D.ru_recs = rd_u32(hdr + 36);
    D.ru_text = rd_u32(hdr + 40);
    D.ru_blk  = rd_u32(hdr + 44);
    D.size    = rd_u32(hdr + 48);
    D.en_rec_n = rd_u32(hdr + 52);
    D.ru_rec_n = rd_u32(hdr + 56);
    D.en_bn   = rd_u32(hdr + 60);
    D.ru_bn   = rd_u32(hdr + 64);
    /* Reject incomplete or corrupted files before trusting any offsets. */
    if (D.size != dict_file_size || !D.en_n || !D.ru_n ||
        D.en_keys > D.size || D.en_recs > D.size || D.en_text > D.size ||
        D.en_blk > D.size || D.ru_keys > D.size || D.ru_recs > D.size ||
        D.ru_text > D.size || D.ru_blk > D.size ||
        D.en_n > 200000 || D.ru_n > 200000 ||
        D.en_rec_n > 200000 || D.ru_rec_n > 200000 ||
        D.en_bn > 65535 || D.ru_bn > 65535 ||
        D.en_keys != 72 || D.en_keys + D.en_n * 10u > D.en_recs ||
        D.en_recs + D.en_rec_n * 6u > D.en_text ||
        D.en_text > D.en_blk || D.en_blk + D.en_bn * 8u > D.ru_keys ||
        D.ru_keys + D.ru_n * 10u > D.ru_recs ||
        D.ru_recs + D.ru_rec_n * 6u > D.ru_text ||
        D.ru_text > D.ru_blk || D.ru_blk + D.ru_bn * 8u > D.size) {
        Bfile_CloseFile_OS(dict_fd); dict_fd = -1; return -1;
    }
    return 0;
}

static int dir_sec;                 /* 0 = EN->RU, 1 = RU->EN */

static unsigned sec_keys(void)  { return dir_sec ? D.ru_keys : D.en_keys; }
static unsigned sec_recs(void)  { return dir_sec ? D.ru_recs : D.en_recs; }
static unsigned sec_blk(void)   { return dir_sec ? D.ru_blk  : D.en_blk;  }
static unsigned sec_n(void)     { return dir_sec ? D.ru_n    : D.en_n;    }
static unsigned sec_rec_n(void) { return dir_sec ? D.ru_rec_n : D.en_rec_n; }
static unsigned sec_bn(void)    { return dir_sec ? D.ru_bn   : D.en_bn;   }

/* -- block cache + raw-deflate decode (miniz tinfl, one-shot) -------- */
static unsigned char blk_raw[8192];
static unsigned char blk_comp[8900];
static int blk_sec = -1, blk_id = -1;
static unsigned blk_raw_len;
static tinfl_decompressor blk_decomp;

static int blk_load(int sec, int id)
{
    unsigned char tb[8];
    unsigned coff, cl, rl;
    size_t in_sz, out_sz;
    tinfl_status st;

    if (blk_sec == sec && blk_id == id) return 0;
    if (sec != dir_sec || id < 0 || (unsigned)id >= sec_bn()) return -1;
    if (read_at(sec_blk() + (unsigned)id * 8, tb, 8)) return -1;
    coff = rd_u32(tb); cl = rd_u16(tb + 4); rl = rd_u16(tb + 6);
    if (cl > sizeof(blk_comp) || rl > sizeof(blk_raw)) return -1;
    if (read_at(coff, blk_comp, (int)cl)) return -1;

    memset(&blk_decomp, 0, sizeof(blk_decomp));
    in_sz = cl;
    out_sz = rl;
    st = tinfl_decompress(&blk_decomp, blk_comp, &in_sz,
                          blk_raw, blk_raw, &out_sz,
                          TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF);
    if (st != TINFL_STATUS_DONE || in_sz != cl || out_sz != rl) {
        blk_sec = -1; blk_id = -1; blk_raw_len = 0; return -1;
    }
    blk_sec = sec;
    blk_id = id;
    blk_raw_len = rl;
    return 0;
}

/* key i -> key string + rec id */
static int key_get(int sec, unsigned i, char *out, unsigned *rid)
{
    unsigned char e[10];
    unsigned ko, kl;
    if (sec != dir_sec || i >= sec_n()) return -1;
    if (read_at(sec_keys() + i * 10, e, 10)) return -1;
    ko = rd_u32(e); kl = rd_u16(e + 8);
    *rid = rd_u32(e + 4);
    if (!kl || kl >= KEY_BYTES || *rid >= sec_rec_n()) return -1;
    if (read_at(ko, out, (int)kl)) return -1;
    out[kl] = 0;
    return 0;
}

static int body_get(int sec, unsigned rid, char *out)
{
    unsigned char e[6];
    unsigned blk, off;
    int bl;
    if (rid >= sec_rec_n()) return -1;
    if (read_at(sec_recs() + rid * 6, e, 6)) return -1;
    blk = rd_u16(e); off = rd_u16(e + 2); bl = e[4];
    if (blk_load(sec, (int)blk)) return -1;
    if (bl <= 0 || bl >= BODY_BYTES || off > blk_raw_len ||
        (unsigned)bl > blk_raw_len - off) return -1;
    memcpy(out, blk_raw + off, bl);
    out[bl] = 0;
    return 0;
}

static int match_start, match_count, match_capped, match_morph;
static char g_eff[KEY_BYTES];       /* effective (possibly stemmed) query */

/* Binary lower/upper prefix bounds: O(log n) file reads, even for an empty
   query or a common initial letter. This avoids hundreds of FAT accesses. */
static int prefix_bound(const char *q, int upper)
{
    unsigned lo = 0, hi = sec_n();
    const int ql = (int)strlen(q);
    char kb[KEY_BYTES];
    while (lo < hi) {
        unsigned mid = lo + (hi - lo) / 2, rid;
        int kl, common, c;
        if (key_get(dir_sec, mid, kb, &rid)) return -1;
        kl = (int)strlen(kb);
        common = kl < ql ? kl : ql;
        c = memcmp(kb, q, common);
        if (!c) {
            if (kl < ql) c = -1;      /* key is strictly shorter */
            else c = upper ? -1 : 0;  /* prefix match: after for upper */
        }
        if (c < 0) lo = mid + 1; else hi = mid;
    }
    return (int)lo;
}

static int search_lower_bound(const char *q) { return prefix_bound(q, 0); }

static int count_prefix(int start, const char *pfx)
{
    int upper, n;
    if (start < 0) return 0;
    if (!*pfx) upper = (int)sec_n();
    else upper = prefix_bound(pfx, 1);
    if (upper < start) return 0;
    n = upper - start;
    match_capped = (n > MATCH_CAP);
    return n > MATCH_CAP ? MATCH_CAP : n;
}

static const char *find_stem(const char *q)
{
    static const char *en_suf[] = {
        "ies", "ing", "ed", "es", "ly", "s", 0
    };
    static const char *ru_suf[] = {
        "ами", "ями", "ов", "ев", "ей", "ах", "ях", "ой", "ый",
        "ий", "ая", "яя", "ое", "ее", "ые", "ие", "ом", "ем",
        "ы", "и", "а", "я", "у", "ю", "е", "ь", "й", 0
    };
    const char **suf = dir_sec ? ru_suf : en_suf;
    int len = (int)strlen(q), s;

    for (s = 0; suf[s]; s++) {
        int sl = (int)strlen(suf[s]);
        if (len - sl < 3) continue;
        if (strcmp(q + len - sl, suf[s])) continue;
        strcpy(g_eff, q);
        g_eff[len - sl] = 0;
        {
            int lb = search_lower_bound(g_eff);
            if (lb >= 0 && count_prefix(lb, g_eff) > 0) return g_eff;
        }
    }
    return 0;
}

static void reset_pages(void);

static void do_search(const char *q)
{
    int lb, n;

    match_morph = 0;
    strcpy(g_eff, q);

    lb = *g_eff ? search_lower_bound(g_eff) : 0;
    if (lb < 0) lb = 0;
    n = count_prefix(lb, g_eff);

    if (n == 0 && q[0]) {
        const char *stem = find_stem(q);
        if (stem) {
            match_morph = 1;
            lb = search_lower_bound(stem);
            if (lb < 0) lb = 0;
            n = count_prefix(lb, stem);
        }
    }
    match_start = lb;
    match_count = n;
    reset_pages();
}

/* ------------------------------------------------------------------ */
/* UI state                                                            */
/* ------------------------------------------------------------------ */
enum { V_LIST, V_DETAIL, V_HELP, V_NODICT };
enum { F_LIST, F_KB };

static int view = V_LIST;
static int focus = F_KB;
static int kbd_lang;                /* 0 = EN, 1 = RU */
static int alpha_lock = 1;         /* English physical A-Z; ALPHA toggles */
static char query[KEY_BYTES];
static int q_glyphs;
static int sel;                     /* selection inside matches */
static int gx, gy;                  /* keyboard cursor: first column of key */
static char det_key[KEY_BYTES], det_body[BODY_BYTES];
static int det_top;                 /* first visible wrapped detail line */
static int tick;

/* A page starts at an entry and, for exceptionally long entries, a wrapped
   line within it. The cache is local to a search and is built on demand. */
typedef struct { unsigned short entry, line; } PageStart;
#define PAGE_CACHE_SIZE (MATCH_CAP * 4 + 1)
static PageStart page_starts[PAGE_CACHE_SIZE];
static int page_count, page_no;

static void reset_pages(void)
{
    page_starts[0].entry = page_starts[0].line = 0;
    page_count = 1;
    page_no = 0;
    sel = 0;
}

/* Empty cells extend the previous key: e.g. five cells make ONE space bar.
   They are not drawn or selectable as separate buttons. Each language has
   one space, one backspace, one hyphen and one language-switch key. */
static const char *kb_en[4][10] = {
    { "q","w","e","r","t","y","u","i","o","p" },
    { "a","s","d","f","g","h","j","k","l","-" },
    { "z","x","c","v","b","n","m",",",".","'" },
    { "sp","","","","","<-","","","RU","" }
};
static const char *kb_ru[4][10] = {
    { "й","ц","у","к","е","н","г","ш","щ","з" },
    { "ф","ы","в","а","п","р","о","л","д","ж" },
    { "я","ч","с","м","и","т","ь","б","ю","х" },
    { "ъ","ё","э","sp","","","-","<-","","EN" }
};

static const char *kb_cell(int r, int c)
{
    return (kbd_lang ? kb_ru : kb_en)[r][c];
}

static int kb_owner(int row, int col)
{
    while (col > 0 && !*kb_cell(row, col)) col--;
    return col;
}

static int kb_span(int row, int col)
{
    int span = 1;
    while (col + span < 10 && !*kb_cell(row, col + span)) span++;
    return span;
}

/* ------------------------------------------------------------------ */
/* Query editing                                                       */
/* ------------------------------------------------------------------ */
static void query_add_glyph(const char *g)
{
    int gl = utf8_count(query);
    int bl = (int)strlen(g);
    if (gl >= QUERY_MAX) return;
    if ((int)strlen(query) + bl >= (int)sizeof(query)) return;
    strcat(query, g);
    q_glyphs = gl + 1;
    do_search(query);
    sel = 0;
}

static void query_backspace(void)
{
    int len = (int)strlen(query);
    if (!len) return;
    len--;
    while (len > 0 && (query[len] & 0xC0) == 0x80) len--;
    query[len] = 0;
    q_glyphs = utf8_count(query);
    do_search(query);
    sel = 0;
}

static void query_clear(void)
{
    query[0] = 0;
    q_glyphs = 0;
    do_search(query);
    sel = 0;
}

/* ------------------------------------------------------------------ */
/* Painting                                                            */
/* ------------------------------------------------------------------ */
static void paint_header(void)
{
    char num[16];
    px_rect(0, HDR_Y, VIS_W - 1, HDR_Y + HDR_H - 1, COL_BLACK);
    draw_text("Translator", 3, 1, COL_WHITE, 94);
    px_rect(99, 1, 149, 14, COL_HDR_BADGE);
    draw_text_c(dir_sec ? "RU>EN" : "EN>RU", 99, 1, 51, COL_WHITE);
    px_rect(153, 1, 211, 14, COL_KEY_BD);
    draw_text_c(kbd_lang ? "kbd RU" : "kbd EN", 153, 1, 59, COL_BLACK);
    px_rect(215, 1, 248, 14, COL_KEY_BD);
    draw_text_c(kbd_lang ? "123" : (alpha_lock ? "ABC" : "123"),
                215, 1, 34, COL_BLACK);
    if (focus == F_KB) {
        px_rect(252, 1, 294, 14, COL_KEY_BD);
        draw_text_c("keys", 252, 1, 43, COL_BLACK);
    }
    if (match_capped) strcpy(num, "500+");
    else {
        int v = match_count, n = 0;
        if (!v) num[0] = '0', n = 1;
        else {
            char tmp[8]; int t = 0;
            while (v) { tmp[t++] = '0' + v % 10; v /= 10; }
            while (t) num[n++] = tmp[--t];
        }
        num[n] = 0;
    }
    draw_text_r(num, VIS_W - 3, 1, COL_WHITE);
}

static void paint_input(void)
{
    char q[KEY_BYTES + 2];
    int x_end;
    px_rect(0, INP_Y, VIS_W - 1, INP_Y + 15, COL_WHITE);
    px_box(2, INP_Y + 1, VIS_W - 3, INP_Y + 14, COL_BORDER);
    strcpy(q, query);
    if (tick & 1) strcat(q, "_");
    x_end = match_morph ? 318 : VIS_W - 8;
    draw_text(q, 6, INP_Y + 1, COL_BLACK, x_end);
    if (match_morph) {
        px_rect(322, INP_Y + 1, 377, INP_Y + 14, COL_HDR_STEM);
        draw_text_c("stem", 322, INP_Y + 1, 56, COL_WHITE);
    }
}

static void get_match(int idx, char *key, char *body)
{
    unsigned rid = 0;
    key[0] = body[0] = 0;
    if (idx < 0 || idx >= match_count ||
        key_get(dir_sec, (unsigned)(match_start + idx), key, &rid)) {
        strcpy(body, "read error");
        return;
    }
    if (body_get(dir_sec, rid, body)) strcpy(body, "read error");
}

/* One complete displayed entry: the key is never clipped at an arbitrary
   pixel, and every byte in the stored article remains reachable. */
#define ENTRY_COLS       41
#define ENTRY_TEXT_BYTES (KEY_BYTES + BODY_BYTES + 16)
#define ENTRY_MAX_LINES  ENTRY_TEXT_BYTES  /* at most one line per byte */
#define DETAIL_ROWS      11

typedef struct {
    char text[ENTRY_TEXT_BYTES];
    unsigned short start[ENTRY_MAX_LINES], end[ENTRY_MAX_LINES];
    unsigned short count, key_end, body_start;
} EntryLayout;

/* The app is single-threaded; reuse this scratch area rather than putting a
   large wrapped-layout array on the calculator's stack for every keypress. */
static EntryLayout entry_scratch;

static void layout_entry(EntryLayout *e, const char *key, const char *body)
{
    const char *p;
    int n = 0;
    int has_space = (strchr(key, ' ') != 0);

    /* [phrase] is a UI hint for a multi-word key, not an asserted source
       part of speech. If the stored record already starts with phr., keep
       its own label instead of adding a duplicate. */
    strcpy(e->text, key);
    e->key_end = (unsigned short)strlen(e->text);
    if (has_space && strncmp(body, "phr. ", 5)) strcat(e->text, " [phrase]");
    strcat(e->text, ": ");
    e->body_start = (unsigned short)strlen(e->text);
    strcat(e->text, body);

    p = e->text;
    while (*p && n < ENTRY_MAX_LINES) {
        const char *s = p, *last_space = 0, *end;
        int cols = 0;
        while (*s && cols < ENTRY_COLS) {
            const char *glyph = s;
            utf8_next(&s);
            cols++;
            if (*glyph == ' ') last_space = glyph;
        }
        /* Break at a word boundary if possible. A single longer word is
           split at a UTF-8 glyph boundary, never in the middle of a byte. */
        if (*s && *s != ' ' && last_space && last_space > p) {
            end = last_space;
            s = last_space + 1;
        } else {
            end = s;
        }
        while (*s == ' ') s++;
        e->start[n] = (unsigned short)(p - e->text);
        e->end[n] = (unsigned short)(end - e->text);
        n++;
        p = s;
    }
    e->count = (unsigned short)n;
}

static void draw_entry_line(const EntryLayout *e, int line, int y)
{
    const char *p = e->text + e->start[line];
    const char *end = e->text + e->end[line];
    int x = 6;
    while (p < end && x + CELL_W <= VIS_W - 4) {
        int pos = (int)(p - e->text);
        unsigned code = utf8_next(&p);
        unsigned short col = pos < e->key_end ? COL_ACCENT :
                             pos < e->body_start ? COL_MUTED : COL_BLACK;
        draw_glyph_at(glyph_for(code), x, y, col);
        x += CELL_W;
    }
}

typedef struct { PageStart next; int last_entry, used; } PageInfo;

/* Pack whole articles where possible. An article longer than the seven-line
   viewport continues on the next page. No text or entry is skipped. */
static PageInfo visit_page(PageStart start, int draw)
{
    PageInfo info;
    int idx = start.entry, line = start.line, row = 0;
    info.last_entry = -1;
    if (draw) {
        px_rect(0, INP_Y + 16, VIS_W - 1, KB_Y - 1, COL_WHITE);
        if (!match_count)
            draw_text_c("no matches - try another word", 0, LIST_Y + 1,
                        VIS_W, COL_MUTED);
    }
    while (idx < match_count && row < ROWS) {
        char key[KEY_BYTES], body[BODY_BYTES];
        EntryLayout *e = &entry_scratch;
        int remain, take, j;
        get_match(idx, key, body);
        layout_entry(e, key, body);
        if (line >= e->count) { idx++; line = 0; continue; }
        remain = e->count - line;
        if (line == 0 && row > 0 && remain > ROWS - row) break;
        take = remain < ROWS - row ? remain : ROWS - row;
        for (j = 0; j < take; j++) {
            int y = LIST_Y + (row + j) * ROW_H;
            if (draw) {
                if (idx == sel) {
                    px_rect(0, y, VIS_W - 1, y + ROW_H - 1, COL_SEL_BG);
                    if (focus == F_LIST)
                        px_rect(0, y, 2, y + ROW_H - 1, COL_ACCENT);
                }
                if (j == take - 1 && take == remain)
                    px_rect(0, y + ROW_H - 1, VIS_W - 1,
                            y + ROW_H - 1, COL_BORDER);
                /* Text after the separator preserves the bottom pixels of
                   descenders such as g, p, q, y and Cyrillic letters. */
                draw_entry_line(e, line + j, y);
            }
        }
        info.last_entry = idx;
        row += take;
        line += take;
        if (line >= e->count) { idx++; line = 0; }
    }
    info.next.entry = (unsigned short)idx;
    info.next.line = (unsigned short)line;
    info.used = row;
    return info;
}

static int page_forward(void)
{
    if (page_no + 1 >= page_count) {
        PageInfo info = visit_page(page_starts[page_no], 0);
        if (info.next.entry >= match_count || page_count >= PAGE_CACHE_SIZE)
            return 0;
        page_starts[page_count++] = info.next;
    }
    page_no++;
    sel = page_starts[page_no].entry;
    return 1;
}

static int page_backward(void)
{
    if (page_no <= 0) return 0;
    page_no--;
    sel = page_starts[page_no].entry;
    return 1;
}

static void select_adjacent(int delta)
{
    int target = sel + delta;
    if (target < 0 || target >= match_count) return;
    for (;;) {
        PageInfo info = visit_page(page_starts[page_no], 0);
        if (target < page_starts[page_no].entry) {
            if (!page_backward()) break;
        } else if (target > info.last_entry) {
            if (!page_forward()) break;
        } else {
            sel = target;
            return;
        }
    }
}

static void paint_list(void)
{
    (void)visit_page(page_starts[page_no], 1);
}

static void paint_keyboard(void)
{
    int r, c;
    /* Clear gaps and the strip below the keys when returning from detail. */
    px_rect(0, KB_Y, VIS_W - 1, FKEY_Y - 2, COL_WHITE);
    for (r = 0; r < 4; r++) {
        int y = KB_Y + r * (ROW_H + 1);
        for (c = 0; c < 10; c++) {
            int x, w;
            unsigned short bg = COL_KEY_BG, bd = COL_KEY_BD, lb = COL_KEY_LB;
            const char *lab = kb_cell(r, c);
            if (!*lab) continue;     /* part of the previous wide key */
            x = 2 + c * 38;
            w = kb_span(r, c) * 38 - 2;
            if (focus == F_KB && gx == c && gy == r) {
                bg = COL_SEL_BG; bd = COL_ACCENT; lb = COL_BLACK;
            }
            px_rect(x, y, x + w - 1, y + ROW_H - 1, bg);
            px_box(x, y, x + w - 1, y + ROW_H - 1, bd);
            draw_text_c(lab, x, y, w, lb);
        }
    }
}

static void paint_fkeys(void)
{
    static const char *labels[6];
    char l2[8];
    int i;
    labels[0] = view == V_LIST ? "swap" : "";
    labels[1] = view == V_LIST ? (kbd_lang ? "to EN" : "to RU") : "";
    labels[2] = view == V_LIST ? "clear" : "";
    labels[3] = view == V_LIST ? "help" : "back";
    labels[4] = view == V_HELP ? "" : "pgUp";
    labels[5] = view == V_HELP ? "" : "pgDn";
    px_rect(0, FKEY_Y - 1, VIS_W - 1, FKEY_Y - 1, COL_BORDER);
    px_rect(0, FKEY_Y, VIS_W - 1, VIS_H - 1, COL_WHITE);
    for (i = 0; i < 6; i++) {
        int x1 = i * 64 + 2, x2 = (i + 1) * 64 - 3;
        px_box(x1, FKEY_Y + 2, x2, VIS_H - 2, COL_BORDER);
        strcpy(l2, labels[i]);
        draw_text_c(l2, x1, FKEY_Y + 4, x2 - x1 + 1, COL_ACCENT);
    }
}

static void paint_detail(void)
{
    EntryLayout *e = &entry_scratch;
    int row;
    layout_entry(e, det_key, det_body);
    px_rect(0, INP_Y, VIS_W - 1, FKEY_Y - 2, COL_WHITE);
    for (row = 0; row < DETAIL_ROWS && det_top + row < e->count; row++) {
        int y = INP_Y + 3 + row * ROW_H;
        if (det_top == 0 && row == 0)
            px_rect(0, y, VIS_W - 1, y + ROW_H - 1, COL_SEL_BG);
        draw_entry_line(e, det_top + row, y);
    }
    draw_text_c("up/down scroll  F5/F6 page  EXIT back", 0,
                FKEY_Y - 16, VIS_W, COL_MUTED);
    paint_fkeys();
}

static void paint_help(void)
{
    static const char *lines[] = {
        "Translator - offline RU/EN dictionary",
        "F1 swaps direction and clears query",
        "F2 switches screen keyboard EN/RU",
        "EN: type A-Z using calculator keys",
        "ALPHA: toggle A-Z / numbers (EN)",
        "RU: arrows + EXE choose a key",
        "OPTN: choose keyboard / results",
        "F3 clear   F5/F6 page   DEL erase",
        "EXE: select a key or open a result",
        "n. noun  v. verb  [phrase] multi-word",
        "EXIT: back       MENU: leave"
    };
    int i, y = INP_Y + 4;
    px_rect(0, INP_Y, VIS_W - 1, FKEY_Y - 2, COL_WHITE);
    for (i = 0; i < (int)(sizeof(lines) / sizeof(lines[0])); i++) {
        draw_text(lines[i], 6, y, i == 0 ? COL_ACCENT : COL_BLACK, VIS_W - 6);
        y += ROW_H;
    }
    draw_text_c("[F4/EXIT] back", 0, FKEY_Y - 16, VIS_W, COL_MUTED);
    paint_fkeys();
}

static void paint_nodict(void)
{
    px_rect(0, 0, VIS_W - 1, VIS_H - 1, COL_WHITE);
    px_rect(0, 0, VIS_W - 1, 26, COL_ERR);
    draw_text_c("TDIC.DAT missing or invalid", 0, 6, VIS_W, COL_WHITE);
    draw_text_c("Copy TDIC.DAT together with", 0, 60, VIS_W, COL_BLACK);
    draw_text_c("Translator.g3a to the", 0, 82, VIS_W, COL_BLACK);
    draw_text_c("calculator (\\\\fls0\\).", 0, 104, VIS_W, COL_BLACK);
    draw_text_c("See README_RU.txt", 0, 134, VIS_W, COL_MUTED);
    draw_text_c("[EXIT] back to menu", 0, 190, VIS_W, COL_ACCENT);
}

static void paint(void)
{
    tick++;
    if (view == V_NODICT) { paint_nodict(); Bdisp_PutDisp_DD(); return; }
    if (view == V_HELP) { paint_header(); paint_help(); Bdisp_PutDisp_DD(); return; }
    if (view == V_DETAIL) { paint_header(); paint_detail(); Bdisp_PutDisp_DD(); return; }
    paint_header();
    paint_input();
    paint_list();
    paint_keyboard();
    paint_fkeys();
    Bdisp_PutDisp_DD();
}

/* ------------------------------------------------------------------ */
/* Key handling                                                        */
/* ------------------------------------------------------------------ */
static void kb_press(void)
{
    const char *lab = kb_cell(gy, gx);
    if (!strcmp(lab, "sp")) query_add_glyph(" ");
    else if (!strcmp(lab, "<-")) query_backspace();
    else if (!strcmp(lab, "RU") || !strcmp(lab, "EN")) {
        kbd_lang ^= 1;
        /* Keep focus on the single language key in the new layout. */
        gy = 3;
        gx = kbd_lang ? 9 : 8;
    } else if (*lab) query_add_glyph(lab);
}

static char english_key(int k)
{
    switch (k) {
    case MK_XOT: return 'a'; case MK_LOG: return 'b';
    case MK_LN: return 'c'; case MK_SIN: return 'd';
    case MK_COS: return 'e'; case MK_TAN: return 'f';
    case MK_FRAC: return 'g'; case MK_FD: return 'h';
    case MK_LPAR: return 'i'; case MK_RPAR: return 'j';
    case MK_COMMA: return 'k'; case MK_STORE: return 'l';
    case MK_7: return 'm'; case MK_8: return 'n'; case MK_9: return 'o';
    case MK_4: return 'p'; case MK_5: return 'q'; case MK_6: return 'r';
    case MK_MUL: return 's'; case MK_DIV: return 't';
    case MK_1: return 'u'; case MK_2: return 'v'; case MK_3: return 'w';
    case MK_ADD: return 'x'; case MK_SUB: return 'y'; case MK_0: return 'z';
    case MK_DOT: return ' '; case MK_EXP: return '\'';
    default: return 0;
    }
}

static int handle_key(int k)
{
    char direct;
    if (k == MK_MENU) return 0; /* close dictionary before the OS menu */
    /* F-keys work everywhere (except modal help/detail handled below) */
    if (view == V_HELP) {
        if (k == MK_F4 || k == MK_EXIT || k == MK_EXE) view = V_LIST;
        return 1;
    }
    if (view == V_DETAIL) {
        EntryLayout *e = &entry_scratch;
        layout_entry(e, det_key, det_body);
        if (k == MK_EXIT || k == MK_EXE || k == MK_F4) view = V_LIST;
        else if (k == MK_F5) {
            det_top -= DETAIL_ROWS;
            if (det_top < 0) det_top = 0;
        } else if (k == MK_F6) {
            if (det_top + DETAIL_ROWS < e->count) det_top += DETAIL_ROWS;
        } else if (k == MK_UP) {
            if (det_top > 0) det_top--;
        } else if (k == MK_DOWN) {
            if (det_top + DETAIL_ROWS < e->count) det_top++;
        }
        return 1;
    }

    if (!kbd_lang && alpha_lock && (direct = english_key(k)) != 0) {
        char text[2] = { direct, 0 };
        query_add_glyph(text);
        return 1;
    }
    switch (k) {
    case MK_F1:
        dir_sec ^= 1;
        kbd_lang = dir_sec;  /* swap screen keyboard with the direction */
        focus = F_KB;
        gx = gy = 0;
        query_clear();
        break;
    case MK_F2:
        kbd_lang ^= 1;
        gx = kb_owner(gy, gx);
        break;
    case MK_ALPHA:
        alpha_lock ^= 1;
        break;
    case MK_F3:
    case MK_ACON:
        query_clear();
        break;
    case MK_F4:
        view = V_HELP;
        break;
    case MK_F5:
        (void)page_backward();
        break;
    case MK_F6:
        (void)page_forward();
        break;
    case MK_OPTN:
        focus = (focus == F_LIST) ? F_KB : F_LIST;
        break;
    case MK_UP:
        if (focus == F_KB) {
            if (gy > 0) { gy--; gx = kb_owner(gy, gx); }
        } else select_adjacent(-1);
        break;
    case MK_DOWN:
        if (focus == F_KB) {
            if (gy < 3) { gy++; gx = kb_owner(gy, gx); }
        } else select_adjacent(1);
        break;
    case MK_LEFT:
        if (focus == F_KB) {
            if (gx > 0) gx = kb_owner(gy, gx - 1);
        } else (void)page_backward();
        break;
    case MK_RIGHT:
        if (focus == F_KB) {
            int next = gx + kb_span(gy, gx);
            if (next < 10) gx = next;
        } else (void)page_forward();
        break;
    case MK_EXE:
        if (focus == F_KB) kb_press();
        else if (match_count > 0 && sel < match_count) {
            get_match(sel, det_key, det_body);
            det_top = 0;
            view = V_DETAIL;
        }
        break;
    case MK_DEL:
        query_backspace();
        break;
    case MK_EXIT:
        if (focus == F_KB || view != V_LIST) focus = F_LIST;
        else return 0;                      /* leave add-in */
        break;
    case MK_0: case MK_1: case MK_2: case MK_3: case MK_4:
    case MK_5: case MK_6: case MK_7: case MK_8: case MK_9: {
        /* matrix codes are not contiguous: map explicitly */
        static const int dk[10] = {
            MK_0, MK_1, MK_2, MK_3, MK_4, MK_5, MK_6, MK_7, MK_8, MK_9
        };
        char d[2];
        int i;
        for (i = 0; i < 10; i++)
            if (dk[i] == k) break;
        d[0] = (char)('0' + i);
        d[1] = 0;
        query_add_glyph(d);
        break;
    }
    default:
        break;
    }
    return 1;
}

/* ------------------------------------------------------------------ */
/* OS menu exit (copied from PyCEdit, hardware-verified path)          */
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
        int ev = GetKeyWait_OS(&col, &row, KEYWAIT_HALTOFF_TIMEROFF, 0, 1,
                               &keycode);
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
    DefineStatusAreaFlags(3, SAF_BATTERY | SAF_TEXT | SAF_GLYPH |
                          SAF_ALPHA_SHIFT, 0, 0);
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
            draw_text("PRESS MENU KEY TO LEAVE", 60, 96, COL_WHITE, VIS_W);
            Bdisp_PutDisp_DD();
            GetKeyWait_OS(&col, &row, KEYWAIT_HALTON_TIMERON, 3600, 0,
                          &keycode);
        }
    }

    Bdisp_EnableColor(1);
    EnableStatusArea(3);
    DrawFrame(COL_BLACK);
    wait_all_released();
    drain_os_keys();
}
#else
static void leave_to_menu(void) { }
#endif

/* ------------------------------------------------------------------ */
#ifndef HOST_TEST
int main(void)
{
    Bdisp_EnableColor(1);
    EnableStatusArea(3);
    DrawFrame(COL_BLACK);
    vram = (unsigned short *)GetVRAMAddress();

    /* Same re-entry pattern as hardware-tested PyCEdit: OS menu can return
       to this function when the add-in is selected again. */
    for (;;) {
        view = V_LIST;
        focus = F_KB;
        dir_sec = kbd_lang = 0;
        alpha_lock = 1;
        gx = gy = sel = 0;
        query[0] = 0;
        q_glyphs = 0;
        match_morph = match_capped = 0;
        blk_sec = blk_id = -1;
        blk_raw_len = 0;
        if (dict_open()) view = V_NODICT;
        else do_search(query);
        paint();

        for (;;) {
            int k = poll_key();
            if (k < 0) {
                if (view == V_LIST) { tick++; paint_input(); Bdisp_PutDisp_DD(); }
                continue;
            }
            if (view == V_NODICT) {
                if (k == MK_EXIT || k == MK_MENU) break;
                wait_all_released();
                continue;
            }
            if (!handle_key(k)) break;
            wait_all_released();
            paint();
        }
        if (dict_fd >= 0) Bfile_CloseFile_OS(dict_fd);
        dict_fd = -1;
        leave_to_menu();
    }
}
#endif
