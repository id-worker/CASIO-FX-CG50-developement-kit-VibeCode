#!/bin/sh
#
# build_toolchain.sh -- rebuilds everything needed to produce a .g3a:
#
#   binutils + gcc for sh3eb-elf (the SH4A big endian CPU of the fx-CG50)
#   libfxcg   -- the Prizm syscall library, headers and linker script
#   mkg3a     -- packs a .bin into a .g3a with icons and a name
#
# Everything is built under /var/tmp/pt, which is scratch space: it does not
# survive a restart of this sandbox, so this script has to be run again in a
# new session before `make` works.  The .g3a it produces does survive.
#
#   sh tools/build_toolchain.sh        (takes about half an hour on 2 cores)
#
set -e

PREFIX=${PREFIX:-/var/tmp/pt/cross}
SDK=${SDK:-/var/tmp/pt/libfxcg}
SRC=${SRC:-/var/tmp/pt/src}
JOBS=${JOBS:-$(nproc)}

sudo apt-get install -y -qq build-essential texinfo bison flex \
    libgmp-dev libmpfr-dev libmpc-dev libpng-dev cmake git wget xz-utils

mkdir -p "$SRC"
export PATH="$PREFIX/bin:$PATH"

# ---------------------------------------------------------------- sources
cd "$SRC"
[ -f binutils-2.42.tar.xz ] ||
    wget -q https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz
[ -f gcc-12.4.0.tar.xz ] ||
    wget -q https://ftp.gnu.org/gnu/gcc/gcc-12.4.0/gcc-12.4.0.tar.xz
[ -d binutils-2.42 ] || tar xf binutils-2.42.tar.xz
[ -d gcc-12.4.0 ]    || tar xf gcc-12.4.0.tar.xz

# --------------------------------------------------------------- binutils
if [ ! -x "$PREFIX/bin/sh3eb-elf-as" ]; then
    echo "== binutils"
    mkdir -p "$SRC/b-binutils"
    cd "$SRC/b-binutils"
    ../binutils-2.42/configure \
        --target=sh3eb-elf --prefix="$PREFIX" \
        --disable-nls --disable-werror --disable-gdb --disable-sim
    make -j"$JOBS"
    make install
fi

# -------------------------------------------------------------------- gcc
if [ ! -x "$PREFIX/bin/sh3eb-elf-gcc" ]; then
    echo "== gcc"
    cd "$SRC/gcc-12.4.0"
    ./contrib/download_prerequisites
    mkdir -p "$SRC/b-gcc"
    cd "$SRC/b-gcc"
    ../gcc-12.4.0/configure \
        --target=sh3eb-elf --prefix="$PREFIX" \
        --enable-languages=c --without-headers --with-newlib \
        --disable-nls --disable-threads --disable-shared \
        --disable-libssp --disable-libquadmath --disable-decimal-float \
        --disable-libgomp --disable-libsanitizer --disable-multilib
    make -j"$JOBS" all-gcc all-target-libgcc
    make install-gcc install-target-libgcc
fi

# ---------------------------------------------------------------- libfxcg
if [ ! -f "$SDK/lib/libfxcg.a" ]; then
    echo "== libfxcg"
    mkdir -p /var/tmp/pt
    cd /var/tmp/pt
    [ -d libfxcg ] || git clone --depth 1 https://github.com/Jonimoose/libfxcg.git
    cd libfxcg
    make
    # the SDK layout the project Makefile expects: include/, lib/, toolchain/
    mkdir -p "$SDK/include" "$SDK/lib" "$SDK/toolchain"
    [ -f libc/libc.a ]       && cp -f libc/libc.a       "$SDK/lib/" || true
    [ -f libfxcg/libfxcg.a ] && cp -f libfxcg/libfxcg.a "$SDK/lib/" || true
    [ -f prizm.x ]           && cp -f prizm.x           "$SDK/toolchain/" || true
fi

# ----------------------------------------------------------------- mkg3a
if [ ! -x "$PREFIX/bin/mkg3a" ]; then
    echo "== mkg3a"
    cd "$SRC"
    [ -d mkg3a ] || git clone --depth 1 https://gitlab.com/taricorp/mkg3a.git
    cd mkg3a
    cmake .
    make
    if [ -f src/mkg3a ]; then cp src/mkg3a "$PREFIX/bin/"; else cp mkg3a "$PREFIX/bin/"; fi
fi

echo "== done"
ls -l "$PREFIX/bin/sh3eb-elf-gcc" "$PREFIX/bin/mkg3a" "$SDK/lib/libfxcg.a"
