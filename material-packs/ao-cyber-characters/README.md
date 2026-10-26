# Cyber character artwork preview

14 actual replacement textures. The first six cover scout armour torso, arms, legs, boots and helmet,
plus the picked back/shoulder armour. Brushed metal, carbon weave and restrained
amber inlays follow the Cyber ICC art direction. Gloves and other equipment are
not repainted by that first batch. Eight further atlases cover tech armour
helmet/body/arms/legs/boots and Omni Elite helmet/body/legs. Those preserve the
original designs, colours and insignia, with coated panels and matte joints;
no coloured lights are inferred from paint. Their exact built-in image_gen
prompts are in `scan-prompts.json`. Small maps keep the combined full mip estimate
below 48 MiB for one copy of every map, not including the rest of the client.

Enable **Character & equipment materials** in Materials, with material response
strength above zero and enhancements on. Disabling this option restores original
artwork. This pack is independent of the ICC texture style selector. Strength
adjusts inferred materials; for these authored maps it switches the pack off at
zero, rather than crossfading the artwork.

`source-art/` contains the generated artwork. `prompts.json` records the exact
prompts used with the built-in image_gen editor. `catalog.json` maps public texture
IDs to artwork. Run `python tools/bake-character-art.py` from the repository to
rebuild maps with Pillow and NumPy. `manifest.json` records their hashes.

The renderer keeps original AO alpha from the player's texture because it can
encode glow or coverage. Original game pixels and glow masks are not bundled.
No new geometry or animations are included. Atlas layouts have been inspected;
final fit on the user's live armour still needs confirmation.

The whole-game rework remains in progress. The separate character material
catalog provides broad shading coverage; those assignments are not new artwork.
