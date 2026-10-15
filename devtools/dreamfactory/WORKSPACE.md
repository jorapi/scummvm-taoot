# Development workspace snapshot

The engine, platform build scripts and CI workflow live directly in this ScummVM
fork. `workspace/` preserves the companion tools, synthetic script tests and
workspace notes used during development. It contains no game data, game
executables, downloaded soundtrack, credentials, dependencies or built packages.

## Restore the companion workspace

These tools expect a workspace containing `scummvm/`, `tools/`, `tests/`,
`game-data/`, `references/`, `dependencies/` and `build/`. To recreate that layout:

1. Clone this branch into a directory named `scummvm` inside a new workspace.
2. Copy the contents of `scummvm/devtools/dreamfactory/workspace/` to the workspace
   root. The archived README describes the local development milestones.
3. Supply your own game data and local dependencies. Reference parser tests use
   JamesK89/DreamCatcher and dhobi/dreamrefactory, with TypeScript under
   `dependencies/node_modules/`. Their sources are not vendored in this snapshot.
4. Use `tools/build.ps1` for the local Windows build or the documented platform
   scripts in `devtools/dreamfactory/`. The GitHub workflow builds directly from
   the ScummVM checkout and does not require this workspace layout.

The tools preserve the existing workspace-relative paths. Some helper scripts
are specific to the original Windows/WSL SDK installation and must be adjusted
on another machine. Automated regression tools need the relevant supplied game
data; synthetic fixtures alone do not reproduce the corpus or walkthrough tests.

Build outputs, checkpoint saves, screenshots, audio downloads and local audit
reports are intentionally excluded. Links into `build/` in the archived notes
refer to local evidence, not files published in this repository. The historical
completion record predates subsequent enhancement/controller changes and is not
a claim that every later platform build has completed a device playthrough.

No binaries or game assets are published by committing this source snapshot.
