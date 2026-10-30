# Third-party notices for GraphLab 1.07

GraphLab's original application sources, tests and icons are covered by the
project's `LICENSE` (MIT). The linked libraries and toolchain components below
are **not** being relicensed as GraphLab's MIT code. The exact upstream texts
and file-level notices accompany this file under `third_party/`.

The distributed G3A was built with this independently packaged SDK:

- gint `463f180ffb71ccdcef05c928ad19f2a480b14bc1` (2.11.0-91):
  https://git.planet-casio.com/Lephenixnoir/gint . Its README includes the
  author's free-software permission. `gint-README.txt` reproduces that README.
  The linked binary includes gint's display, default font, keyboard, filesystem
  and runtime. The host test keycode values mirror gint's key matrix.
- FxLibc `a6903066cf4777aa96556a6da9699d4704867185`:
  https://git.planet-casio.com/Vhex-Kernel-Core/fxlibc . Its own code is CC0;
  see `fxlibc-CC0.txt` and `fxlibc-README.txt`. The binary uses its Grisu2b
  formatting implementation by Florian Loitsch: `Grisu2b-LICENSE.txt` and
  `Grisu2b-README.txt` reproduce the upstream notices. The link map did not
  select TinyMT; the latter is a separate FxLibc component.
- OpenLibm `9828d2e3f265a882af6a5bba8ffe6a00c3eeb8d7`:
  https://git.planet-casio.com/Lephenixnoir/OpenLibm . The statically linked
  math routines have several upstream licenses and copyright notices. See
  `OpenLibm-LICENSE.md` and the verbatim `OpenLibm-file-notices.txt` extracted
  from the source files selected by the link map and their headers. OpenLibm's
  LGPL test files are not linked into this application.
- GCC 14.1.0 libgcc runtime is linked. The GCC Runtime Library Exception 3.1
  and GPLv3 texts are in `GCC-RUNTIME-EXCEPTION.txt` and `GPL-3.0.txt`.
- fxSDK `a6e46eb6df060dc4e6e931521205e516c92b6c7f` is used as a build tool;
  `fxSDK-LICENSE.txt` reproduces its MIT notice.

Upstream sources and version details are also included in the separately
provided fxSDK/WSL kit's `SOURCES_AND_LICENSES.txt`; do not replace the
third-party texts with the project's own MIT license.
