#!/bin/sh
# Builds LibreSprite's third-party dependencies for the PS Vita from source
# and installs them into $VITASDK/arm-vita-eabi.
#
# With a regular VitaSDK install most of these are also available through
# vdpm (zlib libpng libjpeg-turbo freetype pixman TinyXML2 libarchive sdl2
# sdl2_image); giflib is the one that always has to be built here.
#
# Usage: VITASDK=/usr/local/vitasdk ./build-deps.sh <source-dir>
#   <source-dir> must contain checkouts named: zlib libpng libjpeg-turbo
#   freetype giflib pixman tinyxml2 libarchive SDL2 SDL2_image
set -eu

: "${VITASDK:?set VITASDK to your VitaSDK install}"
SRC=$(cd "${1:?usage: build-deps.sh <source-dir>}" && pwd)
PREFIX="$VITASDK/arm-vita-eabi"
TOOLCHAIN="$VITASDK/share/vita.toolchain.cmake"
BUILD="${BUILD_DIR:-$SRC/_vita_build}"
JOBS="${JOBS:-$(nproc)}"
export PATH="$VITASDK/bin:$PATH"

mkdir -p "$BUILD"

# Optional: ONLY="name1 name2" builds just those dependencies.
want() { [ -z "${ONLY:-}" ] || echo " $ONLY " | grep -q " $1 "; }

cmake_dep() {
  name=$1; shift
  want "$name" || return 0
  echo "==> $name"
  cmake -S "$SRC/$name" -B "$BUILD/$name" -G "Unix Makefiles" \
    -DCMAKE_TOOLCHAIN_FILE="$TOOLCHAIN" \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
    "$@" > "$BUILD/$name.log" 2>&1
  cmake --build "$BUILD/$name" -j"$JOBS" >> "$BUILD/$name.log" 2>&1
  cmake --install "$BUILD/$name" >> "$BUILD/$name.log" 2>&1
}

cmake_dep zlib -DZLIB_BUILD_EXAMPLES=OFF
# zlib's CMake always builds a shared lib too; keep only the static one.
rm -f "$PREFIX"/lib/libz.so*

cmake_dep libpng -DPNG_SHARED=OFF -DPNG_STATIC=ON -DPNG_TESTS=OFF \
  -DPNG_TOOLS=OFF -DPNG_ARM_NEON=off -DZLIB_ROOT="$PREFIX"

cmake_dep libjpeg-turbo -DENABLE_SHARED=OFF -DENABLE_STATIC=ON \
  -DWITH_SIMD=OFF -DWITH_TURBOJPEG=OFF

cmake_dep freetype -DFT_DISABLE_HARFBUZZ=ON -DFT_DISABLE_BROTLI=ON \
  -DFT_DISABLE_BZIP2=ON -DFT_DISABLE_PNG=ON -DFT_REQUIRE_ZLIB=ON

cmake_dep tinyxml2 -Dtinyxml2_BUILD_TESTING=OFF

cmake_dep libarchive -DENABLE_TEST=OFF -DENABLE_TAR=OFF -DENABLE_CPIO=OFF \
  -DENABLE_CAT=OFF -DENABLE_UNZIP=OFF -DENABLE_OPENSSL=OFF -DENABLE_MBEDTLS=OFF \
  -DENABLE_NETTLE=OFF -DENABLE_LIBB2=OFF -DENABLE_LZ4=OFF -DENABLE_LZO=OFF \
  -DENABLE_LZMA=OFF -DENABLE_ZSTD=OFF -DENABLE_BZip2=OFF -DENABLE_LIBXML2=OFF \
  -DENABLE_EXPAT=OFF -DENABLE_PCREPOSIX=OFF -DENABLE_PCRE2POSIX=OFF \
  -DENABLE_CNG=OFF -DENABLE_XATTR=OFF -DENABLE_ACL=OFF -DENABLE_ICONV=OFF \
  -DENABLE_WERROR=OFF -DZLIB_ROOT="$PREFIX" \
  -DCMAKE_C_FLAGS="-std=gnu11 -Dtimezone=_timezone -Wno-error=implicit-function-declaration -Wno-error=int-conversion -Wno-error=incompatible-pointer-types"
rm -f "$PREFIX"/lib/libarchive.so*

# giflib has no CMake/autotools build: compile the library sources directly.
want giflib && (
  echo "==> giflib"
  mkdir -p "$BUILD/giflib"
  cd "$BUILD/giflib"
  for f in dgif_lib egif_lib gifalloc gif_err gif_font gif_hash openbsd-reallocarray; do
    arm-vita-eabi-gcc -O2 -std=gnu99 -c "$SRC/giflib/$f.c" -o "$f.o"
  done
  arm-vita-eabi-ar rcs libgif.a ./*.o
  install -m644 libgif.a "$PREFIX/lib/"
  install -m644 "$SRC/giflib/gif_lib.h" "$PREFIX/include/"
)

# pixman: plain C paths only (its ARM SIMD assembly does not assemble with
# recent GCC without patching).
want pixman && (
  echo "==> pixman"
  cd "$SRC/pixman"
  [ -f configure ] || NOCONFIGURE=1 ./autogen.sh > "$BUILD/pixman.log" 2>&1
  mkdir -p "$BUILD/pixman" && cd "$BUILD/pixman"
  "$SRC/pixman/configure" --host=arm-vita-eabi --prefix="$PREFIX" \
    --disable-shared --enable-static --disable-arm-simd --disable-arm-neon \
    --disable-arm-iwmmxt --disable-arm-a64-neon --disable-gtk --disable-libpng \
    --disable-openmp CFLAGS="-O2 -std=gnu11" >> "$BUILD/pixman.log" 2>&1
  make -C pixman -j"$JOBS" >> "$BUILD/pixman.log" 2>&1
  make -C pixman install >> "$BUILD/pixman.log" 2>&1
  make install-pkgconfigDATA >> "$BUILD/pixman.log" 2>&1 || true
)

cmake_dep SDL2 -DSDL_SHARED=OFF -DSDL_STATIC=ON -DSDL_TEST=OFF \
  -DVIDEO_VITA_PIB=OFF -DVIDEO_VITA_PVR=OFF

cmake_dep SDL2_image -DSDL2IMAGE_SAMPLES=OFF -DSDL2IMAGE_TESTS=OFF \
  -DSDL2IMAGE_BACKEND_STB=ON -DSDL2IMAGE_VENDORED=OFF -DSDL2IMAGE_DEPS_SHARED=OFF \
  -DCMAKE_POSITION_INDEPENDENT_CODE=OFF -DSDL2IMAGE_QOI=OFF -DSDL2IMAGE_AVIF=OFF -DSDL2IMAGE_JXL=OFF -DSDL2IMAGE_TIF=OFF \
  -DSDL2IMAGE_WEBP=OFF -DSDL2IMAGE_PNG=ON -DSDL2IMAGE_JPG=ON \
  -DSDL2_DIR="$PREFIX/lib/cmake/SDL2"

echo "All dependencies installed into $PREFIX"
