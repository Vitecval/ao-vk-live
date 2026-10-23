# Modern texture pack

The first ICC preview upgrades six architectural surfaces with 1024-pixel albedo,
normal and roughness/metallic/occlusion maps. It preserves AO's broad designs and
palette. It is an incremental material upgrade, not a UE5 engine conversion or a
complete remaster; characters, terrain and most objects retain their usual art.

Open **Ctrl+Shift+F12 → Materials → Modern texture pack (ICC preview)**.
The setting is off by default and saved as `RVK_ModernTex=0` or `1` in
`randy-vk.ini`. The normal map, PBR material and Albedo replacements controls also
need to be enabled for those parts of the material to appear. Toggling the pack
refreshes loaded surfaces; decoding takes place in the background. A brief delay
while the textures load is normal. No zoning or restart is required to toggle.

This pack does not need replacement ground rendering. Keep that disabled on live.
Master Enhancements off temporarily returns to the original materials and retains
the pack preference. Personal materials and workbench overrides have priority.

Install/Update includes the preview's baked maps under
`randy-vk/packs/ao-modern-icc`. The installer verifies their hashes and records
ownership for safe updates and uninstall. Other packs and settings are preserved.

Maintainers: see `material-packs/ao-modern-icc/README.md`, `prompts.json`, and
`tools/bake-modern-icc.py` for the source artwork and reproducible map bake.

Validation: the retail-renderer harness registers all six IDs through the normal
texture-creation export hook, loads all three maps, toggles repeatedly, cancels
in-flight loading, restores original maps on off and respects master Enhancements.
Headless GPU material previews check decoding and rendering. Visual alignment on
the actual ICC models still needs a live-game comparison.
