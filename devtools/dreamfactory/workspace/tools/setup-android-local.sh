#!/usr/bin/env bash
set -euo pipefail
root=/mnt/c/Android
mkdir -p "$root/downloads" "$root/linux-sdk/cmdline-tools" "$root/jdk21"
if ! test -x "$root/jdk21/bin/java"; then
  curl -fL --retry 3 'https://api.adoptium.net/v3/binary/latest/21/ga/linux/x64/jdk/hotspot/normal/eclipse' -o "$root/downloads/jdk21-linux.tar.gz"
  tar -xf "$root/downloads/jdk21-linux.tar.gz" -C "$root/jdk21" --strip-components=1
fi
export JAVA_HOME="$root/jdk21"
export PATH="$JAVA_HOME/bin:$PATH"
export ANDROID_SDK_ROOT="$root/linux-sdk"
if ! test -x "$root/cmake/bin/cmake"; then
  curl -fL --retry 3 https://github.com/Kitware/CMake/releases/download/v3.31.8/cmake-3.31.8-linux-x86_64.tar.gz -o "$root/downloads/cmake-linux.tar.gz"
  mkdir -p "$root/cmake"
  tar -xf "$root/downloads/cmake-linux.tar.gz" -C "$root/cmake" --strip-components=1
fi
if ! test -f "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager"; then
  curl -fL --retry 3 https://dl.google.com/android/repository/commandlinetools-linux-15859902_latest.zip -o "$root/downloads/commandlinetools-linux.zip"
  echo '4e4c464f145a7512b57d088ac6c278c03c9eea610886b35a5e0804e74eedf583  /mnt/c/Android/downloads/commandlinetools-linux.zip' | sha256sum -c -
  python3 -m zipfile -e "$root/downloads/commandlinetools-linux.zip" "$ANDROID_SDK_ROOT/cmdline-tools"
  mv "$ANDROID_SDK_ROOT/cmdline-tools/cmdline-tools" "$ANDROID_SDK_ROOT/cmdline-tools/latest"
  chmod +x "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/"*
fi
set +o pipefail
yes | "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$ANDROID_SDK_ROOT" --licenses >/dev/null
set -o pipefail
packages=()
test -f "$ANDROID_SDK_ROOT/ndk/23.2.8568313/source.properties" || packages+=('ndk;23.2.8568313')
test -f "$ANDROID_SDK_ROOT/platforms/android-37.0/android.jar" || packages+=('platforms;android-37.0')
test -f "$ANDROID_SDK_ROOT/build-tools/37.0.0/aapt2" || packages+=('build-tools;37.0.0')
test -f "$ANDROID_SDK_ROOT/platform-tools/adb" || packages+=('platform-tools')
if ((${#packages[@]})); then
  "$ANDROID_SDK_ROOT/cmdline-tools/latest/bin/sdkmanager" --sdk_root="$ANDROID_SDK_ROOT" "${packages[@]}"
fi
# A local pkg-config avoids changing the Ubuntu system installation.
if ! test -x "$root/linux-tools/bin/pkg-config"; then
  curl -fL --retry 3 https://pkgconfig.freedesktop.org/releases/pkg-config-0.29.2.tar.gz -o "$root/downloads/pkg-config.tar.gz"
  mkdir -p /tmp/taoot-android-build-tools
  tar -xf "$root/downloads/pkg-config.tar.gz" -C /tmp/taoot-android-build-tools
  (cd /tmp/taoot-android-build-tools/pkg-config-0.29.2 && ./configure --prefix="$root/linux-tools" --with-internal-glib && make -j3 && make install)
fi
