# AO Cyber World artwork pack

Preview.11 adds **1,828 new Rubi-Ka repaints** in 20 completed core-art batches: Omni modern/Entertainment/Rome, Athen/Tir/Avalon, Home and settlements, AI cities, commercial buildings, outposts, factories, shared architecture, caves, subway/sewer, laboratories, prison, offices, castles/temples, alien ships and Dust Brigade environments. Premium ICC artwork and the earlier neutral families retain their existing maps.

There are now 29 active logical families, 2,618 approved resources and 2,436 inventoried active models. This is **not a complete Rubi-Ka overhaul**. The broader worklist inventories 4,746 models. There are 294 distinct shared companion identities still using their originals in active model inventories; these are listed separately from original functional signs, maps and controls. Other owned prop, furniture, additional-building and mission-surface batches stay queued until their art is complete. The image generation account limit stopped this pass; reset is reported as 2026-10-26 20:37 UTC. Completed source artwork is saved for continuation, not discarded or counted as installed.

Five manufactured paving variants (20, 23, 24, 25, 26) add restrained satin stone relief while keeping the game's original RGB colours and coverage. Asphalt remains non-metal with roughness 0.32, including road/soil transitions. Ultra shared terrain stays at 1 and daylight grading at 0. Natural materials remain rough; Omni metalwork uses a distinct satin metal response.

The world pack has 2,696 retained repaint IDs. The cross-pack library has 2,744 unique repaint IDs, including history. Image estimates for the entire world library are 1,308.1 MiB after the per-map cap and 1,142.8 MiB for eligible world maps; these assume all such images resident together and are not measured zone use. Production keeps the 512-pixel built-in map cap, 24 MiB decoded cache and bounded uploads. Live appearance, zone residency and long-session stability still need the user's test run.

All 2,630 earlier world maps outside the intentionally revised paving-20 triplet retain their hashes. The complete ICC, equipment and modern packs retain their previous hashes. Original alpha and personal overrides remain supported. The expanded architecture policy applies only to the selected world pack, preserving independently selected equipment textures.

Validation: all 2,618 approved architecture resources and all 29 equipment repaints load through the production proxy. Thirteen quality aliases, original alpha, explicit ICC providers, required-map failure fallback, personal overrides, style toggles and low-address-space recovery pass. Twelve family render modes and 49 ground renders pass; all original-colour and shared-on/off paving comparisons have zero pixel difference. Installer rollback and preference-preservation checks pass. Live model fit and long-session play remain unverified.

Artwork provenance: built-in image_gen edits, versioned generated sources, exact prompts in `material-packs/ao-cyber-world/rubika-prompts.json` and `rubika-ground-prompts.json`. `material-packs/rubika-worklist.json` records active core batches and the queued work. Private original references and game binaries are excluded from packages. Rubi-Ka buildings and props remain the priority; thorough Shadowlands work, including creatures, follows afterward.

Historical planning and source notes follow.

# Cyber world architecture and terrain preview

Preview.10 restores the earlier premium ICC panel artwork and the earlier Newland/neutral window, wall and stripe artwork. Their original maps are preserved. Another 203 missing companion surfaces complete the selected building sets: ICC city, neutral midtech/Newland, Last Ditch, neutral backyard/tax office, neutral city structures and neutral ruins, alongside the existing clan, sandwall and Omni HQ/tower groups.

The inventory contains 381 available models across nine logical families and 679 approved resources. This includes reused artwork and material-only inputs, not 679 new paintings. The loader selects an explicit pack for each resource and checks its declared maps; an ORM-only material can retain AO colour. Families that share model parts switch together after their live maps are ready. Missing/corrupt/deferred inputs retain originals for that connected group. Personal overrides keep priority. Original functional signs, masks and cutouts remain.

Shared terrain stays at 1 on Ultra; daylight grading stays off. Decks/plaza paving preserve original colours with authored relief and gloss. Asphalt retains restrained non-metal sheen and original road/soil transitions.

The retained library has 912 repaint IDs, including inactive history; 751 pass identity eligibility before style/loading/character options. The world pack retains 864 IDs. Its capped whole-library estimate is 634.8 MiB; eligible world maps total at most 413.9 MiB if all were resident together. These are upper-bound image estimates, not measured zone residency. Other districts, live UV fit and long-session zoning still need review.

Exact generated companion edits are in `restored-family-prompts.json`; versioned sources are in `source-art/`. The 511-entry family catalog includes previous whole-family art and 203 new companions. Existing single-surface artwork outside that catalog can also be reused by the explicit family provider table.

Historical batches follow. The first twenty include fifteen architectural textures covering the plateyell
building family, neutral walls, concrete and shared metal panels, plus five picked
arid/gravel/rock/grass-transition ground tiles. Source artwork was produced with
the built-in image_gen editor; exact prompts are recorded in `prompts.json`.
The generated images are in `source-art/`. `tools/bake-world-art.py` rebuilds the
colour, normal, ORM and selected emissive maps using Pillow and NumPy.

The next thirty-eight use captured city building/prop IDs: facade bands, windows,
concrete, screens, doors, crates, shipping containers, lamps, pipe trim, Whom-Pah
and memorial panels. Their catalog is `city-catalog.json` and the exact built-in
image_gen edit prompts are in `city-prompts.json`. Original UV references remain
private and are not distributed. Source filenames ending in v2 are corrected
variants; the catalog selects the delivered version. Current live fit is pending.

Another sixty-four repaints cover shared props discovered beyond the captures:
terminals, bins, benches, chairs, containers, cabinets, lamps, pipes, doors,
crates, barrels, computer panels, monitors, tables and insurance signage.
`props-catalog.json` selects their artwork and material regions;
`props-prompts.json` records the exact built-in image_gen edits. Glass, metal,
paint, fabric, rubber and wood receive separate responses, with bounded light
emission. Those 64 originally shipped with the preview.2 renderer unchanged.

Another 24 reusable architectural textures cover wall bases, vents, windows,
skirting, steel panels, grilles, concrete and sand-colour plaster. Their metadata
and exact built-in edit prompts are `shared-catalog.json` and `shared-prompts.json`.
Another 75 picked/captured surfaces include Rome, Adonis, Jobe and ICC selections
plus 29 leaf/bark textures. See `picked-catalog.json` and `picked-prompts.json`.
Leaf RGB extends behind the original runtime cutout mask, with natural material
response and single-leaf veins where required. All 389 earlier world maps are
byte-identical. This remains partial coverage of the wider capture inventory.

Choose **Materials → World texture style → Cyberpunk**. These IDs are shared, so
the artwork appears wherever AO uses them; it is not restricted to Newland or ICC.
Classic HD retains the earlier ICC pack. Original removes the optional authored
world/ICC artwork. Personal overrides keep priority. Original architecture alpha
is preserved from the player's texture at runtime. The terrain uses the original
mesh, including the live combined lightmap path. The optional replacement ground
now handles that live pass too and preserves original draws for unsupported passes;
its default remains off pending live review.

Window panes have authored smooth non-metal glass regions, frames have a separate
metal response, and walls/terrain stay rough and non-metal. Reflections use AO-VK's
existing screen-space/environment approximation. There is no new glass geometry,
physical transmission, ray tracing or traversable building interior.

The independent **Global terrain detail** and **Global building materials** sliders
cover surfaces beyond these repaints. They are detail/material passes, not
individual redraws. Terrain detail preserves original biome colours and boundaries,
adds filtered world-space colour/normal detail and fallback roughness, and creates
no per-tile image cache. Buildings use named profiles plus an opaque static/room
fallback; explicit authored maps win. Zero disables each global pass. Master
Enhancements temporarily disables the packs and both global passes.

Global colour grain and relief are deliberately restrained. Authored ground maps
take priority and do not receive another grain layer. Global passes stay off
outside an active game playfield, including the login backdrop.

The latest scan batch adds 83 IDs: 42 terrain IDs (including two additional
pixel-identical aliases), 16 building atlases, eight rock/pale-stone surfaces,
16 props/bark atlases and one world alias of an identical ash terrain texture.
`scan-catalog.json` selects artwork; `scan-prompts.json` contains the exact built-in
image_gen prompts, including corrected variants. Terrain and rocks stay rough and
non-metallic; pale stone is honed rather than mirror-polished. Small shared prop
maps use bounded dimensions to keep this pack's full mip estimate below 512 MiB
for one independently loaded copy of every map. This is not total client memory.

GPU/loader tests cover switching, cancellation and quality aliases. Atlas layouts
and material masks were inspected; live model fit, shared uses and ground-tile
boundaries still need review. Full-game custom artwork remains in progress.
