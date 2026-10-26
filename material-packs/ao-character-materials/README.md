# Character and equipment material catalog

First stage of the broader AO material upgrade, enabled with **Materials →
Character & equipment materials** (`RVK_AutoMat=1`). Material response strength
(`RVK_MatStrength`, 0–1) adjusts the fallback response; zero disables it.

The catalog contains **3,661 automatically classified resource IDs** from the
audited classic live inventory, including 1,718 unresolved character surfaces
given a conservative non-metal baseline. Profiles distinguish skin, hair,
textiles, leather, rubber, polymers, coated metal, steel, organic material,
bone and wood. Names provide hints rather than guaranteed semantic segmentation.
Unknown identities remain original. These are material assignments, not newly
painted textures, new geometry, subsurface scattering or anisotropic hair.

Entries apply to identified, lit 3D surfaces. Transparent/additive effects and
interface draws are excluded. Armour and skin assembled into untagged dynamic
textures do not yet receive individual material masks; that requires the next
compositing pass. Existing authored ORM maps, including ICC's, take priority.

The compiled runtime table stores IDs and two material bytes, requiring no
additional image allocations, decoded image cache or asset preload. It covers
matching lower-quality world/base-skin IDs as well. HDR, per-pixel lighting and
PBR are enabled temporarily while the feature is active; saved preferences are
restored when disabled. It does not force screen-space reflections or weather.

`catalog.json` records provenance names, inferred profiles and classification
rules, without any original game pixels. Rebuild the embedded runtime include:

```text
python tools/build-material-catalog.py
python tools/build-material-catalog.py --check
```

To import a local inventory, pass `--names-json` and optionally `--index-json`.
Review changes to the inferred catalog before distributing it. Future curated
normal/ORM/albedo packs can override these defaults without changing this table.
