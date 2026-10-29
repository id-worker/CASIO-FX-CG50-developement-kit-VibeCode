Translator 1.11 — source and rebuilding (Linux)
================================================

The installation files are in the repository's releases directory and the
GitHub release; TDIC.DAT is unchanged from 1.10 and is in the parent directory.

Own code: ../LICENSE. Dictionary: ../LICENSE_DATA.txt. Dependencies and
notices: ../THIRD_PARTY_NOTICES.txt and ../licenses/.

Build the add-in:
  * Use a big-endian sh3eb-elf GCC toolchain, libfxcg and mkg3a.
    The 1.11 binary was built with GCC 14.1.0, libfxcg v0.6 rebuilt
    without -flto, and mkg3a. The Makefile detects libgcc_be or libgcc.
  * Set FXCGSDK to the libfxcg directory and put the cross-compiler and
    mkg3a on PATH. From this source directory run: make
  * tools/fix_g3a.py is called automatically by make after mkg3a; it
    embeds the two BMP icons as RGB565 and finalizes both checksums. Changing
    an icon triggers repackaging. Verify using fxgxa -d Translator.g3a.

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
