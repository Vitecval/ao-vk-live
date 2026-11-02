# Optional 4 GB memory support

Lite and Full HD Setup offer **Allow up to 4 GB memory (recommended)**, selected
by default on 64-bit Windows. This enables Large Address Aware for the supported
classic 18.8.62_EP1 game executable. It increases the 32-bit game's address-space
limit from 2 GB to 4 GB; it is not a guarantee against every crash.

Setup handles the official launcher's executable integrity check as part of the
same transaction. Only exact, audited executable hashes are changed. The game
executable gains its LAA flag; the known launcher receives the eight-byte
compatibility change used by the independently verified AOSharp-patched launcher.
An AOPP/custom loader executable copy is checked too. Original game DLLs remain
unchanged. Unknown non-LAA executables or launchers are refused for this option;
uncheck it to retain executable contents and use the existing graphics checks.

Existing LAA flags and launcher patches are preserved, including those enabled
through AOSharp or AOPP. Enabling the flag more than once does not increase the
limit beyond 4 GB. The option is disabled on 32-bit Windows and during Uninstall.
For command-line installs/updates, use `-DisableLargeAddressAware` to opt out.

Before changing an executable, setup stages a hash-verified original backup in
the selected game folder's `.ao-vk-laa` directory. Ownership hashes are recorded
in `ao-vk-live.install.json`. Executable changes, backups, add-on files and receipt
are committed together; a failed install/update restores the previous files.

Use the current Uninstall.cmd to undo this setup's changes. Uninstall stages the
original bytes, removes the add-on and restores its own executable changes in one
transaction. It does not remove a pre-existing AOSharp/AOPP patch. If an executable
or its backup has changed since installation, the affected group is left intact
and its backups retained with a clear message. This avoids overwriting a later
game update or another tool's changes. Keep these backups if manual recovery is
needed. Settings and personal material packs remain untouched.

No game executable, launcher, or personal backup is included in the downloads.
The public configuration contains only supported file hashes and a patch offset;
all executable editing and backups happen locally after the player runs setup.

Validation uses synthetic PE fixtures, actual WinForms button handlers, injected
transaction failures, existing download/host regressions, and isolated copies of
the audited retail executables. No live game client is patched or started by the
tests. Exact launcher byte/hash compatibility is checked against the existing
working AOSharp patch; a new launcher login session is not launched automatically.
