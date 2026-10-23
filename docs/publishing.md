# Preparing the repository and downloads

Suggested repository name: **ao-vk-live**

Description: **Vulkan graphics for classic Anarchy Online live: modern lighting, procedural grass, in-game settings, and easy Windows install/uninstall. Based on AO-VK.**

Create a new repository, retaining LICENSE, THIRD-PARTY-NOTICES.md and the upstream credit. The provided source ZIP is arranged as a clean repository root. Extract it and push its contents; omit build/ and dist/. No game DLLs are included.

The MIT license permits this distribution with its notice preserved. This source snapshot does not carry upstream Git history. For future upstream updates, keep a separate upstream checkout and compare changes; the original README is preserved in docs/upstream-readme.md in the exported source.

For the first release, create a GitHub release tagged **v0.1.0**, titled **AO-VK Live 0.1.0 — classic live preview**. Attach:

- `ao-vk-live-0.1.0-windows-x86.zip` — the ready-built player download, including setup.
- `ao-vk-live-0.1.0-source.zip` — the complete source snapshot.
- `SHA256SUMS.txt` — checksums for both ZIPs.

GitHub's automatic Source code download is source only. Point players at the **windows-x86.zip** asset, which contains the ready-built DLLs. The packages are currently unsigned.

Suggested release notes are in RELEASE-NOTES.md. The package has been tested on the classic 18.8.62_EP1 live client; support is limited to the audited DLL hashes. Keep this release marked as a prerelease while broader hardware, zoning and long-session testing continues.

To create future archives after building:

```powershell
python tools/package-release.py --version 0.1.1 --build-dir build/windows-release
```

Packaging includes only whitelisted source folders and release files. It excludes local builds, logs, captures, personal settings and proprietary game DLLs. Add a known prior release's DLL hashes to release/known-builds.json when needed for receipt-free upgrades from that release.

Reference: https://docs.github.com/en/repositories/releasing-projects-on-github/managing-releases-in-a-repository
