Translator 1.10 — source and rebuilding (Linux)
================================================

The installation files Translator.g3a and TDIC.DAT sit at the ZIP root;
this source folder is not needed to install the app on a calculator.

Own code: ../LICENSE. Dictionary: ../LICENSE_DATA.txt. Dependencies and
notices: ../THIRD_PARTY_NOTICES.txt and ../licenses/.

Build the add-in:
  * Use a big-endian sh3eb-elf GCC toolchain, libfxcg and mkg3a.
    The shipped binary was built with GCC 12.4.0, libfxcg rebuilt without
    -flto, and mkg3a. A symlink libgcc_be.a -> libgcc.a may be necessary.
  * Set FXCGSDK to the libfxcg directory and put the cross-compiler and
    mkg3a on PATH. From this source directory run: make
  * tools/fix_g3a.py is called automatically by make after mkg3a; it
    finalizes both checksums. Verify using fxgxa -d Translator.g3a.

Rebuild the dictionary (requires Python 3):
  * Download/extract the original version 2025.11.23 TEI archives linked
    in ../LICENSE_DATA.txt; check their SHA-512 hashes.
  * python3 tools/make_dict.py --en /path/to/eng-rus.tei \
        --ru /path/to/rus-eng.tei --output dicts/TDIC.DAT
  * python3 tools/verify_dict.py dicts/TDIC.DAT
    This also produces TSVs for inspection, needed to regenerate the font.
  * To rebuild font_tr.h: pip install Pillow, install DejaVu Sans Mono or
    set TRANSLATOR_FONT to its TTF path, then python3 tools/make_font.py.

Host tests (Linux): sh tools/host/run_tests.sh. They exercise the real C UI,
search and decompression against TDIC.DAT at the ZIP root (or dicts/),
plus a generated synthetic test-only dictionary for oversized entries and
page/keyboard navigation. The test-generated file is deleted afterwards.
They do not emulate a SuperH CPU, the calculator's keyboard hardware,
OS syscalls, display controller or actual storage timing. Test on hardware.
