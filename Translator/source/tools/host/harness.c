/* Actual Translator code + emulated fx-CG50 file/display calls.
 * Runs natively and on a big-endian MIPS Linux target (qemu-mips).
 * Hardware key codes are fed to handle_key(); screenshots go to frames/. */
#define HOST_TEST 1
#include "../../src/main.c"
#include <stdio.h>
#include <stdlib.h>

static int failures, checks;
#define CHECK(condition, message) do { \
    checks++; \
    if (!(condition)) { fprintf(stderr, "FAIL: %s (line %d)\n", message, __LINE__); failures++; } \
    else printf("ok: %s\n", message); \
} while (0)

static void screenshot(const char *name)
{
    char path[128];
    paint();
    snprintf(path, sizeof(path), "frames/%s.ppm", name);
    host_dump(path);
}

static void key(int k) { CHECK(handle_key(k) == 1, "key accepted"); }

static void clear_query(void)
{
    key(MK_F3);
    CHECK(query[0] == 0 && match_count > 0, "F3 clears query");
}

static void move_to(int row, int col)
{
    CHECK(focus == F_KB, "on-screen keyboard has focus");
    while (gy < row) key(MK_DOWN);
    while (gy > row) key(MK_UP);
    while (gx < col) key(MK_RIGHT);
    while (gx > col) key(MK_LEFT);
    CHECK(gy == row && gx == col, "arrow navigation reaches key");
}

static void press_at(int row, int col)
{
    move_to(row, col);
    key(MK_EXE);
}

static void first(char *k, char *body)
{
    CHECK(match_count > 0, "has search matches");
    get_match(0, k, body);
    printf("  [%s] -> %s\n", k, body);
}

static void test_word(const char *word, const char *translation)
{
    char k[KEY_BYTES], b[BODY_BYTES];
    do_search(word);
    first(k, b);
    CHECK(strcmp(k, word) == 0, "exact entry is first");
    CHECK(strstr(b, translation) != NULL, "expected translation present");
}

static void scan_all_records(void)
{
    int s;
    char body[BODY_BYTES];
    for (s = 0; s < 2; s++) {
        unsigned i, n, longest = 0;
        dir_sec = s;
        n = sec_rec_n();
        for (i = 0; i < n; i++) {
            if (body_get(s, i, body) || !body[0]) {
                fprintf(stderr, "FAIL: unreadable record section=%d id=%u\n", s, i);
                failures++;
                return;
            }
            if (strlen(body) > longest) longest = (unsigned)strlen(body);
        }
        printf("Scanned all %u records of section %d; longest=%u bytes\n", n, s, longest);
        checks++;
    }
    dir_sec = 0;
}

/* Verify that wrapping keeps every non-space byte in order, never splits
   UTF-8, and fits within the actual on-screen column count. */
static int layout_valid(const EntryLayout *e)
{
    int i, previous = 0;
    int len = (int)strlen(e->text);
    if (!e->count || e->count > ENTRY_MAX_LINES) return 0;
    for (i = 0; i < e->count; i++) {
        const char *p, *end;
        int columns = 0;
        if (e->start[i] < previous || e->end[i] <= e->start[i] ||
            e->end[i] > len) return 0;
        while (previous < e->start[i])
            if (e->text[previous++] != ' ') return 0;
        if (((unsigned char)e->text[e->start[i]] & 0xC0) == 0x80) return 0;
        if (e->end[i] < len &&
            ((unsigned char)e->text[e->end[i]] & 0xC0) == 0x80) return 0;
        p = e->text + e->start[i]; end = e->text + e->end[i];
        while (p < end) { utf8_next(&p); columns++; }
        if (p != end || columns > ENTRY_COLS) return 0;
        previous = e->end[i];
    }
    while (previous < len) if (e->text[previous++] != ' ') return 0;
    return 1;
}

static int count_label(const char *(*keys)[10], const char *label)
{
    int r, c, n = 0;
    for (r = 0; r < 4; r++)
        for (c = 0; c < 10; c++)
            if (!strcmp(keys[r][c], label)) n++;
    return n;
}

static void test_keyboard_layout(void)
{
    int language, r, c, ok = 1;
    CHECK(count_label(kb_en, "sp") == 1 && count_label(kb_en, "<-") == 1 &&
          count_label(kb_en, "-") == 1 && count_label(kb_en, "RU") == 1,
          "EN keyboard has one space, backspace, hyphen and language key");
    CHECK(count_label(kb_ru, "sp") == 1 && count_label(kb_ru, "<-") == 1 &&
          count_label(kb_ru, "-") == 1 && count_label(kb_ru, "EN") == 1,
          "RU keyboard has one space, backspace, hyphen and language key");
    for (language = 0; language < 2; language++) {
        kbd_lang = language;
        for (r = 0; r < 4; r++) {
            int total = 0;
            for (c = 0; c < 10; c++) {
                int owner = kb_owner(r, c), span = kb_span(r, owner);
                if (owner > c || owner + span <= c || !*kb_cell(r, owner)) ok = 0;
                if (*kb_cell(r, c)) total += kb_span(r, c);
            }
            if (total != 10) ok = 0;
        }
    }
    CHECK(ok, "wide key cells have valid, non-overlapping owners");
    kbd_lang = 0; focus = F_KB; gy = 3; gx = 0;
    query_clear();
    key(MK_EXE);
    CHECK(!strcmp(query, " "), "wide space bar adds one space");
    key(MK_RIGHT);
    CHECK(gx == 5, "right arrow jumps directly to wide backspace");
    key(MK_EXE);
    CHECK(!query[0], "wide backspace removes a glyph");
    key(MK_RIGHT);
    CHECK(gx == 8, "right arrow jumps directly to language switch");
    key(MK_EXE);
    CHECK(kbd_lang == 1 && gy == 3 && gx == 9 && !strcmp(kb_cell(gy, gx), "EN"),
          "on-screen switch selects the RU layout and its language key");
    key(MK_EXE);
    CHECK(kbd_lang == 0 && gy == 3 && gx == 8 && !strcmp(kb_cell(gy, gx), "RU"),
          "on-screen switch returns to the EN layout");
    key(MK_LEFT); key(MK_LEFT);
    CHECK(gx == 0, "left arrow skips the same wide keys in reverse");
    gx = gy = 0; query_clear();
}

static void test_pagination_and_phrases(void)
{
    int ok = 1, page_total = 0, i, last_page;
    PageStart expected;
    EntryLayout e;
    char k[KEY_BYTES], b[BODY_BYTES];

    strcpy(query, "to"); dir_sec = 0; focus = F_KB;
    do_search(query);
    CHECK(match_count == 385, "to search finds the 385 stored results");
    screenshot("11_to_first");
    expected.entry = expected.line = 0;
    for (;;) {
        PageStart start = page_starts[page_no];
        PageInfo info = visit_page(start, 0);
        if (start.entry != expected.entry || start.line != expected.line ||
            info.used < 1 || info.used > ROWS) { ok = 0; break; }
        /* Independently advance through exactly the number of displayed lines. */
        for (i = 0; i < info.used; i++) {
            if (expected.entry >= match_count) { ok = 0; break; }
            get_match(expected.entry, k, b);
            layout_entry(&e, k, b);
            if (!layout_valid(&e) || expected.line >= e.count) { ok = 0; break; }
            expected.line++;
            if (expected.line == e.count) { expected.entry++; expected.line = 0; }
        }
        if (!ok || expected.entry != info.next.entry || expected.line != info.next.line) {
            ok = 0; break;
        }
        page_total++;
        if (expected.entry >= match_count) break;
        if (!page_forward() || page_total > MATCH_CAP * 4) { ok = 0; break; }
        if (page_total == 1) screenshot("12_to_next");
    }
    CHECK(ok && expected.entry == match_count && expected.line == 0 && page_total > 1,
          "F6 pages cover every stored line exactly once through all to results");
    CHECK(!page_forward(), "F6 stops cleanly after last page");
    last_page = page_no;
    while (page_no > 0) {
        PageStart was = page_starts[page_no];
        PageInfo prev;
        if (!page_backward()) { ok = 0; break; }
        prev = visit_page(page_starts[page_no], 0);
        if (prev.next.entry != was.entry || prev.next.line != was.line) ok = 0;
    }
    CHECK(ok && page_no == 0 && last_page > 0,
          "F5 reverses every page boundary without skipping");

    focus = F_LIST; sel = 0;
    for (i = 1; i < match_count; i++) {
        PageInfo visible;
        handle_key(MK_DOWN);
        visible = visit_page(page_starts[page_no], 0);
        if (sel != i || i < page_starts[page_no].entry || i > visible.last_entry) ok = 0;
    }
    for (i = match_count - 2; i >= 0; i--) {
        PageInfo visible;
        handle_key(MK_UP);
        visible = visit_page(page_starts[page_no], 0);
        if (sel != i || i < page_starts[page_no].entry || i > visible.last_entry) ok = 0;
    }
    CHECK(ok && sel == 0, "up/down selection crosses variable-height pages in both directions");

    strcpy(query, "to a t"); do_search(query); first(k, b);
    CHECK(!strcmp(k, "to a t") && strstr(b, "комар носа не подточит"),
          "to a t is a complete headword with the full stored translation");
    layout_entry(&e, k, b);
    CHECK(layout_valid(&e) && e.count > 1 &&
          strstr(e.text, "to a t [phrase]:") && strstr(e.text, "подточит"),
          "multi-word key is labelled and its entire saved translation wraps");
    strcpy(query, "to be continued"); do_search(query); first(k, b);
    layout_entry(&e, k, b);
    CHECK(layout_valid(&e) && strstr(e.text, "phr. продолжение следует") &&
          !strstr(e.text, "[phrase]"), "existing phrase POS label is preserved without duplicate");
    focus = F_KB; gx = gy = 0; query_clear();
}

/* The production TDIC has at most five lines per entry. A tiny test-only
   TDIC exercises multi-page articles and detail scrolling as well. */
static int run_synthetic(void)
{
    char keybuf[KEY_BYTES], body[BODY_BYTES];
    EntryLayout e;
    PageInfo p0, p1;
    int ok = 1, x, y;
    vram = GetVRAMAddress();
    CHECK(dict_open() == 0, "synthetic dictionary opens");
    if (dict_fd < 0) return 2;
    dir_sec = 0; focus = F_KB; view = V_LIST; do_search("");
    get_match(0, keybuf, body);
    layout_entry(&e, keybuf, body);
    CHECK(strlen(keybuf) == 63 && e.count > DETAIL_ROWS && layout_valid(&e) &&
          strstr(e.text, "end"), "long key and body wrap without clipping or broken UTF-8");
    p0 = visit_page(page_starts[0], 0);
    CHECK(p0.used == ROWS && p0.next.entry == 0 && p0.next.line == ROWS,
          "oversize list entry continues at the correct line on page 2");
    CHECK(page_forward() && page_starts[1].entry == 0 && page_starts[1].line == ROWS,
          "F6 opens the continuation of the same list entry");
    p1 = visit_page(page_starts[1], 0);
    CHECK(p1.next.entry == match_count && p1.next.line == 0 &&
          p1.last_entry == match_count - 1, "continuation page also includes following entries");
    paint();
    /* Page two contains five continuation lines, followed by "b: long".
       The g glyph has ink in its bottom row; the separator must not erase it. */
    CHECK(e.count == 12 &&
          vram[(LIST_Y + 5 * ROW_H + 13) * VRAM_W + (6 + 6 * CELL_W + 3)] == COL_BLACK,
          "list separator does not erase bottom pixels of descenders");
    CHECK(!page_forward() && page_backward() && page_no == 0,
          "oversize list page navigates backward reliably");
    focus = F_LIST; sel = 0;
    CHECK(handle_key(MK_EXE) == 1 && view == V_DETAIL && det_top == 0,
          "oversize entry opens detail at its first line");
    CHECK(handle_key(MK_F6) == 1 && det_top == DETAIL_ROWS,
          "detail F6 shows the rest of an article taller than a screen");
    CHECK(handle_key(MK_F5) == 1 && det_top == 0, "detail F5 returns to the start");
    CHECK(handle_key(MK_DOWN) == 1 && det_top == 1 &&
          handle_key(MK_UP) == 1 && det_top == 0, "detail arrows scroll by one line");
    paint(); handle_key(MK_EXIT); paint();
    for (y = KB_Y + 3 * (ROW_H + 1) + ROW_H; y < FKEY_Y - 1; y++)
        for (x = 0; x < VIS_W; x++) if (vram[y * VRAM_W + x] != COL_WHITE) ok = 0;
    CHECK(ok, "returning from detail clears the area below the keyboard");
    Bfile_CloseFile_OS(dict_fd); dict_fd = -1;
    printf("Synthetic: %d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}

int main(void)
{
    char k[KEY_BYTES], b[BODY_BYTES];
    const char *russian[] = {
      "а","б","в","г","д","е","ё","ж","з","и","й","к","л","м","н","о","п",
      "р","с","т","у","ф","х","ц","ч","ш","щ","ъ","ы","ь","э","ю","я" };
    unsigned i;

    if (getenv("TDIC_SYNTHETIC")) return run_synthetic();
    vram = GetVRAMAddress();
    CHECK(dict_open() == 0, "dictionary opens");
    if (dict_fd < 0) return 2;
    CHECK(D.en_n > 50000 && D.ru_n > 70000, "both large dictionaries present");
    host_bfile_reads = 0;
    do_search(query);
    CHECK(host_bfile_reads == 0, "empty search needs no file scans");
    host_bfile_reads = 0;
    do_search("c");
    CHECK(host_bfile_reads <= 75, "prefix search uses logarithmic file reads");
    do_search(query);
    CHECK(focus == F_KB && kbd_lang == 0 && alpha_lock == 1, "ready to type English");
    CHECK(match_count == MATCH_CAP && match_capped, "empty prefix capped at 500");
    screenshot("01_start");
    test_keyboard_layout();
    test_pagination_and_phrases();

    /* Real Casio A-Z keycodes: c = LN, a = XOT, t = DIV. */
    key(MK_LN); key(MK_XOT); key(MK_DIV);
    CHECK(!strcmp(query, "cat"), "physical EN keys type cat");
    first(k, b);
    CHECK(!strcmp(k, "cat") && strstr(b, "кошка"), "cat -> кошка");
    screenshot("02_cat_list");
    key(MK_OPTN);
    CHECK(focus == F_LIST, "OPTN focuses results");
    key(MK_EXE);
    CHECK(view == V_DETAIL && strstr(det_body, "кошка"), "EXE opens article");
    screenshot("03_cat_detail");
    key(MK_EXIT);
    CHECK(view == V_LIST, "EXIT leaves article");

    clear_query();
    key(MK_LOG); key(MK_9); key(MK_9); key(MK_COMMA); /* book */
    CHECK(!strcmp(query, "book"), "physical EN keys type book");
    first(k, b);
    CHECK(!strcmp(k, "book") && strstr(b, "n.") && strstr(b, "v."),
          "book keeps noun and verb");
    key(MK_EXE);
    screenshot("04_book_grammar");
    key(MK_EXIT);

    clear_query();
    key(MK_6); key(MK_1); key(MK_8); /* run */
    first(k, b);
    CHECK(!strcmp(k, "run") && strstr(b, "n.") && strstr(b, "v.") && strstr(b, "бежать"),
          "run contains noun, verb and бежать");
    test_word("computer", "компьютер");
    test_word("apple", "яблоко");
    test_word("hello", "привет");

    clear_query();
    key(MK_ALPHA); key(MK_5);
    CHECK(!strcmp(query, "5"), "ALPHA switches physical keys to digits");
    clear_query(); key(MK_ALPHA);
    key(MK_0);
    CHECK(!strcmp(query, "z"), "ALPHA switches back to physical letters");

    /* F1 switches language, resets the query and activates the RU keyboard. */
    key(MK_F1);
    CHECK(dir_sec == 1 && kbd_lang == 1 && focus == F_KB && !query[0],
          "F1 opens clean Russian search");
    screenshot("05_ru_keyboard");
    for (i = 0; i < sizeof(russian)/sizeof(russian[0]); i++) {
        int row, col, found = 0;
        for (row = 0; row < 4; row++)
            for (col = 0; col < 10; col++)
                if (!strcmp(kb_ru[row][col], russian[i])) found = 1;
        if (!found) {
            fprintf(stderr, "FAIL: Russian key missing: %s\n", russian[i]);
            failures++;
        }
    }
    checks++;
    printf("Checked all 33 Cyrillic letters on the keyboard\n");

    /* Navigate with the actual arrow handlers to type жук. */
    press_at(1, 9); press_at(0, 2); press_at(0, 3);
    CHECK(!strcmp(query, "жук"), "arrow keyboard types жук");
    first(k, b);
    CHECK(!strcmp(k, "жук") && strstr(b, "beetle"), "жук -> beetle");
    screenshot("06_ru_word");

    clear_query();
    press_at(0, 3); press_at(1, 6); press_at(2, 5); /* кот */
    CHECK(!strcmp(query, "кот"), "arrow keyboard types кот");
    first(k, b);
    CHECK(!strcmp(k, "кот") && strstr(b, "cat"), "keyboard кот -> cat");

    clear_query();
    press_at(2, 9); press_at(1, 7); press_at(0, 4); press_at(2, 7); /* хлеб */
    CHECK(!strcmp(query, "хлеб"), "Russian х is typeable");
    first(k, b);
    CHECK(!strcmp(k, "хлеб") && strstr(b, "bread"), "хлеб -> bread");

    clear_query();
    press_at(1, 6); press_at(2, 7); press_at(3, 0); press_at(0, 4);
    press_at(0, 3); press_at(2, 5); /* объект */
    CHECK(!strcmp(query, "объект"), "Russian ъ is typeable");
    first(k, b);
    CHECK(!strcmp(k, "объект"), "объект has a dictionary entry");

    clear_query();
    press_at(3, 2); press_at(0, 3); press_at(1, 5); press_at(1, 3); press_at(0, 5);
    CHECK(!strcmp(query, "экран"), "Russian э is typeable");
    first(k, b);
    CHECK(!strcmp(k, "экран") && strstr(b, "screen"), "экран -> screen");

    clear_query();
    press_at(2, 2); press_at(1, 3); press_at(2, 3); press_at(1, 6);
    press_at(1, 7); press_at(3, 1); press_at(2, 5); /* самолёт */
    CHECK(!strcmp(query, "самолёт"), "Russian ё is typeable");
    first(k, b);
    CHECK(!strcmp(k, "самолёт") && strstr(b, "plane"), "самолёт -> airplane");
    screenshot("07_russian_airplane");
    key(MK_OPTN); key(MK_EXE);
    CHECK(view == V_DETAIL, "Russian result detail opens");
    screenshot("08_russian_detail");
    key(MK_EXIT);
    key(MK_F4);
    CHECK(view == V_HELP, "help opens");
    screenshot("09_help");
    key(MK_EXIT);

    test_word("самолет", "plane"); /* fallback spelling without ё */
    test_word("кот", "cat");
    test_word("говорить", "speak");

    key(MK_F1);
    CHECK(dir_sec == 0 && kbd_lang == 0 && !query[0], "F1 returns to clean English search");
    do_search("zebras");
    CHECK(match_count > 0 && match_morph == 1, "unknown form offers approximate stem");
    first(k, b);
    CHECK(!strcmp(k, "zebra") && strstr(b, "зебра"), "zebras stem suggests zebra");
    screenshot("10_stem");

    scan_all_records();
    CHECK(handle_key(MK_MENU) == 0, "MENU requests clean exit");
    Bfile_CloseFile_OS(dict_fd);
    dict_fd = -1;
    printf("%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
