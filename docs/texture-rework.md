# Whole-game artwork rework

Preview.11 adds **1,828 new Rubi-Ka repaints** in 20 completed core-art batches: Omni modern/Entertainment/Rome, Athen/Tir/Avalon, Home and settlements, AI cities, commercial buildings, outposts, factories, shared architecture, caves, subway/sewer, laboratories, prison, offices, castles/temples, alien ships and Dust Brigade environments. Premium ICC artwork and the earlier neutral families retain their existing maps.

There are now 29 active logical families, 2,618 approved resources and 2,436 inventoried active models. This is **not a complete Rubi-Ka overhaul**. The broader worklist inventories 4,746 models. There are 294 distinct shared companion identities still using their originals in active model inventories; these are listed separately from original functional signs, maps and controls. Other owned prop, furniture, additional-building and mission-surface batches stay queued until their art is complete. The image generation account limit stopped this pass; reset is reported as 2026-10-26 20:37 UTC. Completed source artwork is saved for continuation, not discarded or counted as installed.

Five manufactured paving variants (20, 23, 24, 25, 26) add restrained satin stone relief while keeping the game's original RGB colours and coverage. Asphalt remains non-metal with roughness 0.32, including road/soil transitions. Ultra shared terrain stays at 1 and daylight grading at 0. Natural materials remain rough; Omni metalwork uses a distinct satin metal response.

The world pack has 2,696 retained repaint IDs. The cross-pack library has 2,744 unique repaint IDs, including history. Image estimates for the entire world library are 1,308.1 MiB after the per-map cap and 1,142.8 MiB for eligible world maps; these assume all such images resident together and are not measured zone use. Production keeps the 512-pixel built-in map cap, 24 MiB decoded cache and bounded uploads. Live appearance, zone residency and long-session stability still need the user's test run.

All 2,630 earlier world maps outside the intentionally revised paving-20 triplet retain their hashes. The complete ICC, equipment and modern packs retain their previous hashes. Original alpha and personal overrides remain supported. The expanded architecture policy applies only to the selected world pack, preserving independently selected equipment textures.

Validation: all 2,618 approved architecture resources and all 29 equipment repaints load through the production proxy. Thirteen quality aliases, original alpha, explicit ICC providers, required-map failure fallback, personal overrides, style toggles and low-address-space recovery pass. Twelve family render modes and 49 ground renders pass; all original-colour and shared-on/off paving comparisons have zero pixel difference. Installer rollback and preference-preservation checks pass. Live model fit and long-session play remain unverified.

Artwork provenance: built-in image_gen edits, versioned generated sources, exact prompts in `material-packs/ao-cyber-world/rubika-prompts.json` and `rubika-ground-prompts.json`. `material-packs/rubika-worklist.json` records active core batches and the queued work. Private original references and game binaries are excluded from packages. Rubi-Ka buildings and props remain the priority; thorough Shadowlands work, including creatures, follows afterward.

Historical planning and source notes follow.

# Whole-game artwork rework

Preview.10 restores the earlier premium ICC panel artwork and the earlier Newland/neutral window, wall and stripe artwork. Their original maps are preserved. Another 203 missing companion surfaces complete the selected building sets: ICC city, neutral midtech/Newland, Last Ditch, neutral backyard/tax office, neutral city structures and neutral ruins, alongside the existing clan, sandwall and Omni HQ/tower groups.

The inventory contains 381 available models across nine logical families and 679 approved resources. This includes reused artwork and material-only inputs, not 679 new paintings. The loader selects an explicit pack for each resource and checks its declared maps; an ORM-only material can retain AO colour. Families that share model parts switch together after their live maps are ready. Missing/corrupt/deferred inputs retain originals for that connected group. Personal overrides keep priority. Original functional signs, masks and cutouts remain.

Shared terrain stays at 1 on Ultra; daylight grading stays off. Decks/plaza paving preserve original colours with authored relief and gloss. Asphalt retains restrained non-metal sheen and original road/soil transitions.

The retained library has 912 repaint IDs, including inactive history; 751 pass identity eligibility before style/loading/character options. The world pack retains 864 IDs. Its capped whole-library estimate is 634.8 MiB; eligible world maps total at most 413.9 MiB if all were resident together. These are upper-bound image estimates, not measured zone residency. Other districts, live UV fit and long-session zoning still need review.

Historical batches below describe how the source library grew.

The chosen direction is the Cyber ICC / Cyber character artwork: industrial
metalwork, fabric and rubber with distinct responses, readable silhouettes,
detailed wear and restrained light inlays. Existing geometry and animations stay
in place. Natural surfaces, skin and cloth need their own designs rather than
being turned into metal. The current deliverable is a preview, not a finished
whole-game repaint.

The current priority order is terrain, buildings, props/vegetation and equipment;
character faces/bodies and creatures come last. UI atlases, effects, alpha cutouts and
dynamically assembled character textures need separate review. Shared texture
IDs can improve multiple zones or models together.

Each family goes through ID discovery, original UV-layout inspection, generated
replacement artwork, region-appropriate material maps, memory/loading checks,
and a live model-fit review. A good flat atlas is not proof of correct model fit.
Completed batches remain available through the optional settings and installer.

`tools/texture-rework-status.py` reports actual new-art IDs separately from
authored material-only and inferred material assignments. Optional local index
and name inputs produce an authoring queue without reading any original pixels:

```text
python tools/texture-rework-status.py --index local-index.json --names local-names.json --queue local-queue.json --report progress.json
```

Only full-quality world/equipment, terrain and character resource tables are
enumerated. Lower-quality copies are aliases, not extra completed textures.
These tables may also contain effects, unused resources and interfaces, so the
raw inventory is an upper-bound discovery list, not a promise that every ID is
an eligible model repaint. Texture counts do not estimate art-production time.

Generated sources and exact prompts live with each pack; deterministic map
bakes do not require the game database. Original game pixel data and personal
captures stay outside the distributable source. Pack manifests record delivered
coverage, hashes and review limitations. The equipment source pack currently
contains 29 actual repaints; broader inferred coverage is not counted as art.

The world preview adds twenty actual repaints: fifteen Newland/shared building
textures and five terrain tiles. The source artwork, exact built-in image_gen
prompts and reproducible map bake are in `material-packs/ao-cyber-world`.
Select **World texture style → Cyberpunk** to load this batch alongside ICC.
Original restores the original authored world artwork.

Two independent sliders extend improvements beyond painted batches:
**Global terrain detail** adds filtered world-space detail to actual terrain
draws, preserving biome colours and tile boundaries. **Global building materials**
uses 2,631 named architecture profiles and a conservative opaque static/room
fallback. It distinguishes rough walls, reflective metal and smooth dark panes
in window-tagged textures. Explicit authored material maps take priority.
These passes are not thousands of newly painted textures. Both default to zero
in new installs, honor master Enhancements, and create no per-surface texture
cache. Glass uses existing reflection approximations on the original geometry;
live building fit and terrain boundaries remain preview checks.

A second world-art batch adds 38 captured building/prop textures, for 58 world
repaints total. `city-catalog.json` records the delivered IDs and UV/material masks;
`city-prompts.json` records the exact built-in image_gen prompts. It includes
windows, facade bands, doors, terminal screens, concrete, wood/steel crates,
containers, lamps, pipe trim, Whom-Pah and memorial panels. UI/effect/character
captures were excluded from this batch. Shared uses improve across zones; all-city
artwork coverage remains incomplete. Global detail is now gentler, skips authored
ground maps and stays out of the login backdrop.

A third world-art batch adds 64 shared prop repaints discovered beyond city
captures, bringing this pack to 122 repaints. It covers terminal assemblies,
bins, benches, chairs, containers, cabinets, lamps, pipes, doors, crates,
barrels, computer panels, monitors, tables and insurance signage. Sources and
exact built-in image_gen edits are recorded in `props-catalog.json` and
`props-prompts.json`. Separate region masks distinguish glass, metal, painted
panels, fabric, rubber and wood; selected light regions receive emission.
The bake preserves all 183 earlier map files exactly. Automated atlas/mask and
runtime loading checks supplement the remaining live model-fit review. This
artwork batch uses the existing renderer without graphics-code changes.

A further 99 resource IDs add 24 reusable architectural textures and 75
picked/captured environment textures, including 29 foliage/bark surfaces. The
world pack now contains 221 repaint IDs. `shared-catalog.json` / `shared-prompts.json`
and `picked-catalog.json` / `picked-prompts.json` record sources, exact built-in
image_gen edits, material regions and capture-count discovery evidence. Captured
frame counts indicate repeated sightings, not the number of cities or placed
instances. Leaf RGB covers the original cutout; original alpha stays in the
player's database and is restored by the runtime. This batch adds no gear art.

Art direction: preserve original equipment designs and colours, improving
clarity and material response with shine where appropriate. Larger style changes
belong to environment artwork, props and buildings. Foliage stays botanical and
non-metallic. The remaining capture inventory includes many more architecture
textures, plus interfaces/effects and equipment requiring separate treatment;
the current repaint totals must not be presented as full scan or whole-game
completion.
