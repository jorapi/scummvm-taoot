# DreamFactory — independent ScummVM fork

This project is maintained as its own ScummVM fork at
[jorapi/scummvm-taoot](https://github.com/jorapi/scummvm-taoot/tree/dreamfactory),
with AI-assisted development. DreamFactory work is not being prepared for an
upstream pull request. Upstream updates are merged to maintain the shared core.
Current enhancements focus on controller/couch play, readable text, shared
ScummVM graphics settings and the optional remastered soundtrack. See
[the enhancement guide](scummvm/devtools/dreamfactory/ENHANCEMENTS.md) for configuration.

A native C++ ScummVM engine for **Titanic: Adventure Out of Time** (retail and the supplied September 1996 interactive demo) and **Dust: A Tale of the Wired West**. All three supplied English Windows editions pass native player-input runs from fresh startup through completion on the same archived build. Retail reaches the best ending with all four evidence items, credits and quit; the demo completes its developed-photo route; Dust completes all five days, both gunfights, the duel, four temples, Bloodstone's defeat, the Yunni ending and credits quit. Separate regressions cover alternate endings, native save/load, invalid saves, settings and death recovery. The engine remains experimental and is not included in stock ScummVM. Normal launch runs the game's scripts; use `-Viewer` for the developer room viewer.

The engine is in `scummvm/engines/dreamfactory/`. It detects the three supplied English Windows editions, reads DreamFactory containers, decodes indexed delta images, and displays authored room turns and walks. Titanic uses DreamFactory 4 scene/view registers; Dust uses a separate DreamFactory 1 grid-transition reader. Both share the archive and image decoder.

The validated target includes story progression, puzzles, conversations, media and native saves in these supplied editions. The user confirmed the September 1996 demo as the target for “The Lost Mission/demo”; a separate release is outside the target. This is completion-path and feature validation, not an exhaustive test of every dialogue or quest permutation. The native script parser/interpreter, STG/FLT readers, volume-aware resource resolver, and audio bank decoder/playback host are implemented and audited. BOOTFILE runs in the normal gameplay loop with native input and the authored idle handler. Progress entries below are historical milestones; the current status is summarized above.

Music uses one shared queued channel for bank themes and movie beds, with
authored order and repeat boundaries. Repeated bank opens retain independent
controls and playback ownership. DFSV v8 checkpoints preserve those bank
instances and pending music; v1-v7 checkpoints remain readable. Dust's sound
directory now separates one-shots from music and preserves duplicate records.
The requirement audit and its evidence are recorded in `build/completion-requirements.json`.

## Completion validation — 14 October 2026

The completion-validated executable is preserved at
`build/releases/validated-0ab/scummvm.exe`, SHA-256
`0AB6CD9A8B06D8B6D8E8EBFCCCFA796773504D1FE2A11E4F85BDE43AB7D62027`.
Checkpoint ancestry links each successful trace to fresh startup on that same
executable, including resumed runs. No story globals, inventory, actor damage
or RNG state were injected to complete these paths.

| Edition | Completion proof | Linked traces |
| --- | --- | ---: |
| Titanic retail | [Best ending, credits and quit](build/full-walkthrough/20261014-103639-041/ending/lineage.json) | 57 |
| September 1996 demo | [Developed-photo completion and fresh restore](build/demo-current/20261014-105343-100/lineage.json) | 2 |
| Dust | [Five-day route, Yunni ending and credits quit](build/dust-walkthrough/20261014-121819-988/ending/lineage.json) | 330 |

[The requirement audit](build/completion-requirements.json) combines these paths
with component, controls, save/recovery and ending regressions. All four retail
smokestack layouts have traversal coverage; layouts 1, 3 and 4 were additionally
checked with fresh restoration on the final executable. Earlier optional
checkers, poker and fencing checks are archived separately and are not counted
as fresh-start completion paths. Tests do not exhaust every quest/dialogue
permutation. Other languages/platforms and original TI/DF save import are outside
the validated scope; modal saves are disabled, physical controllers are untested,
and seven optional retail TourGuide movies are absent from the supplied data.

The walkthrough helpers use physical inputs and earned checkpoints. Random
gambling outcomes or hunter spawns can require another legal attempt or a resume
from the last successful checkpoint. Use `-StartAt` to resume a walkthrough;
`-CasinoHitBelow 16` selects the blackjack policy used by the recorded Dust run.
Historical progress notes below retain earlier incomplete milestones.

## Platform packages and shared controls

The current Windows build adds shared ScummVM subtitle preferences. Nine
edition/mode cases, the original-panel/persisted-settings suite, and an actual
conversation using the portable package's `fonts.dat` pass. These are targeted
regressions; the complete walkthrough chains above refer to the archived 0ab
build, not a newly replayed chain on this settings update.

The Windows renderer update enables the standard OpenGL backend to address
black or frozen output, refreshes cursors during held movie frames, and fixes
Titanic's arrival-facing direction after walking. Fonts are now embedded in
the executable to prevent the life-preserver control panel from failing when
launched outside its application folder. The prior settings build/audit is
preserved in `build/releases/validated-platforms-before-render-fix`.
That renderer build passed a real OpenGL bedsit walk and control-panel test with
no external font path. Android ARM64 packages also include the shared engine
cursor and walking corrections.

The latest Windows and Android ARM64 builds also antialias conversation and
interface text while retaining the previous grid-fitted metrics, and switch
from the busy cursor to the arrow when dialogue choices accept input. All
three editions' puppet suites passed, including blended text edges, unchanged
font metrics and the cursor during modal choice selection. Android package
checks passed; visual confirmation on an Android device is pending. Previous
packages and audit records are in `build/releases/validated-platforms-before-dialogue-fix`.

The staircase NPC pop-in is also corrected: Titanic walking sequences borrow
their endpoint eye heights from the adjacent standpoints, avoiding stale
upper-deck camera heights in GSTAIR2 Road58 and similar records. Interior stair
heights and game files remain unchanged. All three native room suites passed;
`node tools/inspect-staircase-npc.mjs` reproduces the faulty projection and its
correction with the reference renderer. Previous packages are archived in
`build/releases/validated-platforms-before-road-height-fix`.

Dialogue text now renders at the 640x480 presentation resolution instead of
stretching 512x384 glyph bitmaps. A separate 90-DPI font supplies the screen
glyphs while the original font still determines wrapping and logical layout.
Native puppet suites passed for all three editions, including the preserved
artwork, input and fade behavior. For smoother enlargement to an Android
display, enable **Filter graphics** in ScummVM's Graphics options. Previous
packages are in `build/releases/validated-platforms-before-text-scaling-fix`.

For the portable Windows package, run `scummvm.exe` normally. If OpenGL is
unavailable, use `Run-Software-Renderer.cmd`. From this workspace, use
`./tools/run.ps1 full` or `./tools/run.ps1 full -SoftwareRenderer`.

- [Windows x64 portable ZIP](build/releases/platforms/dreamfactory-windows-x64-unsigned.zip)
- [Android ARM64 installable debug-signed APK](build/releases/platforms/dreamfactory-android-arm64-v8a-debug.apk)
- [Android ARM64 unsigned release APK](build/releases/platforms/dreamfactory-android-arm64-v8a-unsigned.apk)
- [Android compile and package verification](build/android-package-audit.json)
- [ScummVM source overlay, including CI](build/releases/platforms/dreamfactory-source-overlay.zip)
- [Platform build instructions, touchscreen navigation and UI review](scummvm/devtools/dreamfactory/README.md)
- [Current integration audit](build/platform-integration-audit.json)

CI is prepared for Windows, macOS Intel/Apple Silicon, unsigned iOS, three
Android ABIs and experimental PSP homebrew. Windows and Android ARM64 were built
here. Android APK contents, signatures and 16 KB alignment pass verification;
Android device gameplay remains untested. The other platforms/ABIs have not been built here. The workflow has not
been published or executed remotely. No game data is included in the packages.

For Android, install the debug-signed APK, copy the desired `game-data/full`,
`game-data/demo`, or `game-data/dust` folder to the device, and select it using
**Add Game**. Touchscreen controls are documented in the platform guide above.
The unsigned release APK requires your own signing key before installation.

Local Android rebuild helpers are `tools/extract-android-ndk-local.py`,
`tools/setup-android-local.sh`, and `tools/build-android-local.sh`. Run the Python
extractor on Windows, then the shell helpers under WSL Ubuntu 20.04. They use
`C:\Android` for tooling and persistent Linux home storage for native objects.

## Run the current build

From this folder in PowerShell:

```powershell
./tools/run.ps1 demo
./tools/run.ps1 full
./tools/run.ps1 dust

# Optional developer room viewer
./tools/run.ps1 dust -Viewer
```

In the development viewer, use Left/Right to turn, Up to walk within a room, Page Up/Page Down to browse room files, Tab to jump to another scene, and Escape to exit. The viewer bypasses scripted room gates and has no actors, props, inventory, dialogue, sound, or save support. Debug room switching is not the game's tour mode. Gameplay uses the original scripted interface, with mouse interaction and arrow-key navigation. Experimental gameplay supports native ScummVM saves through the game's save/load controls and ScummVM menus; see the checkpoint validation below.

Executable: `build/scummvm/Release/scummvm.exe` (keep its adjacent SDL2.dll). You can also use Add Game in this custom ScummVM build and select one of `game-data/demo`, `game-data/full`, or `game-data/dust`. An installed stock ScummVM does not include this engine.

## Rebuild and extract data

Prerequisites: Python 3, 7-Zip on PATH, CMake, Visual Studio 2022 with C++ tools and Windows SDK, and SDL2's VC development package extracted to `dependencies/SDL2-2.32.10`. All are available in this workspace except that dependencies must be downloaded again for a fresh checkout.

```powershell
python tools/prepare-data.py
./tools/build.ps1
```

The extraction script reads the supplied DBGL ZIPs without launching their executables. It preserves runtime resources from all game directories, including puzzles, puppets, movies, inventory, installed LOCAL/TOUR files, and the original cursor-resource executables. Original CD paths are kept under `game-data/<edition>/volumes/`, installed resources under `installed/`, and a SHA-256 resource index records every same-name variant. The flat compatibility view beside BOOTFILE still serves the room viewer: installed resources take priority, then CD 1. Retail Titanic contains 74 differing same-name resource groups. The script runtime resolver honors classic colon-separated paths and search slots, preserving CD-specific files instead of falling back to the flat winner. Slot 0 is the read-only install root; scripts can set slots 1–8.

Original ZIPs are preserved. Game data, reference checkouts, dependencies, and build output are ignored by the workspace `.gitignore`; distribute engine sources rather than the game files. ScummVM is a separate Git checkout, so engine changes appear as its untracked `engines/dreamfactory/` directory.

The engine is disabled by default in ScummVM's configure system. On other supported build platforms, use the normal ScummVM build instructions with `--disable-all-engines --enable-engine=dreamfactory`. The PowerShell build script generates a minimal SDL2 Windows build and patches only its generated CMake file for Windows import-library, entry-point, and manifest handling.

## Validation

```powershell
npm install --prefix dependencies --no-audit --no-fund --ignore-scripts typescript@5.9.3
./tools/verify.ps1
./tools/verify-scripts.ps1
./tools/verify-scripts.ps1 -Rooms
./tools/verify-scripts.ps1 -Cursors
./tools/verify-scripts.ps1 -Screens
./tools/verify-scripts.ps1 -Stages
./tools/verify-scripts.ps1 -Resources
./tools/verify-scripts.ps1 -Audio
./tools/verify-scripts.ps1 -Casts
./tools/verify-scripts.ps1 -Stars
./tools/verify-scripts.ps1 -Movies
./tools/verify-scripts.ps1 -Puppets
./tools/verify-scripts.ps1 -Shops
./tools/trace-boot.ps1 -ChooseGame -TimeoutSeconds 300
```

The native audit runs inside the actual ScummVM executable with SDL's dummy video/audio drivers. It decodes each authored turn/walk sequence and compares per-room FNV-1a pixel hashes with the independent TypeScript reference implementation. Dust's departure stills are also included. A second smoke test opens every room, renders the default viewpoint, and exercises turn/walk calls. Screenshots and logs are written under `build/`.

Validated on the supplied packages:

| Edition | Room files | Decoded frames | Reference hashes |
| --- | ---: | ---: | --- |
| Titanic demo | 9 | 2,184 | Match |
| Titanic retail | 56 | 15,958 | Match |
| Dust retail, including underground/target sets | 35 | 13,394 | Match |

The audit skips zero image references: these are unrendered camera entries, not frame data in container 0. Counts include frames repeated across authored sequences; these are not counts of unique image containers. Pixel hashes validate the indexed decoder, not complete game behavior. This work has been tested on Windows x64; other ScummVM backends remain untested.

Script and stage audits traverse the preserved CD and installed trees, so both versions of differing same-name resources are checked. The script audit compares handler/statement counts against the reference parser and executes 25 language/host fixtures covering precedence, strings, short-circuit conditions, variable scopes, calls, loops, switch labels, context, event consumption, scalar helpers, packed points, computed variables, and path/configuration round trips. Unknown native calls fail explicitly instead of taking the reference interpreter's zero fallback; recursion and zero-step loops are also checked. Native execution has call-depth and instruction-budget limits, and an epoch for abandoning old script chains on load/restart.

| Edition | Script containers | Handlers | STG/FLT files |
| --- | ---: | ---: | ---: |
| Titanic demo | 242 | 728 | 14 |
| Titanic retail | 1,620 | 4,225 | 40 |
| Dust retail | 831 | 4,050 | 21 |

These counts include copies in separate volumes and installed folders. Stage metadata (including palette, dimensions, script references and button rectangles) and all flat image hashes match the independent reference. The stage session fixtures also check open/close ordering, nested flat navigation, deferred button/flat messages, containment and boot fallback, flat timer dispatch, enumeration, packed-point button tests, and inclusive overlapping hit regions for v1 and v4. These checks do not validate completed playthroughs.

The resource audit runs 2,398 lookup cases across the three packages, comparing the resolved physical path, file size and first-5,000-byte hash against the preparation manifest. It covers explicit volume paths, search slots, case folding, duplicate-name priority, missing volumes/files, protected slot 0 and rejected parent-directory traversal.

The audio audit compares bank metadata, V40/V41 signed PCM, and mixed-rate joined music against the independent reference:

| Edition | Banks | Referenced chunks | Decoded samples |
| --- | ---: | ---: | ---: |
| Titanic demo | 17 | 142 | 9,628,672 |
| Titanic retail | 76 | 775 | 68,647,936 |
| Dust retail | 46 | 625 | 29,190,144 |

Counts include separate installed/CD copies. All hashes match. Compiled script fixtures also exercise the actual ScummVM mixer with SDL dummy audio: speech completion, repeated looping effects, stopping, volume/pan controls, track enumeration, and closing banks by filename or internal track name. Decoder boundary tests cover malformed/truncated input and transactional failure. These checks do not establish audible output quality, positional sound behavior, movie audio, or save/load playback restoration.

The cast audit checks all CST character directories, palettes, script references, pose names, animation steps, playback lists, facing angles and reference scales. It also compares every referenced sprite's dimensions, signed anchor, indexed pixels and transparency mask:

| Edition | Cast files | Member records | Poses | Sprite frame references |
| --- | ---: | ---: | ---: | ---: |
| Titanic demo | 2 | 10 | 18 | 640 |
| Titanic retail | 4 | 66 | 138 | 3,332 |
| Dust retail | 7 | 121 | 304 | 4,884 |

All metadata and sprite hashes match the reference; counts include installed/CD copies. Sprite boundary fixtures cover all four run modes, opaque palette index zero, signed anchors, truncated segments, invalid dimensions and row overflow/underflow, with transactional failure.

Session cast loading now fires `opencast` and per-member `openactor`, creates/deletes actor copies with fresh state, and provides basic actor state commands and enumeration. Actor/cast `sendto` and `sendto...fx` evaluate deferred arguments in the caller, then dispatch the handler with recipient context and cast/boot containment fallback. Compiled synthetic v1/v4 fixtures test these semantics, scriptless cast members, original-member scripts after deletion, re-opening, and closing a cast inside its own handler without invalidating the running script. Actor animation, projection, screen/world composition and session room/star binding are connected; native gameplay input is connected in experimental session mode; full playthrough validation remains pending.

The session also implements the 32-entry loop table (`makeloop`, `stoploop`, pause, query and enumeration), 60 Hz `tick`/`delay` units, displayed-frame counting, and paced `forceupdate`. Loops are one-shot and remove themselves before dispatch; handlers can re-arm them. Tests cover ordered replacement of another due entry, pause/resume, capacity, busy-script countdowns, self-rearming callbacks, re-entry prevention and late passes without catch-up bursts. Compiled timing fixtures exercise actor callbacks with a deterministic clock; a second check uses the real SDL clock for delay/frame pacing. Actor, prop, scene and flat loop dispatch are connected. Boot idle/game-clock dispatch remains to implement; unsupported callback kinds fail explicitly.

Titanic's 16-entry positional ambience table implements `makecricket`, `stopcricket`, pause, queries and enumeration (Dust uses these opcode numbers for a separate physical projectile service). Each entry keeps its room and fixed coordinates and uses an independent random stream for repeat jitter. Timed repeats arm on entering audible range and count down on 50 ms service passes only while audible in their room, including busy-script passes; looped PCM starts on range entry. Timers continue while a sample plays; retail repeats use the original SFX-slot replacement rules. Room changes silence absent-room handles immediately while retaining repeat records. Positional playback overlaps other ambience and applies distance/facing gain and panning plus the wave master volume, independently of named sound volume/pan controls. Compiled API fixtures and native SDL mixer checks cover replacement, capacity, pause, one-shot removal, jitter, room exit/return, overlap, bank closure and reset. Distance falloff and folded bearing pan now follow the supplied originals, including live listener updates; see the positional ambience milestone below. Session room entry and settled camera views now supply the room and listener position/facing; explicit developer viewer mode does not run that session.

The 16-entry actor motion service implements `walktoxyz`, timed `turntodeg`, `stopwalk`, pause/resume, walk queries and enumeration. Actors turn before moving and advance at their scripted speed per 50 ms service pass, including during script waits; `endturn` and `endwalk` run with actor context. Turns alone preserve position and star and fire no arrival callbacks. Dust dispatches endturn after removing a turn-only record, including already-facing turns, and replaces a running walk when a new turn is requested; Titanic retains its existing turn policy. Compiled fixtures check approach/wait sequences, pause/cancel, mid-walk turns, callbacks starting a new journey, actor deletion/cast closure and table capacity. Named-star placement and walking now use checked room star/route tables; session room entry now binds these tables and settles pending actor placements. The demo opening movie also exercises a dangling timed cue; the player consumes absent cue targets and continues, matching the reference rather than aborting the film.

The session room runtime loads set, scene and painting scripts with stable storage, runs `openset`/`openscene` and departure callbacks, and supports authored turns, walks, named viewpoint jumps and scene timers. `currentset` uses the internal name: Dust’s night town stays `town`, and its custom-named grid cells (including `chicken`) are found by coordinates. Scene scripts resolve local helpers through their set main; the active stage is an unqualified-call library ahead of boot. Nested scene changes and room replacement preserve the callback’s destination. Painting sends keep their own event chain, and keyboard forwarding reaches the boot movement handler without re-entering its router. Room entry binds stars, pending actors and positional ambience.

Room metadata/script-reference hashes match all preserved SET files: 9 demo files (61 scenes, 46 paintings), 78 retail files (546 scenes, 648 paintings), and 35 Dust files (1,067 grid scenes). Native checks reconstruct every available standpoint, including delta images and authored zero-image staircase scenes. Compiled v1/v4 fixtures cover lifecycle ordering, deferred calls, containment, navigation, re-entry, self-closing handlers, scene loops and Dust grid queries. SDL checks verify centered indexed output, palettes, real-clock movement pacing and quit abandoning a waiting script. Player-input routing remains pending; stage/room and actor/prop composition are connected; these component checks do not establish story playability.

Star metadata audits compare all primary and nested secondary placements, facing, route endpoints, and every route point against the reference: 9 demo files (16 stars, 3 paths, 30 points), 78 retail files (446 stars, 9 paths, 39 points), and 35 Dust files (218 stars, 37 paths, 343 points), including preserved duplicates. Reader checks reject truncated registers, invalid route references/counts, and malformed names. Placement fixtures check pending Dust placements, explicit coordinate overrides, remembered rooms, and live-room precedence. Path fixtures check corners, reversed leg lengths, resume from the current position, final-point resume, and Dust's remembered routes across rooms.


`trace-boot.ps1` runs the actual BOOTFILE handlers with session-scoped globals and boot-library fallback calls, stopping explicitly at unsupported commands. All three editions now log `BOOT READY`: the supplied September 1996 demo completes its four-segment opening movie and reaches `demo.stg` / `demo 1`; retail completes its logo and game/tour menu and opens `bedsit1`, Scene2/View14, with `main.stg` / `main 1`. The diagnostic `-ChooseGame` option clicks the authored game region; without it the menu waits for player input. Dust completes all three intro movies (17 segments) and opens Town at Scene G15 facing north with `new.flt` / `mainpanel`. Its startup now returns normally with puppet name/enumeration/state commands connected. All three establish their authored search paths; retail executes its library `setpath(1)` and mounts Titanic1. The demo has one cast/five actors and one shop/two props; retail has one cast/25 actors and two shops/72 props; Dust has two casts/54 actors (16 copies) and two shops/73 props (six copies). These are diagnostic startup results; normal interactive mode runs the authored session. Puppet configuration flags now control the native conversation display; stage controls and other UI interfaces still need gameplay integration. `-Editions demo,full` or `-Editions dust` can select individual startup traces.

The movie reader, playback controller and ScummVM display/audio host are implemented. Session `playmovie` blocks the calling script across the full movie chain; `actionframe` reads its accumulated action flags. The audit compares every Windows `.MOV` file's segment metadata, frame actions, regions, palettes, decoded delta/composited images, and referenced PCM against the reference:

| Edition | Movie files | Segments | Frames | Click regions | Audio chunks |
| --- | ---: | ---: | ---: | ---: | ---: |
| Titanic demo | 22 | 56 | 1,633 | 102 | 49 |
| Titanic retail | 278 | 330 | 13,653 | 2,711 | 625 |
| Dust retail | 235 | 265 | 8,570 | 619 | 437 |

All hashes match; counts include separate installed/CD copies. Eleven unused big-endian Macintosh assets in Dust's `ZUNUSED` directory are identified and excluded from this Windows-format audit. Synthetic native checks cover malformed directories, dimensions, references and segment cycles, plus frame holds, voice waits, clickable regions, timed cues, sound-follow jumps, movie chains/calls/returns, action flags, skips and explicit errors. The controller uses a deterministic clock/audio host for these checks. Compiled fixtures also exercise real SDL display, timing and mixer operations for both movie formats: canvas scaling and offsets, palette output, voice waits independent of a looping bed, bed inheritance between segments, natural narration tails and replacement by a following line, stale-click flushing, outside/inside region clicks, Escape, reset flags and quit abandoning the waiting script. A new film cannot wait on the previous film's lingering narration. These checks use dummy video/audio drivers and do not establish audible output quality or complete gameplay.

The SHP/PRP shop reader now parses prop groups, scripts, named states, degree/group/ref-scale metadata and animation play lists. It shares the audited transparent sprite decoder:

| Edition | Shop files | Prop groups | States | Sprite frame references |
| --- | ---: | ---: | ---: | ---: |
| Titanic demo | 11 | 96 | 336 | 1,493 |
| Titanic retail | 33 | 541 | 1,493 | 4,812 |
| Dust retail | 17 | 360 | 1,168 | 2,881 |

Metadata and indexed pixel/transparency hashes match the reference for all files; counts include installed/CD copies. The reader preserves repeated animation steps, distinguishes degree selectors from animation states, drops invalid/vestigial play lists and uses the reference's structural idle-pose correction for reversed transition art. Empty archive slots are non-drawing frames (Dust's SCORP desk). Synthetic native checks cover repeated lists, selectors, reference-scale defaults, empty slots, malformed dimensions/counts/references and invalid name lengths. Session shop loading now fires per-group `openprop` before `openshop`, preserves existing prop state on re-entry, and fires `closeshop` before removing the shop and its copies. Prop/shop dispatch evaluates deferred arguments in the caller and keeps script storage alive through nested close/delete handlers, with shop and boot fallback. Prop instances copy the v1/v4 display fields while retaining fresh ownership and placement defaults. Basic state, visibility, placement, owner/value, degree, scale, speed, distance and enumeration commands are connected, along with prop loop callbacks. Compiled synthetic v1/v4 fixtures exercise lifecycle order, re-entry, aliases, copies, scriptless props, containment, deletion, self-closing shops and loop re-entry guards. Separate visibility checks confirm that hiding currently visible props does not hide controls shown later, and that named hide/release operations preserve the visible flag. Prop-star placement now resolves named/remembered room stars, defers unknown qualified placements until their own room opens, preserves later facing overrides and explicit coordinates, and applies Dust’s world-scale default. Compiled checks cover immediate/deferred placement, foreign-room filtering, sentinels, copies and remembered tables. Screen-space animation, composition and hit testing are now connected; scene-dependent existence queries and normal input routing remain to implement. World projection is connected to stage composition.

The session reads named cursor artwork from the supplied TI.EXE/DF.EXE using ScummVM's Windows resource and cursor readers. These files are retained by the data preparation script and never executed by ScummVM. Independent PE decoding checks all 31 named cursors across the three builds (11 demo, 11 retail, nine Dust), including 32×32 pixels, transparency and hotspots. Compiled calls check case folding, nested hide/show counters, the invisible `none` cursor, missing-name fallback and reset.

Palette commands now install named set/stage/current/black palettes, mix ranges, clear the screen, and run blocking black-to-screen/screen-to-black ramps on the 60 Hz clock. Drawing a room under a black palette retains that black until an explicit reveal. Fades queue input and honor quit without running script loops, walking or ambient countdowns inside the ramp. Native SDL and virtual-clock fixtures check endpoints, range boundaries, retained source palettes, pacing, input retention and abandoned scripts. `visualeffect(plain,0)` preserves the black palette; animated wipes/turns remain to implement. The RGB conversation display preserves separate room and character palettes, and fades retain the latest displayed picture, including a movie frame.

Dust's v1 opcode 16034 now stores signed hit box dimensions for actors and a separate player pair; axis getters, fresh copies and invalid targets/axes have compiled checks. Titanic's opcode at that number still mounts/queries CDs. Camera-height state uses a signed 16-bit value. Packed-point distance uses signed halves, a wrapping 32-bit squared sum and a truncated unsigned integer square root, matching static analysis of the supplied Dust and retail Titanic executables; the reference runtime instead rounds this result. Native fixtures cover negative coordinates, fractional distances and extreme-word arithmetic. Actor collision and projection still need to consume these fields.

The PUP conversation reader now parses character names, dialogue/subtitle records, speech references, per-line stances, eleven sprite layers, lip-sync frame/anchor records, answer-band art and idle intervals. Metadata, sprite pixels, animation records and PCM hashes match the independent reader for all 95 supplied Windows puppet files (four demo, 52 retail Titanic, 39 Dust). Every referenced conversation script also parses natively. Dust stores its only stance directly in container 3; Titanic uses a directory whose unused slots contain authoring scratch. Native boundary fixtures reject truncated headers/animations, invalid counts, names, references and stances.

The session can load/close puppets, report their internal names and scripts, dispatch deferred messages with branch/main containment, control visibility, select a line's base pose, collect up to five choice plaques, and scramble the existing plaques using the script random stream. Compiled checks cover missing/invalid loads, defaults, case folding, parent helper context, passcode, self-closing/replacing handlers, line-name/subtitle lookup and appended choices. The modal `puppetspeak` host now decodes the line’s PCM into the voice channel, waits its duration plus 150 ms, and advances the line’s lip-sync poses at approximately 30 Hz. Escape silences the voice and skips the rest of that speech run while retaining up to three lines for repeat; clicks do not skip, Ctrl+digits change wave volume, and quit abandons the calling script. Missing audio uses the original text-byte timing fallback. Compiled SDL checks and deterministic fixtures cover timing, stances, final poses, subtitles, malformed animation, repeat-queue limits, owner lifetime and independent voice/effect channels. The session now presents animated portraits, subtitles and answer plaques during speech and answer waits. Normal gameplay invokes these services through its event loop.

The native puppet art compositor now places all eleven layers and the supplied answer-band sprite on the original 512×384 conversation canvas for both Dust and Titanic. It preserves the live room’s separate palette beneath matte/transparent character layers, clips the picture when subtitles occupy its bottom 40 pixels, and handles signed sprite anchors, frame clamping and character changes. Composed-image hashes match the reference renderer across all 95 puppet files, using each represented stance’s opening/final poses with and without subtitles and changing backdrop palettes. Synthetic checks cover opacity, reserved palette endpoints, layer order, clipping, band anchors, stale sprite caches, visibility and half-open choice bounds. The session presents this canvas through a native RGB display at 640×480. Normal launch runs the authored session.

The session now implements the modal `puppetevent` wait. Releasing in the pressed answer row returns its authored id after the ten-tick feedback floor; dragging out cancels, Escape returns -1, and the answered plaques stay up until `puppetclear`. It preserves the previous exchange for picture-click repeats, replays a selected answer only through subtitle-text lookup, and keeps repeat playback out of the script dispatcher. The four authored idle intervals run only when puppet parameter 8 enables them, use the original shared script random stream, and ignore mouse clicks during their blocking speech. Escape during an idle line exits the conversation with the speech-interrupted flag; a silent-plaque Escape leaves subsequent parting lines playable. Compiled/native tests cover selection, drag tracking, press feedback, queue/plaque retention, repeat after Escape, idle interruption, volume controls and quit abandonment. These input services and their display run in session diagnostics; normal gameplay uses these services; the developer viewer is selected explicitly.

Conversation text now draws through ScummVM’s TrueType font API on the RGBA conversation canvas. It uses the original fixed baselines and margins, at most two subtitle lines split at the last fitting literal space, raw puppet palette inks, CP1252 punctuation, and three-pixel chosen/inverted press frames. Overlong unbroken subtitles follow the original Windows behavior rather than the browser’s CJK fallback. Native tests check exact layout with a deterministic font and load/cache real TrueType fonts, including lazy glyph-stream ownership, signed character/cell sizes and failed-load preservation. Native portrait/text previews are written under `build/puppet-session-<edition>/text-preview.rgba` during `-Puppets` checks. The native display host now presents it during speech and answer waits.

`tools/build.ps1` now prepares pinned FreeType 2.13.3 and Liberation Sans 2.1.5 dependencies through `tools/prepare-fonts.ps1`, verifying the downloaded archives’ SHA-256 hashes. The test/build font and redistribution notices are in `build/runtime-data/`. The engine font loader accepts supplied TrueType bytes, including Arial; Liberation Sans is the portable, openly licensed test asset, designed for Arial-compatible metrics, not pixel-identical Arial artwork. It is covered by its accompanying SIL Open Font License. The FreeType dependency uses its included FreeType License and notice. Other ScummVM build configurations now require high-resolution, 16-bit/RGB and FreeType support for this engine.

The shared native display now handles indexed room/movie output and RGB conversation composition, including scaling, palette dimming, black holds, fades and world restoration after hiding or closing a puppet. Physical-screen pixel checks cover both movie formats after RGB promotion and independently dimmed room/character layers. Each edition also plays a short authored voiced line while capturing the native screen. `-Puppets` writes these diagnostic captures to `build/puppet-session-<edition>/display-preview.rgba`; `python tools/preview-rgba.py <capture>` exports a PNG. These captures use supplied portraits and speech with a synthetic backdrop and answer plaque. The launch, trace and verification configurations select Liberation Sans through `dreamfactory_font`; an explicit TrueType path can replace it.

The native session now composes STG/FLT flat art beneath the visible room on the logical 512×384 screen, with independent stage and room palettes. It honors `setvisible`, caches art by loaded stage ownership, retains the prior picture when a matte interface has no room covering it, and supplies the stage background beneath conversations. Reference comparisons cover 1,635 composed pictures across all 75 supplied stage files; native screen checks cover flat changes, visibility, separate palette edits, black holds and fades. Projected actors/props and scenery depth are connected. Startup tracing also exports the native opening screen to `build/boot-<edition>.rgba` for visual inspection.

Screen-space SHP/PRP props now render over the stage and beneath conversations, with transparent pixels, signed anchors, stable `propdist` ordering, shared room/interface palettes and sprite caches owned by each loaded shop. Degree selectors use stored degrees; animations honor group-based play lists and degree variants, including changing variants mid-sequence. A fixed 50 ms service advances these states: Dust holds the last frame, while Titanic views with multi-step play lists repeat. Native checks cover same-handler degree pinning, later state changes, invisible props, timing, overlapping sprites, palette edits and owner replacement. Frame-choice/sequence hashes and 3,132 cropped sprite compositions match the reference runtime across all 61 supplied SHP/PRP files. `pointinprop` now tests the selected picture’s opaque pixels using packed signed coordinates; it is distinct from the frontmost-visible-prop query used for future input routing. World props and actors now render in the stage composition.

Room frame decoding now retains the authored scenery depth plane, using bounded row offsets and exact row coverage. Native checks cover shared row offsets, truncation, invalid offsets/runs, failure preserving the prior frame, and clearing depth when the next image has none. Room readers also retain the stored v4 camera coordinates and view heights, depth quantization, and Dust's eye height and camera setback. Independent reference hashes match all 37,452 decoded depth planes across the 122 preserved SET files, including disc variants. The world prop compositor consumes these data for projection and scenery occlusion.

World SHP/PRP props now project into the stage's visible room using fixed-point camera bearings, stored sprite scale references, camera-relative directional frames, and authored scenery depth. Drawing and world hit tests share their scaled pixel mask and occlusion rules, including `zclip`, Dust's ground bias, room filtering and viewport clipping. Independent comparisons cover 12,528 projected crops across all 61 supplied shop files. Room cameras use the stored standpoint/motion poses, `camerahi`, and Dust's focal length and setback. Inspection of the supplied Dust executable confirms five-frame walking/turning cameras include both endpoints (64 world units or 16 angle units per frame), correcting an interpolation estimate in the reference. Bare-room composition and normal gameplay input are connected in experimental session mode. Dust now interleaves actors and props by depth; Titanic keeps the reference renderer’s two-pass order.

Actors now select the authored play-list step and the angularly closest frame, with the first record winning a tie. Their 50 ms animation service advances idle and hidden actors as well as walkers; repeated play-list entries hold pictures, and empty poses remain non-drawing. World actors use camera-relative angles, stored reference scaling and scenery occlusion; screen actors use their own facing and draw at scale zero. Sprite caches belong to loaded casts. Frame-selection hashes cover all 256 facing angles at every play step; 7,360 world/screen crops match the reference across all 13 supplied CST files. Native checks cover depth masks and pixel hit tests, room filtering, transparent holes and Dust/Titanic actor/prop ordering on the physical display. The `actordist` setter stores screen order; its getter uses the reference's drawability gate and ground distance, returning 32000 when an actor cannot be drawn. Input routing is connected in experimental session mode; full story playability remains incomplete.

Rooms without an interface stage now use the same world/screen sprite compositor, with the full room palette. The room occupies the top of the 512×384 logical screen, matching the reference; unused space stays black. Native physical-screen fixtures cover missing stages and unselected flats, actor/prop depth order, screen actors, palette dimming and reveal, black holds, and room visibility. Room navigation checks also verify placement, movement pacing and quit abandonment in RGB mode.

## Next implementation milestones

1. Connect the session to normal interactive mode. The supplied startup scripts now complete for all three editions. Original cursor resources, blocking palette fades, room lifecycle/navigation and modal movie playback are connected to the session. The loop table/timing host and actor callback dispatch are implemented, alongside cast lifecycle, resource resolution and audio playback.
2. Extend stage/room composition with projected SHP/PRP props and actors, hotspots, scripted scene/room changes, and the original controls. Validate boot idle/game-clock dispatch and native player-input routing in complete playthroughs. Actor, prop, scene and flat loop callbacks are implemented. Stage layouts, lifecycle events, addressed button dispatch, and timer callbacks are implemented and tested; flat/room composition is now connected; actor/prop composition is connected; bare-room composition now shares the stage sprite renderer; native player-input routing is connected in experimental session mode; complete interaction/playthrough validation remains pending. The per-format readers are implemented; remaining host commands are being identified through authored interaction traces.
3. Connect the animated, projected actor/prop compositor and the implemented walking, PUP conversation, audio and movie services to playable scenes.
4. Finish inventory/puzzle and timer validation, Titanic's endgame, save/load coverage, and complete playthrough tests for all three supplied editions.
5. Expand detection tables and maintain DreamFactory in this independent ScummVM fork.

## References and provenance

- [ScummVM engine guide](https://wiki.scummvm.org/index.php/HOWTO-Engines#top-page). The wiki blocked automated access during initial implementation; integration was checked against current ScummVM engine sources and its project generator.
- [dreamREfactory](https://github.com/dhobi/dreamrefactory) and its [documentation](https://www.danielhobi.ch/dreamrefactory/docs/): primary reference for format layouts, codec behavior and scripting semantics. Its source is GPL-3.0; the native image/audio decoders, bank readers, parser, interpreter and stage reader adapt that work. Audio layouts follow `df/audio.ts`, `banks.ts` and `snd.ts`; playback uses ScummVM's mixer with host semantics informed by the reference runtime. The separate Dust room reader follows its documented transition model. It credits DFET as the origin of several format readers and opcode names. Reference revision: `d08950b58191fd20e22d2184d7334afbe737bb1c`.
- [DreamCatcher](https://github.com/JamesK89/DreamCatcher): reviewed for DreamFactory structures and Pascal-string research. No DreamCatcher implementation code was copied.
- [FreeType](https://github.com/freetype/freetype/releases/tag/VER-2-13-3) 2.13.3 and [Liberation Fonts](https://github.com/liberationfonts/liberation-fonts/releases/tag/2.1.5) 2.1.5 provide native font rendering and the openly licensed font asset. Download URLs and archive hashes are pinned in `tools/prepare-fonts.ps1`; the font project describes its Arial metric-compatibility goal.
- ScummVM base revision: `4aa1ef851824ea1f5569a9504ab1b1a07fe670cc`.

New engine and verification sources are GPL-3.0-or-later. See `scummvm/COPYING` and the reference project's LICENSE. No proprietary game implementation code is included in the engine source.

Native session input now delivers keys and clicks to the supplied BOOTFILE routers,
and runs the supplied `idle` handler between gestures. That handler owns cursor
selection and game-clock calls. Mouse coordinates map to the 512×384 logical
screen; `mouse`, `button`, `stilldown`, `flushevents`, `hittest`, `pointinactor`
and `pointinset` are connected. Picking includes world depth masks, screen props,
smallest painting rectangles, scene backgrounds and stage regions. A bounded
32-entry queue preserves deliberate presses, coalesces repeats and drops a repeat
first when full; display notifications do not enter it. Modal conversation/movie
handlers retain their input ownership. Native fixtures cover these interfaces,
including overlay flat/stage keyboard priority and quit preservation.

`calcdeg` uses signed packed halves and the original integer bearing algorithm:
16-bit coordinate differences, a truncated 9-bit ratio, and a 512-entry atan table.
The mathematical table is verified against all three supplied executables
(retail table 0x45dce0, demo 0x45bad8, Dust 0x4455c8). Native fixtures compare 4,107
cases per edition, including all table ratios, quadrants, axes, ties and wrapped
coordinates. This differs from the reference renderer's nearest-angle bearing.

`trace-boot.ps1 -Steps 40 -Keys rightarrow,leftarrow -ChooseGame -TimeoutSeconds 300`
now exercises the post-startup native session. `-ClickX` and `-ClickY` inject one
logical-screen click for authored UI traces. `-ClickMovies date.mov` explicitly clicks the first authored region of that named title movie so an unattended demo trace can continue. These diagnostics are not substitutes
for complete story, puzzle, inventory and save/load playthroughs.

Movie cleanup now detaches the host’s borrowed movie pointer before polling event sources. Native modal playback fixtures verify that callbacks see no current segment after playback finishes; the named-title click trace exercises this path with the demo’s date card.

The demo's authored Start Demo route now completes: click the menu's demo region,
wait for its porthole callback, continue DATE.MOV, play FLYBY.MOV, initialize the
inventory/interface, and enter C71 at Scene51/View63. Forty session passes complete
without an unsupported command. Reproduce with:

```powershell
./tools/trace-boot.ps1 -Editions demo -ClickX 260 -ClickY 190 -ClickMovies date.mov -Steps 40 -TimeoutSeconds 300
```

The verified capture is `build/demo-gameplay-start.png`; its native RGBA and detailed
trace are retained alongside it. This is an opening-path check, not a completed
playthrough. The `message` command now emits diagnostic script text to the log.


`./tools/verify-gameplay.ps1` now checks authored opening interactions through the
normal native input routers. The demo trace picks up the bag and trunk key,
opens inventory, selects and examines the key, returns with it in hand, turns
to the trunk, and drags the key onto its hotspot to open `Trunk 1`. The demo continues through gramophone horn/drawer controls, watch/map pickup,
and walking to the cabin door. The retail
trace checks bedsit turning. Dust opens/closes its map and inventory, services
the inventory cash loop, and resumes turning in town. These are opening checks;
full story completion and save/load remain unimplemented or unverified.

The trace tool accepts `-Actions` (semicolon-separated in its engine config):
`key`, `click`, `clickon TYPE NAME`, `dragto TYPE NAME TYPE NAME`, `idle`, location
and property/global expectations, and `targets` for visible hit-test discovery.
Names containing spaces work with `clickon`; `dragto` takes single-word names.
Presses finish with a timed release before the next action. `-SkipMovies` injects
Escape only into named segments that permit it; normal gameplay never receives
these diagnostic inputs. `verify-gameplay.ps1` retains `build/gameplay-*.log`
and `.rgba` captures. Use `python tools/preview-rgba.py build/gameplay-dust.rgba`
to view a capture.

Dust's `barndooropen`/`barndoorclose` and Titanic's `wipeleft`/`wiperight` now
reveal held destination pictures at 60 Hz, retaining the displayed old picture
and its colors. The destination stays offscreen until the reveal starts.
Modal waits retain input and honor quit without servicing world loops. Native
checks cover every physical pixel at each strip boundary, both endpoints,
destination preservation, black captures, timing, and the virtual clock for all
three editions. Other named transition effects remain explicitly unsupported.
`plain` now redraws immediately while preserving a pending black hold.

`drawstring` and `stringwidth` share the native Windows-1252 font renderer.
Script text uses logical coordinates and a baseline, persists between redraws,
and replaces text at the same position/size. Flat changes clear the layer before
`openflat`; newly shown props erase intersecting fields. Palette ink and the
existing independent room/stage dimming are applied during composition. Native
stage checks exercise text bounds, persistence, replacement, local prop erasure,
and flat changes for v1/v4. This follows the reference's retained-text model;
exact matching of every original text layout still needs playthrough validation.


`./tools/verify-demo-story.ps1` extends the demo interaction check through
Smethells's opening conversation: continue `penote.mov`, ask both questions,
choose the parting line, assert `smethphase = 400`, open the door, and walk into
HALLA / Scene52 / View63. It then follows the port corridor, opens the Grand
Staircase door and verifies entry into GSTAIR2 / Scene11 / View23. The story
helper accepts `-ExtraActions` and `-Choices` for continuing this route.
`-Choices` on the trace/gameplay helpers takes explicit
`filename.pup:choice-id` entries. They produce normal mouse down/up events at the
native choice rows, once per choice-wait generation. Wrong/missing or unused
choices fail the trace. The trace also requires a successful process exit,
`BOOT READY`, and a capture; a stopped diagnostic cannot pass on partial output.

`./tools/verify-demo-mission.ps1` continues upstairs into GSTAIR1, approaches
Penny at Scene18 / View38, selects her two mission dialogue choices and checks
the authored `phase = 1` assignment state. The native trace completes all 110
actions with five dialogue choices, then returns to the room.

`./tools/verify-demo-photographer.ps1` returns downstairs, enters the starboard
corridor and speaks to Burns through B59's door. All 155 actions and eight
dialogue choices pass, including the authored `burnsphase = tryagain` and
`burnslike = 1` states. The story helpers accept `-TimeoutSeconds` for longer
playthroughs. Diagnostic `waitpropview name view [timeout-ms]` advances normal
session passes until the requested prop view appears; it never sets puzzle
state and fails after its bounded deadline.

`./tools/verify-demo-darkroom.ps1` finds Burns's wife on the staircase, reports
her location, enters B59, switches off the white light and opens the negative
case under the red lamp. `./tools/verify-demo-photograph.ps1` then develops the
Conkling negative in both timed baths and places the good print in inventory.
The native trace passes 248 actions and ten dialogue choices, including the
15-second first bath, 10-second second bath, `photo1` owned by Frank and its
quality value of 1. This exercises the game's scheduler and drag handlers;
none of these puzzle or story states are injected by the test.

`./tools/verify-demo-completion.ps1` passes the supplied September 1996 demo's
winning story path from startup through completion: deliver the developed print
to Penny, finish her dialogue, play and click through `postdemo.mov`, and return
to `demo.stg / demo 1` with `phase = 2` and no room loaded. The native run passes
306 actions and twelve dialogue choices. Its ending screenshot and trace are
`build/demo-completion.png` and `build/demo-completion.log`. This validates one
winning path; alternate puzzle/dialogue outcomes, save/load, retail Titanic's
story and Dust's full story remain incomplete.

`./tools/verify-dust-opening.ps1` verifies Leroy's first conversation and the
authored `leroyphase = 1` departure flag, returning to Scene G15 / north with
mainpanel visible. The native run passes 21 actions and its dialogue choice.
`./tools/verify-dust-bone.ps1` continues up the street, verifies the dog blocks
Scene G12, turns east to the help character and follows the three-question
branch that puts the bone in the street. All 31 actions and four dialogue
choices pass with the bone's small view set by the supplied original scripts.

`./tools/verify-dust-ring.ps1` picks up that bone, offers it to the dog and
receives the ring: 52 actions and five choices pass with `phase = 2`,
`helpphase = 3` and the ring owned by the stranger. `./tools/verify-dust-cards.ps1`
continues through jug pickup, Jones's arrival and his first conversation to
receive the cards. All 73 actions and six choices pass. Diagnostic
`waitactorstar name star [timeout-ms]` waits for normal actor movement without
changing position or story state.

The `countbevels` script command now returns the active puppet's choice count,
including choices currently hidden from display, or zero without a puppet.
Jones's inventory-offer dialogue requires this command. Native puppet fixtures
verify the empty, hidden, full and cleared counts across all three editions.

`./tools/verify-dust-saloon.ps1` continues to Leroy at the shooting range,
verifies `leroyphase = 88`, opens the Hard Drive door and enters SALLOWER.
The native trace passes 107 actions and six choices, checking the saloon's
music-selection counter and the stored town standpoint. Its screenshot is
`build/dust-saloon.png`.

`./tools/verify-full-bedsit.ps1` scores all seven retail London-flat objects,
including their close-up movies, and verifies the authored bomb trigger.
`./tools/verify-full-boarding.ps1` waits for the scheduled raid and crosses to
C73 / Scene51 / View63 with mission 1, phase 0, at 9:30. All 35 actions pass.
The transition uses normal session passes and only authored movie skips;
`waitset` and `waitglobal` observe state with bounded deadlines. This validates
the retail opening, not the remaining missions.

`./tools/verify-full-steward.ps1` collects the bag/watch, finishes Smethells's
first conversation and note movie, receives his deck map and exits C73 into
HALLC / Scene27 / View35. The native trace passes 61 actions and three choices.
Movie event cues that name no clip now leave existing audio alone: retail
`penote.mov` names Smethells's lines, but already carries their voices in its
soundtrack. Native movie fixtures verify absent cues and preservation of an
earlier film's audio across all three editions.

`./tools/verify-full-max.ps1` continues through Penny's gym briefing, walks the
boat deck to Max, follows him to the cafe, completes the Will/Zeitel exchange
and collects the pipe. The native cold-boot run passes 131 actions and ten
choices, including the sailing movie, both puppet switches and `pipe` owned
by Frank. `build/full-max-pipe.log` and `build/full-max-pipe.png` record the run.
`node tools/room-route.mjs full TURK View116 View119` prints a route using the
room's authored turn rings and road endpoints; it does not account for story
gates or change game state.

`./tools/verify-full-mirror.ps1` continues through the F-deck map destination
and Turkish baths, opens the tap, examines the mirror and clicks the clue's
OK region. All 167 actions and ten dialogue choices pass from cold boot,
ending with mission 1 / phase 2, `zeitclue = mirror` and the water reset.
`build/full-mirror.log` and `build/full-mirror.png` record this milestone.

`./tools/verify-full-control.ps1` walks from the mirror clue through STAIR2C,
HALLF2C and TURB, accepts the control-room engineer's request and opens
`TURBINE.STG / Turbine 1`. The native run passes 232 actions and twelve choices;
`build/full-control.log` and `build/full-control.png` record the result.
This verifies entry to the puzzle.

`./tools/verify-full-turbine.ps1` now regulates the plant using held mouse
gestures. It winds the outlet valve to 7 and the condenser pump to 15, moves
the coal lever to 7 and holds the valve until the electrical output passes the
authored gate. The native run passes 252 actions and twelve choices, including
the engineer's approval and mission 1 / phase 3. `build/full-turbine.log` and
`build/full-turbine.png` record the result; later missions remain unverified.

`./tools/verify-full-rubaiyat.ps1` continues through ENGINE and the first three
boiler rooms, opens chute four and recovers the Rubaiyat: 329 actions and
twelve choices pass. `./tools/verify-full-rubaiyat-stash.ps1` then hides it in
chute five through the inventory panel and its modal placement loop. All 351
actions pass with the book owned by `coal5`, the clue still naming `coal4`,
and Frank's hand empty. Each helper's trace and PNG are under `build/full-…`.

`./tools/verify-full-vlad.ps1` climbs to the upper landing and completes Vlad's
four-choice package-request conversation. All 366 actions and sixteen total
choices pass from cold boot, with Vlad's owner state `help`, the Rubaiyat still
in `coal5`, mission 1 / phase 3 and no death flag. `build/full-vlad.log` and
`build/full-vlad.png` record the result.

`./tools/verify-full-sasha.ps1` exits the boiler rooms before using the map
(authored scripts disable map travel there), enters the A-deck corridor from
the port side and knocks on A-16. Five dialogue choices take Frank inside
Sasha's cabin, obtain Vlad's package and return him to the corridor. All 443
actions and 21 total choices pass with `vladfood` owned by `frank`, the Rubaiyat
still in chute five, phase 3 and no death flag. `build/full-sasha-native.log`
and `build/full-sasha.png` record the result.

`./tools/verify-full-package.ps1` delivers the package to Vlad before recovering
the Rubaiyat from chute five, advancing to mission 1 / phase 4. The complete
`./tools/verify-full-mission1.ps1` route also returns to Penny's F-34 cabin,
reports the book, mirror clue and Vlad/Sasha connection, clicks the Sasha
movie's region and finishes the debrief. All 651 actions and 27 dialogue
choices pass from cold boot, reaching mission 2 / phase 0 with the Rubaiyat
owned by Frank, the package owned by Vlad and no death flag or mixer
exhaustion. `build/full-mission1-native.log` and `build/full-mission1.png`
record the result. These checks use normal input with diagnostic speech
skipping; later missions, alternate routes and save/load remain unfinished.

Retail script effects now use the original two replaceable sound channels.
`singlesound`, `multiplesound`, `dualsound` and `bothsound` have distinct
arbitration rules based on the one-based sound record index within its bank.
Native mixer fixtures cover rejection, replacement, both-channel playback
and 100 repeated effects; audio corpus and mixer checks pass for all three
editions without slot exhaustion. The supplied September demo now uses its
own verified channel rules and bank priority IDs, described below. Dust now
uses its verified original arbitration too. Movie cue ownership still needs
independent verification.
The corrected retail build also passes the complete 366-action replay through
the turbine, book retrieval/stash and Vlad conversation without mixer
exhaustion (`build/full-vlad-sound-slots.log` and its PNG capture).
`python tools/inspect-sound-slots.py` reproduces the read-only, executable
hash-checked disassembly in `build/original-sound-slots-evidence.log`.

For quicker route checks, set `DREAMFACTORY_BOOT_SKIP_SPEECH=1` in the test
process's environment. `trace-boot.ps1` also accepts `-SkipSpeech` directly.
This optional diagnostic sends normal Escape key events during spoken puppet
lines and logs each skip; it does not set story state. It is disabled by default.
The control-room run validates this mode; the mirror run used full speech.

`./tools/verify-dust-blackjack.ps1` enters the blackjack table, declines Jan's
invitation and restores the saloon with cash unchanged: 119 actions and seven
choices pass. `-PlayHand` now passes one ordinary $1 hand, Stand, Jan's timed
next-bet prompt and Quit: 125 actions and ten choices. Winning enough money
for later story progression remains unverified. An early
test pressed Stand after a natural blackjack had already settled the hand;
its nested reset closed the sound bank before the older handler finished.
That rapid-input case remains unresolved; the updated route waits for Jan
when the authored winner flag is already set.

Inherited stage handlers now bind `me` to their flat or stage script while
retaining the addressed button as `target`. Dust's Stand button previously
armed the next-hand timer for a nonexistent flat named `stay`. The correction
follows both original engines' context construction; native context/timer
fixtures pass for all three editions. [inspect-stage-context.py](tools/inspect-stage-context.py)
reproduces the read-only disassembly with executable hash checks.

`./tools/verify-full-gym.ps1` walks from the boat-deck map destination through
the starboard vestibule, waits for Penny to finish her idle turn, and completes
her first briefing and Zeitel movie. The cold-boot route passes 92 actions and
seven choices, reaching mission 1, phase 1 in the gym. `calcvectx`/`calcvecty`
now support the actor's scripted approach; all-heading and signed-magnitude
regressions pass against the primary reference in all three editions. The
demo's complete winning route also passes after the stage-context correction.

Object-loop reentrancy checks now inspect the first resolved handler, rather
than every containing/BOOT script with the same name. The demo's gramophone
`idle` was otherwise permanently held while BOOT's unrelated `idle` yielded.
Prop regressions exercise an independent callback during BOOT idle and a
self-rearmed callback that must not re-enter. Stage, room, cast and prop checks
pass across all three editions. `sendtoboot`/`sendtobootfx` now evaluate deferred
arguments in the caller and address the first matching BOOTFILE container,
including library containers. Native fixtures check recipient/caller context,
return values, absent handlers and the independent-idle path.

`starxyz` now reads the shared named-star registry (axes 1/2 ground, 3 height,
4 signed packed ground point, default axis 1; unknown names/axes return zero).
This lets the corridor's actor setup finish during the demo's cabin exit. Native
fixtures cover case-insensitive names, all axes, negative packed halves and
missing names. Alternate demo outcomes, retail/Dust story progression and
save/load remain pending.

## Native save/load checkpoints

Experimental gameplay now implements ScummVM save slots and the original
`savegame` / `opengame` commands. The game's control-panel save/open buttons
use ScummVM's chooser; runtime menus and loading a slot at startup are wired
through the engine API. The development room viewer still has no saves.

The native `DFSV` version-5 format retains BOOT globals, resource paths and CD,
canonical asset sources, actors/props and runtime copies, inventory, stage and
room/camera state, palettes/text, clocks/random state, timers, actor motion,
crickets, Dust projectiles and audio playback. Version-1/2/3/4 checkpoints remain readable. Audio keeps decoded PCM and an offset estimated
from mixer milliseconds, including clips whose original movie has closed.
Original TI/DF save-file import is not implemented. Saves during conversations,
movies or camera transitions are unavailable; active VM stacks are not saved.

Loading validates bounded fields, checksum, exact BOOTFILE identity, assets and
visible artwork in a detached session before replacing the live state. It
reconstructs script owners without replaying open callbacks. Successful loads
abandon the old calling script, so the control-panel handler cannot overwrite
the restored game. Modal dialogs resynchronize the game's clock on return.

`./tools/verify-save-state.ps1` passes cold-boot round trips, fresh-process
restores, ScummVM slots and the authored control-panel paths in all three
editions. Physical clicks use the actual button centers, including Titanic's
switch props. Nine damaged fixtures and a cross-edition checkpoint reject
without changing the live room, stage, time or globals; navigation still works.
`tools/save-fixtures.py` produces the bounded-count, truncation, checksum,
identity and missing-asset cases.

The optional `-Story` check saves after Sasha's package conversation, completes
retail mission one, rolls back to HALLA / Scene51 / View58 with `vladfood=frank`,
`rubaiyat=coal5`, mission 1 / phase 3, then starts a new process from that save
and completes the remaining route to mission 2 / phase 0. Both the rollback
and fresh-process continuation pass in the native engine. Evidence is in
`build/save-full-story-native.log` and `build/save-full-fresh-story-native.log`.
The latter route writes `build/save-full-mission2.dfsv`; independent restoration
also passes with the delivered package and recovered Rubaiyat intact.

The control-panel diagnostics select a real save slot automatically; interactive
chooser operation and audible playback restoration still need manual QA. Later
story checkpoints, arbitrary puzzle checkpoints and original save imports are
not covered. Full playability of the three games remains the active goal.

Audio, stage, room, cast and prop regressions pass across all three editions.
The room fixture now checks the stage library's own `me` and the addressed
scene's `target`, including closure of the stage while its script is running.
The existing Dust blackjack route also passes with normal dialogue. Sending
Escape to skip Dust's help dialogue changes the jug-pickup route; that diagnostic
mode is not a substitute for the normal-dialogue Dust replay.
The complete demo winning route also passes after save/load integration: all
306 actions and twelve choices reach phase 2 and play `postdemo.mov`
(`build/save-demo-completion-native.log`).

## Retail mission two: telegram and painting

From the mission-two checkpoint produced by the save/story validation:

```powershell
./tools/verify-full-telegram.ps1
./tools/verify-full-painting.ps1
```

The telegram route collects Thayer's message, obtains Morrow's wireless
permission and uses physical drags to power the transmitter, select transmit
mode and tune the radio. Morse input sets the authored `purs=sentgram` state.
The painting route reports back to the Purser, reads the nested cargo manifest,
finds and returns Straus's cufflink, takes the car keys, walks the forecastle and
crew passages, obtains cargo access, switches on the car lights and retrieves
the painting. No globals or inventory are injected. Native checkpoints separate
each verified stage; the final `build/save-full-painting.dfsv` has mission 2 /
phase 1, `painting=frank`, `cufflink=purser` and `bseaphase=1`.
Independent restoration also passes: the player exits the cargo close-up and
turns normally, with the painting and story phase intact
(`build/full-painting-restore-native.log`).

Ordered diagnostic `-MovieClicks` entries select a region by target, event,
sound or a one-based ordinal (`key.mov:#1`). They generate mouse presses and
releases, including nested movies and revisited frames.
The `dragarcuntil` trace gesture releases when the selected prop reaches its
authored owner state; the tuner test watches for the transmit-band light.
The movie host now restores the parent filename when returning from a child. Native traces disable
the connected controller to isolate automated input. The route planner prefers
retail CD2 DATA resources over the flat CD1 aliases, matching story playback.

Evidence is in `build/full-telegram-suite.log`, `build/full-painting-suite.log`
and the corresponding `build/full-*-native.log` stage logs. The retail ending
and Dust's remaining story still require implementation
and validation; this milestone does not establish full playability.

## Retail fencing and ring recovery

`./tools/verify-full-fencing.ps1` continues from the painting checkpoint. It
walks out of cargo through the hold, crew quarters and forecastle (the authored
map prevents jumping out of cargo), approaches Smethells in the staircase,
follows his lead to the squash court and accepts Haderlitz's foil.

Fencing initially stopped at unsupported opcode 12086, `propwarm`. This is now
implemented as sprite preloading into the shop's shared decoded-frame cache,
without changing a prop's pose, visibility, animation or story state. Both
fighters' preload calls and the playable stage now pass in the native engine.

The tested bout leaves the player's foil centered: Haderlitz scores five normal
points, post-game dialogue runs and the player returns to the squash corridor.
Smethells then supplies Haderlitz's ring through the authored fallback dialogue,
advancing mission two to phase two. The resulting `build/save-full-ring.dfsv`
has `ring=frank`, `painting=frank`, `fencecount=1` and `willie=lost`.
Evidence: `build/full-fencing-suite.log` and the individual native stage logs.
Independent restoration of the ring checkpoint also passes, including inventory,
phase and room navigation (`build/full-ring-restore-native.log`).

`./tools/verify-full-fencing-controls.ps1` verifies two player attacks, advancing
from fencing position 8 to 10 with attack counts 1 and 2. Pointer movement
selects the left defensive stance. Saving and restoring that active bout in a
fresh process preserves its position, combat flag, attack count and selected
pose (`build/full-fencing-controls-suite.log`). The trace-only `move` command
posts a normal mouse-motion event without pressing a button. A rendered capture
is in `build/full-fencing-controls-restore.png`.

Successful parries and the winning-bout route still need their own validation;
this lost-bout route and basic control test do not establish complete fencing
support. Later retail and Dust story progression remains incomplete.

## Retail mission-two completion

`./tools/verify-full-mission2.ps1` continues from the ring checkpoint to Claris's
D-deck cabin. Four authored choices identify the ring and retain it, advancing
to phase three. The route then returns to Penny's F34 cabin, reports the
painting and Haderlitz's notebook, plays `plans.mov` and `penybook.mov`, and
accepts the next mission. Normal dialogue and physical navigation pass without
editing story state.

The resulting `build/save-full-mission3.dfsv` has `mission=3`, `phase=0`,
`gaspen=frank`, `painting=frank` and `ring=frank`, in HALLF2C / View76.
Evidence: `build/full-mission2-suite.log`, `build/full-claris-native.log` and
`build/full-mission3-native.log`. The bath death investigation starts in mission
three; it is not required to advance mission two's ring phase.

`./tools/verify-full-bath-investigation.ps1` starts from that mission-three
checkpoint. It questions Smethells and Morrow, waits for Morrow's return walk
before opening the guarded door, enters the Turkish-bath passages and examines
Haderlitz in the electric bath. The clue enlarges and enters inventory through
two normal clicks. `build/save-full-rubiclue.dfsv` preserves `rubiclue=frank`,
`mission=3`, `phase=0`, `morrowphase=1` and `smethphase=1` in EBATH / View19.
The helper restores it in a fresh process and verifies a right turn to View16.
Evidence: `build/full-bath-investigation-suite.log`,
`build/full-bath-permission-native.log`, `build/full-rubiclue-native.log` and
`build/full-rubiclue-restore-native.log`. Later mission-three puzzles and the
remaining retail story still require validation.

`./tools/verify-full-hacker-clue.ps1` follows the bath clue through the
third-class passages to the rope. The rope movie triggers the authored camera
turn and Hacker encounter. Four normal dialogue choices exchange `rubiclue`
for `hackclue`; Hacker then completes his departure walk. The helper saves
before and after that walk, restores the final checkpoint in a fresh process,
opens inventory, selects the clue and examines `hackclue.mov`.

`build/save-full-hacker-clue.dfsv` has `hackphase=1`, `hackclue=frank`,
`rubiclue=xxxfrank`, `mission=3` and `phase=0` in SCOT3 / View22.
Evidence: `build/full-hacker-clue-suite.log`, `build/full-hacker-clue-native.log`
and `build/full-hacker-clue-restore-native.log`. The notebook continuation is
described below.

`./tools/verify-full-cigarette-lead.ps1` continues from Hacker's checkpoint.
It visits the café table, enlarges and collects the Old Reds pack, then returns
it to Max in the smoking room through normal dialogue. The handoff advances
to `mission=3`, `phase=1`; `oldreds=max` and `hackclue=xxxfrank`.
The resulting checkpoint is `build/save-full-smokestack-lead.dfsv`.
See `build/full-cigarette-lead-suite.log`, `build/full-oldreds-native.log` and
`build/full-smokestack-lead-native.log`. The engineer's engine-room door at this
phase requires a second turbine repair (`csea=thanks2`).

`./tools/verify-full-turbine2.ps1` follows that lead back to the engineer and
solves the second turbine puzzle using valve, pump and coal-slider gestures.
It restores the exit checkpoint with the engineer's reply still scheduled,
verifies the saved timer unlocks input, and writes `build/save-full-turbine2.dfsv`
with `csea=thanks2`, `mission=3`, `phase=1`, at CONTROL / View24.
Evidence: `build/full-turbine2-suite.log` and the native ready/repair/restore logs.

`./tools/verify-full-vlad-fight.ps1` enters the engine room and waits for Vlad's
automatic ambush walk and dialogue. A jab–jab–cross combination reduces his
health from 512 to 465. A fresh process restores that active fight and wins
using further normal combinations. `build/save-full-vlad-defeated.dfsv`
preserves `vlad=lostfight`, `mission=3`, `phase=2`, at ENGINE / View119.
Evidence: `build/full-vlad-fight-suite.log`, `build/full-vlad-fight-controls-native.log`
and `build/full-vlad-fight-win-native.log`.

`./tools/verify-full-notebook.ps1` restores the won fight, enters the smokestack
and reads its naturally selected layout from the authored entry message.
`tools/smokestack-route.mjs` plans navigation from the actual room turns, roads,
barrier scripts and stairs; every native gesture is followed by floor and view
checks. Routes through all four layouts are found. Layouts 1 and 3 have passed
native traversal to floor eleven; layouts 2 and 4 still require native replay.

The normal-dialogue route picks up the notebook, plays Zeitel's confrontation
and `berg.mov`, selects the gas pen through the conversation's inventory panel
and gives it to him. `build/save-full-notebook-recovered.dfsv` preserves
`mission=3`, `phase=3`, `notebook=frank` and `gaspen=zeit`, at SMSTACK3 / View52.
A fresh process restores it and turns to View54. Evidence:
`build/full-notebook-suite.log`, `build/full-smokestack-maze-native.log`,
`build/full-notebook-recovered-native.log` and `build/full-notebook-restore-native.log`.

The trace-only `-ModalClicks` parameter takes ordered `flat:prop:name` or
`flat:button:name` cues. These deliver normal mouse presses while the authored
inventory `handleselect` loop is waiting, after the previous item's handler and
release loop finish. This enables validation inside a blocked conversation;
it does not select inventory or change story state directly. The native build,
demo startup and Dust's normal opening regressions pass after this addition.

`./tools/verify-full-mission3.ps1` descends from the recovered notebook encounter,
plays the authored smokestack exit, Vlad's theft and chapter transition, and
switches from CD2 story resources to CD1 sinking resources. The final
`build/save-full-mission4.dfsv` has `mission=4`, `phase=0`, `notebook=vlad`,
`painting=frank` and `rubaiyat=frank`, in C73 / View63. A fresh process restores
that checkpoint and turns to View65. See `build/full-mission3-suite.log`,
`build/full-mission4-native.log` and `build/full-mission4-restore-native.log`.
The sinking and ending continuation is described below.


`./tools/verify-full-sinking-shawl.ps1` passes Penny's post-collision briefing,
the physical route from C73 through the first-class staircase and lounge hall,
and Claris's shawl handoff. Normal dialogue produces `shawl=frank` and
`clarisphase=1`; no map travel or story variables are injected.
Evidence: `build/full-sinking-shawl-suite.log`.

`./tools/verify-full-sinking-notebook.ps1` covers the lounge-to-boat-deck route,
second-class staircase descent, engineer warning and Vlad's shawl-for-notebook
trade. The complete corrected four-stage suite passes
(`build/full-sinking-notebook-suite.log`). `tools/stair2c-route.mjs` models the staircase's
repeated deck state and authored double turns. `tools/room-route.mjs` accepts
an optional retail volume argument (`1` for sinking, default `2`).
`build/save-full-sinking-notebook.dfsv` preserves `notebook=frank`, `shawl=vlad`,
`painting=frank` and `rubaiyat=frank`. Native descent and trade evidence:
`build/full-sinking-stairs-bottom-native.log` and
`build/full-sinking-notebook-native.log`.

The authored clock normalizes `startdisk2` and accumulated seconds during idle
updates. Rapid diagnostic movement can save between those updates; this does
not require a gameplay clock workaround. `tools/verify-full-sinking-clock.ps1`
checks live normalization. The script audit also executes the shipped
`calctime` and tests all six `canadvance` boundaries. Demo, retail and Dust
script audits pass (`build/sinking-clock-boundaries-audit.log`).

`./tools/verify-full-sinking-return.ps1` passes the complete staircase ascent,
engineer's automatic upward warning, boat-deck return, fresh restoration and
ordinary idle progression through `sink1.mov` to `mission=4`, `phase=1`.
`build/save-full-sinking-phase1.dfsv` retains all three recovered evidence
items. Evidence: `build/full-sinking-return-suite.log` and
`build/full-sinking-phase1-native.log`. Later sinking phases and Dust completion remain unverified; the best-ending replay is recorded below.


`./tools/verify-full-ending-return.ps1` follows the Gorse-Joneses' lifeboat
offer from the phase-one boat-deck checkpoint. Normal dialogue and unskipped
movies pass through `leave.mov`, `debris.mov`, the NAREND narration and
`rushend.mov`. A physical click on `playmore.mov`'s first region chooses the
return-to-ship option, playing `bktoship.mov` and `datecab.mov`. The authored
reset returns to `mission=1`, `phase=0`, C73 / View63 and CD2 resources.
`build/save-full-retail-ending-return.dfsv` saves that restart; a fresh process
restores it and turns to View65. Evidence:
`build/full-retail-ending-return-suite.log`,
`build/full-retail-ending-return-native.log` and
`build/full-retail-ending-restore-native.log`.

This verifies one full retail route across the earlier mission checkpoints
through an unsuccessful ending. `realneck=vlad` selects that ending despite
the other three evidence items being recovered. Later sinking phases, other branches and the full Dust story remain incomplete or unverified; the best ending now passes as recorded below. An initial ending-menu probe reached `playmore.mov`
without a queued click and was stopped while awaiting input; its evidence is
`build/full-retail-ending-menu-probe-native.log`. The corrected replay passes.


The necklace route exposed an arrival-callback deadlock in B70: Georgia's
`endwalk` handler starts a turn and waits with `forceupdate`, while the old
movement guard prevented that new turn from advancing. `motion.cpp` now
finishes the protected update pass before dispatching turn/arrival callbacks.
The bounded nested-walk regression in `tools/cast-audit.mjs` distinguishes the
old behavior; actor audits pass for demo, retail and Dust. Evidence:
`build/movement-callback-build.log` and
`build/movement-callback-cast-audit.log`.

`tools/verify-full-necklace.ps1` passes Georgia's boat-deck introduction and
replica-necklace handoff (`neckphase=1`, `fakeneck=frank`). The optional `-Probe`
mode captures the approach separately. `tools/verify-full-georgia-cabin.ps1`
passes the A-deck continuation (`neckphase=2`) and, with `-CabinOnly` and the
A-deck checkpoint, B70's automatic arrival, all three Georgia/Charles puppet
switches and return to HALLB / View49 at `neckphase=5`. The corrected stages passed separately and subsequently passed within the best-ending replay below. Evidence:
`build/full-georgia-necklace-suite.log`, `build/full-georgia-adeck-native.log`
and `build/full-georgia-b70-suite.log`.

`tools/verify-full-charles-necklace.ps1` passes Charles's smoking-room
conversation through `neckphase=6`, retaining the replica. Evidence:
`build/full-charles-necklace-suite.log`. The fuse/doll continuation and necklace recovery are described below; a
best-ending replay subsequently passed as recorded below.
Trace-only diagnostics log actor state/camera on a failed actor click, and
`dumpprop NAME` reports a prop's angle, view and owner without changing it.


`tools/verify-full-necklace-fuses.ps1` now passes as a complete three-stage
suite: restore an open panel, turn off the A14 switch region, let its timer
settle, then restore and turn the power back on. Alex's automatic hallway
greeting runs normally before the panel opens. Switch and indicator hit areas
are distinct; the OK click uses the center of the authored confirmation region.
Evidence: `build/full-necklace-fuses-suite.log`.

`tools/verify-full-necklace-doll.ps1` reads the randomly initialized dial angles
through `dumpprop`, saves, restores, and applies normal mouse arcs to reach
`dial1=6`, `dial2=0`, `dial3=0`, `dial4=0`. It opens the doll, collects the real
necklace and places the replica using inventory selection and a physical drop.
No story or puzzle values are written by the driver. Evidence:
`build/full-necklace-doll-suite.log`, `build/full-necklace-swapped-native.log`
and the inspected `build/full-necklace-swapped.png`.

`tools/verify-full-necklace-escape.ps1` restores that active puzzle, closes it,
leaves A14, plays Sasha's discovery/swap response and retains `realneck=frank`,
`fakeneck=vlad`, `neckphase=9`, with no player death. A fresh process restores
`build/save-full-necklace-recovered.dfsv` in HALLA / View58 and turns to View59.
Evidence: `build/full-necklace-escape-suite.log`.

`tools/verify-full-necklace-resume.ps1` returns this branch by normal map and
corridor navigation to HALLF2C / View76, still at `mission=2`, `phase=0`, with
`realneck=frank` and `rubaiyat=frank`. `build/save-full-necklace-mission2.dfsv`
is the entry point for replaying the remaining retail story with the necklace
quest completed. Evidence: `build/full-necklace-resume-suite.log`.
After the movement fix, demo startup and Dust's normal opening/Leroy dialogue
pass (`build/movement-callback-demo-regression.log` and
`build/movement-callback-dust-regression.log`).

Retail best ending verified (October 13)
--------------------------------------

`tools/verify-full-best-evidence.ps1` replays the remaining retail story from
an ordinary painting checkpoint, retaining the painting, Rubaiyat, real
necklace and notebook. The painting/fencing quest must precede the necklace
quest: the authored cargo script gives the painting to Hacker after its
10,000-frame deadline. `tools/verify-full-painting-deadline.ps1` separately
passes that late-arrival theft behavior; no clock or ownership override is used.

`tools/verify-full-best-ending.ps1` boards the lifeboat through normal dialogue,
plays `leave.mov`, `debris.mov`, the successful NAREND narration, `prozac.mov`
and `credits.mov`, clicks all fourteen credit pages and reaches the scripted
quit. Evidence: `build/full-best-ending-suite.log` and
`build/full-best-ending-native.log`. The necklace and remaining story stages
pass across saved boundaries in `build/full-best-necklace-continuation.log`,
`build/full-best-story-continuation.log` and
`build/full-best-boatdeck-continuation.log`.

Opcode 12071 (`quit`) now stops the session and abandons nested VM calls and
pending input, retaining diagnostic world state until session destruction.
The quit reason is transient and reset on initialization and save restoration;
the DFSV format is unchanged. Nested-call and external-quit distinction checks
pass in all three native script audits (`build/scripted-quit-script-audit.log`).

Dust saloon progression verified (October 13)
-------------------------------------------

`verify-dust-trotter.ps1` passes the peaceful drink conversation and departure.
Blackjack passes Hit/Stand, saved active hands, normal zero-cash automatic
closure/restoration (`verify-dust-bankruptcy.ps1`) and a normal $1 win taking
cash from $5 to $6 (`build/dust-blackjack-five-dollars-suite.log`). Poker passes
entry, ante, betting, physical card discards, draw and scoring of a losing hand
through saved boundaries (`build/dust-poker-five-dollars-*-native.log`); a poker
win, repeated hands and the complete exit remain unverified.

`verify-dust-night-street.ps1` physically leaves the saloon through its authored
D1 east exit with cash $6, advancing day 1/night from phase 2 to 3. The resulting
`build/save-dust-night-street.dfsv` preserves the ring and starts Bloodstone's
street patrol. Dust's later days and completion remain outstanding. The trace
`dumpactor NAME` command reports movement state without modifying it.

Dust's street/hotel continuation also passes: `verify-dust-blood-hotel.ps1`
clicks Bloodstone during his authored patrol, accepts the cigar and follows him
into the hotel (phase 4). `verify-dust-hotel-ring.ps1` selects the ring through
the avatar inventory and drags it onto Fearwitt, paying for the room (phase 5)
without spending the player's $6. `verify-dust-hotel-key.ps1` leaves and returns
through the physical doors, triggering room readiness (phase 6), then plays
Jack's note and collects `hrkey` (phase 7). Corresponding
`build/dust-{blood-hotel,hotel-ring,hotel-key}-suite.log` and native logs pass.
`build/save-dust-hotel-key.dfsv` is the next story checkpoint. Bloodstone never
stops at his street endpoints: his arrival callback immediately reverses his
patrol, so the helper uses normal elapsed time and an on-screen actor click.
No engine movement fix was needed for that encounter.

Dust first night and iris transitions (October 13)
------------------------------------------------

`verify-dust-night-warning.ps1` passes Jones's arrival callback and warning,
retaining his authored message state. `verify-dust-night-wine.ps1` returns to
Bloodstone's automatic hotel encounter and advances phase 8 to 9.
`verify-dust-first-night.ps1` climbs the stairs, answers Buick's introduction,
enters the player's room, clicks both sleep-movie regions and plays the
unskipped overnight movie. `build/save-dust-day2-morning.dfsv` reaches day 2,
clock 1, phase 0 with $6, room key and no death. A fresh process exits the room
and passes Jones's hallway greeting at phase 1
(`build/dust-day2-wakeup-native.log`; `verify-dust-day2-wakeup.ps1`).

Breakfast exposed an unsupported `irisopen` in `HOTPLATE.FLT`. The native
renderer now implements rectangular iris open/close with held source and
destination pictures, scaled logical coordinates and modal waits. The original
Titanic renderer uses the requested number of passes; Dust uses sixteen
opening passes or six closing passes and divides the requested delay among
them. `tools/inspect-iris-effects.py` reproduces the read-only disassembly and
checks both original executable hashes (`build/iris-original-disasm.log`).
All three screen audits pass pixel comparisons at both logical resolutions,
retained destination, displayed capture, timing, virtual clock and quit checks
(`build/iris-transition-screen-audit.log`).

The first breakfast replay passes Buick's morning conversation, Laurel's
Kid information and the meal interface at day 2/clock 1/phase 2. Buick's script
deducts a dollar before presenting his final request, even when declined;
that authored behavior is preserved. Dust's remaining days, combat, late
puzzles and completion still require verification and implementation.

The final breakfast replay also passes with Dust's fixed-pass iris timing
(`build/dust-day2-breakfast-suite.log`). `verify-dust-breakfast-items.ps1`
restores the active meal, collects sugar and biscuits through their authored
buttons, closes it and saves `build/save-dust-day2-after-breakfast.dfsv` in the
hotel lobby. A fresh process restores both items, the room key and ring owner,
confirms day 2/clock 1/phase 2/cash $5 and turns normally without death
(`build/dust-after-breakfast-restore-native.log`).

Dust livery, well and boots (October 13)
--------------------------------------

Ned's automatic livery conversation exposed a movement difference between
Dust and Titanic. Dust removes a completed turn-only record before dispatching
`endturn`; Ned's handler uses that callback to open his conversation. Dust also
queues already-facing turns and replaces a running walk when `turntodeg` is
requested. The native service now implements those behaviors for v1 casts.
`tools/inspect-dust-turns.py` reproduces the read-only original DF.EXE evidence
with an executable hash check (`build/dust-turn-original-disasm.log`). All three
actor/movement audits pass, including standalone-turn callbacks, same-facing
turns, walk replacement without arrival, and nested movement waits
(`build/dust-turn-callback-cast-audit.log`).

`verify-dust-day2-livery.ps1` passes Ned's automatic arrival and departure.
`verify-dust-well-gun.ps1` lets him meet Quist through normal street movement,
clicks both well-movie regions and collects the gun (`gun=stranger`, degree 1,
`nedphase=1`). `verify-dust-pie.ps1` picks up the authored town pie;
`verify-dust-boots.ps1` enters Sidewinder's shop, answers his greeting and drags
the pie onto him, receiving boots (`boots=stranger`, degree 1, `pie=side`).
Corresponding `build/dust-{day2-livery,well-gun,pie,boots}-suite.log` and native
logs pass. The gun and boots satisfy the morning's two-item progression gate;
no story, ownership, timer or RNG values are injected by these helpers.

`verify-dust-day2-afternoon.ps1` leaves Sidewinder's shop and advances through
ordinary town turns/walks, saving and restoring each movement boundary. The
shipped `canadvance`/`scenecounter`/`triggerx` chain plays the unskipped
`d2md2a.mov` and reaches day 2, clock 2, phase 0. The final
`build/save-dust-day2-afternoon.dfsv` retains the gun, boots and sugar. Native
evidence: `build/dust-day2-afternoon-native.log` and
`build/dust-afternoon-walk-19-native.log`; fresh restoration and a normal turn
pass in `build/dust-afternoon-restore-native.log`. The driver reads only native
checkpoint globals and native location logs to choose the next physical move.
Later Dust story, combat, late puzzles and completion remain unfinished.

Dust's normal opening through the saloon also passes after the turn-callback
fix (`build/dust-turn-callback-opening-regression.log` and native log).

Authored room entrances and afternoon handkerchief (October 13)
--------------------------------------------------------------

The pharmacy exposed a shared room-entry bug: `opensetfile("room.set")`
converted its missing scene argument to the numeric value's text, `"0"`, so
room entry fell back to the first available standpoint rather than the authored
entrance. The host now passes an empty scene name when that argument is absent.
A new synthetic room with a non-first default scene verifies omitted scene,
explicit scene/default view, and explicit scene/view cases. It fails on the old
binary at result 17 (`build/room-default-before-fix-native.log`) and all three
room audits pass after the fix (`build/room-default-after-fix-audit.log`).

The affected first-night helper now verifies HOTUPPER's D1/north entrance and
HOTROOM's B1/west entrance before walking to the bed. Its complete replay passes
(`build/room-default-dust-night-regression.log`). Demo and retail startup
regressions also pass (`build/room-default-{demo,retail}-startup-regression.log`).
Earlier successful traces that entered the first available standpoint remain
historical evidence; they do not establish the corrected entrance behavior.

`verify-dust-handkerchief.ps1` enters the pharmacy at C2/west, approaches the
counter and picks up `hankerchief` at degree 0. The misspelling is the game's
resource name. `verify-dust-handkerchief-chit.ps1` returns physically to the
hotel and drags it onto Fearwitt, obtaining degree 1. Both suites pass; a fresh
process restores `build/save-dust-handkerchief-chit.dfsv`, retains the gun,
boots, day 2/clock 2/phase 0 and turns normally without death
(`build/dust-handkerchief-chit-restore-native.log`).

Dust afternoon trades and Cobb confrontation (October 13)
--------------------------------------------------------

`verify-dust-buick-wine.ps1` redeems Fearwitt's chit with Buick and chooses the
wine (hankerchief degree 1 to 3). `verify-dust-blood-skooch.ps1` exchanges it
with Bloodstone (degree 4), and `verify-dust-laurel-bullets.ps1` trades that item
to Laurel, receiving ammunition and reaching `laurelphase=2`, `laurelgood=1`.
All use normal conversation choices and modal inventory selection; corresponding
suite/native logs pass. `verify-dust-bank-deposit.ps1` opens a bank account
with $1, leaving $4 cash and preserving the gun, boots and ammunition.

The modal inventory replay driver previously waited to observe the selected
prop's handler running before delivering OK. Dust's panel selection can return
without yielding, so it was never observed and the driver stalled. It now also
accepts completion after the physical click is released, retaining the delay
before the next press. This affects trace input only. The real Buick, Bloodstone
and Laurel selectors pass, as does Titanic's full smokestack/notebook/gas-pen
sequence with modal inventory selection and fresh restoration
(`build/modal-selection-retail-regression.log`). The naturally selected maze
in that regression is recorded in `build/full-smokestack-entry-native.log`.

`verify-dust-flippo-alarm.ps1` hears the newspaper-office warning, plays Jones's
interruption and places Cobb and Dell at the Chinese store (phase 1).
`verify-dust-cobb.ps1` selects the gun through inventory, approaches physically,
answers Cobb's threat, raises the gun and fires. `cobbshot.mov`, Dell's surrender
and `phase=3`, `bulletcount=5`, no death pass. Checkpoint:
`build/save-dust-cobb-defeated.dfsv`; evidence: `build/dust-cobb-suite.log` and
`build/dust-cobb-defeated-native.log`.

`verify-dust-sheriff.ps1` follows the Cobb checkpoint to the Mayor's appointment,
receives the badge, places Dell in jail and plays the unskipped `d2ad2n.mov`.
`build/save-dust-day2-night.dfsv` reaches day 2, clock 3, phase 0 in JAIL with
`badge=stranger`, `dell=jail`, the gun/boots/ammunition and bank account $1.
A fresh process restores those states and opens/closes inventory normally
(`build/dust-day2-night-restore-native.log`). Native appointment evidence:
`build/dust-sheriff-suite.log` and `build/dust-sheriff-native.log`. Dust's later
night events, days 3–5, remaining combat/puzzles and endings remain unfinished.


Dust third-night progression now passes Jones/Laurel reconciliation, declining
Mezamee's poker offer, the five-hunter bounty fight, Trotter's post-fight dialogue
and Mezamee's Thunderbird reward. The combat trace uses normal inventory input,
physical shots and reload clicks; all five death poses, the rooftop death movie,
player survival and the saloon unlock pass. Checkpoints include
`build/save-dust-bounty-all-defeated.dfsv`, `build/save-dust-bounty-trotter.dfsv`
and `build/save-dust-tbird.dfsv`. Each was earned by authored gameplay.
`tools/verify-dust-bounty.ps1` passes the consolidated combat route in a fresh
native process (`build/dust-bounty-complete-native.log`), including survival
and the post-fight unlock.

The encounter exposed missing `actorwarm`/`castwarm` calls. They now decode all
member artwork into the shared immutable cast cache without altering actor
state. Hash-checked original-binary evidence is in
`build/dust-warm-original-evidence.log`; all three editions pass native cache,
state-preservation and malformed-sprite checks in
`build/dust-actor-warm-cast-audit.log`. `shopwarm` was still pending at that milestone; it is now implemented for the snake puzzle.
Read-only trace assertions now include actor pose and value for combat checks.


`tools/verify-dust-tbird.ps1` passes the post-bounty reward conversation.
`tools/verify-dust-third-night.ps1` then returns to the hotel, hears Buick's
required third-night conversation (101,103,104), and sleeps using two physical
HOTBED.MOV region clicks. The unskipped D3ND4M.MOV transition reaches day 4,
clock 1, phase 0 in HotRoom Scene A1 east, retaining both tbird and tstone.
Native evidence: `build/dust-third-night-native.log`; earned checkpoint:
`build/save-dust-day4-morning.dfsv`. Day-four combat and the remaining story
still need validation.


Dust's fourth-morning warnings and saloon conversations pass
(`tools/verify-dust-day4-morning.ps1`). Returning outside starts the Kid's
three-wave attack (`tools/verify-dust-kid-fight-entry.ps1`). All three waves
pass consolidated native input tests in `tools/verify-dust-kid-wave1.ps1`,
`tools/verify-dust-kid-wave2.ps1` and `tools/verify-dust-kid-wave3.ps1`, with
corresponding `build/dust-kid-wave*-complete-native.log` evidence. This covers
13 attackers, both rooftop death movies, reloading, offscreen wave transitions
and powder-keg explosions. The final-wave test uses the southern street for
long-range shots, then shoots barrel 3 to kill two nearby enemies. It survives
with 24 accumulated hits. A prior close-range route reached the authored
31-hit death branch; its trace is preserved separately. The driver now offers
read-only `waitactorpose` assertions for animation timing.

`tools/verify-dust-kid-duel.ps1` passes the post-wave confrontation, five winning
responses (including the earned scorpion-box distraction), three physical
KIDDIE.MOV region clicks and the unskipped D4AD4N.MOV transition.
`build/save-dust-day4-night.dfsv` is an earned checkpoint at Town Scene G5
south, day 4, clock 3, phase 0, with tbird/tstone and no player death.
The final temple puzzles, Bloodstone confrontation and day-five ending still
need validation; full playability is not yet achieved.


The fourth-night Sonoma warning, tower box and fountain entrance pass in
`verify-dust-temple-box-entry.ps1`, `verify-dust-tstone-box.ps1` and
`verify-dust-hub-entry.ps1`. The stone is selected through actual modal
inventory, dragged into the opened box, and its closing movies play normally.
The fountain then opens the final hub at Scene D7 north. Earned checkpoints:
`build/save-dust-tstone-box.dfsv`, `build/save-dust-hub-entry.dfsv`.

The box exposed missing flat-lifecycle containment. `openflat`/`closeflat` now
try the flat then the stage when missing or passed, while normally handled
events stop propagation. Hash-checked DF.EXE evidence is in
`build/dust-flat-lifecycle-original-evidence.log`. All three native stage audits
pass missing, passed and overriding-handler fixtures
(`build/dust-flat-lifecycle-chain-stage-audit.log`). The original box trace
shows its avatar hidden; reopening with the fix shows it and passes inventory
selection, placement and closing. The diagnostic modal driver now supports
selectors nested in stage handlers as well as puppets. Titanic's late notebook
and gas-pen selector regression passes with both physical modal clicks and
post-dialogue navigation (`build/flat-lifecycle-retail-notebook-native.log`);
speech was skipped only in this repeated regression.


`tools/verify-dust-sundial-entry.ps1` follows the hub's authored darkness
counter, hears the shaman and approaches the sundial.
`tools/verify-dust-flute-puzzle.ps1` passes its 0,0,8 alignment through a physical
small-dial drag, the east portal's darkness sequence, modal flute selection,
placement and the five note buttons. FLUTE.MOV and the return narration play
unskipped; `flutepuzzle=done` returns to Hub Scene D7 north, day 4, clock 3,
phase 0. Earned checkpoint: `build/save-dust-flute-puzzle-solved.dfsv`; native
evidence: `build/dust-flute-puzzle-solved-native.log`. The snake, mine and
Thunderbird puzzles, final confrontation and ending remain incomplete.


The snake chamber requires `shopwarm("puzzle")`. Shop-wide warming now resolves
the internal shop name and predecodes all states of active props/copies while
preserving placement, visibility and animation. All three native prop audits
pass, including removed-prop filtering, internal-name resolution, actual decoded
pixels and malformed-art rejection (`build/dust-shop-warm-prop-audit.log`).
The original dispatcher evidence is in `build/dust-warm-original-evidence.log`.

A post-flute navigation check exposed another v1 loader mismatch: newly opened
rooms retained the previous puzzle's hidden flag. Original DF.EXE 0x421b80
sets both open and visible before callbacks. Dust room opening now does so;
Titanic's behavior is unchanged. All three room audits pass, including visibility
observed inside openset. The flute route was replayed and the returned hub now
accepts darkness/navigation keys and sundial input
(`build/dust-set-visible-room-audit.log`, `build/dust-sundial-after-flute-native.log`).

The snake puzzle passes the real randomized arrangement.
`tools/dust-snake-route.mjs` reads the native seven-rotor dump and solves the
authored coupling modulo six, emitting only physical drag gestures and
assertions. `tools/verify-dust-snake-puzzle.ps1` aligns all seven rotors, plays
SNAKE.MOV unskipped, relinquishes the blade and returns to the hub with both
flute and snake completed. Earned checkpoint:
`build/save-dust-snake-puzzle-solved.dfsv`; evidence:
`build/dust-snake-puzzle-solved-native.log`. Its fresh-process restoration also
passes navigation back to the sundial (`build/dust-sundial-after-snake-native.log`).

The Thunderbird chamber passes the physical five-tumbler alignment
8,8,4,20,12, including inventory placement, TUMTRAN.MOV and the hub return
(`tools/verify-dust-tbird-puzzle-entry.ps1`, `tools/verify-dust-tbird-puzzle.ps1`).
Its restored checkpoint also passes navigation back to the sundial.

The repeating mine passes with the mask equipped before entry. The route planner
reads the earned standx/standy destination and emits normal movement keys through
the authored wrapping tunnels; it never modifies VM or save data. Giving the
mask to the skeleton plays SKELETON.MOV and returns to the hub with all four
puzzles completed and phase 1. Reproduction: `tools/verify-dust-mine-entry.ps1`,
`tools/verify-dust-mine-puzzle.ps1`; native evidence:
`build/dust-mine-puzzle-solved-native.log`; earned checkpoint:
`build/save-dust-mine-puzzle-solved.dfsv`.

`tools/verify-dust-shaman-chest.ps1` passes the shaman's final conversation and
the chest's physical sundial alignment 12,4,12. Clicking the revealed chest and
answering Bloodstone passes the authored transfer to the Thunderbird chamber
(`tools/verify-dust-blood-finale-entry.ps1`). The alternate physical tumbler
alignment 8,8,0,20,16 defeats him. BLOODIES.MOV and D4ND5M.MOV play unskipped,
awarding the chest and reaching day 5, clock 1, phase 0 at Town Scene G4 south.
Reproduction: `tools/verify-dust-blood-finale.ps1`; evidence:
`build/dust-blood-defeated-native.log`; earned checkpoint:
`build/save-dust-day5-morning.dfsv`.

All five day-five ending choices now pass in `tools/verify-dust-ending.ps1`:
Yunni (666), ranch (333), mayor (444), Marie (555), and desert (777). The first
Yunni attempt played the farewell conversations and ending movies unskipped;
its final view assertion was corrected after the authored finale changed the
camera. Subsequent repetitions skip speech, while every distinct ending movie
plays unskipped. Each branch reaches phase 99, saves/restores its credits state,
and quits through the authored physical click handler. `-FullCredits` additionally
passes all eight scrolling names panels and their loop back to panel 1.
Evidence: `build/dust-ending-*-native.log`,
`build/dust-ending-*-credits-native.log`; checkpoints:
`build/save-dust-ending-*-credits.dfsv`. This establishes a completion route
with all endings, rather than full coverage of every earlier story branch.

Normal launch now runs BOOTFILE and the authored session without a gameplay flag.
`dreamfactory_viewer=true` or `tools/run.ps1 <edition> -Viewer` selects the
developer room viewer; `-Gameplay` remains accepted by the helper for older
commands. `tools/verify-launch.ps1` checks ordinary startup without trace/input
flags, recognizing retail's original game/tour selection movie as player-ready.
All three editions pass (`build/launch-*.log`). The probe stops only its own
process at that interface; it does not test an authored quit.

ScummVM's remappable direction actions now emit DreamFactory's arrow keycodes,
with arrow and keypad defaults, retaining the existing mouse/menu/skip actions.
The trace-only `mappedkey` command passes raw keypad events through the actual
registered keymapper before dispatching them to the session. All three editions
pass mapped turning, with Dust additionally checking forward movement and the
down key (`build/keymap-*-native.log`). Physical controller hardware remains
untested.

Games added directly through the launcher can now use Liberation Sans from
ScummVM's standard `fonts.dat` when neither a configured font nor a loose face
is available. The bytes are retained for lazy FreeType glyph loading. Dust's
real inventory text passes with only that archive available and no explicit
font setting (`build/font-archive-dust-native.log`). The development build also
places the loose licensed font and its license beside the executable. Keep them
there when launching the executable directly, or set ScummVM's extra path to
`build/runtime-data` when launching from another working directory.

The first winning Titanic fencing bout also passes five earned points, the
post-bout conversation and fresh restoration of `willie=won`, `fencewins=1`,
`fencecount=1` in SqHall. `tools/verify-full-fencing-win.ps1` reads earned
snapshots and chooses only physical attack quadrants; 20 attacks completed the
verified bout. Evidence: `build/full-fencing-win-suite.log`,
`build/full-fencing-win-attack*-native.log`,
`build/full-fencing-won-restore-native.log`; checkpoint:
`build/save-full-fencing-won.dfsv`. The two-win direct ring award and complete
defensive-parry coverage remain separate validation targets.

Late native ScummVM slots additionally pass ordinary startup loading: Dust's
day-five chest (slot 93), Titanic's four-evidence best-ending preparation
(slot 94), and the demo's developed photograph in B59 (slot 95). The demo
photograph route was replayed through the actual two-bath puzzle before saving.
Evidence: `build/save-dust-day5-startup-native.log`,
`build/save-full-best-startup-native.log`,
`build/save-demo-photograph-startup-native.log`.

Native ScummVM note and yes/no dialogs now implement `notedialog` and
`questiondialog`. English Windows script text is decoded as Windows-1252;
game timers are frozen during the overlay and the opening gesture is cleared
before returning to game input. `tools/verify-script-dialogs.ps1` passes actual
Quit controls, No/cancel followed by navigation, Yes/No exit, and Yes/Yes
save-before-quit followed by fresh restoration and navigation in all three
editions. The demo's in-game Quit returns to its authored menu; its menu Quit
plays CLOSE.MOV, whose three click regions are advanced physically before the
script quits. Evidence: `build/script-dialog-suite.log`,
`build/dialog-*-native.log`. Debug-only `textdialog` remains unsupported.

Titanic's tour selection also passes through the real PLAYMODE.MOV region,
unskipped OPENTOUR.MOV and arrival in C73. Its disabled save/load controls show
native note dialogs; acknowledging both and canceling the tour Quit prompt
returns to navigable gameplay. Evidence: `build/full-tour-entry-native.log`,
`build/dialog-full-tour-notes-native.log`; checkpoint:
`build/save-full-tour-entry.dfsv`. This does not establish full tour coverage.

Dust's optional checkers requires `pluginfx("checkmove", ...)`. The native
portable replacement implements compulsory captures, full jump chains,
mid-chain promotion with backward king jumps, player move alternatives and
alpha-beta search. Rules/search adapt the pinned reference; bit-identical
behavior of the original Windows DLL is not claimed. Native tests pass
handcrafted rule positions and 144 independent reference positions
(`build/checkers-oracle-script-audit.log`). `tools/verify-dust-checkers-entry.ps1`
enters Bolivar's store and chooses a difficulty through normal dialogue.
The first physical player drag passes a legal opponent reply and returns the
turn (`build/dust-checkers-move1-native.log`). Complete native matches now pass
an Easy win in 58 turns and a Hard loss in 20 turns, including captures,
promotion, the losing voice wait, delayed board close, postgame dialogue and
fresh-process return to navigable store gameplay. Reproduction:
`tools/verify-dust-checkers-game.ps1`, using read-only snapshots to plan physical
drags. Evidence: `build/dust-checkers-complete-suite.log`,
`build/dust-checkers-lost-suite.log`, `build/dust-checkers-won-return-native.log`,
`build/dust-checkers-lost-return-native.log`. The postgame script consumes the
score flag; assertions check its value before that conversation and its absence
afterward. An earlier planner repeated a king endgame for 100 turns; its legal
saved state was subsequently continued to a win, and the finalized planner
prefers pursuit among equally rated moves.

`tools/verify-dust-checkers-controls.ps1` additionally passes the thinking
portrait, an actual ScummVM slot-99 rollback after a physical move, and voluntary
exit followed by dialogue/navigation. The board string is compared directly
between native checkpoints, preserving all cells and its authored trailing
space (`build/dust-checkers-slot-rollback-native.log`,
`build/dust-checkers-controls-native.log`). Medium difficulty and a Hard win
remain outside this match coverage; the native rules/search oracle covers
depths 1–5 independently.

The platform query now returns the supplied assets' `win` target, and the
Command-key query reads ScummVM's Meta modifier. Titanic's bedsit, ship and tour
Help controls pass their Windows movies, topic jumps, close clicks and return
to navigation (`build/help-full-*-native.log`). A normal Dust door-close click
also passes the formerly unsupported modifier query
(`build/modifier-dust-door-close-native.log`). The final native script audits,
including the 144-position checkers oracle, pass all three editions in
`build/final-host-queries-script-audit.log`.

Dust poker now passes View Cards and its nested release/press wait, folding,
another ante/deal, three physical discards, a naturally winning queen pair,
payout, declining another hand and leaving the table. The player retains the
winnings and cards and can navigate afterward. Every hand transition is saved
and restored in a fresh process; no cash, card or inventory state is injected.
`tools/verify-dust-poker-winning.ps1` replays the sequence from the earned
six-dollar table checkpoint. Evidence: `build/dust-poker-winning-suite.log`,
`build/dust-poker-repeat-won-native.log`, and
`build/dust-poker-won-return-native.log`. The test event source now accepts an
optional handler gate on modal clicks (`flat:type:name:handler`) and observes
button queries to release before pressing during nested script waits.

F5 now opens ScummVM's game menu through the engine keymap. Inherited mouse,
skip, controller and direction bindings are preserved. Actual menu opening,
Escape cancellation, frozen game time and subsequent mapped keypad navigation
pass in all three editions (`tools/verify-game-menu.ps1`,
`build/game-menu-suite.log`, `build/menu-*-native.log`). The menu spends at least
300 ms open while the authored clock advances only by the normal resumed frame.
Native script audits and the checkers oracle also pass after these changes
(`build/menu-poker-script-audit.log`).

Titanic's direct ring route now passes two earned fencing victories. The second
bout takes 30 physical attacks/waits, then completes both mandatory Haderlitz
conversation branches, awards the ring, advances mission 2 to phase 2, and
returns to SqHall. Fresh restoration checks `fencewins=2`, `fencecount=2`,
`willie=won`, `ring=frank`, the phase and subsequent navigation. Evidence:
`build/full-fencing-second-attack*-native.log`,
`build/full-fencing-second-award-native.log`, and
`build/full-fencing-second-ring-restore-native.log`. The first attempt reached
the award but the test supplied only the first-bout dialogue choice; its
waiting conversation trace is preserved separately. The driver now includes
the second-bout branches and uses separate trace names so the first win's
evidence is retained. Reproduction: `tools/verify-full-fencing-ring.ps1`.
Complete defensive-parry coverage remains a separate validation target.

`tools/verify-full-fencing-parries.ps1` now passes actual right and left
defense against Haderlitz's scheduled attack tells. Each tell is saved and
restored in a fresh process before moving the pointer to defend. Successful
parries retreat one flat, keep the bout active and leave the opponent's score
hidden; defending the opposite side of the same saved attack awards him a
point and recenters the bout. A fresh post-parry restore also passes. Evidence:
`build/full-fencing-parries-suite.log` and
`build/full-fencing-parry-*-native.log`. The diagnostic `waitpropviewany` waits
for one of several authored views without changing runtime state.

`tools/host-call-inventory.mjs` inventories host opcode occurrences across all
supplied script containers, including installed and CD variants. It writes
`build/host-call-inventory.json`, preserving file/container/token locations for
review. IDs absent from Session dispatch are developer script-inspection and
report commands, textdialog, and retail's setparam/stageparam cache controls.
The latter occur only behind `lowmemory()` in the supplied scripts; the current
heap-size response selects the normal-memory branch. This is static triage,
not proof of host semantics or gameplay reachability: finding an ID in C++
does not establish that every use is implemented correctly.

The remaining smokestack layouts 2 and 4 now pass native navigation through
every planned barrier and stair transition to floor eleven, plus fresh
top-floor restoration and turning. They were naturally drawn by leaving and
re-entering from mission 3 / phase 2, without altering the maze global or RNG.
Repeated entrance/exit movies were skipped after prior unskipped coverage.
Layout 1 was also replayed and retained separately; layout 3's earlier native
trace remains available. All four layouts therefore have native traversal
evidence. Reproduction: `tools/verify-full-smokestack-layouts.ps1` (defaults to
layouts 2 and 4; `-Layouts 1,2,3,4` checks all). Evidence:
`build/full-smokestack-layouts-suite.log`,
`build/full-smokestack-maze1-suite.log`,
`build/full-smokestack-maze{1,2,4}-native.log`, and the corresponding
`-restore-native.log` files; layout 3 is in
`build/full-smokestack-maze-native.log`.

Real save/load chooser testing exposed a filename mismatch: Engine wrote
`target.001`, while MetaEngine's missing `kSimpleSavesNames` feature made
listing and metadata lookup search for `target.s01`. Direct slot/startup tests
had bypassed that discovery path. The engine now advertises the matching
three-digit naming scheme. Existing native saves remain readable and are now
discoverable through ScummVM's UI; no save-format change was required.

`tools/verify-save-chooser.ps1` passes both authored control panels and F5's
Save/Load actions in retail, demo and Dust: Save/Load cancellation, typed names,
slot selection, metadata listing, load rollback and fresh-process restoration.
`tools/verify-save-grid.ps1` additionally passes the default thumbnail grid,
its separate name dialog's Cancel button, New Save and thumbnail loading in
all three editions. Escape in that text field restores its prior text; the
test clicks Cancel to leave it. Each run keeps its saves in a unique directory
under `build/`, leaving existing slots intact. Evidence:
`build/save-chooser-with-menu-suite.log`, `build/save-grid-suite.log`,
`build/chooser-*-native.log` and `build/grid-chooser-*-native.log`.

The trace helper accepts `-GuiKeys`, `-Chooser list|grid` and `-SavePath` for
these native UI checks. Diagnostic GUI inputs are actual keyboard/mouse events;
`click:Dialog.Widget` reads theme geometry before clicking its center, and
`gridfirst` clicks the first thumbnail area. `expectsave SLOT DESCRIPTION`
checks both MetaEngine's metadata query and its listed saves. These hooks are
available only in boot tracing; normal Save/Load uses ScummVM's chooser.

Dust checkers now also passes Medium and Hard wins, their distinct Bolivar
speeches, consumed score flags, return to the store and fresh-process
restoration of the post-match saves. Medium completes in 71 turns; Hard in 139.
The Hard match continued legally while the external test player's equal-score
tie-break was improved: it now crowns a stranded man and uses squared pursuit
distances to bring distant kings into the chase. The game rules and Bolivar's
native search were unchanged. Every turn remains a physical drag from an earned
checkpoint, with captures and replies checked before continuing. Evidence:
`build/dust-checkers-medium-winning-suite.log`,
`build/dust-checkers-hard-winning-suite.log`, their per-turn native logs, and
`build/dust-checkers-{medium,hard}-won-return-restore-native.log`.
`tools/verify-dust-checkers-game.ps1 -ExpectedScore winmedium|winhard` supports
these outcomes; use `-ResultPrefix` to preserve separate terminal/return saves.
Together with the earlier Easy win and Hard loss, all difficulty-specific
winning conversations and the losing path now have native coverage.

Retail Titanic's supplied tour guides now pass their complete historical
sequences: Smethells in C73 (`tour9.mov`), Burns in the smoking room
(`tour2.mov`) and Trask in the Turkish bath (`tour8.mov`). These are the three
tour movies present in this package; absent optional guide movies remain
governed by the scripts' file-existence checks. Smethells' opening photograph
needs a physical click, after which its narration and images advance normally.
All narration and guide speech played unskipped. The tour map, ordinary room
walks/turns and doors connect the three guides, and the final checkpoint
restores to navigable Turkish-bath gameplay with `tour=1`. Reproduction:
`tools/verify-full-tour-guides.ps1`, with `-StartAt burns|trask` for existing
milestones. Evidence: `build/full-tour-{smeth,burns,trask}-native.log` and
`build/full-tour-guides-restore-native.log`.

`tools/verify-settings-controls.ps1` now passes native settings interactions
and fresh-process persistence across all three editions. Dust's score-panel
slider selects the authored mute/mid/max levels (0/5/9), its subtitle checkbox
toggles off/on, and a save made with the score panel open restores both values
and resumes normal town navigation after closing it. Both Titanic editions
pass rotary volume changes from 9 to 1 and back, restoration of the temporary
zero frame rate to 3 after each drag, subtitle toggles, and a custom east-turn
key. The custom `d` binding survives saving and actually turns the restored
room before being reset through the control panel. These checks use physical
mouse/keyboard events and a read-only `expectsetting` diagnostic observer.
Evidence: `build/settings-controls-complete-suite.log`,
`build/dust-settings-{panel,restore}-native.log`, and
`build/{full,demo}-volume-dial{-restore,}-native.log`.

Death and recovery now pass in both full games. In Dust, approaching the active
gang without shooting back reaches the authored death movie and narration;
New Game resets to the first-night starting point with fresh money, ammunition,
inventory and combat state. Its death-screen Load chooser can cancel and then
recover an earned day-five save. Titanic's automatic Vlad confrontation while
carrying the Rubaiyat reaches its recovery menu; New Game closes the old
resources, plays datebed.mov and resets to the bedsit. The death-screen chooser
also recovers the four-evidence sinking save across CD resource paths. Restarted
and recovered adventures pass fresh-process restoration and navigation.
Reproduction: `tools/verify-death-recovery.ps1`; `-CreateDeaths` also regenerates
the deaths through physical routes from earned checkpoints. Evidence:
`build/death-recovery-suite.log`, `build/dust-kid-death-native.log`,
`build/full-vlad-death-native.log`, and `build/{dust,full}-death-*-native.log`.

Demo replay exposed a shared room lifecycle defect: leaving the control panel
hid the old room, and opening the next v4 room retained that hidden state.
The quest reset but the cabin could not be seen or navigated. Every original
loader initializes the arriving room's visibility before its callbacks. This
is now implemented for both v4 editions as well as Dust. The original writes
through the room structure pointer, so searching only for direct writes to the
visibility global misses the initialization. Hash-checked, read-only evidence:
`tools/inspect-titanic-set-visibility.py`, retail TI.EXE 0x4077e5 and demo
TI.EXE 0x4083c9; dump: `build/titanic-set-visibility-native-evidence.log`.

The room audits pass all three editions with new regression coverage for
opening over a hidden room, reopening after close, visibility observed by the
opening callback, and a callback intentionally hiding its new room
(`build/room-visibility-audit-suite.log`). Save loading still restores the
serialized visibility after opening its detached room. The demo now replays
from earned late progress through the unskipped date/flyby opening, clears old
quest items, permits cabin navigation and saves/restores correctly
(`tools/verify-demo-replay.ps1`, `build/demo-replay-fixed-suite.log`).

The supplied demo's other menu movies also pass unskipped: the 20-segment tour
and 13-segment trailer, each including the authored final image click, porthole
close animation, menu return and fresh restoration. Reproduction:
`tools/verify-demo-menu-media.ps1`; evidence: `build/demo-tour-menu-native.log`,
`build/demo-trailer-menu-suite.log` and their `-restore-native.log` files.
The diagnostic runner now uses a per-edition mutex to reject overlapping runs
before they can share a config/log; an actual second invocation was rejected
while the trailer continued with its config unchanged. An earlier accidental
overlap was discarded, and the trailer/replay checks were rerun sequentially.

### Demo photograph mistakes and recovery

Native checks now cover putting a negative directly in the second bath,
overdeveloping it in the first, collecting spoiled photos and retaining their
quality/degree flags through fresh loads. Loading an earned pre-bath save
permits a good retry. A save made with the negative still in the first bath
resumes its countdown, completes both baths and retains the good photograph
after another load. All state changes use original scripts and physical input.
Reproduce with `tools/verify-demo-photo-retry.ps1`; evidence:
`build/demo-photo-retry-suite.log` and `build/demo-photo-*-native.log`.

Penny's original handler accepts a spoiled Conkling photograph with different
dialogue. The full route passes this branch, POSTDEMO playback, phase 2/menu
return and fresh restoration of the completed menu. Reproduce with
`tools/verify-demo-completion.ps1 -Spoiled`; the default retains the good-photo
route. Evidence: `build/demo-completion-spoiled-native.log` and
`build/demo-completion-spoiled-restore-native.log`. The ending closes the
inventory shop, so item assertions apply before completion. Two incorrect menu
expectations were discarded and preserved in `build/demo-ending-*-expectation-native.log`.
No production engine change was needed for these branches; broader playability
and fidelity work remains active.

### Positional ambience follows movement

Read-only disassembly recovered the supplied Titanic executables' integer
distance, linear 255-step volume and folded bearing pan. Retail service/pan:
TI.EXE 0x444430/0x444790; demo: 0x444cf0/0x444f90. Reproduction and evidence:
`tools/inspect-cricket-spatial.py`, `build/cricket-spatial-native-evidence.log`.

This exposed an engine defect: a running ambient loop retained its starting
volume and pan when the player moved. The service now updates the existing
mixer handle, stops it beyond its audible radius and permits reentry. Sound
play/slot descriptors stay synchronized for save/load and wave-volume changes.
Audio audits pass all three editions, with Titanic mixer regressions for live
movement/turning, integer volume, diagonal pan, radius exit and reentry
(`build/ambient-position-audit-suite.log`). Dust retains its separate projectile
implementation for these opcode numbers.

The authored turbine-room motor also passes native gameplay checks: volume 148
with pan -54 then 74 after turning, fresh restoration of the running sound,
walking closer to volume 187/pan 24 and silence outside its radius. Reproduce
with `tools/verify-full-ambient-position.ps1`; evidence:
`build/full-ambient-position-suite.log` and its two `-native.log` files.
The `dumpcrickets` and `expectambient` trace commands only observe mixer state.

### Ambient timer arming and save compatibility

The originals distinguish timed repeats from looped PCM. Timed sounds arm on
entering audible range, freeze outside their room/radius, and take a fresh delay
on return. Looping samples start immediately. A timed repeat can fire while its
previous sample is still playing; every overlapping instance follows the same
position and stops on room/radius exit. The engine now implements these rules.
Native evidence includes the loop-flag getter (retail 0x425730), which must not
be mistaken for a playback-active query, and the range/arming branches in
`tools/inspect-cricket-spatial.py`.

DFSV version 3 stores each cricket's armed flag and retains version-1/version-2
loading. Older saves retain their already-started countdown until leaving range.
The read-only globals reader and projectile corruption-fixture tool accept v3.
An invalid armed flag exposed a partially decoded-table cleanup defect; rejected
saves now safely discard null entries and preserve the live game's timer/state.
The original failed run is preserved in `build/full-invalid-armed-hang-native.log`.

Native audio audits pass all editions, including range arming, off-room freeze,
fresh delay on return, timed repeat during a sample, overlapping movement and
cleanup (`build/ambient-timing-audit-suite.log`). The permanent real-ship test
`tools/verify-full-ambient-timing.ps1` passes exact saved countdown restoration,
unarmed-state persistence, range reentry and checksummed invalid-flag rejection
(`build/full-ambient-timing-suite.log`). Genuine version-1 demo and version-2
retail saves load and convert to v3; the upgraded demo loads in a fresh process.
Dust's naturally launched projectile still saves, expires, loads in flight and
expires again (`build/ambient-v3-dust-projectile-suite.log`). Live turbine-room
spatial audio also passes with v3 (`build/full-ambient-position-v3-suite.log`).
The existing all-edition save suite also passes startup round trips, fresh
loads, ScummVM slots, authored save/load controls, nine invalid checkpoints and
cross-edition rejection (`build/ambient-v3-save-state-suite.log`). The original
v1 demo fixture is preserved as `build/save-demo-legacy-v1.dfsv`.

Retail's original two-loop priority arbitration is implemented in the following
milestone. Its native selection routine at 0x4446d0 is included in the evidence tool.

### Retail ambient-loop priority and shared sound channels

Retail now selects the two loudest eligible loops before wave-master scaling,
keeps earlier stored entries on equal volumes and stops unselected descriptors.
The selected loops use the same original bank-local sound-ID arbitration as
ordinary effects. Timed retail repeats also use those physical slots, replacing
their earlier same-sound instance. Selected running loops retain their handles;
older experimental positional voices are rebound into the physical channels.
The save format remains v3.

Audio fixtures now explicitly select retail mode; their small synthetic
BOOTFILE cannot trigger real retail detection. Four generated sound records
cover selection among three loops, movement promotion, ties, missing resources,
handle reuse, shared-descriptor stopping, unrelated effect preemption, resumption
and ranking while wave volume is zero. Native audio audits pass all editions
(`build/ambient-priority-audit-suite.log`, including `CRICKET PRIORITY PASS`).

The earned sinking-deck checkpoint has three audible crowd loops at Scene45:
raw volumes 139, 67 and 39. Only the first two play. Walking to Scene49 places
four loops in range; party3/party4 become the selected pair at volume/pan
158/-30 and 109/-74. Both selections survive fresh loads and navigation.
Reproduce with `tools/verify-full-crowd-priority.ps1`; evidence:
`build/full-crowd-priority-suite.log` and its three native logs. The prior
turbine spatial and timer/save/rejection helpers also pass under this channel
implementation (`build/full-ambient-{position,timing}-priority-suite.log`).

An initial boiler-room probe was invalid: its five fire-loop definitions occur
after an unconditional `exitcode` in BOOTFILE `setupsound`, so they are never
created by the supplied game. That failing test was removed without changing
the original script. Evidence: `build/full-boiler-dead-ambience-expectation-native.log`.
The multi-loop gameplay checks use reachable crowd scripts instead. Broader
playability/fidelity validation remains active.

### Native cricket slot order and saved gaps

Both supplied Titanic executables allocate a new/replaced cricket in the first
inactive slot and enumerate active entries in table order (retail make
0x4443e0/count-index 0x444800, demo make 0x444cb0/count-index 0x445000).
The previous compact list appended replacements, changing enumeration and
equal-volume priority. The engine now preserves interior gaps, fills the first
one, and skips inactive entries in counts, enumeration, scheduling and tracing.
Trailing empty slots carry no ordering information and can be trimmed.

The existing v3 record array preserves those empty slots without another format
change. A native synthetic save/load check retains an interior gap, fills it
after restoration and verifies that the new equal-volume loop takes its native
priority. Compiled API checks also cover replacement order and active
enumeration across a gap. Audio audits pass all three editions, including
`CRICKET SLOT PASS` in `build/ambient-slot-full-native.log`.
The tiny stage used for this save check has valid generated artwork; the first
attempt without artwork correctly failed saved-picture validation.

Real sinking-deck crowd selection/fresh restoration and turbine timer/save/
invalid-flag rejection still pass (`build/full-crowd-priority-slot-suite.log`,
`build/full-ambient-timing-slot-suite.log`). Hash-checked original evidence is
included in `tools/inspect-cricket-spatial.py` and
`build/cricket-spatial-native-evidence.log`.

### Dust third-night poker Thunderbird route

The alternate third-night reward now passes native gameplay: accept Mezamee's
offer after reconciling Jones/Laurel, win the first hand, receive the Thunderbird,
decline another hand and return to the saloon. The item starts unowned (`none`)
and ends owned by the player (`stranger`); phase/mezphase advance to 2. The
cheating-card item remains unused, and reward ownership and navigation survive
a fresh load. Two natural hands passed: two pairs scoring 313 and three twos
scoring 402. Both scores were checked against the game's own scoring result.

Complete reproduction from the earned reconciliation checkpoint:
`tools/verify-dust-night3-poker-win.ps1 -PrepareStake`.
`build/dust-night3-poker-complete-route-suite.log` records a passing combined run
through normal blackjack funding, the offer, betting/discards, reward and fresh
restoration. `tools/verify-dust-night3-poker-stake.ps1` earns the stake through
eleven legal blackjack hands using visible totals for Hit/Stand, retaining
day=3/clock=3/phase=1. The betting sequence reached $22. The cash driver now
supports separate result prefixes, explicit bet sizes/sequences, fixed hand
counts and optional speech skipping; its default $1/target behavior is retained.

`tools/find-dust-night3-poker-hand.ps1` tries ordinary waits before accepting the
offer. `tools/dust-poker-plan.py` is a read-only test oracle for the original
score/draw rules; it changes no game state. The chosen cards are discarded by
physical clicks. Different timing produced successful waits of 20 and 60 frames,
so the verifier searches its bounded wait list instead of assuming a fixed
deal. Repeated introductions/cash play can skip speech; the betting and winner
dialogue in the winning route complete unskipped.

The initial $4 poker attempt correctly exited for insufficient cash. The $8
and $15 betting attempts actually reached discards: I initially misclassified
their diagnostic failures, which came from two unused queued choices. The queue
was corrected to its one required call and rerun successfully. Those discarded
results are labelled `build/dust-night3-poker-*-unused-choices-native.log`.
The slow $1 cash run was stopped after 15 completed hands and is not counted as
a funding pass. No production engine change was needed for this story branch.

### September 1996 demo sound channels and bank priorities

The supplied interactive demo now uses its original two physical SFX channels.
Its dual command aliases single; both unconditionally replaces only the lower
priority channel (channel 2 on a tie). Single and multiple compare a composite
ID: the open-bank ordinal in the upper word and the one-based sound record in
the lower word. Opening a bank renumbers descriptors; closing one preserves
priority gaps. Active channels retain their copied IDs when banks are renumbered.
Hash-checked original evidence is reproducible with
`python tools/inspect-sound-slots.py`.

Demo looping ambience starts on range entry and arms a 0x7fffffff countdown.
A preempted or stopped loop waits for range reentry instead of restarting each
service pass. Timed effects and ambience share the two script-effect channels.
Retail keeps its separate loudest-two ambience selection.

DFSV v4 stores bank priority bases, including gaps, and accepts v1/v2/v3 saves.
Legacy demo priorities are reconstructed from their stored bank order, since
older formats did not record gaps. Native audio checks pass all three editions,
including demo command arbitration, range rearming, priority rejection, gap
save/load, open renumbering and active channel ID copies
(`build/demo-sound-audit-suite.log`). All-edition fresh-process, ScummVM-slot,
authored control-panel and invalid/cross-edition save checks pass
(`build/demo-sound-save-state-suite.log`), as do retail timer preservation and
invalid armed-flag rejection (`build/demo-sound-retail-timer-suite.log`).
The full demo replay also passes photograph development, delivery, POSTDEMO
and fresh-process ending restoration (`build/demo-sound-completion-suite.log`).
A preserved v1 demo save upgrades to v4 and remains navigable after a fresh
load (`build/demo-sound-legacy-v1-upgrade-native.log` and
`build/demo-sound-legacy-v4-restore-native.log`).

### Dust original effect arbitration

Dust now uses its original two physical SFX channels instead of accumulating
overlapping mixer voices. Original 402b90/402c80 compare descriptor priorities;
402d70 aliases single, and 402d80 unconditionally replaces the lower-priority
channel. Like the September demo, Dust combines the open-bank ordinal with the
one-based local sound ID. Open (403185) renumbers bank descriptors; close
(4033a0) preserves priority gaps; active channels retain copied IDs. The
hash-checked evidence is in `build/dust-sound-original-evidence.log`, reproduced
by `python tools/inspect-sound-slots.py` without executing the original games.

DFSV remains v4. Older Dust saves with all-zero bank bases reconstruct their
stored bank order and retain their two recorded SFX channels and speech.
Extra bank effects from the earlier unbounded approximation are discarded.
Active copied priorities distinguish a valid lone zero-base bank from an older
save, so loading does not erase those IDs. The synthetic checks use actual v1
Dust sound banks and cover gaps, bank renumbering, copied IDs, a lone zero-base
bank and 100 successive replacements. Native audio tests pass all editions
(`build/dust-sound-audit-suite.log`); all-edition save/control-panel/rejection
checks pass (`build/dust-sound-save-state-suite.log`). A preserved pre-change
v4 Dust save upgrades and fresh-load navigation passes
(`build/dust-sound-legacy-upgrade-native.log`,
`build/dust-sound-legacy-fresh-native.log`).

Dust's final gang wave passes with all enemies defeated, one bullet remaining
and fightphase 3 (`build/dust-sound-wave3-native.log`). The Yunni ending,
ending movie, fresh credits restoration and authored quit also pass
(`build/dust-sound-ending-suite.log`).

Review found an incomplete-bank rejection path that could enter migration with
fewer loaded banks than the saved count. Loading now returns before migration
when bank decoding fails. `tools/verify-dust-sound-save.ps1` verifies missing
first/second banks and malformed priority bases, then physically navigates the
unchanged live game (`build/dust-sound-rejection-suite.log`). Its checksummed
corruptions are negative fixtures only. Movie cue ownership and bank control
lifetime still need fidelity review; the full-playability goal remains active.

### Sound controls belong to their open bank

Loop, volume and pan settings now belong to each open bank's sound descriptors.
Closing and reopening a bank creates fresh defaults (loop off, volume 255,
pan 128). Different banks can contain the same sound name without inheriting
one another's settings. Original constructors establish these defaults at
retail 424b3f/424b46/424b4c, demo 425cbd/425cdc/425ce2 and Dust
402617/402639/40263f. `tools/inspect-sound-slots.py` reproduces the hash-checked
evidence (`build/bank-controls-original-evidence.log`).

DFSV v5 preserves bank-local controls and the volume/pan copied into active
effects. Versions 1-4 remain readable: their name-global settings migrate to
the first matching bank, and other banks retain defaults because old saves
did not record separate settings. Older effect playback levels are reconstructed
using those legacy controls. Inline clips also retain their copied levels when
the bank supplying the same sound name closes or a different bank becomes visible.

Native synthetic checks pass all three editions, covering hidden duplicate
names, bank reopen defaults, active effect volume/pan restoration and playback
snapshots (`build/bank-controls-audit-suite.log`). All-edition opening,
fresh-process, ScummVM-slot, authored save-panel and invalid/cross-edition
rejection tests pass (`build/bank-controls-save-state-suite.log`). Preserved
demo v1 and Dust v4 checkpoints upgrade to v5 and remain navigable after a fresh
load (`build/bank-controls-legacy-*-native.log`). Movie/audio integration checks
also pass all editions (`build/bank-controls-movie-suite.log`).

The negative bank-save suite now also rejects unknown sound-control keys and
oversized control tables while preserving live navigation
(`build/bank-controls-dust-rejection-suite.log`). Retail's exact ambience timer
restoration, range rearming and malformed armed-flag rejection pass
(`build/bank-controls-retail-timer-suite.log`).
Real turbine-room movement, turning, fresh saved position and radius exit also
pass (`build/bank-controls-retail-position-suite.log`). Live control propagation,
repeated opens of the same bank and movie cue ownership still need original-code
review; full playability remains an active goal.

### Live looping and edition-specific pan behavior

The original loop setters update active channels 0-2, including sounds played
with `voicesound`. Disabling looping removes the repeat link rather than
stopping the current PCM sample. The engine now supports enabling a loop during
playback and draining the current iteration when looping is disabled. Mutable
stream flags are protected from the mixer thread with a mutex. Bank voice
playback also uses its descriptor's loop, volume and pan controls; independent
puppet voice clips retain their own settings.

Titanic retail/demo pan setters update active playback. Dust's setter records
the value but has no audible effect: its PCM setup copies only the first 0x27
descriptor bytes, excluding pan at +0x31, and its volume calculation keeps
equal left/right levels. An initial interpretation that Dust applied pan on
subsequent playback was rejected after tracing that PCM setup. Dust now remains
centered while the getter and saved bank setting preserve the recorded value.
`python tools/inspect-live-sound-controls.py` reproduces the hash-checked
original evidence (`build/live-controls-original-evidence.log`).

DFSV stays v5. When a formerly looping sample is draining, its saved position
uses elapsed time modulo the sample length, then restores as a nonlooping tail.
Without this rebasing, saves after a full iteration could incorrectly restore
an already-finished sample. Native tests enable looping mid-sample on both an
effect and a bank voice, run beyond one iteration, disable looping, save/load
the draining tails, and verify that both subsequently finish. A deterministic
PCM test verifies the exact remaining sample sequence. All three editions pass
these checks (`build/live-controls-audit-suite.log`).

All-edition movie integration and save-panel/fresh-process/rejection suites
also pass (`build/live-controls-movie-suite.log`,
`build/live-controls-save-state-suite.log`). Retail's actual motor ambience
movement/turn/fresh position/range exit passes
(`build/live-controls-retail-position-suite.log`). Dust's physical shooting
range, reload and Leroy matchbox reward pass
(`build/live-controls-dust-range-native.log`), as do its death-screen restart,
cancelled Load, late-game recovery and fresh restores
(`build/live-controls-dust-death-suite.log`). Repeated opens of the same bank
and movie bed/theme sharing remain under review; movie cue ownership is corrected
below. Full playability remains active.

### Movie cues use the shared voice channel

Movie cues now replace the current voice while preserving both effect channels.
The original routines select voice channel 0 at retail 44ad62, demo 44b40b and
Dust 419551/4195d8. Segment/movie cleanup unconditionally stops that channel
(retail 449323, demo 449b41, Dust 419362), including natural completion.
The earlier retained cue-tail policy and effect-channel routing were incorrect.
`python tools/inspect-movie-audio.py` reproduces the hash-checked original
evidence (`build/movie-voice-original-evidence.log`).

Voice-wait frames observe the shared voice channel, including prior bank/puppet
speech. Missing cues leave the voice and pending sound-follow transition alone;
they do not fall back to an unrelated open sound bank. Retail honors cue loop
flags for both preloaded and lazy cues; the September demo does not implement
that flag. Synthetic fixtures now select the edition explicitly and verify it
in their result. Earlier loop-flag assumptions are preserved only as discarded
`build/movie-voice-unverified-loopflag-*-native.log` evidence.

Native movie checks pass all three editions: two running effects survive cues,
the prior voice is replaced, absent cues preserve voice/follow state, global
voice waits work, loop flags follow the edition, and long cues stop at segment
and movie exits (`build/movie-voice-audit-suite.log`). Audio checks also pass
all editions (`build/movie-voice-audio-suite.log`). DFSV stays v5; old saved
policy flags no longer override the running game's edition rules. Synthetic
compatibility fixtures verify this, and all-edition save-panel/fresh-process/
invalid/cross-edition checks pass (`build/movie-voice-save-state-suite.log`).
Six malformed bank/control saves still preserve live navigation
(`build/movie-voice-dust-rejection-suite.log`).

Actual data replays pass retail's best ending through LEAVE, DEBRIS, PROZAC,
all 14 credits clicks and script quit (`build/movie-voice-retail-ending-suite.log`),
Dust's Yunni ending/movie/fresh credits/quit
(`build/movie-voice-dust-ending-suite.log`), and the developed-photo demo ending,
POSTDEMO and fresh ending restore (`build/movie-voice-demo-completion-suite.log`).
The demo's complete tour and trailer also pass final clicks, menu returns and
fresh restores (`build/movie-voice-demo-media-suite.log`); their supplied cue
records have no loop flags, so the later edition-loop correction does not change
those replays. Movie bed/theme-channel sharing and repeated opens of the same
bank remain under review. Full playability remains an active goal.

### PCM ownership and the original movie-theme queue

PCM streams now borrow immutable clips and loop controls from their main-thread
owners. Each caller retains them until its mixer handle has stopped. This
removes `Common::SharedPtr` reference-count changes from mixer-thread stream
destruction; ScummVM's shared-pointer counters are not atomic. The raw-pointer
stream API makes the lifetime contract explicit. Effect/voice plays, bank
themes, movie beds and restored audio all retain their corresponding owners.
Loop-flag access remains protected by its mutex.

Native stress checks release each caller's short-lived clip immediately after
starting playback, alternating natural completion and replacement across 256
clips per edition. Audio suites pass all three editions
(`build/pcm-ownership-audit-suite.log`), including existing live-loop, pan,
draining-save and bank-control checks. Movie integration and all-edition
save-panel/fresh-process/invalid/cross-edition suites pass as well
(`build/pcm-ownership-movie-suite.log`, `build/pcm-ownership-save-suite.log`).

Original-code investigation established that movie beds and bank themes share
physical channel 3. Theme submission appends buffers when that channel is
already active (Dust 41d040). Each movie initializes its previous-bed count to
minus one (retail 448b65, Dust 418c73), so its first bed does not clear a game
theme before submission. Later segments without new bed records inherit the
existing movie bed; later segments with new records clear it before loading.
Final cleanup stops channel 3 only when a movie bed exists.
`python tools/inspect-movie-theme.py` reproduces the hash-checked evidence
(`build/movie-theme-original-evidence.log`).

The engine still uses a separate movie bed mixer and immediate bank-theme
replacement. The required correction is a shared queued theme stream with
pending-buffer save restoration; simply halting game music at movie start
would contradict the original. That integration, along with repeated bank
opens, remains unfinished. The full-playability goal remains active.

### Authored movie soundtrack order

Dust movie beds now follow the header's explicit soundtrack directory, rather
than guessing from unreferenced audio. The original loader (418e0c..418f21)
loads the bed after the preloaded one-shots, then follows the order at header
offset 2110. This preserves rearrangements and repeated chunks: 24 supplied
Dust segments repeat chunks, and INTRO deliberately rearranges its music.
Interactive v4 movies also retain every ordered occurrence; the previous
deduplication shortened their authored sequences.

Native movie suites pass the demo's 22 movies, retail's 278, and Dust's 235
supported movies (11 unused/incompatible assets skipped), with independent
frame/pixel/audio comparisons. New fixtures verify repeated chunks, exclusion
of unrelated audio, invalid directory bounds and interactive v4 playback
(`build/movie-order-audit-suite.log`, `build/movie-order-*-native.log`). All
supplied authored beds fit the existing PCM join bounds
(`build/movie-order-bounds.log`).

The original also stores a repeat boundary within the sequence (Dust header
2238, v4 loop-table offset 0). Honoring that boundary and sharing the queued
theme channel remain unfinished; this change corrects the initial authored
sequence. Dust 4207d0..420841 transfers pending nodes in order and selects the
last marked node as the channel's repeat root; the hash-checked evidence is
included in tools/inspect-movie-theme.py.

Actual-data replays pass Dust's Yunni ending with the corrected ordered bed,
fresh credits restore and authored quit (`build/movie-order-dust-ending-suite.log`),
and the demo's unskipped tour/trailer, final clicks, menu returns and fresh
restores (`build/movie-order-demo-media-suite.log`). These checks do not yet
establish correct repeat-boundary or shared-channel behavior.

### Shared theme queue and repeat boundaries

Bank themes and movie beds now submit ordered PCM chunks to one Music mixer
handle, matching original physical channel 3. New submissions append to its
pending tail and select the authored repeat root. Already-consumed history is
released on the main thread; closing a consumed old bank preserves newer music.
Mixed-rate chunks use a fixed-rate stream with a retained fractional source
cursor. Repeated chunks share immutable PCM ownership.

The first movie bed queues behind game music. Later zero-record segments
inherit it, later new beds replace it, and final cleanup clears the shared
channel only when the movie supplied a bed. Silent movies preserve game music.
Dust movies repeat from header 2238; v4 movies/banks use the loop-table root
(including the original record-count clamp). Dust banks follow their explicit
header+30 order and repeat from its first valid entry (403308..40335f), instead
of playing every stored music record once. Hash-checked evidence is reproduced
by tools/inspect-movie-theme.py.

DFSV v6 stores the pending sequence, repeat root, source cursor and a unique
PCM pool. It validates lengths, references and cursor bounds before committing.
Older checkpoints upgrade their joined theme into the queue. Native fixtures
check exact ordered PCM, mixed rates, fractional resume, pending-tail append,
consumed-bank closure, shared handles, pool reuse and six malformed queue saves.

Tracing the Windows backend also corrected a reference approximation:
wavevolume() calls waveOutSetVolume in all three originals. It scales the mixed
output, including music. The engine applies this master to its own music handle
in addition to relative theme gain; it does not change Windows device settings.
tools/inspect-wave-volume.py resolves the named imports and reproduces the
hash-checked setter paths (build/wave-volume-original-evidence.log).

Final all-edition audio, movie and native save-panel/fresh-process/rejection
suites pass (`build/theme-master-gain-{audio,movie,save}-suite.log`). These
include live master gain changes, relative-gain preservation and v6 restoration.
The shared-queue build also passed retail's best ending/all 14 credits clicks/
script quit, Dust's Yunni ending/fresh credits/quit, the developed-photo demo
ending and complete demo tour/trailer/menu restores
(`build/theme-queue-{retail-ending,dust-ending,demo-completion,demo-media}-suite.log`).
Those story replays precede the final master-gain correction; the final native
fixtures and save suites verify that volume-only change. Repeated bank opens,
per-bank theme controls and the remaining completion audit are still pending.
The full-playability goal remains active.

All three freshly generated opening checkpoints are v6. The updated Dust
bank/control corruption tools also pass six native rejection/navigation cases
against that format (`build/theme-master-gain-dust-rejection-suite.log`).

### Independent bank ownership and theme controls

Repeated opens now append separate banks; lookups and closes select the first
matching instance. Effect/voice plays and queued music carry an owner ID, so
same-filename instances retain their own controls and close behavior. Theme
volume is stored per bank, starts at 255, and reads as zero for banks with no
music. Changing an inactive bank no longer overwrites the physical theme gain
or the gain restored by a save. Theme playback uses that bank's descriptors;
the previous fallback to the game's global themevolume variable was removed.

Track enumeration includes every bank, including effects-only banks and repeated
opens, and returns internal names. This fixes retail CTL.STG's closetracks loop,
which must close all open libraries before reopening UNILIB and INVEN. The
original paths and matching rules are reproduced without running the originals
by `python tools/inspect-bank-controls.py`
(`build/bank-controls-original-evidence.log`).

DFSV v7 stores bank IDs, per-bank theme gain, and owner references on active
effects/voices, the theme descriptor and pending PCM parts. Duplicate IDs,
missing owners and invalid gain values reject before commit. Older saves
derive owner IDs from their stored bank keys and migrate their legacy theme
maps. Updated checkpoint and rejection tools accept v7.

Native audio suites pass all editions (`build/bank-enumeration-audio-suite.log`),
including compiled script getters/enumeration, independent defaults, inactive
gain changes, same-name bank closes, nonordinal owner IDs across saves, five
malformed owner saves and eight malformed queue saves per edition. Movie and
all-edition authored/native save-panel/fresh-process/rejection checks pass
(`build/bank-owners-movie-suite.log`, `build/bank-owners-save-suite.log`). Six
v7 bank/control rejections preserve live Dust navigation
(`build/bank-owners-dust-rejection-suite.log`).

Current-build story replays pass retail best ending/all credits/script quit,
Dust Yunni/fresh credits/quit, the developed-photo demo ending and complete demo
tour/trailer/menu restores
(`build/bank-owners-{retail-ending,dust-ending,demo-completion,demo-media}-suite.log`).
Physical volume/subtitle controls, key remapping and fresh v7 settings restores
also pass all three editions (`build/bank-owners-settings-suite.log`).

The full-playability audit remains open. Dust's named one-shot directory still
includes stored music chunks; the original bank layout separates them. That
classification and remaining requirement coverage are the next checks.

### Dust one-shot directory and legacy cleanup

Dust's named sound directory now contains only the header+24 one-shot records.
Music records remain available to the authored theme sequence. Sound counts and
indexed names preserve duplicate records; named lookup resolves the first,
matching original 404070. This corrects UNILIB's duplicate pageturn, which the
previous map collapsed and resolved to the last record. Corpus checks cover
46 preserved SND variants, including 145 stored music entries in 17 variants;
the installed/CD UNILIB copies contain the duplicate. The supplied Titanic
banks have no duplicate one-shot names. These counts include preserved copies.

DFSV v8 keeps v7's payload layout but enforces the corrected directory. Valid
older saves discard controls for retired music-as-effect names and remove their
obsolete effect/voice plays, including loops that could occupy a high-priority
slot indefinitely. Legitimate speech, actual theme buffers and world state
remain intact. Current-format retired controls and unknown keys reject before
commit. Native synthetic checks cover compiled sound count/index calls,
first-match lookup, v7 cleanup, preserved world/voice/theme and v8 resaving.

All-edition audio and save-panel/fresh-process/invalid/cross-edition suites pass
(`build/dust-directory-final-{audio,save}-suite.log`). Original one-shot array,
count, indexed lookup and first-match evidence is reproduced by
tools/inspect-bank-controls.py (`build/bank-controls-original-evidence.log`).
Static triage found no music-only literal effect/voice candidates among 627
candidate calls; this does not prove computed arguments or bank lifetime
(`build/dust-directory-literal-calls.log`).

Actual native inputs pass Dust's timed final gang wave, Yunni ending, fresh v8
credits restore and authored quit (`build/dust-directory-gang-suite.log`,
`build/dust-directory-ending-suite.log`). The full-playability goal remains
active while the requirement and coverage audit continues.

Fresh opening checkpoints are v8 in all editions. Six v8 bank/control rejection
cases also preserve live Dust navigation (`build/dust-directory-rejection-suite.log`).

### Version-pinned completion audit

The third target is the supplied September 1996 interactive demo. The native
ScummVM detector identifies that demo, Titanic retail and Dust correctly
(`build/completion-detect-{demo,full,dust}.log`).

`tools/verify-completion-baseline.ps1` runs regression suites serially and archives
their logs with SHA-256 hashes for the engine sources, verification tools,
executable and input checkpoints. It checks the expected per-edition component
result so a script-only run cannot satisfy another component's gate. Select
`-Suite Components`, `-Suite Story`, `-Suite Controls`, or `-Suite All`;
`-Component Puppets` selects a targeted component rerun. Story checks resume
earned checkpoints and do not independently establish their startup history.

The fresh component baseline passed 12 categories and exposed a puppet host
timing failure (`build/completion-baseline/20261014-084353-172/manifest.json`).
Isolated retail runs reproduced 1,501–1,509 ms failures because the test included
first display/font setup. The audit now prepares the display through the compiled
fixture before measuring speech, retaining the 350–1,500 ms limits and all state
assertions. Failure diagnostics include return value, volume, voice completion,
queued input and speech activity. No production playback behavior changed.

The targeted puppet suite passes all three editions with speech measurements
of 367/377/379 ms for demo/retail/Dust, including authored portrait rendering,
speech, choices and modal input
(`build/completion-baseline/20261014-085528-991/manifest.json`). Both manifests
have matching source/tool/executable hashes before and after their runs;
script_audit.cpp is the only engine
source changed between them. The earlier ad hoc component-labelled run is
explicitly invalidated in `build/completion-validation.json` because its wrapper
repeated script checks instead of forwarding the component switches.

`build/completion-audit.json` records the remaining concrete completion gates:
current ending/media regressions, launch/save/control checks, and walkthrough
continuity/timer evidence review. Component success alone does not complete the
full-playability goal.

The controls baseline now passes ordinary startup in all three editions, including
Dust's unskipped intro sequence; opening/fresh-process/ScummVM-slot/authored-panel
save round trips and nine invalid/cross-edition rejections; physical volume,
subtitle and remapping persistence; F5 menu/cancellation/pause; modal dialogs;
and earned Dust/retail death recovery
(`build/completion-baseline/20261014-085647-120/manifest.json`). Source, tools and
executable hashes match before and after that run. The remaining gates are fresh
ending/media regressions and the walkthrough continuity/timer evidence review.

### Fresh walkthrough provenance

Fresh ending/media regressions pass retail's best ending, both developed and
spoiled demo photo outcomes, the demo tour/trailer, and all five Dust endings
(`build/completion-baseline/20261014-090603-071/manifest.json`). That run's one
failure was a stale test input: the supposed unsuccessful retail checkpoint had
been overwritten by the best route. The corrected test leaves from the earned
boat-deck save while Vlad still owns the notebook, plays the Russian ending,
chooses return to ship, and verifies a fresh cabin restore. Its input, helper,
logs and hashes are preserved in `build/completion-missing-notebook/proof.json`.

`trace-boot.ps1` now loads an archived copy of each input checkpoint and preserves
the launch config before ScummVM rewrites it, the native log, and any newly saved
output in `build/trace-history/`. `audit-trace-lineage.py` requires matching
checkpoint bytes, edition and executable from a successful fresh boot to the
target trace. It rejects unlinked saves, cycles without a root, changed bytes,
failed traces, different editions/executables and untracked load controls.
Ten synthetic boundary checks pass (`python tools/test_trace_lineage.py`), as
does a native fresh-Dust/start/turn/save/restore ancestry test
(`build/lineage-dust-proof.json`). These checks prove continuity, not unplayed
branch coverage.

The retail walkthrough now passes fresh startup, mission one, wireless/telegram,
painting, fencing, the necklace quest, the remaining missions, Vlad's fight,
smokestack/notebook recovery, sinking, all four best-ending evidence items,
14 physical credits clicks and authored quit. The complete checkpoint ancestry
is in `build/full-walkthrough/20261014-103639-041/ending/lineage.json`; its earlier
stages are in `build/full-walkthrough/20261014-101124-687/`. The resumed final run
also verifies unchanged sources/tools/executable in its manifest.

Run `./tools/verify-full-walkthrough.ps1 -SkipSpeech` to reproduce the route.
Speech skips use actual Escape input. The diagnostic driver accepts explicitly
planned optional replies without consuming required replies; this handles the
authored Jones greeting that can interrupt the route. It does not change normal
gameplay. The sinking-clock verifier now checks normalization and increasing
time across minute rollovers, respecting the original randomized seconds.

`./tools/verify-dust-walkthrough.ps1 -SkipSpeech` replays Dust's earned day-by-day
route, both gunfights, four temples and Yunni ending with the same ancestry
checks. Its fresh run is in progress. The opening helper waits for Leroy's
authored arrival at the range before taking the guarded jug; fast dialogue
skips can otherwise reach it while he is still guarding it. Full playability
remains under audit until the remaining walkthrough and requirement checks pass.

The fresh Dust route now passes the third-night bounty fight and Trotter reward
with all five hunters defeated (`build/completion-dust-bounty-lineage.json`).
The combat helper follows visible targets and engages walking hunters at close
range: their authored `makemove()` clears accumulated damage when they start
another movement step. A temporary read-only diagnostic confirmed the hit and
subsequent reset; its logging was removed and the verified executable restored
before continuing (`build/completion-bounty-diagnosis.json`). The remaining
day-four route is still being checked.
