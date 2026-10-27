# AO-VK Live 0.6.0 Preview 14 — local test candidate

Trees gain fuller 3D foliage through their original crowns. Plants → Fuller 3D
tree foliage keeps original branches, leaf colours and distant tree shapes,
with existing distance thinning and leaf limits.

Shared Ground+ now means regular Shared Ground plus nearby pebbles at density 1.
It retains AO's terrain and lighting, adding no ripples, cliffs or tile-edge
blending. Ultra selects this mode with GroundOn off. Enabling GroundOn overrides
either shared mode and uses its separately saved terrain controls. Colour
grading stays off by default.

The picked metal floor and 22 companion plate/transition tiles retain AO's
original tread, colour and panel borders with a rough, lightly shiny finish.
Old generated albedo, height and noisy normal maps are excluded for this family.
Full HD and Lite include the same floor correction.

Fix expired terrain mesh metadata being reused after graphics-mode changes,
which could corrupt the grass cache later during movement. A reproduced
AddressSanitizer use-after-free now passes 4,800 frames of mode switching, grass
regeneration, zoning, movement and turning. Twelve worker-failure tests also
verify that blocked callers stop promptly instead of waiting indefinitely.

Renderer comparisons verify regular-ground brightness, pebble-only additions,
GroundOn override, floor aliases and transitions, and stable tree toggles.
Live visual fit and long-session testing remain for the user. This candidate
has not been published to GitHub. See docs/tree-pebbles-validation.md for evidence
and limitations.

## Retained from Preview 12

Resident replacements stay selected while other material IDs load. Required maps publish together after render-thread validation; failed reloads retain a valid previous set. ICC provider and eligibility rules remain intact.

Original ground colours and asphalt sheen remain. Saved manual terrain preferences are retained.

Quick Controls groups Graphics, Performance and Weather. New settings default to Automatic weather, Render in Background off and colour grading off. Existing preferences migrate without being reset.

Choose Full HD for optional building, prop and equipment replacements plus generated authoring atlases. Lite uses identical renderer DLLs and includes core shared terrain and paving assets, with original models. Update safely between editions with Update.cmd; personal assets and preferences are preserved. Restart the game after changing installed packs.

Texture 17824 now extracts correctly across its database boundary. Its replacement artwork is unfinished. All 838 staged inactive source entries were checked; none satisfy the existing whole-owned-batch rule yet, so none are falsely activated. Two deliberately excluded 8x8 inputs are explicitly retained. No new artwork was generated.

Earlier artwork validation is documented in docs/preview12-validation.md. No release tag or GitHub upload has been made.
