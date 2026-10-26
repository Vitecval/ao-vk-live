# Cyberpunk ICC material pass

Preview.10 restores these original premium building panels through explicit ICC resource providers in `../building-families.json`. The original artwork/maps are unchanged. Family requirements allow material-only profiles. Current manufactured ground policy preserves AO colours with authored relief/gloss; partial natural/asphalt repaints remain library history.

Select **Ctrl+Shift+F12 → Materials → ICC material style → Cyberpunk**.
Original and Classic HD remain available. The selection saves automatically.

This pass assigns physical materials to **127 identified resource IDs** across
ICC buildings, terminals, vents, metalwork, bins, lamps, statues, tree bark and
the ground surfaces seen in live captures. **18 surfaces receive new albedo and
normal maps**, from 12 authored source images; other assignments keep the game's
artwork and change roughness/metallicity. Natural ground and bark remain non-metallic.
These numbers describe asset coverage, not 127 individually redrawn textures or
complete coverage of the playfield. Further shared or unnamed assets may remain.

Brushed titanium and graphite replace selected dull panels and pipe surfaces;
painted cyan/amber inlays have emissive maps. Asphalt, paving and decking get new
1024-pixel artwork and material response. Grass-ground tiles and the picked bark
texture are updated; this does not replace tree geometry or canopy textures.

The style temporarily enables per-pixel lighting, normal/albedo/PBR maps, HDR and
screen-space reflections, with effective minima for reflection/highlight strength.
It does not overwrite those saved preferences. Original or Classic HD restores
their saved values; the master Enhancements switch temporarily suspends the style.
Rain continues to control actual wetness. No weather or fog mode is forced.

Screen-space reflections can show only information visible to the camera; the
existing environment probe supplies an approximate fallback. This is AO-VK running
AO's geometry and scene, not Cyberpunk 2077's engine, ray tracing or a geometry remake.
Material IDs reused by other zones also receive the style while it is selected.

## Source and rebuild

`catalog.json` records every resource ID, material profile and optional source-art
assignment. `source-art/` contains generated raster artwork; `prompts.json` records
the built-in image generation prompts. Original extracted game images are excluded.
`python tools/bake-cyber-icc.py` (Pillow + NumPy) reproduces `maps/` and the hashed
manifest without needing the game database. Normal/roughness/metallicity/emission
maps are deterministic numerical bakes and authored material values, not scans.

The installer verifies and owns only the baked maps, manifest and this README in
`randy-vk/packs/ao-cyber-icc`. Updates and uninstall preserve preferences and other
packs. Personal `randy-vk/materials` overrides retain priority. All artwork follows
the repository's MIT license; original game artwork and trademarks remain their
owners' property.
