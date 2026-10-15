#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
src=$(cd "$(dirname "$0")/../.." && pwd)
: "${PSPDEV:?Install the PSPDEV toolchain and set PSPDEV}"
export PATH="$PSPDEV/bin:$PSPDEV/psp/bin:$PATH"
command -v psp-g++ >/dev/null
out="$src/build-dreamfactory/psp"
mkdir -p "$out" "$src/build-dreamfactory/artifacts"
cd "$out"
"$src/configure" --host=psp --disable-all-engines --enable-engine=dreamfactory \
  --enable-highres --enable-16bit --disable-debug --enable-release
# Fail rather than silently packaging a build with the engine/TTF support omitted.
grep -Eq '^ENABLE_DREAMFACTORY[[:space:]]*[:?+]?=[[:space:]]*STATIC_PLUGIN' config.mk
grep -q '#define USE_FREETYPE2' config.h
make -j3 scummvm_psp.zip
cp scummvm_psp.zip "$src/build-dreamfactory/artifacts/dreamfactory-psp-experimental.zip"
