#!/usr/bin/env bash
set -euo pipefail
root=$(cd "$(dirname "$0")/.." && pwd)
export JAVA_HOME=/mnt/c/Android/jdk21
bt=/mnt/c/Android/linux-sdk/build-tools/37.0.0
apks="$root/scummvm/build-dreamfactory/artifacts"
for variant in unsigned debug; do
  apk="$apks/dreamfactory-android-arm64-v8a-$variant.apk"
  "$bt/aapt" dump badging "$apk" > "$root/build/android-$variant-manifest.txt"
  "$bt/zipalign" -c -P 16 -v 4 "$apk" > "$root/build/android-$variant-alignment.txt"
done
"$bt/apksigner" verify --verbose --print-certs "$apks/dreamfactory-android-arm64-v8a-debug.apk" > "$root/build/android-debug-signature.txt"
if "$bt/apksigner" verify "$apks/dreamfactory-android-arm64-v8a-unsigned.apk" > "$root/build/android-unsigned-signature.txt" 2>&1; then
  echo 'Release APK was expected to be unsigned' >&2
  exit 1
fi
out=$(getent passwd "$(id -u)" | cut -d: -f6)/taoot-build/android-arm64-v8a
/mnt/c/Android/linux-sdk/ndk/23.2.8568313/toolchains/llvm/prebuilt/linux-x86_64/bin/llvm-readelf --dynamic "$out/libscummvm.so" > "$root/build/android-native-dependencies.txt"
if grep -q 'libc++_shared.so' "$root/build/android-native-dependencies.txt"; then
  echo 'Unexpected unbundled shared C++ runtime' >&2
  exit 1
fi
echo 'Android manifest, signature, and alignment checks passed.'
