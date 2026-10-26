# Texture overhaul priorities

Preview.11 adds **1,828 new Rubi-Ka repaints** in 20 completed core-art batches: Omni modern/Entertainment/Rome, Athen/Tir/Avalon, Home and settlements, AI cities, commercial buildings, outposts, factories, shared architecture, caves, subway/sewer, laboratories, prison, offices, castles/temples, alien ships and Dust Brigade environments. Premium ICC artwork and the earlier neutral families retain their existing maps.

There are now 29 active logical families, 2,618 approved resources and 2,436 inventoried active models. This is **not a complete Rubi-Ka overhaul**. The broader worklist inventories 4,746 models. There are 294 distinct shared companion identities still using their originals in active model inventories; these are listed separately from original functional signs, maps and controls. Other owned prop, furniture, additional-building and mission-surface batches stay queued until their art is complete. The image generation account limit stopped this pass; reset is reported as 2026-10-26 20:37 UTC. Completed source artwork is saved for continuation, not discarded or counted as installed.

Five manufactured paving variants (20, 23, 24, 25, 26) add restrained satin stone relief while keeping the game's original RGB colours and coverage. Asphalt remains non-metal with roughness 0.32, including road/soil transitions. Ultra shared terrain stays at 1 and daylight grading at 0. Natural materials remain rough; Omni metalwork uses a distinct satin metal response.

The world pack has 2,696 retained repaint IDs. The cross-pack library has 2,744 unique repaint IDs, including history. Image estimates for the entire world library are 1,308.1 MiB after the per-map cap and 1,142.8 MiB for eligible world maps; these assume all such images resident together and are not measured zone use. Production keeps the 512-pixel built-in map cap, 24 MiB decoded cache and bounded uploads. Live appearance, zone residency and long-session stability still need the user's test run.

All 2,630 earlier world maps outside the intentionally revised paving-20 triplet retain their hashes. The complete ICC, equipment and modern packs retain their previous hashes. Original alpha and personal overrides remain supported. The expanded architecture policy applies only to the selected world pack, preserving independently selected equipment textures.

Artwork provenance: built-in image_gen edits, versioned generated sources, exact prompts in `material-packs/ao-cyber-world/rubika-prompts.json` and `rubika-ground-prompts.json`. `material-packs/rubika-worklist.json` records active core batches and the queued work. Private original references and game binaries are excluded from packages. Rubi-Ka buildings and props remain the priority; thorough Shadowlands work, including creatures, follows afterward.

Historical planning and source notes follow.

# Texture overhaul worklist

Preview.10 restores the earlier premium ICC panel artwork and the earlier Newland/neutral window, wall and stripe artwork. Their original maps are preserved. Another 203 missing companion surfaces complete the selected building sets: ICC city, neutral midtech/Newland, Last Ditch, neutral backyard/tax office, neutral city structures and neutral ruins, alongside the existing clan, sandwall and Omni HQ/tower groups.

The inventory contains 381 available models across nine logical families and 679 approved resources. This includes reused artwork and material-only inputs, not 679 new paintings. The loader selects an explicit pack for each resource and checks its declared maps; an ORM-only material can retain AO colour. Families that share model parts switch together after their live maps are ready. Missing/corrupt/deferred inputs retain originals for that connected group. Personal overrides keep priority. Original functional signs, masks and cutouts remain.

Shared terrain stays at 1 on Ultra; daylight grading stays off. Decks/plaza paving preserve original colours with authored relief and gloss. Asphalt retains restrained non-metal sheen and original road/soil transitions.

The retained library has 912 repaint IDs, including inactive history; 751 pass identity eligibility before style/loading/character options. The world pack retains 864 IDs. Its capped whole-library estimate is 634.8 MiB; eligible world maps total at most 413.9 MiB if all were resident together. These are upper-bound image estimates, not measured zone residency. Other districts, live UV fit and long-session zoning still need review.

Historical scan ranking remains a discovery aid below.

## Terrain material consolidation

Preview.7 adds five shared world-space terrain masters. The terrain queue now
primarily needs palette/classification and transition review rather than a new
painting for every original tile variation. Existing 53 scanned terrain repaint
IDs are retained in the library; natural/asphalt repaints are bypassed in both detail modes. They
should not be counted as another 53 active material sets. Grass/dirt/gravel/sand/
rock masters preserve the original AO colour masks and path shapes. Individual
unusual palettes can receive explicit classification in later batches. Buildings,
props and character priorities below are unchanged.

## Priority and remaining work

| Order | Family | Painted in scans | Pending artwork |
| --- | --- | ---: | ---: |
| 1 | Terrain | 53 | 247 |
| 2 | Buildings | 238 | 426 |
| 3 | Props and foliage | 258 | 1,281 |
| 4 | Armour and equipment | 29 | 398 |
| 5 | Characters and creatures | 0 | 217 |

Another 50 effect resources require individual review; three constant/noise inputs
are intentionally retained. Missing artwork totals 2,569 candidates, not a promise
that all are eligible surface repaints. The scans also contain 9,407 runtime draw
records without a database identity and 42 unresolved ID occurrences. Many runtime
draws reuse a dynamically assembled texture: these are not 9,407 unique textures.
Future frame dumps include resource type to disambiguate shared numeric IDs.
One initially extracted architecture ID, 1010004:17824, failed database decoding;
it stays pending rather than being silently omitted.

Use complete model dependency families for production; use observed on-screen scan frequency only to choose between future families. Capped draw counts
and picks break ties. These measure reuse in the captured sample; they are not
global instance counts across the game. Add neighboring transition tiles and the
other faces of a building/prop family to each batch, so a popular wall does not get
new art beside an obviously mismatched trim. Update the queue after each bake.

## Batch workflow

1. Finish the highest-ranked terrain families, including their edge/corner and
   rotated variants. Preserve biome colour, path footprint, crack positions and
   tile junctions. Compare repeated tiles and lower mip levels together.
2. Work through building families: walls, corners, roofs, doors, window frames and
   panes. Separate material masks. Stone/plaster/concrete stay matte; only actual
   metal, glass and appropriate polished marble receive stronger reflections.
   Shadowlands mineral architecture and volcanic rock remain rough and non-metal.
3. Complete shared props and foliage families. Keep the game's original runtime
   cutout alpha. Check leaf silhouettes, grass boundaries and distant mip behaviour.
   Terminal lights get bounded emissive regions, not bright paint across the atlas.
4. Enhance armour/equipment sets together, preserving their original shapes,
   palette and insignia. Differentiate coated panels, bare fittings, fabric and
   joints. Review torso/arms/legs/boots/helmet on an actual equipped character.
5. Review character/creature skin, hair and clothing separately. Avoid metal
   classification on skin. Model geometry and animation replacement require a
   separate project; this worklist covers textures on existing models.

Each batch keeps versioned generated sources and exact prompts. Inspect the UV
layout and palette before baking, reject presentation borders, verify original
alpha at runtime, compare material masks and ensure previous map hashes remain
unchanged. Live checks must cover neighboring tiles, shared uses, changing sun,
wet/dry weather, zoning and multiple clients before marking model fit confirmed.
Current new atlases are reviewed flat; live fit remains pending.

## Rendering fixes delivered with this batch

- **Effects → Highlight glare reduction** (`RVK_SoftHi`, default 0.75): a smooth
  shoulder on bright world surfaces after grading. Keeps pale sunlit walls and
  floors gentler without reducing dark tones. Sky/interface stay outside it;
  additive lamps retain their emitted contribution. HDR and an active game
  playfield are required. Zero restores the previous highlight response.
- Known rock/lava materials override glossy metallic guesses, including authored
  ORM maps. Honed white stone also has a rough, non-metal response; real metal and
  glass controls retain their appropriate material response.
- Grass uses finer coverage masks, filtered edge coverage, clamped lightmaps and
  partial texture-update preservation. Paths no longer inherit a coarse green
  square just because a neighbouring sample contains grass.
- Distant terrain gains restrained medium-scale variation after close detail
  filters away; the haze layer varies with height. These improve presentation of
  existing terrain, without adding missing distant geometry or world objects.
- Replacement ground recognizes live's combined lightmap draw, which previously
  was consumed without being replaced. Unsupported passes keep AO's original
  draw. The setting stays optional and off by default until live review.

The classic client still has a limited address space. This batch keeps full mip
estimates below 512 MiB for the entire world pack and 48 MiB for equipment, each
assuming one loaded copy of every map. Actual residency depends on zones, quality
copies and other rendering resources. Full-resolution generated sources are
retained; smaller runtime maps are used for small props. Keep decoded upload
backpressure, cold CPU compression and allocation-safe object-cache cleanup.

## Refreshing the plan

Run `tools/audit-texture-scans.py` against local scans, a locally extracted name
table and database index, writing the private audit outside public pack folders.
Then run `tools/plan-texture-rework.py PRIVATE_AUDIT --output
material-packs/scan-coverage.json`. Audit inputs remain private. The exported plan
contains IDs, names, frequencies, statuses and reasons, without game pixels,
player names, window titles or capture/log paths.
