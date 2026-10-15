# Optional enhancements

## Current direction

Use the original visual assets, ScummVM runtime presentation, readable text and
the optional remastered soundtrack. Independent AI image processing and the
de-dithering experiment did not satisfy temporal consistency; those exports are
research artifacts, not recommended game packs. No further API batch is planned.

### Runtime presentation

In ScummVM's per-game Graphics settings, use OpenGL where available. Start with
Filter graphics enabled and Fit to window; the engine's 640x480 screen is already
4:3, so preserve that shape rather than stretching it to a wide display. Compare
Filter graphics disabled with Pixel-perfect scaling if you prefer sharper pixels;
integer scaling may leave larger borders. Filtering smooths pixels; it does not
recover missing detail. These are shared backend settings, not engine options.

The local helper uses an isolated config and supports both baselines:

```powershell
./tools/run.ps1 full -Presentation Smooth -EnhancedMusic
./tools/run.ps1 full -Presentation Sharp -EnhancedMusic
```

Use `-PrepareOnly` to write that helper config without launching a window. These
presets use the default shader and do not change normal launcher preferences.
Try custom shaders through ScummVM's Graphics UI. Shader availability depends on
the build, backend, installed shader packs and GPU; do not assume Windows and
Android offer identical choices. Start with subtle CRT/VGA presentation if you
want a period display look; curvature, bloom and strong scanlines can reduce text
legibility. Whole-screen shaders also affect the engine's dialogue and artwork.
ScummVM's shader packs are available through its download facilities; see
https://docs.scummvm.org/en/latest/settings/graphics.html .

Dialogue subtitles and choices already rasterize their antialiased TrueType
glyphs directly onto the 640x480 game canvas. They are not yet display-resolution
text on a 4K monitor. Original bitmap labels and interface art are still bitmaps;
sharp vector fonts alone cannot improve those assets.

### Enhancement priorities

1. Listen to each replacement music track, author intro/loop intervals and
   verify room changes, dialogue, pause and save/restore. Current album loops
   are a draft. Keep original music as the fallback and compare levels without
   compressing away the score's dynamics.
2. Accessibility options now include larger/high contrast dialogue, keyboard and
   controller reply selection, and optional touch/confirm speech skipping.
   Device usability and longer-text layout review remain useful follow-up work.
3. Keep the original artwork. Replacement artwork is no longer an active
   enhancement priority, and its Engine menu option has been removed.
4. Hotspot markers are now optional, and the shared autosave timer is serviced
   only at existing safe save points. No puzzle or timer changes are implied.
5. Research suspected missing/cut content against scripts, resources and the
   original interpreter. Restore only verified omissions with regression checks;
   absence of a literal filename reference is insufficient evidence.

The optional soundtrack is implemented and disabled by default. Animated/world high-resolution replacement and restored
cut content are not implemented. Original archives and scripts stay intact.

## Accessibility and controls

The per-game Engine tab exposes separate switches, all initially off:

* Larger dialogue text: increases normal 12-point dialogue to 14 points and
  rasterizes directly at 640x480. Long replies use the original-size screen font
  to retain their authored row targets. Exceptional script font sizes are kept.
* High contrast dialogue: white captions/choices, black choice area, yellow
  selection border. Portraits and the original input geometry are preserved.
* Enhanced dialogue controls: Up/Down cycles replies, Enter confirms, 1-5 selects
  a reply directly, and F7 repeats the last dialogue during reply selection.
  Controller reply actions work independently of this keyboard option; see the
  touch and gamepad bindings below. Confirm skips only the current speech line. A fresh paired tap during speech skips the current
  run like Escape; the first 200 ms and orphan releases are ignored to avoid
  carrying the click that opened a conversation into an accidental skip.
* Show hotspot hints: enabling the option displays sampled yellow target markers
  from startup, including on touch-only devices. F6 toggles their visibility. Remap this
  action through ScummVM's keymapper for your controller or available backend
  input. Original mouse and keyboard bindings remain intact; controller bindings
  are listed below.
  Held interactive close-ups also show markers at their authored click regions,
  including the apartment newspaper and OK buttons. The same toggle works inside
  these views and remains in effect on returning to the world. Moving/play-through
  frames and click-anywhere-to-dismiss movies do not receive markers.
  Markers cover sampled props, actors, painting hotspots and interface buttons,
  using ordinary occlusion-aware hit tests. They do not prove a target's script
  will currently do anything, and tiny targets may fall between sample points.

`./tools/run.ps1 full -Accessible -HotspotHints -EnhancedMusic` enables the
accessibility group in the helper's isolated config. The normal launcher keeps
its independent per-option settings. Autosave is controlled by ScummVM's shared
Autosave setting, including Off; the existing reserved slot and overwrite prompt
are used. It is checked in navigable world views outside modal scripts, speech,
movement and held gestures. Title/menu/document flats are excluded. Time spent
in the save operation or its overwrite prompt is excluded from the game clock.
This is periodic autosave, not a new checkpoint or undo system.

## Touch and gamepad controls

Mouse and keyboard retain their existing bindings. The keyboard accessibility
option remains independent of the new controller actions. Controller bindings
are customizable through ScummVM's per-game Keymaps settings.

* D-pad: original walking/turning in world views; select targets on flat menus;
  Up/Down selects dialogue replies. Navigation advances once per press.
* Left/right shoulder: previous/next visible hotspot, including original interface
  buttons and authored interactive movie regions. The cursor moves to the selected
  hotspot, without adding a selection box to the artwork. During dialogue, the
  shoulders move the cursor to the selected reply. Moving the pointer takes over
  from the previous hotspot selection.
* A: interact with the selected hotspot (or current pointer), confirm dialogue,
  or skip the current speech. B: Escape/full skip to the next reply decision. Y: repeat dialogue.
* Left stick: ScummVM's existing virtual pointer. X: skip the current speech line, or the original left mouse button outside speech;
  hold X while pointing to perform drags and other precise gestures.
* Start: ScummVM's shared game menu. Its GUI handles its own controller input.

Enable **Touch direction pad** in the Engine settings for four visible walking
and turning buttons in navigable world views. Tap once per movement. The pad
hides during movement, conversations and close-up movies; ordinary tapping still
handles objects, replies and original interface buttons. Disable the option to
recover the original pointer interaction across that area of the picture.
The launch helper accepts `-TouchControls`.

Hotspot selection uses actual occlusion-aware hit tests. Very small targets can
fall between samples; use the pointer for those and for drag/hold puzzles.
Interactive close-ups activate the region under the cursor when no hotspot is
selected. An empty-space click is consumed rather than activating region zero
(often the OK/exit control). The native movie audit covers this behavior,
shoulder selection, pointer takeover and mouse/touch parity in v1 and v4 movies.
Physical controller and Android touch usability still require device playtesting.

Hovering a visible actor, prop, painting or interface button uses the game's
original hand cursor. Held interactive movie frames update the hand from their
authored regions, including shoulder-selected targets and pointer movement.
Leaving a target restores the authored navigation cursor; transitions, hidden
cursors and explicit scripted cursor changes retain their original behavior.

## Artwork research (retired)

Replacement artwork is no longer exposed in the Engine settings or local launch
helper. Original artwork remains the normal presentation. The experimental
static-document loader and original-image catalog are retained for development;
no replacement artwork is installed or included in releases.

## Install and select music

Extract `build/enhancements/dreamfactory-taoot-hq-music-draft.zip` into the game
folder. Its `enhancements/` directory belongs alongside `BOOTFILE`, `installed/`
and `volumes/`. The workspace already has it at `game-data/full/enhancements/`.
For Android, copy this directory into the existing Titanic data folder.

Enable **Use enhancement soundtrack** in the game's **Engine** options and
restart. Existing launcher targets may need to be started once to refresh their
available options. ScummVM music volume and mute settings remain effective.
The workspace helper also accepts `./tools/run.ps1 full -EnhancedMusic`.

## Prepared soundtrack

The requested playlist contains 13 remakes by Louis Stever, described by the
creator as using original E-MU Proteus samples; the original score is by Erik
Holt. There are 20 exact resource-path mappings, including disc and low-memory
variants. Unmapped tracks fall back to original music. This is not the entire
game's score.

Downloaded best-available audio was decoded to 44.1 kHz, 16-bit stereo WAV.
Conversion does not restore information lost in the source encoding. Credits,
download hashes and original-resource hashes are retained in the pack report.
The private local pack is separate from binaries, APKs and the source overlay.
Preparation is repeatable with `python tools/prepare-enhancement-music.py`.

**Musical loop refinement is pending.** The supplied draft repeats complete
album tracks, including their intros and fades. It needs listening and loop
alignment before being described as seamless. Engine support for separate
intros and explicit loop intervals is implemented and tested.

`python tools/refine-enhancement-music.py` now creates a separate audition pack
at `build/enhancements/music-refined/`, leaving the installed pack unchanged.
It matches four-second spectral contexts in repeated passages, refines waveform
alignment, preserves the intro and applies a six-millisecond seam blend. Twelve
tracks have candidate loop intervals; the radio prelude has no sufficiently
similar repeat and retains its album loop. No gain normalization is applied.
Open `music-refined/index.html` for ten-second seam previews and full tracks.
The separate `build/enhancements/dreamfactory-taoot-hq-music-loop-candidates.zip`
is installable with the same layout as the earlier draft, but is not automatically
installed. It passed WAV/hash/loop-bound validation and ZIP CRC checks.
`loop-review.json` records scores, exact sample boundaries, source/output hashes
and `listeningReviewed=false`. Automatic similarity is not proof of correct
phrasing or seamless music; these candidates still require musical audition.

## Pack format 1

`pack.ini` binds to the complete BOOTFILE MD5, not the detector's partial hash.
Each `music-` section identifies a case-insensitive canonical resource path:

```ini
[pack]
format=1
boot_md5=1d59e1f2109bcbceb7f09767fa3538fb
title=Example music pack

[music-a-deck]
source=titanic2/DATA/DECKA.TRK
file=music/a-deck.wav
loop_start=0
loop_end=6705665
```

Files are relative to the enhancement folder. Absolute paths, empty components,
`.` and `..` are rejected. Missing/invalid entries use original music. Loop
boundaries are PCM frame numbers, independent of channel count; end is exclusive.
Frames before a nonzero start play as the intro. Without boundaries, the entire
recording loops. ScummVM's WAV decoder preserves mono/stereo. Limits are 64 MiB
per input file and 32 Mi decoded samples. Playback uses the original shared
theme queue, music gain and bank ownership; mono beds duplicate to both channels.

## Saves and tests

Mono-only saves remain DFSV version 8. Active stereo music uses version 9 with
channel counts. Versions 1–8 remain readable. Embedded stereo PCM allows current
music to resume without a pack, but increases save size substantially. These
new saves require this or a later engine. Later tracks follow the current pack
setting, even when restored current music came from another setting.

Replacement entries can set `preserve_original_intro=true` to retain the original
bank chunks before its authored repeat root, then play the replacement intro/loop.
This is also the compatibility default for older `BEDRAD1.TRK` entries. The apartment's
`BEDRAD1.TRK` begins with silence and seven BBC broadcast chunks; those play once
before the replacement piano starts. They are not included in the music repeat.
Other replacements keep their existing behavior unless that flag is enabled;
this avoids duplicating original musical lead-ins such as C-Deck's introduction.
An existing save retains its embedded audio queue, so a save made by the earlier
build while that replacement was already playing cannot recover the omitted
broadcast. Start a new apartment sequence to hear the corrected introduction.

All three editions' native audio/save and movie suites passed. Additional tests
cover edition mismatch, stereo separation, intro/loop boundaries, invalid files,
traversal, original fallback and stereo save/restore with pack loading disabled.
A real retail checkpoint-to-grand-staircase probe loaded the A-deck replacement
and restored its save with pack loading disabled. Android device runtime and
musical listening are pending; compile/package checks are separate evidence.

## Gigapixel pilot

`node tools/export-enhancement-scenes.mjs` exports C73 and GSTAIR2: 520 decoded
background PNGs across 52 turn/walk sequences. Sources, decoded content and
depth hashes are recorded in `build/enhancements/visual-pilot/manifest.json`.
No original archive or gameplay mask is changed.

Batch the `original/` PNGs in Gigapixel at exactly **2x**, keeping PNG format and
the exact filenames, into `enhanced/`. Open `compare.html` for split comparisons
and whole-sequence playback. Validate with:

```powershell
python tools/verify-enhancement-images.py --originals
python tools/verify-enhancement-images.py
```

The installed CLI requires an Enterprise license. Desktop automation was stopped
with Escape; the subsequent API pilot uses `tools/topaz-enhancement-pilot.py`.
Its default is a dry run. It prepares six consecutive `GSTAIR2/Road58/0` frames
at 2x for High Fidelity V2 and CGI. Execution requires `--execute --max-credits 12`
and a local `TOPAZ_API_KEY` or `--key-stdin` hidden prompt. Credentials and signed
download URLs are not saved. A persistent ledger reserves costs before submitting;
ambiguous submissions cannot be automatically retried. Existing process IDs are
resumed without resubmission. Estimates and available credit balance are checked
before processing. The helper refuses a pilot cap above 12 credits.

Results and separate comparison pages are under `build/enhancements/topaz-pilot/`.
The authorized pilot completed all 12 images: exactly 12 credits consumed,
balance 4200 -> 4188. Both models passed six-file name/dimension/PNG CRC checks.
Initial still-image review favors CGI for cleaner woodwork; High Fidelity V2
accentuates original dithering. CGI also simplifies fine carvings, so review
sequence playback before accepting a batch. Temporal consistency is not yet
validated. No further API batch is authorized; larger batches will run locally.
Each model has `original/`, `enhanced/`, `manifest.json` and `compare.html`.
Use `python tools/verify-enhancement-images.py build/enhancements/topaz-pilot/cgi`
(or `high-fidelity-v2`) to check filenames, dimensions and PNG CRCs.
Live image-pack loading and a higher-resolution engine compositor remain later
work; this workbench is not an in-game HD toggle.

## Local de-dithering preparation

`python tools/dedither-enhancement-frames.py` writes same-size PNGs with the
same filenames into `build/enhancements/visual-pilot/dedithered/`, alongside
`original/` and `enhanced/`. Originals and enhanced outputs are untouched.
Use the cleaned PNGs as input to the upscaler of your choice. Install NumPy and
Pillow if needed (`python -m pip install numpy Pillow`); workspace-local copies
in `dependencies/image-processing/` are also supported.

The filter uses a 5x5 edge-aware weighted average with a lightly smoothed RGB
guide so individual dither dots do not protect themselves as edges. Default
spatial sigma is 1.15 pixels and range sigma is 24 in 0-255 RGB units. It adds
no sharpening, resizing, grain or generated detail and preserves alpha.
`--range-sigma` and `--spatial-sigma` allow repeatable adjustments;
`--sequence GSTAIR2/Road58/0` exports only that sequence for preview.
Settings and per-file input/output hashes are recorded in
`dedithered/dedither-report.json`. Narrow lines and fine texture can soften;
inspect original versus cleaned frames and motion before committing to a batch.

## Content research

`node tools/inventory-enhancement-content.mjs` lists filenames without exact
script-literal references: 92 retail Titanic, two demo and 107 Dust candidates.
These are **not verified cut content**. Dynamic filenames, native tables, rare
branches and alternate endings require further analysis. No storyline, puzzle
or timer patches have been applied.

`node tools/triage-enhancement-content.mjs` adds source/container/string evidence
for extensionless and path-qualified references and checks filenames in original
executable data without running it. Of the earlier candidates, script leads
were found for five retail Titanic resources, both demo resources and 58 Dust
resources. There remain 87 retail and 49 Dust unresolved candidates; none has
been verified as a restoration target. See `build/enhancements/content-triage.json`.

Useful primary archival leads are the Video Game History Foundation's
[Titanic development binder, 1994-1996](https://library.gamehistory.org/repositories/2/archival_objects/20002)
and [production documents/correspondence, 1994-1995](https://library.gamehistory.org/repositories/2/archival_objects/20012),
in the Andrew Nelson papers. Their catalog describes draft scripts and production
research. The catalog records have been checked; the underlying scans have not
yet been compared against shipped scripts. An early draft alone is not enough
to establish that assets for a scene survive or that restoring it is appropriate.
There is also a cataloged [September 29, 1995 second-draft script binder](https://library.gamehistory.org/repositories/2/digital_objects/10382).
The digital archive viewer currently exposes only its page shell through the
text browsing tool; no claim of having reviewed those scan contents is made.

## Dialogue journal

Recording is enabled by default and can be disabled with **Record dialogue
journal** in ScummVM's game options. Open the journal with **J** or controller
**Back/Select**; the binding is configurable in ScummVM's keymapper.

The Log view presents heard conversation lines and selected replies as a
continuous transcript, ordered oldest to newest and opened at the latest entry.
Each entry shows its speaker, location, elapsed game time and complete wrapped
text. Its Play/Stop button replays the original voice recording. Scroll with a
mouse wheel, scrollbar, Page Up/Down or a touch drag. Up/Down selects adjacent
entries; Space replays the selected line and Enter opens its full-page details.
The transcript uses ivory text, brass speaker headings and a navy background.
Small GUI fonts use ScummVM's existing larger body font for readable dialogue.
Location and time share the heading row where space permits; full context remains
available in Read. Compact headers show only the time, avoiding truncated room
names. An untitled speaker is displayed as "Conversation", preserving
the stored identity and avoiding guesses from internal resource filenames.

The Choices view lists complete encountered reply sets in the full-width
transcript, marking the chosen reply with `>`. Read opens a scrolling detail page
for text longer than the screen. The three main views are Log, Choices and Flow;
the separate Starred view and bookmark buttons have been removed. Existing
bookmark fields are retained when loading and saving older history.
Search matches character names, locations, text and offered replies, with all
search words required. Search is shown on taller overlays; compact screens
dedicate that space to the transcript instead.
Some replies have no voice recording. Movie/radio audio without conversation
text is not transcribed.

Journal controller controls use a dedicated, configurable ScummVM GUI keymap:
D-pad or left stick selects entries; A replays/stops the selected line, X or Y
opens details, B or Back closes, and the shoulders switch views.
Left/Right also switches views when the search field is not focused. Keyboard
editing shortcuts and mouse input remain available. The normal ScummVM GUI
keymap is restored on exit; these bindings do not replace gameplay controls.

Browsing pauses gameplay and existing audio. Replays use a separate speech
channel and never rerun dialogue scripts. Closing the journal resumes the game.
The journal is unavailable during movies.

History and bookmarks belong to the loaded save. Old saves load with an empty
journal; earlier dialogue cannot be reconstructed. Saves containing history use
format v10 (v11 when a route position is recorded) and require a compatible build of this fork. The most recent
16,384 entries are retained. Disabling recording keeps existing entries.

The Choices view is a record of replies in the loaded save. **Flow** is a compact
story/outcome map for returning to useful parts of the game. It does not draw a
node or an unexplored stub for every reply. Routine dialogue remains in Log and
Choices, and repeated discoveries of the same ending share one outcome tile.

Return points follow authored story sections: Titanic's apartment, three main
assignments, the sinking and final evacuation; Dust's five days; and the supplied
demo's briefing and investigation. Only reached sections appear. The engine
captures the earliest safe world-input checkpoint available in each section;
loading an old mid-game save cannot reconstruct earlier return points.

The chart keeps these chapter checkpoints and reached endings, plus safe return
points shared by paths that actually produced different outcomes. Ordinary
conversation chains are folded away. A fixed ending catalog adds spoiler-hidden
`?` tiles for unreached outcomes and a reached/total count: eight Titanic history
combinations, five Dust finales and one supplied-demo conclusion. Tour mode has
no ending catalog. Unreached tiles have no invented connecting path or checkpoint;
they are readable with the same pointer/controller controls as reached outcomes.
Copies of a chapter with materially different recorded ancestry retain their own
checkpoint; revisiting never fabricates inventory or combines story states.

In Flow, Up/Down follows the parent/child path and Left/Right moves between sibling
branches. A or Y opens details, X requests a checkpoint revisit, B/Back closes,
and LB/RB switches views. Keyboard Enter opens details, Space replays an available
voice line and R requests a revisit. R3/Z switches Overview/Cards; L3/Home or the
Current button returns to the active position. The on-screen Cards/Overview and
Current controls keep stick-click inputs optional on PSP-style controllers.

At 480x272, all views use one tab row without a redundant title or permanent
search row. Log and Choices have Read, Replay and Back actions. Long details open
on a separate scrolling page. Pointer selection, wheel scrolling, touch dragging
and scrollbar routing are tested on desktop and handheld layouts. Hidden alternate
views are kept outside the hit-test area.

**Revisit** restores the complete recorded checkpoint after confirmation. It
replaces unsaved progress while preserving manual saves and cumulative discovery.
A section without a safe snapshot cannot be revisited. Missing/damaged snapshots
leave the live game intact and show a controller-dismissable notice. Restoration
happens after the journal and its replay player close; it never resumes a
fabricated suspended conversation stack.

Inventory, item ownership, object values/poses, story flags and the clock are
restored from the same snapshot. Titanic outcomes are determined by the authored
game scripts, including what you kept, used, lost or gave away; the chart never
assigns an ending from a selected reply or alters inventory to force a result.
A later chapter preserves earlier decisions, so changing an earlier object choice
requires an earlier recorded return point. Missing earlier snapshots cannot be
reconstructed from an old save. Native tests mutate ownership, held item, object
value/pose, history flag and clock, then verify that a route rewind restores them
together without erasing cumulative discoveries.

Completed finale movies add ending tiles. Titanic's authored narration flags
distinguish eight history combinations, including combinations sharing a movie.
Dust's five day-five finales and the supplied demo's conclusion are recognized in
their story contexts. Credits, ordinary quitting and unrelated movies do not add
outcomes. Duplicate reached outcomes are merged in Flow without deleting their
recorded history or checkpoints.

Journal presentation references: [Ace Attorney: Dual Destinies' conversation
backlog](https://news.capcomusa.com/lets/browse/phoenix-wright-ace-attorney-dual-destinies-localization-the-trials-and-tribulations-of-the-gameplay-mechanics-team)
and [Ren'Py's speaker/text/voice history model](https://www.renpy.org/doc/html/history.html).
The interface uses ScummVM theme widgets and fonts, without borrowing game art.

### Route discovery and scene revisiting

Route discovery is now recorded during gameplay, independently of the loaded
save's dialogue history. The shared game options include **Record branching
routes**; disabling it preserves existing discoveries. Profiles are isolated by
ScummVM game target and BOOTFILE identity, and use ScummVM's save-file manager.
Route metadata v2 adds section tags and reads existing v1 profiles. Alternating
checksummed metadata files retain the previous committed generation
if the newest write is damaged. If neither generation is intact, recording is
disabled for that session rather than overwriting the damaged profile.

Before an input gesture is dispatched at an idle script boundary, the engine
prepares a native checkpoint. At a newly reached story section, a safe world
checkpoint is stored immediately, independently of dialogue. Normal conversations
inside an already captured section do not each store another snapshot. Legacy
encounters without section metadata retain their existing safe snapshots. These
files stay separate from manual/autosave slots. Conversations with no safe
pre-interaction snapshot remain discoverable without a revisit checkpoint.
Each snapshot is immutable, bounded to the native save limit, and counted against
a default 1024 MiB uncompressed storage budget. The optional configuration key
`dreamfactory_route_storage_mib` accepts 64–4096 MiB. Reaching the budget stops
new checkpoint storage while discovery recording continues.

Native v11 saves retain the playthrough's route position and a lineage token;
loading an older save resets that position without erasing discoveries. A save
from a different discovery profile cannot accidentally select a reused numeric
node ID. All prior native save versions remain loadable.

The internal scene-restore operation validates and loads the complete checkpoint
transactionally, then reuses the discovered scene when its conversation starts
again. Tests cover input-triggered capture, authored state restoration, replay
without duplicate scenes, cumulative discoveries, failed writes, recovery,
storage limits, and missing/corrupt checkpoints for all three supported games.
The Flow view exposes these checkpoints with a confirmation before restoring
the complete scene, including story variables, inventory, RNG and game clock.
Native UI fixtures cover controller traversal, return-point details, desktop and
handheld layouts, confirmation cancellation, successful restoration,
and a damaged checkpoint dismissed with the controller.

Current native saves intentionally exclude active conversations and running
script frames. A journal entry is not a resumable script checkpoint. Scene
checkpoints must therefore be captured at a stable point before an interaction;
revisiting restores that scene rather than fabricating a suspended call stack.
Keep route discovery separate from the loaded playthrough, preserve manual save
slots, bound checkpoint storage, and validate a checkpoint before replacing the
live session. Tests must cover alternate paths, corrupt/missing checkpoints,
loading old saves, controller input, and all three supported games. The branch
map has native real-data verification from earned checkpoints: Dust's Yunni
ending, chapter rewind and Marie ending retain both outcomes while displaying
one day-five return point, two reached endings and three unreached ending slots;
Titanic's best outcome survives through credits and
scripted quit; the supplied demo's developed-photograph route records its
conclusion. These checks use normal player input and checkpoint restoration,
with dialogue/movie skips where stated, without injecting story variables.
Run `tools/verify-dust-route-revisit.ps1`, `tools/verify-taoot-route-ending.ps1`
and `tools/verify-demo-route-ending.ps1` in the prepared workspace to repeat them.
The route-planning helper uses the final motion-frame facing on arrival, matching
the engine's current walking behavior.

The overview was compared visually against the original flowchart screenshot in
[Kotaro Uchikoshi's GDC presentation](https://media.gdcvault.com/gdc2013/slides/824343Uchikoshi_Kotaro_VisualNovelsNarrative.pdf),
page 25. The shared patterns are compact state tiles, orthogonal branch lines,
selected-node inspection and returning to meaningful story sections. The
original's fixed story graph is adapted to reached chapters and actual outcomes
in these open-world games; return points restore complete native checkpoints. The
period palette and text controls are drawn by the engine with ScummVM fonts,
without using VLR artwork.

Additional screenshot references: [VLR's dialogue presentation](https://www.rpgfan.com/gallery/zero-escape-virtues-last-reward-screenshots/)
and [the conversation-log screenshot page](https://www.mobygames.com/game/59163/zero-escape-volume-2-virtues-last-reward/screenshots/ps-vita/650026/).
The latter page was located through its indexed caption; its image download is
not available in this environment, so no pixel-level log comparison is claimed.

The native UI audit reduces a 30-record dialogue trace to 13 story return points
and reached outcomes at desktop and handheld sizes. A separate stress fixture
reduces 2,000 routine conversation/decision records to a return point and an
ending. Pixel comparisons verify that Current
returns to the active position and switching Overview/Cards preserves selection.
The log audit covers wrapped text, pointer selection, scrolling, voice replay and
controller navigation. Device runtime validation remains separate from these
native GUI and Android package checks.

The VLR interaction model is adapted to these games: browse a connected overview,
inspect a discovered scene, then confirm a return to its beginning. Controller
branch traversal, current-position focus and hidden outcomes work in both map
layouts. Unlike VLR's authored fixed chart, this map grows from reached story sections;
it cannot reveal the location or number of story branches not yet encountered.
The conversation log remains a continuous speaker-labelled transcript with
individual voice replay, rather than requiring a scene jump to hear a line.

### Modernization roadmap

The active development goal follows this order, extending existing engine and
shared ScummVM facilities. Enhancements must remain optional where they alter
presentation or assistance, preserve original mouse/keyboard actions and story
rules, and remain usable with a controller at 480x272.

1. **Implemented:** checkpoint previews with saved thumbnail, location, clock and inventory before restoring.
2. **Implemented:** optional controller inventory: larger original item previews, readable descriptions and authentic examine/use/give actions, including conversation inventory.
3. Recovery: rotating recovery saves separate from route/manual saves and a Continue view.
4. Replay: distinct line/full skips and fast-forward of previously heard dialogue only.
5. Accessibility: subtitle size, background opacity, speaker labels and shared audio controls.
6. Journal: discovered clues, promises and unresolved leads with optional progressive hints.
7. Map: visited rooms, current location, encountered characters and optional next-direction guidance.
8. Ending hints: optional progressively more specific guidance based on discovered information.

Checkpoint preview companions use a bounded, checksummed format tied to the
profile identity, checkpoint ID and native snapshot digest. A 128x96 RGB thumbnail
is averaged from the backing game picture before the triggering action. Preview
metadata records that same moment's location, clock, held item and carried/key
item ownership. Route details and restore confirmation read only the companion
and native save header; they do not load a second running game or change inventory.
Missing, damaged or older previews fall back to normal checkpoint restoration.
Native save and route-profile formats remain unchanged. Preview storage reserves
128 KiB per companion within the existing route budget; insufficient preview
space or an optional companion write failure leaves the native checkpoint usable.
Native storage tests pass for all three editions. Desktop and 480x272 controller
tests cover preview details, cancel, restore and damaged native checkpoints.
`tools/verify-taoot-route-preview.ps1` checks a real earned Titanic sinking state:
the saved view, clock, thumbnail and four ending-critical item owners agree before
and after a normal turn and route rewind. A real Dust alternate-ending rewind
also passes. Windows compilation and Android ARM64 package checks are separate
from physical handheld testing, which remains outstanding.

The inventory adapter follows Titanic retail's `countitems`/`indextoitem` order
and original `getname` labels. The supplied demo uses its carried inventory props.
Dust follows the `avatar` flat's `countday`/`indextoday` functions: an item still
owned from an earlier day is not necessarily available in the current inventory.
Original large item sprites are decoded from a copy of the prop, without changing
its view, animation, ownership or story globals. `tools/verify-inventory-model.ps1`
checks earned retail, demo and Dust saves, including the day-five chest-only list.
Enable **Modern inventory** in the game's ScummVM engine settings to replace the
original inventory panel automatically. F8 or the right controller trigger opens
the new inventory explicitly. The original panel remains the default. Within the
new panel, D-pad/left stick or LB/RB selects an item, Y invokes its original
inspection, A chooses Use/Give and B closes the panel. After Use/Give, aim at a
world target and press A or click; B/Escape or the on-screen Cancel button cancels.
Pointer selection, wheel scrolling, touch dragging and a scrollbar are supported.
Item previews use the supplied artwork with bilinear scaling, without replacement
art or generated detail. Labels do not reveal authentic versus counterfeit items.

Native inventory selection and original button scripts handle every action.
Conversation inventory feeds normal paired clicks back to the waiting script,
preserving its return flow. Giving an item uses the original held-item target
handler; the interface never assigns ownership or quest variables itself.
`tools/verify-inventory-ui.ps1` exercises desktop and 480x272 layouts, mapped
gamepad actions, scrolling and earned object/NPC/conversation exchanges. Native
save formats are unchanged. Physical Android/controller and PSP testing remains
outstanding; compiling an APK does not establish device behavior.
