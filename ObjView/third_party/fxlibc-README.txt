# fxlibc: The FX C Library

This directory contains the sources of the FxLibc Library. See `CMakeLists.txt`
to see what release version you have.

The FxLibc is the standard system C library implementation for all Casio
fx calculators, and is an important part of what makes up programs on these
devices. It provides the system API for all programs written in C and
C-compatible languages such as C++ and Objective-C; the runtime facilities of
other programming languages use the C library to access the underlying operating
system.

---

## Dependencies

FxLibc requires a GCC compiler toolchain the PATH to build for any calculator.
You cannot build with your system compiler! The tutorial on Planète Casio
builds an `sh-elf` toolchain that supports all models using multilib. See also
[Lephenixnoir/sh-elf-gcc](/Lephenixnoir/sh-elf-gcc).

For Vhex targets, the headers of the kernel are also required (but not for gint; the fxlibc is installed before gint).

---

## Building and installing FxLibc

FxLibc supports several targets:
* Vhex on SH targets (`vhex-sh`)
* CASIOWIN for fx-9860G-like calculators (`casiowin-fx`)
* CASIOWIN for fx-CG-series calculators (`casiowin-cg`)
* gint for all calculators (`gint`)

Each target supports different features depending on what the kernel/OS
provides.

For automated gint/fxSDK setups using [GiteaPC](/Lephenixnoir/GiteaPC) is recommended. The instructions below are for manual installs.

#### Configuration

Configure with CMake; specify the target with `-DFXLIBC_TARGET`. For SH
platforms, set the toolchain to `cmake/toolchain-sh.cmake`.

You can either install FxLibc in the compiler's `include` folder (for Vhex), or another folder of your choice (eg. the fxSDK sysroot). If you choose non-standard folders you might need `-I` and `-L` options to use the library.

```bash
# Install in the compiler's include folder
% PREFIX="$(sh-elf-gcc -print-file-name=.)"
# Install in the fxSDK sysroot
% PREFIX="$(fxsdk path sysroot)"
# Custom target
% PREFIX="$HOME/.sh-prefix/"
# For gint, do not specify anything, the fxSDK will be used dynamically
```

Example for a static Vhex build:

```bash
% cmake -B build-vhex-sh -DFXLIBC_TARGET=vhex-sh -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-sh.cmake -DCMAKE_INSTALL_PREFIX="$PREFIX"
```

Or for a traditional gint/fxSDK build:

```bash
% cmake -B build-gint -DFXLIBC_TARGET=gint -DCMAKE_TOOLCHAIN_FILE=cmake/toolchain-sh.cmake
```

#### Build and install

Build in the directory specified in `cmake -B`.

```bash
% make -C build-X
% make -C build-X install
```

---

### Contributing
Bug reports, feature suggestions and especially code contributions are most
welcome.

If you are interested in doing a port, or want to contribute to this project,
please, try to respect these constraints:
* Document your code.
* One function per file (named like the function).
* Use the same formatting conventions as the rest of the code.
* Only use hardware-related code (DMA, SPU, etc) in target-specified files
  when the target explicitly supports it.

---

### Using FxLibc

Include headers normally (`#include <stdio.h>`); on SH platforms where
`sh-elf-gcc` is used, link with `-lc` (by default the `-nostdlib` flag is
used for a number of reasons).

If you're installing in a custom folder, you also need `-I "$PREFIX/include"`
and `-L "$PREFIX/lib"`. If you're installing in the GCC install folder, you
don't need `-I` but you still need the `-L` as the default location for
libraries is at the root instead of in a `lib` subfolder.

---

### Licences
This work is licensed under a CC0 1.0 Universal License. To view a copy of this
license, visit: https://creativecommons.org/publicdomain/zero/1.0/legalcode.txt
Or see the LICENSE file.

FxLibc also includes third-party code that is distributed under its own
license. Currently, this includes:

* A stripped-down version of the [TinyMT random number generator](http://www.math.sci.hiroshima-u.ac.jp/~m-mat/MT/TINYMT/index.html)
  ([GitHub repository](https://github.com/MersenneTwister-Lab/TinyMT)) by
  Mutsuo Saito and Makoto Matsumoto. See `3rdparty/tinymt32/LICENSE.txt`.
* A stripped-down version of the [Grisu2b floating-point representation
  algorithm](https://www.cs.tufts.edu/~nr/cs257/archive/florian-loitsch/printf.pdf)
  with α=-59 and γ=-56, by Florian Loitsch. See `3rdparty/grisu2b_59_56/README`
  for details, and [the original code here](https://drive.google.com/open?id=0BwvYOx00EwKmejFIMjRORTFLcTA).

---

### Special thanks to
* Lephenixnoir - For all <3
* Kristaba - For the idea with the shared libraries workaround !
