# Preview 23 installer audit - Setup 6 / candidate revision 11

This is an installer-only update. The renderer, loader and GPU preparation executable are byte-identical to revision 10. No game was launched, closed or modified for this audit. The user's latest report is that FSR works but frame rate is lower; this is a user report, not a new benchmark or confirmation on other GPUs. The conservative presentation ordering remains unchanged.

## Corrected failure paths

- Drive-root and shallow extraction folders no longer produce an empty parent path while searching for companion HD archives. The same search handles UNC path syntax without inventing an empty parent.
- Browse tolerates an empty installation field and a Windows folder-picker exception. A malformed startup manifest produces an error banner instead of aborting before the window opens. Detection ignores obsolete paths and unavailable drives, and avoids probing unsolicited network installations before the setup window opens.
- Filesystem folders are checked explicitly. Linked/junction paths, directory/file collisions, missing hashes, invalid classic-client manifest entries and malformed metadata fail before protected files are changed.
- Metadata is read explicitly as UTF-8, including ownership receipts and recovery records. This fixes update/uninstall failures after installing into non-English account/folder names. Metadata size bounds and model-index/executable size checks avoid unbounded reads in the 32-bit host.
- Install, update and uninstall acquire locks for the selected game and shared loader. Another setup using the same protected directory is refused before mutation.
- A recovery record describes all protected file moves and expected hashes before the first commit move. Interrupted commits restore the previous files on the next setup. A completed commit has a separate marker so cleanup interruptions retain the new installation. Changed user files and unexpected recovery data are preserved rather than overwritten.
- Uninstall now compares each file against its preflight snapshot immediately before removing it. Recovery attempts every entry even if one file cannot be restored, and retains the record/backups with an actionable error when recovery is incomplete.
- Cleanup failure after a successful commit is a warning, including in the GUI. It is retried next time and does not falsely report a rolled-back installation. Current activity and recovery counts remain visible.
- A GPU preparation helper with five minutes of no CPU or log activity cannot hold setup forever. Only the helper started by setup is stopped; a bounded exit wait follows. Working helpers are not stopped for total elapsed time alone. Helper logs use the user's temporary directory rather than requiring a writable setup folder.
- Missing/corrupt extracted HD textures can be repaired from a pinned verified archive. Extraction processes only the files needed by the current install/update while still validating archive membership. Progress is bounded, verified archives/resumable downloads are reused, and low-space/network/checksum errors retain the installed client.
- Disk-space querying uses the Windows API that also supports network-share directories. A live network share was not available for an end-to-end network install; UNC archive-search syntax was tested.
- The generated packages include every required helper, including the new shared guard file. Direct 64-bit Windows PowerShell forwarding no longer tries to serialize a progress callback as a command-line string.
- Failures save their original error, line location and stack details in `%TEMP%\AO-VK-Setup-Logs`; the GUI links the log and resets its controls after an error.

## Validation

All game/client inputs in these tests are synthetic. The tests never execute their dummy EXEs. The downloader uses a loopback server and checks the production release pins before its test-only transport override; it does not contact public GitHub.

- **53 resilience cases:** every before/after protected-file move boundary for install/update/uninstall, forced termination of the test's own installer, interrupted post-commit cleanup, concurrent setup, malformed manifests, bad hashes, unsafe client paths, file/directory collisions, locked files, simulated disk full, external changes during uninstall, linked targets, drive-root/UNC search and Unicode install/update/uninstall.
- **17 downloader/preparation cases:** automatic and offline multi-pack installs, frozen Preview 21 materials reused by Preview 23, interrupted resume, verified cache reuse, repair of corrupted decoded material, corrupt downloads, wrong-release/low-space refusal, notice validation and transaction rollback.
- **13 executable-memory cases:** enable/disable/preserve, shared loader, invalid/unknown executables, backup checks, failure rollback, external changes and restoration.
- **17 model-index cases:** missing, owned/unowned, compatible annotations, incompatible identities/categories, damaged headers and uninstall preservation/refusal.
- **Actual GUI handlers for Lite and Full:** Install, Update, Uninstall, repeated Uninstall, empty Browse, negative/string/oversized progress values, default/disabled memory option, malformed-manifest errors, diagnostic logs, working UI timer and control reset.
- **Host/launcher checks:** Windows PowerShell x86/x64 and PowerShell Core where installed; paths with spaces, parameter forwarding, failure exits, and all three shipped CMD actions.
- **Helper waits:** active work beyond the old ten-minute cutoff, idle status, native failure and bounded termination of an unresponsive helper.
- **HD-scale metadata:** 26,916 recovery entries in the 32-bit host, about 24.9 MB of JSON, 279 MiB peak working set and 47 seconds on this machine. This measures metadata validation/recovery with nonexistent dummy targets; it is not a texture copy or download benchmark.
- **Packaged manifests:** Lite/Full fresh/update preflight, payload/archive hashes, source archive checks, required-helper dependency checks, unchanged renderer/HD pins and no Living payload.

Evidence is under `build/preview23/checks/installer-*-final`, `installer-*-release` and the candidate package audit. Interrupted-process tests do not simulate physical power loss, disk corruption or every antivirus/driver configuration. An interruption before the commit record is created can leave uniquely named temporary staging files; original files have not been changed at that point. Backup/record conflicts are preserved for diagnosis rather than silently discarded.

## Release handoff

The installer candidate is prepared for the final texture pass. After the final textures are supplied, regenerate their asset/download hashes and validate the exact resulting packages before publishing. Generated/reference-derived art and final texture readiness remain the existing release conditions. Frame-rate tuning and additional physical-GPU testing are not established by these installer tests.
