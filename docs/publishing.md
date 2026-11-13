# Preview 21 Full HD Setup

Players download one Lite ZIP or one Full HD Setup ZIP. Setup automatically retrieves
the pinned material companions from tag `v0.6.0-preview.21`. The release upload set
is Lite, Full HD Setup, **every** matching material companion and SHA256SUMS.txt.
Use RELEASE-ASSETS.json and the updated publisher; do not upload only the two
player-facing ZIPs. Publishing remains disabled until owner approval. The older
release workflow below describes historical single-archive releases.

# Preparing releases

Target repository: https://github.com/Vitecval/ao-vk-live

After building, run `python tools/package-release.py --version VERSION --build-dir build/windows-live --no-source --omit-authoring-images`.
The version directory contains Full HD and Lite Windows x86 ZIPs and SHA256SUMS.txt. No optional source ZIP is needed; GitHub provides automatic source archives from the tagged commit. Both player editions contain the same renderer and installer. Full HD includes every optional runtime map, material catalog, generation provenance and a complete source-art hash inventory; non-runtime authoring images remain in the preserved production library; Lite retains core ground maps, embedded cliff/shared masters and every renderer feature.

To add Lite to an existing validated Full HD release without rebuilding Full, use `python tools/package-release.py --version VERSION --build-dir build/windows-live --lite-only`. This refuses differing renderer/loader hashes. `--full-only` remains available for local Full HD tests. Verify both final archives with `python tests/release_editions_tests.py dist/releases/VERSION --no-source`. Every distribution must remain below 2 GiB.

Install.cmd installs, Update.cmd upgrades or switches editions, Uninstall.cmd removes owned addon files. Preferences, original game files and personal packs are preserved. Modified owned files cause a refusal instead of being silently removed. Restart after installing optional packs; there is no runtime downloader.

Publishing is a separate explicit action. `tools/publish-release.ps1 -Version VERSION` (or `-CheckOnly`) defaults to offline validation of the frozen source, notes, ZIP checksums and asset sizes. It makes no Git or GitHub changes. `-PrepareSource` creates a separate release branch in the publishing clone and copies only the reviewed snapshot. It stages approved source for `git diff --cached` review before `-CommitSource`; the commit step uses only inventoried files, refuses unexpected changes, commits and records the exact revision without pushing. Only `-Publish` tags the verified revision, pushes the release branch/tag, uploads the two editions and checksums, verifies remote hashes and publishes the prepared notes. Existing matching uploads are reused after interruption; mismatched assets/tags are refused, never overwritten. See the release directory's PUBLISHING.md. Never run a mutation step without authorization.

`tools/prepare-release-source.py --manifest PATH --export DIRECTORY` prepares a reviewed source-only snapshot and a relative-path/hash inventory. It keeps code, shaders, configuration, catalogs, lightweight metadata and essential embedded renderer assets. Optional source-art collections, generated material maps, archives, binaries, builds, private references and logs are excluded. `tools/sync-release-source.ps1` can copy that exact inventory into an existing clean clone; it does not stage, commit, tag or push. No local artwork is removed and Git LFS is not required. LICENSE, upstream attribution and third-party notices remain included.
