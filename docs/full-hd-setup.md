# Full HD: one download, one setup

Download **ao-vk-live-0.6.0-preview.21-windows-x86-full-setup.zip**, extract it,
then open **Install.cmd**. Choose the AO folder and Install or Update as usual.
Setup downloads the matching Preview 21 HD materials automatically. No separate
download manager, manual pack selection, Python or extra installed software is
needed. Lite remains one ZIP: extract it and open Install.cmd.

The setup window and console show progress and downloaded/total sizes. SHA256
pins, archive sizes and decoded material hashes come from the manifest embedded
in this Setup ZIP. Downloads use only the exact
`Vitecval/ao-vk-live` release tag `v0.6.0-preview.21`; Setup never uses "latest".
The companion archives satisfy GitHub's per-file limit and are handled for you.

If the connection fails, reopen Install.cmd. Setup retries temporary failures and
resumes matching partial downloads when the server supports byte ranges. A server
that cannot resume starts that archive again. Verified downloads in
`.ao-vk-downloads` are reused. Keep the extracted setup folder until installation
is complete. Checksum failures are never installed.

Offline: place **all three matching material ZIPs beside Install.cmd** (the parent
download folder also works), then run Install.cmd. Verified cached packs work
offline too. No network request is needed when all required packs are present.

Allow free space for downloaded archives, expanded textures and transactional
game-file staging (about 10 GiB total for a fresh installation when everything is
on one drive). Setup checks the actual pinned sizes and reports which drive needs
space. Close AO and its launcher yourself; Setup never terminates them.

All packs must pass verification before game-file staging. A failed download or
preparation leaves the existing installation unchanged. Installation commits as a
transaction and restores the previous files on failure. Graphics preferences and
personal packs are preserved.

This release has not been published. Until the matching GitHub release is made
available, automatic online downloads report an explicit unavailable-release
message; local/offline installation and the verified test server need no publish.

Developer verification: run `python tests/material_download_tests.py` in the
source checkout on Windows. This standard-library fixture uses loopback HTTP and
the real 32-bit built-in downloader. It checks automatic multi-pack downloads,
verified reuse, interrupted/resumed transfers, corrupt bytes, offline mode,
mismatched-release refusal, insufficient space and installer rollback. It never
contacts the public release or modifies a real game installation.
