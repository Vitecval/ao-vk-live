# AO-VK Live

Optional **world texture styles**: choose Original or AnarchyHD in **Ctrl+Shift+F12 → Materials → World texture style**. Ultra enables AnarchyHD, Shared Ground+, building materials and equipment art, with GroundOn off. Shared Ground+ uses GroundOn's detailed terrain, cliffs, boulders and pebbles with classic AO-style lighting and no added ripples. Standard Shared Ground keeps AO's terrain submissions. Enabling GroundOn selects its separate native lighting for comparison. Metal decks keep their original tread and transition artwork with a rough, lightly shiny finish; roads retain their asphalt sheen. Fuller 3D tree foliage is controlled by **Plants → Fuller 3D tree foliage**. Brighter daylight grading stays off by default. See [Preview 16 validation](docs/preview16-validation.md) and [modern materials](docs/modern-materials.md).

Preview 17 retains 621 additional replacements compared with Preview 15. The full-quality database inventory has 4,380 baked replacement IDs; 35 recorded material families have complete dependency coverage and three remain partial. The retained library contains 4,389 unique repaint identities, including nine outside that database index and style-dependent history. Deliberate originals and original model UVs/animations are preserved. Live-game visual fit and extended stability testing remain ongoing; this experimental preview may contain bugs.

Weather preview: optional **Halloween fog** (about 30 m visibility), pale mist and custom fog colours, plus selectable Clear, Overcast, Rain and Thunderstorm weather. Open **Ctrl+Shift+F12 → Weather**. Both selectors default to Automatic. See [weather controls](docs/weather.md).

A Vulkan renderer for the **classic Anarchy Online live client**, based on [wannabuh's AO-VK](https://github.com/wannabuh/ao-vk). Adds modern lighting, shadows, procedural grass and an in-game options panel while preserving the official launcher and original renderer on disk.

## Download and install

Download **`ao-vk-live-0.6.0-preview.17-windows-x86-lite.zip`** or **`ao-vk-live-0.6.0-preview.17-windows-x86-full.zip`** from this repository's **Releases** page, together with `SHA256SUMS.txt`. Lite has the complete renderer, embedded terrain/shader resources and core ground maps. Full HD adds the optional world/character replacement library. Both editions use identical renderer/loader binaries. Players do not need a compiler, Python, or the Vulkan SDK.

1. Extract the whole ZIP to a folder. Close AO and its launcher.
2. Double-click **Install.cmd**. Select your AO installation folder, or use **Browse** to find it.
3. Click **Install**, then start AO with your normal launcher.
4. Select **Primary Display Driver** in AO's graphics configuration. This name refers to the Vulkan wrapper; your GPU does the rendering.

In-game options: **Ctrl+Shift+F12**. Tab changes category; Up/Down selects; Left/Right adjusts; Esc closes. Changes save automatically. Grass defaults to **on**, performance HUD to **off**. Select Shared Ground+ for native terrain with classic lighting and no added ripples; GroundOn is available separately. Existing saved settings are preserved on update.

For an existing AO-VK Live installation, use **Update.cmd** and select the same AO folder. To remove it, use **Uninstall.cmd** and select that folder. Uninstall preserves game files, AO preferences, AO-VK settings, logs and shader cache. The installer records any extra loader copy so uninstall can remove it too.

If Windows denies access to your game folder, close setup and right-click the .cmd file, then choose **Run as administrator**. Setup does not automatically request elevation.

## Supported client

- Windows classic live **18.8.62_EP1**, 32-bit; the installer verifies six original game DLLs before installing.
- A GPU/driver supporting Vulkan 1.3 and the renderer's required features. Gameplay testing so far used an RTX 2070 Super.
- **New Engine, TestLive, other client builds and Linux/Wine are not validated by this live package.**

Do not replace `randy31.dll`. The package adds `ddraw.dll`, `ao-vk.dll` and a small installation record. If an installed launcher addon relocates the game executable, setup also handles its detected AppData loader location. Settings remain in the real AO folder.

Another wrapper such as dgVoodoo cannot use the same `ddraw.dll` filename. Setup refuses to overwrite unknown files. If an official patch changes the supported client, uninstall AO-VK Live until that build has been checked.

## Source and releases

Source code is available from the repository/tag and GitHub's automatic source archives. No separate source ZIP is distributed. Optional generated artwork and baked replacement maps are kept outside Git history; prebuilt Full HD contains the player payload. Essential embedded renderer assets, catalogs and metadata remain in source. Build instructions are in [docs/build-windows.md](docs/build-windows.md). Technical compatibility notes are in [docs/live-client.md](docs/live-client.md).

Release maintainers: see [docs/publishing.md](docs/publishing.md) for packaging and uploading ready-built downloads. SHA-256 checksums accompany release archives.

## Credits and license

The renderer is derived from **wannabuh's AO-VK**. This project adds classic live loading and compatibility, standalone options, live terrain grass support, Halloween fog and weather controls, shutdown fixes, and a Windows installer. It is an unofficial community project and is not affiliated with Funcom or the upstream author.

The upstream [MIT license](LICENSE) and copyright are preserved. Third-party licenses and credits are included in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and `licenses/`. No Funcom game DLLs are distributed.
