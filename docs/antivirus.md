# Antivirus and download warnings

Keep the exact filename, antivirus product, detection name, and protection-history
entry when reporting a block. A Windows reputation prompt, a malware detection,
and a folder-permission error need different responses. A hash match proves the
download matches this release; it does not independently prove a file is safe.

AO-VK Live includes unsigned community renderer/loader DLLs and a source-built
`ao-vk-cache-warm.exe` helper. That helper prepares a local, GPU-specific pipeline
cache without starting the game. The resulting `.cache` is data, not the helper
program. Please identify which of the two was flagged.

The optional **Allow up to 4 GB memory** setting modifies exact recognized local
files: the LAA flag in `AnarchyOnline.exe`, and, when needed, eight bytes in
`Anarchy.exe` for the official launcher's compatibility check. An AOPP loader copy
may also be patched. This changes file hashes and can trigger security heuristics;
the detection's cause cannot be established without the vendor's report. The
supported original/patched hashes and offset are in `large-address-aware.json`.
Verified originals are saved locally in `.ao-vk-laa`, with ownership in the install
receipt. No game executable or original backup is downloaded or distributed.

Unchecking the memory option leaves existing executable contents unchanged. It
does not undo a previously applied patch. Use Uninstall to restore this installer's
own unchanged executable/backup pair. Patches made by other tools are preserved.
See [large-address-aware.md](large-address-aware.md).

Setup verifies package/staged hashes, then checks that committed files still
exist and that program/index files retain their expected hashes before declaring
success. If a file disappears during that transaction, setup reports a possible
quarantine and attempts rollback. A vendor can still quarantine a file later.
Check protection history before retrying. Do not disable antivirus or add blanket
drive/folder exclusions as a project-recommended workaround.

Publisher signing can reduce reputation warnings for our own binaries, but needs
a validated signing identity and does not guarantee every vendor will accept a
new release. We do not sign Funcom executables as our own. Suspected false malware
detections should be submitted to the actual antivirus vendor after identifying
the detection; no sample is submitted automatically by this installer.

Microsoft documentation:
- [SmartScreen reputation for developers](https://learn.microsoft.com/en-us/windows/apps/package-and-deploy/smartscreen-reputation)
- [Investigating false detections](https://learn.microsoft.com/en-us/defender-endpoint/defender-endpoint-false-positives-negatives)
- [Microsoft file submission](https://www.microsoft.com/en-us/wdsi/filesubmission)
