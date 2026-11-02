# Preview 19 validation scope

Preview 19 imports 1,445 worker texture-ID bindings and retains all Preview 18 maps.
The renderer implementation is unchanged apart from the generated resource-provider
table for accepted world maps. Native per-cell terrain, AO lighting-disabled terrain
classification, classic lighting, alpha/void handling and production foliage remain
identical to the frozen Preview 18 source. No new native ground IDs are imported.

Worker artwork, original IDs/alpha/aspect, companion maps, source hashes and
documented family ownership are checked independently. Earlier rounds are included;
held families and incomplete runs are itemized in the release integration report.
No approved map is silently replaced. Five rubber-catsuit materials are excluded
after the existing equipment response regression failed; assertions are unchanged.

The expanded player library omits non-runtime authoring sheet images while retaining
all runtime maps, catalogs, prompts/provenance and source-art hash inventories.
Original authoring files remain preserved in production and worker directories.

Use the release REPORT.md, TEXTURE-INTEGRATION.json and validation.json for the exact
checks and results. A first cold terrain capture differed; the unchanged warmed-cache
rerun matched all opaque/transparent pairs exactly. Cold-start timing is not certified
by that warmed rerun. Offline verification does not certify all game zones, equipped
mesh fit, moving LOD, long sessions or live gameplay. Database identity coverage is
not visible-world coverage. No Shadowlands completion percentage or FPS improvement
is claimed. Nothing is installed, committed, pushed or published during preparation.
