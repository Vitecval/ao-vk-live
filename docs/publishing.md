# Preparing releases

Target repository: https://github.com/Vitecval/ao-vk-live

After building, run `python tools/package-release.py --version VERSION --build-dir build/windows-live --no-source`.
The version directory contains Full HD and Lite Windows x86 ZIPs and SHA256SUMS.txt. No optional source ZIP is needed; GitHub provides automatic source archives from the tagged commit. Both player editions contain the same renderer and installer. Full HD includes optional model maps and provenance atlases; Lite retains core ground maps, embedded cliff/shared masters and every renderer feature.

To add Lite to an existing validated Full HD release without rebuilding Full, use `python tools/package-release.py --version VERSION --build-dir build/windows-live --lite-only`. This refuses differing renderer/loader hashes. `--full-only` remains available for local Full HD tests. Verify both final archives with `python tests/release_editions_tests.py dist/releases/VERSION --no-source`. Every distribution must remain below 2 GiB.

Install.cmd installs, Update.cmd upgrades or switches editions, Uninstall.cmd removes owned addon files. Preferences, original game files and personal packs are preserved. Modified owned files cause a refusal instead of being silently removed. Restart after installing optional packs; there is no runtime downloader.

Publishing is a separate explicit action. `tools/publish-release.ps1 -Version VERSION -CheckOnly` validates the two ZIP checksums and asset sizes without contacting GitHub. Without that switch it creates a draft release for an already-existing tag; `-Publish` publishes it. The helper targets Vitecval/ao-vk-live, uses the release directory's RELEASE-NOTES.md and marks preview versions as prereleases. Never run a publishing operation, push or create a tag without authorization.

`tools/prepare-release-source.py --manifest PATH --export DIRECTORY` prepares a reviewed source-only snapshot and a relative-path/hash inventory. It keeps code, shaders, configuration, catalogs, lightweight metadata and essential embedded renderer assets. Optional source-art collections, generated material maps, archives, binaries, builds, private references and logs are excluded. `tools/sync-release-source.ps1` can copy that exact inventory into an existing clean clone; it does not stage, commit, tag or push. No local artwork is removed and Git LFS is not required. LICENSE, upstream attribution and third-party notices remain included.
