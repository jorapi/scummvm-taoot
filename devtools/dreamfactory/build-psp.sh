#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
src=$(cd "$(dirname "$0")/../.." && pwd)
: "${PSPDEV:?Install the PSPDEV toolchain and set PSPDEV}"
export PATH="$PSPDEV/bin:$PSPDEV/psp/bin:$PATH"
command -v psp-g++ >/dev/null
out="$src/build-dreamfactory/psp"
# Keep local usernames and source paths out of compiler diagnostics in binaries.
export CFLAGS="${CFLAGS:-} -ffile-prefix-map=$src=/scummvm -fdebug-prefix-map=$src=/scummvm"
export CXXFLAGS="${CXXFLAGS:-} -ffile-prefix-map=$src=/scummvm -fdebug-prefix-map=$src=/scummvm"
mkdir -p "$out" "$src/build-dreamfactory/artifacts"
cd "$out"
"$src/configure" --host=psp --disable-all-engines --enable-engine=dreamfactory \
  --enable-highres --disable-debug --enable-release
# Fail rather than silently packaging a build with the engine/TTF support omitted.
grep -Eq '^ENABLE_DREAMFACTORY[[:space:]]*[:?+]?=[[:space:]]*STATIC_PLUGIN' config.mk
grep -q '#define USE_FREETYPE2' config.h
grep -q '#define USE_RGB_COLOR' config.h
make -j3 scummvm_psp.zip
cp scummvm_psp.zip "$src/build-dreamfactory/artifacts/dreamfactory-psp-experimental.zip"
