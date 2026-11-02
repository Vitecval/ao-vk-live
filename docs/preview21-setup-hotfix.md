# Preview 21 setup correction, 2 November 2026

The original Preview 21 WinForms progress callback had an untyped percentage
parameter. Windows PowerShell can pass the literal command argument `-1` as a
string. A culture-sensitive string comparison can classify it as nonnegative;
assigning it to ProgressBar.Value then throws. Install and Update encounter
indeterminate progress, and Full HD Uninstall encounters it at the 250-file
preflight boundary. The reported preflight failure occurs before game files
are changed.

The callback now binds the message as string and the percentage as int, with
an explicit -1 default. Negative progress selects Marquee; nonnegative progress
is capped at 100 as before. Renderer binaries and all HD material archives are
unchanged. Lite and Full HD Setup ZIPs and SHA256SUMS.txt are refreshed.

`python tests/setup_gui_tests.py --output build/setup-gui-tests` exercises the
actual WinForms click handlers and callback in 32-bit Windows PowerShell, with
only modal dialogs and the final display loop replaced by test probes. It
checks string/integer negative progress, bounded percentages, Install, Update,
Uninstall, repeated Uninstall, settings/original-file preservation and three
offline companion archives. Both fixture editions exceed 250 asset files.
The unpatched callback reproduces the reported exception; the patched callback
passes. Existing host, downloader/corruption/resume and transactional rollback
tests remain applicable.

The original v0.6.0-preview.21 source tag is retained. The setup correction is
committed on codex/release-0.6.0-preview.21 and identified by setupSourceCommit
in the corrected package manifests; renderer sourceCommit retains its original
validated integration identity. A separate setup source tag records the fix
without rewriting the original tag or repository history.
