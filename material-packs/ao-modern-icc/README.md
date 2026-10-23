# Modern ICC materials preview

Six optional replacement materials for the classic live client's ICC architecture:
concrete, two ivory wall panels, building trim, shuttle grilles and shuttle backing.
This is a small environment showcase, not a whole-game texture replacement.

In AO-VK, press **Ctrl+Shift+F12**, open **Materials**, and enable **Modern texture
pack (ICC preview)**. It starts off. The switch is saved and updates existing
surfaces without restarting. Disabling it restores the original game material
or your other installed override. The master Enhancements switch also disables
the preview temporarily without forgetting your choice.

The usual Albedo replacements, Normal maps and PBR materials controls still apply.
Leave those enabled to see the full material. Other texture packs and files in
`randy-vk/materials` or the live workbench retain their usual precedence.

The player installer copies only the baked maps, this README and the asset manifest
to `randy-vk/packs/ao-modern-icc`. It does not alter the game's resource database.
Uninstall removes only recognized installed files, preserving personal settings
and other packs. Close this AO installation before installing or updating files.

## Authoring

`source-art` holds reference-guided raster art generated using the built-in
OpenAI image generation tool. `prompts.json` records the prompts. The original
extracted game images are not included. Atlas layouts are approximate remasters;
some fine grille and edge details differ from the classic originals.

Run `python tools/bake-modern-icc.py` from the repository root, with Pillow and
NumPy installed, to reproduce `maps` and `manifest.json`. Outputs are 1024 square,
except the 1024 by 256 trim. Fine detail supplies restrained tangent-space normal
maps; authored UV regions and conservative surface values supply roughness and
metallicity. These are approximate material properties, not scanned surfaces.
No height displacement, emissive lights or geometry changes are introduced.

Material filenames use the classic resource IDs. Lower texture-quality variants
share the same overrides. These six IDs may appear on assets outside ICC too.
The pack is distributed under this repository's MIT license; original AO artwork
and trademarks remain their owners' property.
