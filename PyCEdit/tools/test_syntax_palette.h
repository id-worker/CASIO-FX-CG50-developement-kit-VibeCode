/* Included by test_pycedit.c after its host stubs; not calculator code. */
static int syntax_assertions = 0;

static void set_code_for_test(const char *src, int language)
{
    assert(strlen(src) < sizeof(edit_buf));
    strcpy(edit_buf, src);
    edit_len = (int)strlen(src);
    edit_lang = language;
    cursor_pos = edit_len;
    target_col = 0;
    scroll_line = 0;
    scroll_col = 0;
    clear_selection();
    shift_sticky = 0;
    alpha_mode = ALP_OFF;
    toast_active = 0;
    active_tab = TAB_EDIT;
    strcpy(open_path, language == LANG_PY ? "\\\\fls0\\syntax.py" :
                      language == LANG_C ? "\\\\fls0\\syntax.c" : "\\\\fls0\\note.txt");
    memset(host_vram_buf, 0x5a, sizeof(host_vram_buf));
}

/* Check the actual foreground pixels of an individual rendered glyph. This
   checks tokenization, the active language, the RGB565 palette AND the VRAM
   drawing path; checking constants or just making PNGs is not enough. */
static void assert_cell_ink(int visible_row, int logical_col, unsigned short color)
{
    int ls = pos_of_line(scroll_line + visible_row);
    int pos = ls + logical_col;
    int visual_col = logical_col - scroll_col;
    unsigned char ch;
    const unsigned char *g;
    int x = CODE_X + visual_col * FONT_W;
    int y = BODY_Y + visible_row * LINE_H + 1;
    int rx, ry, pixels = 0;
    assert(pos < edit_len && visual_col >= 0 && visual_col < CODE_COLS);
    ch = (unsigned char)edit_buf[pos];
    assert(ch >= 32 && ch <= 126);
    g = font6x10[ch - 32];
    for (ry = 0; ry < FONT_H; ry++) for (rx = 0; rx < FONT_W; rx++) {
        if (g[ry] & (0x20 >> rx)) {
            unsigned short actual = host_vram_buf[(y + ry) * VRAM_W + x + rx];
            if (actual != color) {
                fprintf(stderr, "glyph %c at row %d col %d: got %04x expected %04x\n",
                        ch, visible_row, logical_col, actual, color);
                abort();
            }
            pixels++;
        }
    }
    assert(pixels > 0);
    syntax_assertions++;
}

static int col_of_line(int line, const char *word)
{
    int pos = pos_of_line(line);
    const char *found = strstr(edit_buf + pos, word);
    assert(found && found < edit_buf + line_end_of(pos));
    return (int)(found - (edit_buf + pos));
}

static void test_syntax_rendering(void)
{
    assert(detect_lang("FOO.PY") == LANG_PY);
    assert(detect_lang("FOO.C") == LANG_C);
    assert(detect_lang("FOO.H") == LANG_C);
    assert(detect_lang("FOO.CPP") == LANG_C);
    assert(detect_lang("FOO.txt") == LANG_PLAIN);
    assert(COL_WHITE != COL_BLACK && SYN_NORMAL != COL_WHITE);
    assert(SYN_KEYWORD != SYN_NORMAL && SYN_COMMENT != SYN_NORMAL &&
           SYN_STRING != SYN_NORMAL && SYN_NUMBER != SYN_NORMAL &&
           SYN_FUNCTION != SYN_NORMAL);
    /* 1.15: both requested semantic classes use their new high-contrast
       colors; this is a computer check, not a physical LCD measurement. */
    assert(SYN_STRING == RGB565(192, 0, 0));
    assert(SYN_FUNCTION == RGB565(0, 72, 192));
    assert(SYN_FUNCTION != SYN_STRING && SYN_FUNCTION != SYN_KEYWORD &&
           SYN_STRING != SYN_COMMENT && SYN_STRING != SYN_NUMBER);

    set_code_for_test("def square(n):\n    return n + 12  # comment\nprint(\"hello\")\n", LANG_PY);
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_KEYWORD);
    assert_cell_ink(0, 4, SYN_FUNCTION);
    assert_cell_ink(1, 4, SYN_KEYWORD);
    assert_cell_ink(1, col_of_line(1, "12"), SYN_NUMBER);
    assert_cell_ink(1, col_of_line(1, "#"), SYN_COMMENT);
    assert_cell_ink(2, 0, SYN_FUNCTION);
    assert_cell_ink(2, col_of_line(2, "hello"), SYN_STRING);
    save_vram_png("syntax_python.png");

    set_code_for_test("#include <fxcg/display.h>\nint main() {\n  int n = 42; // comment\n  printf(\"ok\");\n}\n", LANG_C);
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_PREPROC);
    assert_cell_ink(1, 0, SYN_KEYWORD);
    assert_cell_ink(1, col_of_line(1, "main"), SYN_FUNCTION);
    assert_cell_ink(2, 2, SYN_KEYWORD);
    assert_cell_ink(2, col_of_line(2, "42"), SYN_NUMBER);
    assert_cell_ink(2, col_of_line(2, "//"), SYN_COMMENT);
    assert_cell_ink(3, col_of_line(3, "printf"), SYN_FUNCTION);
    assert_cell_ink(3, col_of_line(3, "ok"), SYN_STRING);
    save_vram_png("syntax_c.png");

    /* Exercise the literal example from the calculator photograph and the
       equivalent Python code. Check every non-space glyph, including quotes,
       rather than just the first pixel of each token. */
    {
        int col;
        set_code_for_test("#include <fxcg/display.h>\n\nprintf(\"Hello World\");\n", LANG_C);
        strcpy(open_path, "\\\\fls0\\ctest.c");
        render_edit_tab();
        for (col = 0; col < 6; col++) assert_cell_ink(2, col, SYN_FUNCTION);
        for (col = 7; col <= 19; col++)
            if (col != 13) assert_cell_ink(2, col, SYN_STRING);
        assert_cell_ink(2, 6, SYN_OP);
        assert_cell_ink(2, 20, SYN_OP);
        save_vram_png("hello_c.png");

        set_code_for_test("print(\"Hello World\")\n", LANG_PY);
        strcpy(open_path, "\\\\fls0\\hello.py");
        render_edit_tab();
        for (col = 0; col < 5; col++) assert_cell_ink(0, col, SYN_FUNCTION);
        for (col = 6; col <= 18; col++)
            if (col != 12) assert_cell_ink(0, col, SYN_STRING);
        assert_cell_ink(0, 5, SYN_OP);
        assert_cell_ink(0, 19, SYN_OP);
        save_vram_png("hello_python.png");
    }

    /* Function declarations/calls and both kinds of quotes. A dotted module
       prefix stays normal while the call name receives the function color. */
    set_code_for_test("def name(x):\n    text = 'alpha'\n    mod.call(\"beta\")\n", LANG_PY);
    render_edit_tab();
    assert_cell_ink(0, col_of_line(0, "name"), SYN_FUNCTION);
    assert_cell_ink(1, col_of_line(1, "alpha"), SYN_STRING);
    assert_cell_ink(2, col_of_line(2, "mod"), SYN_NORMAL);
    assert_cell_ink(2, col_of_line(2, "call"), SYN_FUNCTION);
    assert_cell_ink(2, col_of_line(2, "beta"), SYN_STRING);

    /* Identifiers containing keywords must remain normal. */
    set_code_for_test("if iffy def define\n", LANG_PY);
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_KEYWORD);
    assert_cell_ink(0, 3, SYN_NORMAL);
    assert_cell_ink(0, 8, SYN_KEYWORD);
    assert_cell_ink(0, 12, SYN_NORMAL);

    /* The state before the visible window must include multi-line strings. */
    set_code_for_test("text = r\"\"\"begin\nif 42 # still a string\nend\"\"\"; print(1)\nreturn 3\n", LANG_PY);
    assert(syntax_state_before_line(0) == SYN_CODE);
    assert(syntax_state_before_line(1) == SYN_PY_TRIPLE_DQ);
    assert(syntax_state_before_line(2) == SYN_PY_TRIPLE_DQ);
    assert(syntax_state_before_line(3) == SYN_CODE);
    scroll_line = 1;
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_STRING);
    assert_cell_ink(0, col_of_line(1, "#"), SYN_STRING);
    assert_cell_ink(1, 0, SYN_STRING);
    assert_cell_ink(1, col_of_line(2, "print"), SYN_FUNCTION);
    assert_cell_ink(2, 0, SYN_KEYWORD);

    /* Three quotes whose first quote is escaped are not a closing delimiter. */
    set_code_for_test("x=\"\"\"one\\\"\"\"\nif 1\n\"\"\"\n", LANG_PY);
    assert(syntax_state_before_line(1) == SYN_PY_TRIPLE_DQ);
    assert(syntax_state_before_line(2) == SYN_PY_TRIPLE_DQ);
    assert(syntax_state_before_line(3) == SYN_CODE);

    set_code_for_test("char *s = \"/* not comment */\";\n/* start\nint 42 still comment\n*/ int b = 3;\n", LANG_C);
    assert(syntax_state_before_line(1) == SYN_CODE);
    assert(syntax_state_before_line(2) == SYN_C_BLOCK);
    assert(syntax_state_before_line(3) == SYN_C_BLOCK);
    assert(syntax_state_before_line(4) == SYN_CODE);
    scroll_line = 2;
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_COMMENT);
    assert_cell_ink(0, col_of_line(2, "42"), SYN_COMMENT);
    assert_cell_ink(1, 0, SYN_COMMENT);
    assert_cell_ink(1, col_of_line(3, "int"), SYN_KEYWORD);

    /* Horizontal scrolling must still parse the quote before the viewport. */
    set_code_for_test("s = \"inside # string\" # real comment\n", LANG_PY);
    scroll_col = 10;
    render_edit_tab();
    assert_cell_ink(0, col_of_line(0, "# string"), SYN_STRING);
    assert_cell_ink(0, col_of_line(0, "# real"), SYN_COMMENT);

    set_code_for_test("if 123 # plain text\n", LANG_PLAIN);
    render_edit_tab();
    assert_cell_ink(0, 0, SYN_NORMAL);
    assert_cell_ink(0, 3, SYN_NORMAL);
    assert_cell_ink(0, 7, SYN_NORMAL);

    /* Presenting a frame must re-enable full color immediately before DD. */
    color_enables = frame_presents = 0;
    last_color_mode = -1;
    present_frame();
    present_frame();
    assert(color_enables == 2 && frame_presents == 2 && last_color_mode == -1);
    printf("Syntax/VRAM assertions: %d; language, multiline and color-mode tests passed.\n", syntax_assertions);
}

static void test_render_fuzz_and_guards(void)
{
    static unsigned short guarded[VRAM_W * VIS_H + 32];
    const char alphabet[] = "abcDEF0123_ #/\\*'\"()[]{}@=.+-:;\n";
    unsigned int seed = 0x51a7cafeu;
    int t, j;
    unsigned short *saved = vram;
    vram = guarded + 16;
    for (t = 0; t < 500; t++) {
        int lines;
        seed = seed * 1664525u + 1013904223u;
        edit_len = (int)(seed % 700u);
        for (j = 0; j < edit_len; j++) {
            seed = seed * 1664525u + 1013904223u;
            edit_buf[j] = alphabet[(seed >> 8) % (sizeof(alphabet) - 1)];
        }
        edit_buf[edit_len] = '\0';
        edit_lang = t % 3;
        cursor_pos = edit_len;
        clear_selection();
        toast_active = 0;
        shift_sticky = 0;
        alpha_mode = ALP_OFF;
        scroll_col = t % 80;
        lines = total_lines();
        scroll_line = t % lines;
        for (j = 0; j < 16; j++) guarded[j] = guarded[VRAM_W * VIS_H + 16 + j] = 0xa35c;
        render_edit_tab();
        for (j = 0; j < 16; j++) {
            assert(guarded[j] == 0xa35c);
            assert(guarded[VRAM_W * VIS_H + 16 + j] == 0xa35c);
        }
    }
    vram = saved;
    printf("Fuzz: 500 randomized C/Python/plain renderings; VRAM sentinels intact.\n");
}

static void run_palette_script(const int *keys, int count)
{
    assert(count > 0 && count <= (int)(sizeof(scripted_keys) / sizeof(scripted_keys[0])));
    memcpy(scripted_keys, keys, (size_t)count * sizeof(keys[0]));
    scripted_count = count;
    /* A real modal opens over an already-rendered editor frame. */
    render_edit_tab();
    scripted_start = fake_ticks;
    scripted_active = 1;
    memset(key_state, 0, sizeof(key_state));
    shift_sticky = 0;
    alpha_mode = ALP_OFF;
    reset_key_poller();
    open_symbol_palette();
    scripted_active = 0;
}

static void test_include_catalog(void)
{
    static const int digit_keys[8] = { MK_1, MK_2, MK_3, MK_4, MK_5, MK_6, MK_7, MK_8 };
    int i, j;
    palette_state_t p;
    assert(INCLUDE_COUNT == 62 && include_page_count() == 8);
    for (i = 0; i < INCLUDE_COUNT; i++) {
        char text[100], expected[100];
        int keys[12], n = 0;
        assert(strlen(include_catalog[i].name) < 60);
        for (j = i + 1; j < INCLUDE_COUNT; j++)
            assert(strcmp(include_catalog[i].name, include_catalog[j].name) != 0);
        p.page = PALETTE_LIBS; p.sel = i;
        sprintf(expected, "#include <%s>\n", include_catalog[i].name);
        assert(strcmp(palette_selected_text(&p, text), expected) == 0);
        assert(palette_key(&p, MK_DOWN, 0) == PALETTE_STAY);
        assert(p.sel == (i + 1) % INCLUDE_COUNT);
        assert(palette_key(&p, MK_UP, 0) == PALETTE_STAY);
        assert(p.sel == i);

        /* Navigate via the actual F4, F5 and 1..8 key loop for every header. */
        set_code_for_test("", LANG_C);
        keys[n++] = MK_F4;
        for (j = 0; j < i / LIB_PAGE_SIZE; j++) keys[n++] = MK_F5;
        keys[n++] = digit_keys[i % LIB_PAGE_SIZE];
        run_palette_script(keys, n);
        if (strcmp(edit_buf, expected) != 0) {
            fprintf(stderr, "palette insertion %d failed: got %s expected %s\n", i, edit_buf, expected);
            abort();
        }
    }

    /* Last page has only six entries: 7 and 8 must not insert or close. */
    set_code_for_test("", LANG_C);
    { int keys[] = { MK_F4, MK_UP, MK_7, MK_8, MK_6 };
      run_palette_script(keys, 5); }
    assert(strcmp(edit_buf, "#include <sys/types.h>\n") == 0);
    save_vram_png("palette_last_page.png");

    /* Shift+F5/left wraps backward, and EXIT never inserts. */
    set_code_for_test("", LANG_C);
    { int keys[] = { MK_F4, MK_SHIFT, MK_F5, MK_6 };
      run_palette_script(keys, 4); }
    assert(strcmp(edit_buf, "#include <sys/types.h>\n") == 0);
    set_code_for_test("keep", LANG_C);
    { int keys[] = { MK_F4, MK_RIGHT, MK_F6 };
      run_palette_script(keys, 3); }
    assert(strcmp(edit_buf, "keep") == 0);

    /* Pre-existing tabs and digit shortcuts remain available. */
    set_code_for_test("", LANG_C);
    { int keys[] = { MK_F2, MK_1 }; run_palette_script(keys, 2); }
    assert(strcmp(edit_buf, "#include <stdio.h>\n") == 0);
    set_code_for_test("", LANG_PY);
    { int keys[] = { MK_F3, MK_1 }; run_palette_script(keys, 2); }
    assert(strcmp(edit_buf, "import gint\n") == 0);
    set_code_for_test("", LANG_C);
    { int keys[] = { MK_RIGHT, MK_EXE }; run_palette_script(keys, 2); }
    assert(strcmp(edit_buf, ":") == 0);
    printf("Palette: all %d includes inserted via real key polling; 8 pages and boundaries passed.\n", INCLUDE_COUNT);
}

static void test_pymodule_catalog(void)
{
    static const int digit_keys[8] = { MK_1, MK_2, MK_3, MK_4, MK_5, MK_6, MK_7, MK_8 };
    int i, j;
    palette_state_t p;
    assert(PYMODULE_COUNT == 18 && pymodule_page_count() == 3);
    for (i = 0; i < PYMODULE_COUNT; i++) {
        char text[100], expected[100];
        int keys[8], n = 0;
        assert(strlen(pymodule_catalog[i].name) < 60);
        assert(pymodule_catalog[i].availability <= PYMOD_FILE);
        for (j = i + 1; j < PYMODULE_COUNT; j++)
            assert(strcmp(pymodule_catalog[i].name, pymodule_catalog[j].name) != 0);
        p.page = PALETTE_PYMOD; p.sel = i;
        sprintf(expected, "import %s\n", pymodule_catalog[i].name);
        assert(strcmp(palette_selected_text(&p, text), expected) == 0);
        assert(palette_key(&p, MK_DOWN, 0) == PALETTE_STAY);
        assert(p.sel == (i + 1) % PYMODULE_COUNT);
        assert(palette_key(&p, MK_UP, 0) == PALETTE_STAY && p.sel == i);
        /* The real modal loop: F5 opens PYMOD; subsequent F5s paginate. */
        set_code_for_test("", LANG_PY);
        keys[n++] = MK_F5;
        for (j = 0; j < i / LIB_PAGE_SIZE; j++) keys[n++] = MK_F5;
        keys[n++] = digit_keys[i % LIB_PAGE_SIZE];
        run_palette_script(keys, n);
        if (strcmp(edit_buf, expected) != 0) {
            fprintf(stderr, "Python module %d: got %s expected %s\n", i, edit_buf, expected);
            abort();
        }
        if (i == 7) save_vram_png("palette_pymod_page1.png");
        if (i == 15) save_vram_png("palette_pymod_page2.png");
    }
    save_vram_png("palette_pymod_last_page.png");
    /* Last page has 2 entries; 3..8 do not insert or close the dialog. */
    set_code_for_test("", LANG_PY);
    { int keys[] = { MK_F5, MK_UP, MK_3, MK_8, MK_2 }; run_palette_script(keys, 5); }
    assert(strcmp(edit_buf, "import matplotl\n") == 0);
    /* SHIFT+F5 and left arrow wrap; these test the real SHIFT latch. */
    set_code_for_test("", LANG_PY);
    { int keys[] = { MK_F5, MK_SHIFT, MK_F5, MK_1 }; run_palette_script(keys, 4); }
    assert(strcmp(edit_buf, "import turtle\n") == 0);
    set_code_for_test("", LANG_PY);
    { int keys[] = { MK_F5, MK_LEFT, MK_2 }; run_palette_script(keys, 3); }
    assert(strcmp(edit_buf, "import matplotl\n") == 0);
    /* F5 opens modules from each existing noncatalog tab too. */
    set_code_for_test("", LANG_PY);
    { int keys[] = { MK_F3, MK_F5, MK_EXE }; run_palette_script(keys, 3); }
    assert(strcmp(edit_buf, "import math\n") == 0);
    set_code_for_test("", LANG_C);
    { int keys[] = { MK_F2, MK_F5, MK_2 }; run_palette_script(keys, 3); }
    assert(strcmp(edit_buf, "import random\n") == 0);
    /* F4->F3->F5 allows leaving the C include catalog for Python modules. */
    set_code_for_test("keep\n", LANG_PY);
    { int keys[] = { MK_F4, MK_F3, MK_F5, MK_3 }; run_palette_script(keys, 4); }
    assert(strcmp(edit_buf, "keep\nimport casioplot\n") == 0);
    set_code_for_test("keep", LANG_PY);
    { int keys[] = { MK_F5, MK_RIGHT, MK_F6 }; run_palette_script(keys, 3); }
    assert(strcmp(edit_buf, "keep") == 0);
    printf("Palette: all %d Python import templates inserted via real key polling; 3 pages and boundaries passed.\n", PYMODULE_COUNT);
}
