# Third-party notices and provenance — Wireworld 1.64

The project's own code is offered under the MIT terms in `LICENSE` (Copyright
(c) 2026 Contributors, Alex Work). The notices and qualifications below apply
separately to third-party material. In particular, the legacy menu-flag
routine below has **unresolved permissions**; this distribution must not be
represented as a fully MIT-cleared work for publication without further review.

## Legacy menu-flag routine — publication review required

`src/main.c:set_menu_return_flag()` is adapted from Simon Lothar's
`SetGetkeyToMainFunctionReturnFlag` and matches the version used in the fx-CG50
fork of Utilities (including the RAM range for 0x8C...). The original author's
routine is explicitly identified on WikiPrizm:

* https://prizm.cemetech.net/Useful_Routines/SetGetkeyToMainFunctionReturnFlag/
* https://github.com/gbl08ma/utilities/blob/master/src/keyboardProvider.cpp
* https://github.com/gbl08ma/utilities (GPL version 2 license in that project)

The WikiPrizm page does not specify an MIT grant for this routine. Its inclusion
in a GPL project does not by itself establish permission to publish the routine
under MIT. The exact, unmodified `LICENSE` from the Utilities copy is included
as `licenses/Utilities_GPL-2.0.txt` for reference; including it is not a claim
that the entire Wireworld add-in is separately cleared under GPL or MIT.
Resolve the original author's permissions and the fork's contribution rights,
or replace this routine with a suitably licensed alternative and re-test the
OS exit/re-entry path on hardware **before public redistribution**. The
routine is retained here to preserve the known 1.63 behavior during testing.

Earlier Wireworld 1.63 and PyCEdit/Translator builds use the same or similar
legacy routine; their own public-distribution terms should also be reviewed.

## gint menu-discovery method

`src/main.c:find_os_menu_function()` adapts the instruction-pattern search
used in gint's `src/kernel/osmenu.c`, with bounds and signed branch displacement
handling. The gint source credits Dr-Carlos for reverse engineering; upstream
and fork authorship/permission statements are preserved in
`licenses/gint_README.md` (copied without alteration from the inspected fork).
The inspected file is:

* https://github.com/cheparukhin/gint/blob/a62ba8a026cc44c787e46d69beecebe019cbde25/src/kernel/osmenu.c
* Upstream: https://git.planet-casio.com/Lephenixnoir/gint
* Dr-Carlos' investigation: https://www.cemetech.net/forum/viewtopic.php?t=18944

The cited gint README states that the software may be used, shared and modified
for any purpose. That statement is reproduced as published; it is not relabeled
as a standard MIT license. Wireworld uses libfxcg, not the gint runtime.

## DejaVu Sans Mono bitmap glyphs

`src/font_ww.h` contains 95 glyphs rasterized from DejaVu Sans Mono by
`tools/make_font.py`. The complete upstream font licensing text and notices are
in `licenses/DejaVu_fonts.txt`. The bitmap is named `font_ww`, not Bitstream
Vera or Arev. https://dejavu-fonts.github.io/

## libfxcg

The binary links libfxcg syscall stubs and its libc implementation.
Copyright (c) 2014, The libfxcg Project Contributors; BSD 3-Clause license,
reproduced without alteration in `licenses/libfxcg_BSD.txt`.
https://github.com/Jonimoose/libfxcg

## GCC runtime

The binary includes compiler helper code from GCC 12.4.0's libgcc. Applicable
libgcc files are licensed under GPL version 3 or later with the GCC Runtime
Library Exception 3.1. The exception and complete GPL version 3 text are in
`licenses/GCC_runtime_exception.txt` and `licenses/GPL-3.0.txt` respectively.
GCC source: https://ftp.gnu.org/gnu/gcc/gcc-12.4.0/gcc-12.4.0.tar.xz

## fxSDK / fxgxa

The G3A checksum layout used in `tools/fix_g3a.py` was cross-checked against
fxSDK's fxgxa and the output was independently verified by fxgxa. Its MIT
notice is reproduced in `licenses/fxSDK_MIT.txt`.
https://gitea.planet-casio.com/Lephenixnoir/fxsdk

## mkg3a

The build-time packager mkg3a, Copyright (c) 2011 Peter Marheine, is used to
create G3A files. Its original zlib-style notice is in
`licenses/mkg3a_zlib.txt`. https://gitlab.com/taricorp/mkg3a

## Hardware documentation and trademarks

RTC behavior is documented at https://prizm.cemetech.net/Syscalls/RTC/RTC_GetTicks/.
CASIO and fx-CG50 are trademarks of CASIO COMPUTER CO., LTD. This independent
project is not affiliated with or endorsed by CASIO. No CASIO ROM is bundled.
