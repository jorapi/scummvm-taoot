# ScummVM DreamFactory

An independent fork of [ScummVM](https://www.scummvm.org/) with an experimental
DreamFactory engine for **Titanic: Adventure Out of Time** and **Dust: A Tale of
the Wired West**.

This is an AI-assisted project maintained in
[jorapi/scummvm-taoot](https://github.com/jorapi/scummvm-taoot/tree/dreamfactory).
Development, support and future releases belong to this fork. The DreamFactory
engine is not being prepared for submission to upstream ScummVM. We continue to
merge upstream improvements while maintaining our own engine and enhancements.

## Games and status

The engine targets these English Windows editions:

| Game | Edition |
| --- | --- |
| Titanic: Adventure Out of Time | Retail |
| Titanic: Adventure Out of Time | Supplied September 1996 interactive demo |
| Dust: A Tale of the Wired West | Retail |

The engine implements original scripts, navigation, conversations, puzzles,
movies, audio and native saves. Completion paths have been exercised for all
three supplied editions; later integration checks cover input, dialogue and movie
playback. This is still experimental, with broader edition and device testing
remaining. Original Macintosh game data and importing original-game saves are
outside the current supported scope.

Windows x64 and Android ARM64 builds have been built and checked locally.
GitHub builds all eight platform packages and publishes a development prerelease
after every successful push to `dreamfactory`. Download them from
[Releases](https://github.com/jorapi/scummvm-taoot/releases); signing and device
validation status is documented in the [platform guide](devtools/dreamfactory/README.md).
The local packaging and CI recipes build DreamFactory-only applications. The
repository retains ScummVM's other engines, which can be enabled separately.

## Playing

1. Use a DreamFactory-enabled build of this fork, or build it from source using
   the [platform guide](devtools/dreamfactory/README.md) and
   [build workflow](.github/workflows/dreamfactory-builds.yml).
2. Prepare the game data from your copy of the supported Windows edition.
3. Start ScummVM, choose **Add game**, and select its prepared data folder.
4. Adjust the shared Graphics, Audio, Keymaps and Engine settings, then start.

Game data and the optional replacement soundtrack are supplied separately from
this source repository and application packages. Stock ScummVM downloads do not
include this fork's DreamFactory engine.

## Controls and optional enhancements

Mouse and keyboard retain their original bindings. Standard gamepad defaults:

| Control | Action |
| --- | --- |
| D-pad | Walk/turn; navigate menus and dialogue replies |
| Left/right shoulder | Previous/next hotspot |
| A | Interact or confirm dialogue |
| B | Back/skip |
| Y | Repeat dialogue |
| Left stick + X | Point, click and drag |
| Start | ScummVM menu |

Bindings are customizable in ScummVM's Keymaps settings. Touch devices retain
backend gesture navigation; an optional direction pad is off by default.
Physical controller and device playtesting remain ongoing.

Optional enhancements include larger/high-contrast dialogue, hotspot hints,
and replacement music with original-track fallback. Graphics scaling uses
ScummVM's shared backend settings. See the
[enhancement guide](devtools/dreamfactory/ENHANCEMENTS.md) for installation,
configuration and current limitations.

## Development and support

```sh
git clone --branch dreamfactory https://github.com/jorapi/scummvm-taoot.git
cd scummvm-taoot
```

* [CI builds and automated releases](devtools/dreamfactory/CI.md)
* [Platform builds and controls](devtools/dreamfactory/README.md)
* [Upstream integration and validation](devtools/dreamfactory/UPSTREAM.md)
* [Companion development workspace and regression tools](devtools/dreamfactory/WORKSPACE.md)
* [Fork issue tracker](https://github.com/jorapi/scummvm-taoot/issues)

Report DreamFactory issues in this repository, including the build/commit,
platform, game edition, reproduction steps and relevant logs. Contributions to
this engine should target this fork's `dreamfactory` branch.

## Credits and license

This fork builds on the work of the [ScummVM team](AUTHORS), and the DreamFactory
reverse-engineering efforts in
[JamesK89/DreamCatcher](https://github.com/JamesK89/DreamCatcher) and
[dhobi/dreamrefactory](https://github.com/dhobi/dreamrefactory).

ScummVM's copyright notices and licenses are preserved; see [COPYING](COPYING)
and [COPYRIGHT](COPYRIGHT). DreamFactory engine sources are GPL-3.0-or-later.
The [original upstream README](README.upstream.md) remains available for general
ScummVM information.
