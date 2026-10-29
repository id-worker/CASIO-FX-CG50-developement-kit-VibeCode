/*
 * Host integration test & 384x216 PNG screenshot renderer for PyCEdit.
 */
#include <assert.h>
#include <png.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../src/main.c"

/* ------------------------------------------------------------------ */
/* Mock Casio \\fls0\ in-memory filesystem                             */
/* ------------------------------------------------------------------ */
typedef struct {
    int used;
    int is_dir;
    char fullpath[MAX_PATH_LEN];
    char data[65536];
    int size;
} mock_entry_t;

static mock_entry_t mock_fs[32];
static int open_handles[16]; /* maps handle -> mock_fs index + 1 */
static int find_cursor = -1;
static char find_prefix[MAX_PATH_LEN];
static int active_find_handles = 0;

static unsigned short host_vram_buf[VRAM_W * VIS_H];
static int fake_ticks = 100;
static int key_state[128];
/* Optional hardware-matrix event script for the real modal palette loop. */
static int scripted_keys[32], scripted_count, scripted_start, scripted_active;

int host_key_down(int code)
{
    if (scripted_active) {
        int elapsed = fake_ticks - scripted_start;
        int i;
        for (i = 0; i < scripted_count; i++) {
            int t0 = 20 + i * 60;
            if (code == scripted_keys[i] && elapsed >= t0 && elapsed < t0 + 20)
                return 1;
        }
        return 0;
    }
    if (code >= 0 && code < 128) return key_state[code];
    return 0;
}

static int last_color_mode = -1, color_enables = 0, frame_presents = 0;
void Bdisp_EnableColor(int mode) { last_color_mode = mode; color_enables++; }
void Bdisp_PutDisp_DD(void) { assert(last_color_mode == 1); frame_presents++; last_color_mode = -1; }
void OS_InnerWait_ms(int ms) {
    fake_ticks += (ms > 0 ? ms : 1);
    if (scripted_active && fake_ticks - scripted_start > 20 + scripted_count * 60 + 300) {
        fprintf(stderr, "palette event script exhausted\n");
        abort();
    }
}
int  RTC_GetTicks(void) { return fake_ticks; }
int  RTC_Elapsed_ms(int start, int dur) { return (fake_ticks - start) * 8 >= dur; }
void host_leave_to_menu(void) {}

void Bfile_StrToName_ncpy(unsigned short *dest, const char *src, size_t n)
{
    size_t i = 0;
    while (src[i] && i + 1 < n) { dest[i] = (unsigned char)src[i]; i++; }
    dest[i] = 0;
}

void Bfile_NameToStr_ncpy(char *dest, const unsigned short *src, size_t n)
{
    size_t i = 0;
    while (src[i] && i + 1 < n) { dest[i] = (char)(src[i] & 0xFF); i++; }
    dest[i] = '\0';
}

static int mock_find_idx(const char *path)
{
    int i;
    for (i = 0; i < 32; i++)
        if (mock_fs[i].used && strcmp(mock_fs[i].fullpath, path) == 0)
            return i;
    return -1;
}

static void mock_add_file(const char *fullpath, const char *content, int is_dir)
{
    int i;
    for (i = 0; i < 32; i++) {
        if (!mock_fs[i].used) {
            mock_fs[i].used = 1;
            mock_fs[i].is_dir = is_dir;
            strcpy(mock_fs[i].fullpath, fullpath);
            if (!is_dir && content) {
                mock_fs[i].size = (int)strlen(content);
                memcpy(mock_fs[i].data, content, (size_t)mock_fs[i].size);
            } else {
                mock_fs[i].size = 0;
            }
            return;
        }
    }
}

static int find_next_match(char *foundfile, fs_info_t *info)
{
    size_t plen = strlen(find_prefix);
    while (find_cursor < 32) {
        int idx = find_cursor++;
        if (!mock_fs[idx].used) continue;
        if (strncmp(mock_fs[idx].fullpath, find_prefix, plen) != 0) continue;
        const char *rem = mock_fs[idx].fullpath + plen;
        if (!rem[0] || strchr(rem, '\\') != NULL) continue;
        Bfile_StrToName_ncpy((unsigned short *)foundfile, rem, MAX_PATH_LEN);
        memset(info, 0, sizeof(*info));
        info->type = mock_fs[idx].is_dir ? 0x0000 : 0x0001;
        info->fsize = (unsigned long)mock_fs[idx].size;
        return 0;
    }
    return -1;
}

int Bfile_FindFirst_NON_SMEM(const char *pathname, int *FindHandle, char *foundfile, void *fileinfo)
{
    char mask[MAX_PATH_LEN];
    size_t len;
    Bfile_NameToStr_ncpy(mask, (const unsigned short *)pathname, sizeof(mask));
    len = strlen(mask);
    assert(len > 0 && mask[len - 1] == '*');
    memcpy(find_prefix, mask, len - 1);
    find_prefix[len - 1] = '\0';
    find_cursor = 0;
    active_find_handles++;
    *FindHandle = 1;
    return find_next_match(foundfile, (fs_info_t *)fileinfo);
}

int Bfile_FindNext_NON_SMEM(int FindHandle, char *foundfile, char *fileinfo)
{
    (void)FindHandle;
    return find_next_match(foundfile, (fs_info_t *)fileinfo);
}

int Bfile_FindClose(int FindHandle)
{
    (void)FindHandle;
    assert(active_find_handles > 0);
    active_find_handles--;
    return 0;
}

int Bfile_OpenFile_OS(const unsigned short *filename, int mode, int null)
{
    char p[MAX_PATH_LEN];
    int idx, h;
    (void)mode; (void)null;
    /* Enforce Casio rule: FindHandle MUST be closed before Bfile_OpenFile_OS! */
    assert(active_find_handles == 0);
    Bfile_NameToStr_ncpy(p, filename, sizeof(p));
    idx = mock_find_idx(p);
    if (idx < 0 || mock_fs[idx].is_dir) return -1;
    for (h = 0; h < 16; h++) {
        if (!open_handles[h]) {
            open_handles[h] = idx + 1;
            return h;
        }
    }
    return -1;
}

int Bfile_GetFileSize_OS(int handle)
{
    return mock_fs[open_handles[handle] - 1].size;
}

int Bfile_ReadFile_OS(int handle, void *buf, int size, int readpos)
{
    mock_entry_t *e = &mock_fs[open_handles[handle] - 1];
    int pos = (readpos < 0) ? 0 : readpos;
    int avail = e->size - pos;
    if (avail <= 0) return 0;
    if (size > avail) size = avail;
    memcpy(buf, e->data + pos, (size_t)size);
    return size;
}

int Bfile_WriteFile_OS(int handle, const void *buf, int size)
{
    mock_entry_t *e = &mock_fs[open_handles[handle] - 1];
    memcpy(e->data, buf, (size_t)size);
    e->size = size;
    return size;
}

int Bfile_CloseFile_OS(int handle)
{
    open_handles[handle] = 0;
    return 0;
}

int Bfile_CreateEntry_OS(const unsigned short *filename, int mode, size_t *size)
{
    char p[MAX_PATH_LEN];
    int i;
    (void)mode;
    assert(active_find_handles == 0);
    assert(size != NULL && *size >= 1);
    Bfile_NameToStr_ncpy(p, filename, sizeof(p));
    if (mock_find_idx(p) >= 0) return -1;
    for (i = 0; i < 32; i++) {
        if (!mock_fs[i].used) {
            mock_fs[i].used = 1;
            mock_fs[i].is_dir = 0;
            strcpy(mock_fs[i].fullpath, p);
            mock_fs[i].size = (int)*size;
            memset(mock_fs[i].data, 0, *size);
            return 0;
        }
    }
    return -1;
}

int Bfile_DeleteEntry(const unsigned short *filename)
{
    char p[MAX_PATH_LEN];
    int idx;
    assert(active_find_handles == 0);
    Bfile_NameToStr_ncpy(p, filename, sizeof(p));
    idx = mock_find_idx(p);
    if (idx < 0) return -1;
    mock_fs[idx].used = 0;
    return 0;
}

int Bfile_RenameEntry(const unsigned short *oldpath, const unsigned short *newpath)
{
    char op[MAX_PATH_LEN], np[MAX_PATH_LEN];
    int idx;
    assert(active_find_handles == 0);
    Bfile_NameToStr_ncpy(op, oldpath, sizeof(op));
    Bfile_NameToStr_ncpy(np, newpath, sizeof(np));
    idx = mock_find_idx(op);
    if (idx < 0 || mock_find_idx(np) >= 0) return -1;
    strcpy(mock_fs[idx].fullpath, np);
    return 0;
}

/* ------------------------------------------------------------------ */
/* Save RGB565 VRAM to PNG                                             */
/* ------------------------------------------------------------------ */
static void save_vram_png(const char *path)
{
    FILE *fp = fopen(path, "wb");
    assert(fp != NULL);
    png_structp png = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
    png_infop info = png_create_info_struct(png);
    png_init_io(png, fp);
    png_set_IHDR(png, info, VIS_W, VIS_H, 8, PNG_COLOR_TYPE_RGB,
                 PNG_INTERLACE_NONE, PNG_COMPRESSION_TYPE_DEFAULT, PNG_FILTER_TYPE_DEFAULT);
    png_write_info(png, info);

    uint8_t row[VIS_W * 3];
    for (int y = 0; y < VIS_H; y++) {
        for (int x = 0; x < VIS_W; x++) {
            uint16_t px = host_vram_buf[y * VRAM_W + x];
            uint8_t r = (uint8_t)(((px >> 11) & 0x1F) * 255 / 31);
            uint8_t g = (uint8_t)(((px >> 5)  & 0x3F) * 255 / 63);
            uint8_t b = (uint8_t)(( px        & 0x1F) * 255 / 31);
            row[x * 3 + 0] = r;
            row[x * 3 + 1] = g;
            row[x * 3 + 2] = b;
        }
        png_write_row(png, row);
    }
    png_write_end(png, NULL);
    png_destroy_write_struct(&png, &info);
    fclose(fp);
    printf("Saved screenshot: %s\n", path);
}

int host_step_after_frame(void) { return 1; }

static void tap_key_in_edit(int k)
{
    int mc = 0;
    memset(key_state, 0, sizeof(key_state));
    poll_editor_key(&mc);
    fake_ticks += 10;
    poll_editor_key(&mc);
    key_state[k] = 1;
    int got = poll_editor_key(&mc);
    if (got) {
        if (active_tab == TAB_FILES) handle_files_key(got);
        else handle_edit_key(got);
    }
    key_state[k] = 0;
    poll_editor_key(&mc); /* starts 32ms release-debounce timer */
    fake_ticks += 10;     /* 10 * 8 = 80ms >= 32ms */
    poll_editor_key(&mc); /* completes release cleanly */
}

#include "test_syntax_palette.h"

int main(void)
{
    vram = host_vram_buf;
    strcpy(cur_dir, SMEM_ROOT);

    mock_add_file("\\\\fls0\\scripts", NULL, 1);
    mock_add_file("\\\\fls0\\gfx_demo.c",
        "/* PicoC RGB565 Pixel & Key Demo */\n"
        "#include <fxcg/display.h>\n"
        "#include <fxcg/keyboard.h>\n\n"
        "int main() {\n"
        "    unsigned short *vram = GetVRAMAddress();\n"
        "    int x = 192, y = 108;\n"
        "    Bdisp_EnableColor(1);\n"
        "    while (1) {\n"
        "        vram[y * 384 + x] = 0x07E0; // bright green\n"
        "        Bdisp_PutDisp_DD();\n"
        "    }\n"
        "    return 0;\n"
        "}\n\n"
        "main();\n", 0);
    mock_add_file("\\\\fls0\\hello.py",
        "import gint\n\n"
        "# PythonExtra graphic demo\n"
        "def draw_box(x, y):\n"
        "    gint.dclear(gint.C_WHITE)\n"
        "    gint.drect(x, y, x + 60, y + 40, gint.C_BLUE)\n"
        "    gint.dtext(12, 12, gint.C_BLACK, \"PyCEdit + gint!\")\n"
        "    gint.dupdate()\n\n"
        "draw_box(40, 50)\n"
        "gint.getkey()\n", 0);
    mock_add_file("\\\\fls0\\libs.c", "#include <stdio.h>\nmain();\n", 0);
    mock_add_file("\\\\fls0\\PyCEdit.g3a", "BIN", 0);

    refresh_dir(NULL);
    /* Filter hides .g3a, keeps folder + 3 code files = 4 items */
    assert(dir_count == 4);
    assert(dir_items[0].is_dir == 1 && strcmp(dir_items[0].name, "scripts") == 0);
    assert(strcmp(dir_items[1].name, "gfx_demo.c") == 0);
    assert(strcmp(dir_items[2].name, "hello.py") == 0);

    /* Select hello.py and render FILES tab screenshot */
    dir_sel = 2;
    render_files_tab();
    save_vram_png("preview_files.png");

    /* Test 1-press F4 .py <-> .c extension toggle! */
    tap_key_in_edit(MK_F4);
    assert(strcmp(dir_items[dir_sel].name, "hello.c") == 0);
    assert(mock_find_idx("\\\\fls0\\hello.c") >= 0);
    assert(mock_find_idx("\\\\fls0\\hello.py") < 0);

    /* Press F4 again -> toggles right back to hello.py! */
    tap_key_in_edit(MK_F4);
    assert(strcmp(dir_items[dir_sel].name, "hello.py") == 0);
    toast_active = 0;

    /* Open gfx_demo.c in EDIT tab via F2 (EDIT) button directly! */
    dir_sel = 1;
    tap_key_in_edit(MK_F2);
    assert(active_tab == TAB_EDIT);
    assert(edit_lang == LANG_C);
    assert(edit_len > 100);
    cursor_pos = pos_of_line(9) + 8;
    alpha_mode = ALP_LOCK_LO;
    render_edit_tab();
    save_vram_png("preview_edit_c.png");

    /* Switch to FILES (F1), open hello.py, test typing + auto-indent + save */
    tap_key_in_edit(MK_F1);
    assert(active_tab == TAB_FILES);
    dir_sel = 2;
    tap_key_in_edit(MK_EXE);
    assert(active_tab == TAB_EDIT);
    assert(edit_lang == LANG_PY);

    /* Test SHIFT + * -> '{' and backspace */
    cursor_pos = edit_len;
    alpha_mode = ALP_OFF;
    tap_key_in_edit(MK_SHIFT); /* sticky [SHF] */
    assert(shift_sticky == 1);
    render_edit_tab();
    save_vram_png("preview_edit_py.png");

    tap_key_in_edit(MK_MUL);   /* '{' via PythonExtra SHIFT + * */
    assert(shift_sticky == 0);
    assert(edit_buf[edit_len - 1] == '{');
    tap_key_in_edit(MK_DEL);
    assert(cursor_pos == edit_len); /* MUST stay at EOF, never skip left! */
    assert(fs_save_file(open_path) == 0);
    assert(edit_dirty == 0);

    /* 1. Regression test: Backspace at EOF must NEVER skip characters
          and smart-unindent at EOF must NEVER make cursor_pos negative! */
    mock_add_file("\\\\fls0\\tab_eof.py", "\tdef f():\r\n\t\tx = 12345", 0);
    assert(fs_load_file("\\\\fls0\\tab_eof.py") == 0);
    assert(strcmp(edit_buf, "    def f():\n        x = 12345") == 0);

    cursor_pos = edit_len; /* right after '5' at EOF (no trailing \n) */
    tap_key_in_edit(MK_DEL);
    assert(strcmp(edit_buf, "    def f():\n        x = 1234") == 0);
    assert(cursor_pos == edit_len);
    tap_key_in_edit(MK_DEL);
    assert(strcmp(edit_buf, "    def f():\n        x = 123") == 0);
    assert(cursor_pos == edit_len);
    tap_key_in_edit(MK_DEL);
    assert(strcmp(edit_buf, "    def f():\n        x = 12") == 0);
    assert(cursor_pos == edit_len);

    /* Test smart-unindent at EOF on pure-spaces buffer ("    ") */
    strcpy(edit_buf, "    ");
    edit_len = 4;
    cursor_pos = 4;
    tap_key_in_edit(MK_DEL);
    assert(edit_len == 0 && cursor_pos == 0);

    /* 2. Regression test: Hardware contact bounce (<32ms) on MK_DEL
          must fire ONLY ONCE, never double-deleting! */
    strcpy(edit_buf, "abcd");
    edit_len = 4;
    cursor_pos = 4;
    {
        int mc = 0, got;
        memset(key_state, 0, sizeof(key_state));
        poll_editor_key(&mc);
        fake_ticks += 10;
        poll_editor_key(&mc);

        key_state[MK_DEL] = 1;
        got = poll_editor_key(&mc);
        assert(got == MK_DEL);
        handle_edit_key(got);
        assert(strcmp(edit_buf, "abc") == 0 && cursor_pos == 3);

        /* Simulate 8ms contact bounce: 1 -> 0 -> 1 */
        key_state[MK_DEL] = 0;
        assert(poll_editor_key(&mc) == 0);
        fake_ticks += 1; /* 8ms < 32ms debounce threshold */
        key_state[MK_DEL] = 1;
        assert(poll_editor_key(&mc) == 0); /* Bounce IGNORED! */
        assert(strcmp(edit_buf, "abc") == 0 && cursor_pos == 3);

        /* Clean release >= 32ms */
        key_state[MK_DEL] = 0;
        poll_editor_key(&mc);
        fake_ticks += 10;
        poll_editor_key(&mc);
    }

    /* 3. Test Selection (VARS + Arrows + Block Indent/Unindent + Copy/Cut/Paste + Select All) */
    assert(fs_load_file("\\\\fls0\\hello.py") == 0);
    active_tab = TAB_EDIT;
    cursor_pos = pos_of_line(3); /* "def draw_box(x, y):" */
    tap_key_in_edit(MK_VARS);    /* Enter [SEL] mode */
    assert(select_mode == 1 && sel_anchor == cursor_pos);
    tap_key_in_edit(MK_DOWN);
    tap_key_in_edit(MK_DOWN);
    tap_key_in_edit(MK_SHIFT);
    tap_key_in_edit(MK_RIGHT);   /* Extend selection to end of line 5 */
    assert(has_selection());
    render_edit_tab();
    save_vram_png("preview_sel.png");

    /* Block indent (+4 spaces) then block unindent (-4 spaces) */
    {
        char before_indent[1024];
        strcpy(before_indent, edit_buf);
        tap_key_in_edit(MK_F4); /* TAB> indents all 3 selected lines */
        assert(strstr(edit_buf, "    def draw_box(x, y):") != NULL);
        tap_key_in_edit(MK_SHIFT);
        tap_key_in_edit(MK_F4); /* <TAB unindents right back! */
        assert(strcmp(edit_buf, before_indent) == 0);
    }

    /* Select All (SHIFT + VARS), Copy (F1), Select All, DEL, Paste! */
    clear_selection();
    {
        char orig_hello[1024];
        strcpy(orig_hello, edit_buf);
        tap_key_in_edit(MK_SHIFT);
        tap_key_in_edit(MK_VARS); /* Select All! */
        assert(has_selection() && sel_anchor == 0 && cursor_pos == edit_len);
        tap_key_in_edit(MK_F1);   /* COPY in selection F-bar */
        assert(!has_selection());
        assert(clip_len == (int)strlen(orig_hello));

        /* Select All again -> DEL -> buffer becomes empty! */
        tap_key_in_edit(MK_SHIFT);
        tap_key_in_edit(MK_VARS);
        tap_key_in_edit(MK_DEL);
        assert(edit_len == 0 && cursor_pos == 0 && !has_selection());

        /* Paste back full file via insert_text_at_cursor */
        insert_text_at_cursor(clip_buf);
        assert(strcmp(edit_buf, orig_hello) == 0);
    }

    test_syntax_rendering();
    test_render_fuzz_and_guards();
    test_include_catalog();
    test_pymodule_catalog();
    printf("ALL PyCEdit host assertions PASSED!\n");
    return 0;
}
