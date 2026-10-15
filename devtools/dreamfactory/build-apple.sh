#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
set -euo pipefail
platform=${1:?Usage: build-apple.sh macos|ios [arm64|x86_64]}
arch=${2:-arm64}
src=$(cd "$(dirname "$0")/../.." && pwd)
out="$src/build-dreamfactory/$platform-$arch"
# Keep local usernames and source paths out of compiler diagnostics in binaries.
export CFLAGS="${CFLAGS:-} -ffile-prefix-map=$src=/scummvm -fdebug-prefix-map=$src=/scummvm"
export CXXFLAGS="${CXXFLAGS:-} -ffile-prefix-map=$src=/scummvm -fdebug-prefix-map=$src=/scummvm"
mkdir -p "$out" "$src/build-dreamfactory/artifacts"
# The CMake generator target does not set POSIX itself. Xcode generation needs
# it to resolve output paths for generated engine headers and XCFrameworks.
cmake -S "$src/devtools/create_project/cmake" -B "$src/build-dreamfactory/generator" -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS=-DPOSIX
cmake --build "$src/build-dreamfactory/generator" --config Release --parallel 3
generator="$src/build-dreamfactory/generator/create_project"
# Match the minimal, native Windows engine build rather than enabling unrelated libraries.
disabled=()
while IFS= read -r feature; do disabled+=("--disable-$feature"); done < <("$generator" 2>&1 | awk '$1 == "enabled" && $2 == "|" {print $3}')
# The official static iOS FreeType also references PNG and bzip2 (llvm-nm).
flags=(--xcode --output-dir "$out" --include-dir "$out" --disable-all-engines --enable-engine=dreamfactory "${disabled[@]}" --enable-opengl --enable-builtin-resources --enable-highres --enable-16bit --enable-freetype2 --enable-zlib --enable-png --enable-detection-static)
case "$platform" in
  macos)
    case "$arch" in arm64|x86_64) ;; *) echo 'Unsupported macOS architecture' >&2; exit 2;; esac
    "$generator" "$src" "${flags[@]}"
    xcodebuild -project "$out/scummvm.xcodeproj" -scheme ScummVM-macOS -configuration Release \
      -destination "platform=macOS,arch=$arch" -derivedDataPath "$out/DerivedData" \
      CODE_SIGNING_ALLOWED=NO CODE_SIGN_IDENTITY="" \
      "OTHER_CFLAGS=\$(inherited) $CFLAGS" "OTHER_CPLUSPLUSFLAGS=\$(inherited) $CXXFLAGS" build
    app="$out/DerivedData/Build/Products/Release/scummvm.app"
    test -d "$app"
    # Homebrew paths must not leak into a supposedly portable application.
    command -v dylibbundler >/dev/null
    dylibbundler -od -b -x "$app/Contents/MacOS/scummvm" \
      -d "$app/Contents/Frameworks" -p @executable_path/../Frameworks/
    cp "$src/dists/engine-data/fonts.dat" "$app/Contents/Resources/"
    # Library relocation invalidates Mach-O signatures. Ad-hoc signing needs
    # no Apple account/certificate and permits execution on Apple Silicon.
    codesign --force --deep --sign - "$app"
    ditto -c -k --keepParent "$app" "$src/build-dreamfactory/artifacts/dreamfactory-macos-$arch-adhoc.zip"
    ;;
  ios)
    test "$arch" = arm64 || { echo 'The device build targets arm64' >&2; exit 2; }
    # Same library package used by this checkout's upstream Xcode CI.
    if ! test -d "$out/frameworks/freetype.xcframework"; then
      curl --fail --location --retry 3 https://downloads.scummvm.org/frs/build/scummvm-ios7-libs-v4.zip -o "$out/ios-libraries.zip"
      unzip -o -q "$out/ios-libraries.zip" -d "$out"
    fi
    test -d "$out/frameworks/freetype.xcframework"
    bz2="$out/frameworks/bz2.xcframework/ios-arm64-armv7-armv7s/libbz2.a"
    test -s "$bz2"
    "$generator" "$src" "${flags[@]}" --ios --use-xcframework
    xcodebuild -project "$out/scummvm.xcodeproj" -scheme ScummVM-iOS -configuration Release \
      -destination 'generic/platform=iOS' -derivedDataPath "$out/DerivedData" \
      CODE_SIGNING_ALLOWED=NO CODE_SIGNING_REQUIRED=NO CODE_SIGN_IDENTITY="" \
      "OTHER_LDFLAGS=\$(inherited) \"$bz2\"" \
      "OTHER_CFLAGS=\$(inherited) $CFLAGS" "OTHER_CPLUSPLUSFLAGS=\$(inherited) $CXXFLAGS" build
    app="$out/DerivedData/Build/Products/Release-iphoneos/scummvm.app"
    test -d "$app"
    cp "$src/dists/engine-data/fonts.dat" "$app/"
    mkdir -p "$out/package/Payload"
    ditto "$app" "$out/package/Payload/scummvm.app"
    (cd "$out/package" && zip -q -r "$src/build-dreamfactory/artifacts/dreamfactory-ios-arm64-unsigned.ipa" Payload)
    ;;
  *) echo 'Expected macos or ios' >&2; exit 2;;
esac
