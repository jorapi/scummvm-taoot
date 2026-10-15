# DreamFactory platform builds and controls

See [ENHANCEMENTS.md](ENHANCEMENTS.md) for optional soundtrack packs, the Gigapixel
pilot and content research. Music replacement is disabled by default and uses a
standard ScummVM Engine option. Live HD images and restored cut content remain
unimplemented.

The engine reads the supplied **Windows game data** on every host. These builds
do not add support for original Macintosh game data. Game files are not included
in any application package.
These are DreamFactory-only ScummVM builds: other game engines are disabled,
while the shared ScummVM launcher, UI and platform backends remain.

The workspace also provides `dreamfactory-source-overlay.zip`. On another
computer, apply it to the matching ScummVM source revision:

```sh
git clone https://github.com/scummvm/scummvm.git scummvm-dreamfactory
cd scummvm-dreamfactory
git checkout 4aa1ef851824ea1f5569a9504ab1b1a07fe670cc
unzip /path/to/dreamfactory-source-overlay.zip
```

The local build commands below run from that ScummVM source directory.

## Build status

| Host | Deliverable | Verification in this workspace |
| --- | --- | --- |
| Windows x64 | Portable executable, SDL2, embedded fonts and licenses | OpenGL display, held-movie cursor, walking arrival and control-panel font fixes; native OpenGL panel test passes |
| macOS arm64 / x86_64 | Separate ad-hoc application ZIPs; no Developer ID | Recipe and Xcode options prepared; native compilation needs macOS |
| iOS arm64 | Unsigned device IPA | Official dependency layout verified; compilation/signing needs Xcode |
| Android arm64-v8a | Unsigned release and debug-signed installable APKs | Built under WSL; signatures, contents and 16 KB alignment checked; device runtime untested |
| Android armeabi-v7a / x86_64 | Unsigned APKs | CI recipe prepared; these ABIs have not been built here |
| PSP | Experimental homebrew ZIP / EBOOT.PBP | PSPDEV CI recipe prepared; not compiled or hardware-tested here |

The workflow is `.github/workflows/dreamfactory-builds.yml`, relative to the
**ScummVM repository**, not the surrounding game workspace. Commit the engine,
`devtools/dreamfactory`, and workflow into a fork before running Actions. No
workflow was published or run remotely by this task. Artifacts upload separately;
one failed platform does not cancel the others. They contain no game data.

The Windows build enables ScummVM's standard **OpenGL** graphics backend and
embeds `fonts.dat`, so the original control panel and conversations can draw
text even when the process starts outside its application directory. OpenGL
avoided the black/frozen output observed with SDL's default Windows renderer.
Graphics choices remain in ScummVM's shared **Global Options → Graphics**.
`Run-Software-Renderer.cmd` launches the portable application with SDL Surface
and a process-local software-renderer override as a fallback.

Movie holds now refresh the software cursor even when no new movie image is
due. Titanic road arrivals use the final motion-frame direction rather than
the endpoint view that faces back down the road. Synthetic room cases, all
three editions' room/movie suites, a real bedsit walk, and a native OpenGL
control-panel test cover these changes. Android ARM64 APKs include the shared
cursor/arrival corrections; Android device testing remains outstanding.

Conversation and interface TrueType text now uses antialiased glyph edges with
the same grid-fitted metrics as before. Entering the modal dialogue-choice band
sets the arrow cursor, clearing the busy cursor left by approach scripts. All
three editions' puppet suites check blended RGB edges, unchanged text metrics,
and arrow selection during the answer wait. Android device visual confirmation
remains pending.

Titanic walking playback corrects stale endpoint camera heights using the
departure and arrival standpoints. GSTAIR2 Road58 retained height 7645 from the
upper landing instead of A deck's 5976, briefly projecting an upstairs crowd
NPC into the walk toward Trask. The intermediate camera heights remain authored
so stair climbs still ramp. Both runtime and development viewer use the same
corrected sequence copy; game resources remain unchanged. Native room suites
and a reference-renderer reproduction cover this correction. Android visual
confirmation remains pending.

Dialogue captions and answer text are now rasterized directly onto the 640x480
presentation canvas, after scaling the 512x384 artwork. A separate cached
90-DPI TrueType font avoids the uneven 1.25x pixel duplication of small glyph
bitmaps. Wrapping still uses the original 72-DPI font and logical coordinates;
answer hit testing, caption bands and choice rows retain their original layout.
The native puppet suites cover the new raster output, artwork preservation,
font caches, choice input and fades/restoration. On Android, enable ScummVM's
shared **Graphics -> Filter graphics** option to smooth the final enlargement
from 640x480 to the device screen. The engine respects that preference.

### macOS and iOS on your Mac

Install Xcode and its command-line tools, CMake and Homebrew. For macOS:

```sh
brew install cmake sdl2 freetype dylibbundler
bash devtools/dreamfactory/build-apple.sh macos arm64
# Use x86_64 on an Intel Mac with Intel Homebrew libraries.
```

This bundles Homebrew dynamic libraries into the app. Separate architectures are
intentional: an arm64 app is not advertised as a universal binary.
After relocation it applies an ad-hoc signature so Apple Silicon can execute
the binary. This uses no Apple certificate/account and provides no notarization.

For the unsigned iOS device application:

```sh
bash devtools/dreamfactory/build-apple.sh ios arm64
```

Output is in `build-dreamfactory/artifacts/`. The generated Xcode project is
`build-dreamfactory/ios-arm64/scummvm.xcodeproj`. An unsigned IPA cannot be
installed directly on an ordinary device. To install later, open that project,
select your development team and a suitable bundle identifier, enable signing,
choose your connected device and build using Xcode. CI uses no certificates or
Apple account secrets. This is a device build, not a simulator build.

### Android

Use Linux with a JDK compatible with this checkout's Gradle wrapper, CMake 3.22.1 or newer, GNU make,
pkg-config and the Android command-line tools. The workflow reads compile SDK
and the exact NDK version from `dists/android/build.gradle`, installs those
versions, and builds static FreeType 2.13.3 and Oboe 1.10.0 for each ABI. API 37's
SDK package is named `platforms;android-37.0`. Set `ANDROID_SDK_ROOT` and
`ANDROID_NDK_ROOT` before running locally:

```sh
bash devtools/dreamfactory/build-android.sh arm64-v8a
```

The local ARM64 build uses WSL Ubuntu 20.04, Linux NDK r23c (23.2.8568313),
SDK platform 37.0, build-tools 37.0.0, JDK 21 and CMake 3.31.8. Tooling is
installed separately under `C:\Android`; the existing Windows SDK/NDK remains
available. `DREAMFACTORY_ANDROID_BUILD_DIR` can select Linux storage for faster
builds, and `DREAMFACTORY_BUILD_JOBS` controls parallelism (default 3).

The release APK is unsigned.
Sign it with Android's `apksigner` and your own key before installation. Preserve
that key for upgrades. The app uses the upstream package identifier, so take
care when replacing an existing ScummVM installation and preserve saves first.
The supplied debug-signed APK installs directly with package identifier
`org.scummvm.scummvm.debug`, separately from the release application. It uses a
local development certificate, not a publishing identity. Copy the corresponding
`game-data/full`, `game-data/demo`, or `game-data/dust` folder to the device, then
select it with **Add Game**. No commercial game files are bundled.

Both ARM64 packages passed ZIP integrity, engine/font/theme presence, native ELF
alignment and Android `zipalign` checks. The debug signature was verified with
`apksigner`; the release was confirmed unsigned. This is compile/package
verification, not an Android gameplay or touchscreen test.

### PSP

```sh
export PSPDEV=/path/to/pspdev
bash devtools/dreamfactory/build-psp.sh
```

The toolchain must include FreeType and the backend's libraries. The recipe
fails if DreamFactory or FreeType was silently omitted. CI runs the PSPDEV
Alpine container through Docker; JavaScript Actions run on the Ubuntu host.
The container currently follows `pspdev/pspdev:latest`; pin an image digest when
you have a known-good toolchain. PSP runtime support is **unverified**. Video,
decoded audio and font caches need memory/performance profiling on hardware;
neither 32 MiB nor 64 MiB models are promised playable by a compile recipe.

## Touchscreen-only navigation

Enable **Touch direction pad** in the per-game Engine options for visible
walking/turning buttons above the original interface band. Normal tapping still
handles objects and dialogue. The pad is optional and hides during movement,
conversations and movie close-ups. See ENHANCEMENTS.md for the new gamepad
bindings and precision-pointer/drag controls. Platform backend controls remain
available as alternatives; all navigation uses the original game scripts.

- **Android:** use direct-mouse or touchpad mode to tap hotspots and drag items.
  Enable the on-screen controls in **Global Options → Backend**. Tap the
  controller icon to switch to **gamepad emulation** for directional navigation,
  then switch back for mouse interactions. The inherited game keymap maps its
  virtual D-pad to the engine's arrow keys. Long-press the controller icon to
  open the virtual keyboard if needed. The on-screen menu icon opens ScummVM's
  Global Main Menu.
- **iPhone/iPad:** tap to click, press and hold to drag, and use **three-finger
  directional swipes** for arrow-key navigation. Left/right turn; up advances
  where permitted by the game. A two-finger downward swipe opens ScummVM's
  Global Main Menu; two-finger double-tap sends Escape. Pinch toggles the keyboard.
- **PSP:** the backend's D-pad emits arrow keys, analog stick moves the pointer,
  Cross clicks, Left Trigger sends Escape, and Start opens the shared menu.

Keep original game direction bindings at their defaults when using these
instructions. If you change those bindings inside the original game panel,
make corresponding changes in ScummVM **Game Options → Keymaps**. The two
interfaces are separate; the engine does not silently rewrite custom bindings.

Official backend references: [Android controls](https://docs.scummvm.org/en/latest/other_platforms/android.html#controls),
[iOS controls](https://docs.scummvm.org/en/latest/other_platforms/ios.html#controls),
[shared keymaps](https://docs.scummvm.org/en/latest/settings/keymaps.html).

## Shared ScummVM UI and remaining review work

Implemented integration includes launcher detection, standard remappable game
actions, the Global Main Menu, runtime save/load, launcher load, extended save
metadata/thumbnails, pause, return to launcher, standard message dialogs,
backend graphics/input and typed music/SFX/speech mixer channels. ScummVM's
volume, mute and speech-mute controls act on those mixer channels. Authored
volume controls remain a separate per-game gain, underneath the shared mixer.

Explicit ScummVM `subtitles` preferences now override the original caption
switch. Subtitles-only (`speech_mute`) forces captions on; with no explicit
shared preference, the original caption switch still works. The preference is
read when conversations render, so it is not overwritten by a gameplay save.
This change passed all nine edition/mode checks and the existing original-panel
and persisted-settings regression on Windows.

This is **not yet an upstream-quality approval**. Remaining work includes actual
Apple/PSP and remaining Android ABI builds, device tests, PSP memory profiling, moving the large
developer audit driver out of release engine code, and ScummVM maintainer review
of coding conventions and architecture. Original game interfaces remain game
content; replacing every original panel with ScummVM widgets is not claimed.
Saves are native DFSV saves, with modal movie/dialogue/camera saves disabled.
