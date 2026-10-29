/* SuperH/QEMU smoke/regression test. CPU and VRAM buffer are emulated;
   Casio firmware, LCD, keyboard matrix, storage and timing are NOT. */
#define HOST_TEST 1
#include "../src/main.c"

static unsigned short sh_vram[VRAM_W * VIS_H];
unsigned char target_stack[16384];
static int enabled, displays;
/* libfxcg implements strlen as a Casio OS syscall. QEMU has no such OS;
   replace only that dependency with an ordinary implementation for this test. */
size_t strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
void Bdisp_EnableColor(int mode) { enabled = mode; }
void Bdisp_PutDisp_DD(void) { if (enabled == 1) displays++; enabled = 0; }

static int cell_has_ink(int row, int col, unsigned short expected)
{
    int pos = pos_of_line(scroll_line + row) + col;
    int ry, rx, count = 0;
    unsigned char ch = (unsigned char)edit_buf[pos];
    const unsigned char *glyph;
    if (ch < 32 || ch > 126) return 0;
    glyph = font6x10[ch - 32];
    for (ry = 0; ry < FONT_H; ry++) for (rx = 0; rx < FONT_W; rx++) {
        if (glyph[ry] & (0x20 >> rx)) {
            int x = CODE_X + (col - scroll_col) * FONT_W + rx;
            int y = BODY_Y + row * LINE_H + 1 + ry;
            if (x < 0 || x >= VIS_W || y < 0 || y >= VIS_H ||
                vram[y * VRAM_W + x] != expected) return 0;
            count++;
        }
    }
    return count > 0;
}

static void setup(const char *s, int lang)
{
    strcpy(edit_buf, s);
    edit_len = (int)strlen(s);
    edit_lang = lang;
    cursor_pos = edit_len;
    scroll_col = scroll_line = 0;
    edit_dirty = 0;
    sel_anchor = -1;
    select_mode = shift_sticky = alpha_mode = toast_active = 0;
    strcpy(open_path, lang == LANG_PY ? "demo.py" : "demo.c");
}

int main(void)
{
    int i;
    char text[100];
    palette_state_t p;
    vram = sh_vram;
    if (detect_lang("DEMO.PY") != LANG_PY || detect_lang("DEMO.C") != LANG_C) return 1;
    if (COL_WHITE != 0xffff || COL_BLACK != 0x0000) return 3;
    /* Exact RGB565 values as interpreted by the big-endian target. */
    if (SYN_STRING != 0xc000 || SYN_FUNCTION != 0x0258) return 31;
    setup("def func(x):\n    return 42 # note\nprint(\"hi\")\n", LANG_PY);
    render_edit_tab();
    if (!cell_has_ink(0, 0, SYN_KEYWORD) || !cell_has_ink(0, 4, SYN_FUNCTION)) return 4;
    if (!cell_has_ink(1, 4, SYN_KEYWORD) || !cell_has_ink(1, 11, SYN_NUMBER)) return 5;
    if (!cell_has_ink(1, 14, SYN_COMMENT) || !cell_has_ink(2, 7, SYN_STRING)) return 6;
    setup("#include <fxcg/display.h>\nint main() {\n  printf(\"ok\");\n}\n", LANG_C);
    render_edit_tab();
    if (!cell_has_ink(0, 0, SYN_PREPROC) || !cell_has_ink(1, 0, SYN_KEYWORD)) return 7;
    if (!cell_has_ink(1, 4, SYN_FUNCTION) || !cell_has_ink(2, 2, SYN_FUNCTION)) return 8;
    setup("#include <fxcg/display.h>\n\nprintf(\"Hello World\");\n", LANG_C);
    render_edit_tab();
    if (!cell_has_ink(2, 0, SYN_FUNCTION) || !cell_has_ink(2, 7, SYN_STRING) ||
        !cell_has_ink(2, 8, SYN_STRING) || !cell_has_ink(2, 19, SYN_STRING)) return 32;
    setup("print(\"Hello World\")\n", LANG_PY);
    render_edit_tab();
    if (!cell_has_ink(0, 0, SYN_FUNCTION) || !cell_has_ink(0, 6, SYN_STRING) ||
        !cell_has_ink(0, 7, SYN_STRING) || !cell_has_ink(0, 18, SYN_STRING)) return 33;
    setup("text=\"\"\"one\nif # in a string\nend\"\"\"\nreturn 2\n", LANG_PY);
    if (syntax_state_before_line(1) != SYN_PY_TRIPLE_DQ ||
        syntax_state_before_line(2) != SYN_PY_TRIPLE_DQ ||
        syntax_state_before_line(3) != SYN_CODE) return 9;
    scroll_line = 1;
    render_edit_tab();
    if (!cell_has_ink(0, 0, SYN_STRING) || !cell_has_ink(2, 0, SYN_KEYWORD)) return 10;
    setup("/* a\nint n;\n*/ int n;\n", LANG_C);
    if (syntax_state_before_line(1) != SYN_C_BLOCK ||
        syntax_state_before_line(2) != SYN_C_BLOCK ||
        syntax_state_before_line(3) != SYN_CODE) return 11;
    scroll_line = 1;
    render_edit_tab();
    if (!cell_has_ink(0, 0, SYN_COMMENT) || !cell_has_ink(1, 3, SYN_KEYWORD)) return 12;

    if (INCLUDE_COUNT != 62 || include_page_count() != 8) return 13;
    p.page = PALETTE_LIBS; p.sel = 0;
    for (i = 0; i < INCLUDE_COUNT; i++) {
        if (p.sel != i) return 14;
        if (palette_key(&p, MK_DOWN, 0) != PALETTE_STAY) return 15;
    }
    if (p.sel != 0) return 16;
    if (palette_key(&p, MK_F5, 1) != PALETTE_STAY || p.sel != 56) return 17;
    if (palette_key(&p, MK_7, 0) != PALETTE_STAY) return 18;
    if (palette_key(&p, MK_6, 0) != PALETTE_INSERT || p.sel != 61) return 19;
    if (strcmp(palette_selected_text(&p, text), "#include <sys/types.h>\n") != 0) return 20;
    if (PYMODULE_COUNT != 18 || pymodule_page_count() != 3) return 22;
    p.page = PALETTE_SYMBOLS; p.sel = 0;
    if (palette_key(&p, MK_F5, 0) != PALETTE_STAY || p.page != PALETTE_PYMOD) return 23;
    for (i = 0; i < PYMODULE_COUNT; i++) {
        if (p.sel != i) return 24;
        if (palette_key(&p, MK_DOWN, 0) != PALETTE_STAY) return 25;
    }
    if (p.sel != 0) return 26;
    if (palette_key(&p, MK_F5, 1) != PALETTE_STAY || p.sel != 16) return 27;
    if (palette_key(&p, MK_3, 0) != PALETTE_STAY) return 28;
    if (palette_key(&p, MK_2, 0) != PALETTE_INSERT || p.sel != 17) return 29;
    if (strcmp(palette_selected_text(&p, text), "import matplotl\n") != 0) return 30;
    enabled = displays = 0;
    present_frame();
    present_frame();
    if (enabled != 0 || displays != 2) return 21;
    return 0;
}
