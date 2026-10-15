#!/usr/bin/env bash
set -euo pipefail
export JAVA_HOME=/mnt/c/Android/jdk21
export ANDROID_SDK_ROOT=/mnt/c/Android/linux-sdk
export ANDROID_HOME="$ANDROID_SDK_ROOT"
export ANDROID_USER_HOME=/mnt/c/Android/android-user
export ANDROID_NDK_ROOT="$ANDROID_SDK_ROOT/ndk/23.2.8568313"
export GRADLE_USER_HOME=/mnt/c/Android/gradle-cache
export PATH="$JAVA_HOME/bin:/mnt/c/Android/cmake/bin:/mnt/c/Android/linux-tools/bin:$PATH"
# WSL clears /tmp on restart; keep incremental objects in Linux home storage.
export DREAMFACTORY_ANDROID_BUILD_DIR="${DREAMFACTORY_ANDROID_BUILD_DIR:-$(getent passwd "$(id -u)" | cut -d: -f6)/taoot-build/android-arm64-v8a}"
export DREAMFACTORY_BUILD_JOBS=8
root=$(cd "$(dirname "$0")/.." && pwd)
# Git for Windows may have checked out Linux build inputs with CRLF endings.
python3 - "$root/scummvm" <<'PY'
import pathlib, sys
src = pathlib.Path(sys.argv[1])
paths = [src / p for p in ('configure', 'config.guess', 'config.sub', 'engines.awk', 'dists/android/gradlew')]
paths += list(src.glob('engines/*/configure.engine'))
for path in paths:
    data = path.read_bytes()
    if b'\r\n' in data:
        path.write_bytes(data.replace(b'\r\n', b'\n'))
PY
bash "$root/scummvm/devtools/dreamfactory/build-android.sh" arm64-v8a
cd "$DREAMFACTORY_ANDROID_BUILD_DIR"
revision=$(git -C "$root/scummvm" rev-parse --short HEAD)
make -j"$DREAMFACTORY_BUILD_JOBS" VER_REV="$revision-dreamfactory" ScummVM-debug.apk
cp ScummVM-debug.apk "$root/scummvm/build-dreamfactory/artifacts/dreamfactory-android-arm64-v8a-debug.apk"
mkdir -p "$root/scummvm/build-dreamfactory/android-arm64-v8a"
cp config.mk config.h config.log "$root/scummvm/build-dreamfactory/android-arm64-v8a/"
