# Preparing releases

Target repository: https://github.com/Vitecval/ao-vk-live

After building, run `python tools/package-release.py --version 0.6.0-preview.12 --build-dir build/windows-live`.
The version directory contains Full HD and Lite Windows x86 ZIPs, source ZIP and SHA256SUMS.txt. Both player editions contain the same renderer and installer. Full HD includes optional model maps and generated atlases; Lite retains core ground maps, embedded cliff/shared masters and every renderer feature.

Install.cmd installs, Update.cmd upgrades or switches editions, Uninstall.cmd removes owned addon files. Preferences, original game files and personal packs are preserved. Modified owned files cause a refusal instead of being silently removed. Restart after installing optional packs; there is no runtime downloader.

Publishing is a separate explicit action. `tools/publish-release.ps1 -Version VERSION` creates a draft release for an already-existing tag; `-Publish` creates a public release. The helper verifies all three ZIP checksums, targets Vitecval/ao-vk-live and marks preview versions as prereleases. Never run it, push or create a tag without permission.

Source exports omit builds, logs, personal settings, private original references and proprietary game binaries. LICENSE, upstream attribution and third-party notices are included.
