#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Linux host; the CI installs the SDK/NDK versions declared by this checkout.
set -euo pipefail
abi=${1:-arm64-v8a}
jobs=${DREAMFACTORY_BUILD_JOBS:-3}
src=$(cd "$(dirname "$0")/../.." && pwd)
out=${DREAMFACTORY_ANDROID_BUILD_DIR:-"$src/build-dreamfactory/android-$abi"}
: "${ANDROID_SDK_ROOT:?Set ANDROID_SDK_ROOT}"
: "${ANDROID_NDK_ROOT:?Set ANDROID_NDK_ROOT}"
case "$abi" in
  arm64-v8a) triplet=aarch64-linux-android; api=21;;
  armeabi-v7a) triplet=arm-linux-androideabi; api=16;;
  x86_64) triplet=x86_64-linux-android; api=21;;
  *) echo 'Supported Android ABIs: arm64-v8a, armeabi-v7a, x86_64' >&2; exit 2;;
esac
toolchain="$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/linux-x86_64"
export PATH="$toolchain/bin:$PATH"
cc="$triplet$api-clang"
if test "$abi" = armeabi-v7a; then cc="armv7a-linux-androideabi$api-clang"; fi
mkdir -p "$out" "$src/build-dreamfactory/artifacts"
if ! test -f "$out/oboe/lib/$abi/liboboe.a"; then
  curl --fail --location --retry 3 https://github.com/google/oboe/archive/refs/tags/1.10.0.tar.gz -o "$out/oboe.tar.gz"
  tar -xf "$out/oboe.tar.gz" -C "$out"
  cmake -S "$out/oboe-1.10.0" -B "$out/oboe-build" \
    -DCMAKE_TOOLCHAIN_FILE="$ANDROID_NDK_ROOT/build/cmake/android.toolchain.cmake" \
    -DANDROID_ABI="$abi" -DANDROID_PLATFORM="android-$api" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON -DCMAKE_INSTALL_PREFIX="$out/oboe"
  cmake --build "$out/oboe-build" -j "$jobs"
  cmake --install "$out/oboe-build"
fi
export CPPFLAGS="${CPPFLAGS:-} -I$out/oboe/include"
export LDFLAGS="${LDFLAGS:-} -L$out/oboe/lib/$abi"
if ! test -f "$out/freetype/lib/libfreetype.a"; then
  curl --fail --location --retry 3 https://download.savannah.gnu.org/releases/freetype/freetype-2.13.3.tar.xz -o "$out/freetype.tar.xz"
  tar -xf "$out/freetype.tar.xz" -C "$out"
  (cd "$out/freetype-2.13.3" && \
    CC="$cc" AR=llvm-ar RANLIB=llvm-ranlib CFLAGS='-O2 -fPIC' ./configure --host="$triplet" --prefix="$out/freetype" \
      --disable-shared --enable-static --with-pic --with-zlib=no --with-bzip2=no --with-png=no --with-harfbuzz=no --with-brotli=no && \
    make -j"$jobs" && make install)
fi
export PKG_CONFIG_LIBDIR="$out/freetype/lib/pkgconfig"
export PKG_CONFIG_PATH="$PKG_CONFIG_LIBDIR"
cd "$out"
hostabi=$abi
if test "$abi" = armeabi-v7a; then hostabi=arm-v7a; fi
"$src/configure" --host="android-$hostabi" --disable-all-engines --enable-engine=dreamfactory \
  --enable-highres --with-freetype2-prefix="$out/freetype" --disable-debug --enable-release
grep -Eq '^ENABLE_DREAMFACTORY[[:space:]]*[:?+]?=[[:space:]]*STATIC_PLUGIN' config.mk
grep -q '#define USE_FREETYPE2' config.h
grep -q '#define USE_RGB_COLOR' config.h
mkdir -p android_project/mainAssets/src/main/assets/doc
cp "$out/oboe-1.10.0/LICENSE" android_project/mainAssets/src/main/assets/doc/OBOE-LICENSE.txt
cp "$out/freetype-2.13.3/docs/FTL.TXT" android_project/mainAssets/src/main/assets/doc/FREETYPE-LICENSE.txt
revision=$(git -C "$src" rev-parse --short HEAD 2>/dev/null || printf snapshot)
make -j"$jobs" VER_REV="$revision-dreamfactory" androidrelease
test -s ScummVM-release-unsigned.apk
cp ScummVM-release-unsigned.apk "$src/build-dreamfactory/artifacts/dreamfactory-android-$abi-unsigned.apk"
