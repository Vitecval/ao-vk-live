# Preview 24 r60 / Horizon 0.7.3

Shared Ground+ applies its HD materials to AO's submitted terrain triangles.
It no longer replaces the surface with an independently triangulated height
grid. This preserves cliff cutouts, overhangs and stitched LOD edges. Native
terrain remains an independent option. A controlled cliff scene preserves the
source's coverage exactly in combined and two-pass rendering; the reconstructed
control differs by 48,223 pixels. Live Inferno confirmation remains pending.

Adonis wall 1010004:208052 has a rough, non-metal masonry rule even when an
older optional pack supplies its material maps. It does not receive rock-object
replacement treatment. Lower-quality aliases use the same rule.

Quick Controls includes Character & equipment materials. Standard Enhanced
remains the default artwork; the independent Reworked selection is ready for
the separately staged pack, with Standard Enhanced fallback where art is absent.
This release contains no new Reworked artwork. The rework is held for approval.

Render scale aligns with the main quick-control rows and steps through all
100 / 85 / 75 / 67 / 50 choices in the authored order, in either direction.
Fast zoning retains its image for at least 650 ms, then fades away after four
fresh world frames. The non-activating popup appears above a topmost game window
and hides when the game loses focus. Click-to-continue remains optional.

Horizon requires an explicit edition choice for a new installation, labels its
folder change action, explains app-only installation, and offers Remove Full HD
as a downgrade to Lite alongside a separate graphics uninstall. Tooltips have
readable colors. Graphics lists Ctrl+Shift+F12 as the in-game shortcut. Installing
the app refreshes the available update action. Existing download buckets remain
available for verified ZIP reuse; the unchanged 19 material parts stay pinned.

The previous local texture-update safeguards are included: bidirectional asset
ownership checks detect removals, cached bootstrap files refresh safely, and
packaging verifies that the app catalogue matches its embedded bootstrap.
