# Upstream integration

The DreamFactory branch integrates ScummVM upstream `master` through
`c6777a499f23247b91469ea86b361dd8a655f4e0` (260 commits after the original
`4aa1ef851824ea1f5569a9504ab1b1a07fe670cc` base).

The merge applied without conflicts and did not modify DreamFactory engine code.
Source-overlay instructions and packaging metadata use the updated upstream base.
Historical completion notes retain the original validation revision.

## Validation

* Windows x64: project regenerated and Release build passed.
* Native input/stage checks: Titanic retail, September 1996 demo and Dust passed.
* Native movie checks: all three editions passed, including controller region
  selection, optional hotspot hints and shared audio cleanup.
* Native dialogue checks: all three editions passed, including controller replies,
  speech skipping and original keyboard behavior.
* Android ARM64: native/Gradle builds, APK signatures, manifests, ZIP CRC and
  16 KB alignment checks passed. No Android device playthrough was performed.

These are integration checks, not new full-game or physical-device playthroughs.
macOS, iOS and PSP CI configurations remain available; this merge does not claim
that those platforms were built or tested locally.
