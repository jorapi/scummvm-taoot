# CI builds and releases

The maintained/default branch is `dreamfactory`; the older `master` remains in
the repository. Build workflows run on branch changes, pull requests
and manual dispatch. The same reusable workflow builds Windows x64, macOS arm64
and x86_64, unsigned iOS arm64, three Android ABIs and experimental PSP homebrew.

Each push to `dreamfactory` automatically publishes a downloadable development
prerelease after all eight builds and package checks succeed. A push containing
several commits creates one release for its final commit. Local commits alone
do not trigger builds or releases; batch related work before pushing.
Find downloads on the repository's [Releases page](https://github.com/jorapi/scummvm-taoot/releases).
Development tags use `dreamfactory-dev-<build-run-id>` and existing releases are
never overwritten. Failed builds publish no release. PR and manually dispatched
platform builds upload artifacts without publishing a development release.

## Release a version

Use **Actions → DreamFactory release → Run workflow** on `dreamfactory`, enter a
new tag such as `dreamfactory-v0.1.1`, and leave **publish** unchecked to create a
draft. After inspecting it, publish through GitHub. Alternatively, push a new
`dreamfactory-v*` tag to build and automatically publish an experimental
prerelease once every platform succeeds. Existing releases are never overwritten.

All packages come from GitHub-hosted runners. Each build checks package contents
for credentials and unexpected personal home-directory paths. Release preparation
requires all eight expected packages, repeats privacy checks, and generates
`manifest.json` with the source commit/run URL and `SHA256SUMS.txt`.

Android APKs and the iOS IPA are unsigned; installable signing requires a stable
signing key/setup. macOS applications are ad-hoc signed and not notarized.
Compilation and package checks do not establish hardware or full-game coverage.

Only publishing jobs receive `contents: write`; PR builds use read-only tokens.
No local game data, soundtrack files or local build outputs are uploaded by CI.
Future local commits use the repository's configured GitHub noreply identity.
This setup does not erase author metadata in existing commits or replace the
already-published local-build assets.
