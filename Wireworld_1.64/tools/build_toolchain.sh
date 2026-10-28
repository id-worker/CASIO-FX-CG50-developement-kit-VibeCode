#!/bin/sh
# Optional Debian/Ubuntu helper for the Prizm big-endian SH toolchain.
# See README.md first; the resulting SDK is *not* needed to run the .g3a.
# Downloads source archives and may take a long time; build as a normal user.
set -eu
PREFIX=${PREFIX:-/var/tmp/pt/cross}
SDK=${FXCGSDK:-/var/tmp/pt/libfxcg}
SRC=${SRC:-/var/tmp/pt/src}
JOBS=${JOBS:-2}
export PATH="$PREFIX/bin:$PATH"
mkdir -p "$PREFIX/bin" "$SRC"
cd "$SRC"
if [ ! -x "$PREFIX/bin/sh3eb-elf-gcc" ]; then
    [ -f binutils-2.42.tar.xz ] || curl -fL --retry 3 -o binutils-2.42.tar.xz https://ftp.gnu.org/gnu/binutils/binutils-2.42.tar.xz
    [ -f gcc-12.4.0.tar.xz ] || curl -fL --retry 3 -o gcc-12.4.0.tar.xz https://ftp.gnu.org/gnu/gcc/gcc-12.4.0/gcc-12.4.0.tar.xz
    echo 'f6e4d41fd5fc778b06b7891457b3620da5ecea1006c6a4a41ae998109f85a800  binutils-2.42.tar.xz' | sha256sum -c -
    echo '704f652604ccbccb14bdabf3478c9511c89788b12cb3bbffded37341916a9175  gcc-12.4.0.tar.xz' | sha256sum -c -
    [ -d binutils-2.42 ] || tar xf binutils-2.42.tar.xz
    [ -d gcc-12.4.0 ] || tar xf gcc-12.4.0.tar.xz
    mkdir -p b-binutils b-gcc
    (cd b-binutils && ../binutils-2.42/configure --target=sh3eb-elf --prefix="$PREFIX" \
        --disable-nls --disable-werror --disable-gdb --disable-sim && make -j"$JOBS" && make install)
    (cd b-gcc && ../gcc-12.4.0/configure --target=sh3eb-elf --prefix="$PREFIX" \
        --enable-languages=c --without-headers --with-newlib --disable-nls --disable-threads \
        --disable-shared --disable-libssp --disable-libquadmath --disable-decimal-float \
        --disable-libgomp --disable-libsanitizer --disable-multilib && \
        make -j"$JOBS" all-gcc all-target-libgcc && make install-gcc install-target-libgcc)
fi
# This toolchain calls the big-endian archive libgcc.a; PrizmSDK expects libgcc_be.a.
LIBGCC=$(sh3eb-elf-gcc -print-libgcc-file-name)
[ -f "$LIBGCC" ] || { echo 'libgcc.a not found' >&2; exit 1; }
[ -e "$(dirname "$LIBGCC")/libgcc_be.a" ] || ln -s "$LIBGCC" "$(dirname "$LIBGCC")/libgcc_be.a"
if [ ! -f "$SDK/lib/libfxcg.a" ] || [ ! -f "$SDK/lib/libc.a" ]; then
    mkdir -p "$SDK"
    curl -fL --retry 3 -o "$SRC/libfxcg.tar.gz" https://codeload.github.com/Jonimoose/libfxcg/tar.gz/refs/heads/master
    tar xzf "$SRC/libfxcg.tar.gz" --strip-components=1 -C "$SDK"
    # Build without LTO so the SDK archives are compatible with this compiler.
    find "$SDK" -type f \( -name Makefile -o -name '*.mk' -o -name '*rules' \) -exec sed -i 's/-flto//g' {} +
    make -C "$SDK" -j"$JOBS"
fi
if [ ! -x "$PREFIX/bin/mkg3a" ]; then
    mkdir -p "$SRC/mkg3a-src" "$SRC/mkg3a-build"
    curl -fL --retry 3 -o "$SRC/mkg3a.tar.gz" https://gitlab.com/taricorp/mkg3a/-/archive/master/mkg3a-master.tar.gz
    tar xzf "$SRC/mkg3a.tar.gz" --strip-components=1 -C "$SRC/mkg3a-src"
    cmake -S "$SRC/mkg3a-src" -B "$SRC/mkg3a-build"
    cmake --build "$SRC/mkg3a-build" --parallel "$JOBS"
    BIN=$(find "$SRC/mkg3a-build" -type f -name mkg3a -print -quit)
    [ -n "$BIN" ] || { echo 'mkg3a not found' >&2; exit 1; }
    cp "$BIN" "$PREFIX/bin/mkg3a"
    chmod +x "$PREFIX/bin/mkg3a"
fi
printf '\nSDK ready. For this shell:\n  export FXCGSDK="%s"\n  export PATH="%s/bin:$PATH"\n' "$SDK" "$PREFIX"
