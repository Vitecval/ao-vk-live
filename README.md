# AO-VK Live

Optional **modern ICC material preview**: six architectural surfaces with higher-resolution textures, normal maps and material response. Open **Ctrl+Shift+F12 → Materials → Modern texture pack (ICC preview)**. Off by default. See [modern materials](docs/modern-materials.md).

Weather preview: optional **Halloween fog** (about 30 m visibility), pale mist and custom fog colours, plus selectable Clear, Overcast, Rain and Thunderstorm weather. Open **Ctrl+Shift+F12 → Weather**. Both selectors default to Automatic. See [weather controls](docs/weather.md).

A Vulkan renderer for the **classic Anarchy Online live client**, based on [wannabuh's AO-VK](https://github.com/wannabuh/ao-vk). Adds modern lighting, shadows, procedural grass and an in-game options panel while preserving the official launcher and original renderer on disk.

## Download and install

Download **`ao-vk-live-0.3.0-preview.1-windows-x86.zip`** from this repository's **Releases** page. Players do not need a compiler, Python, or the Vulkan SDK.

1. Extract the whole ZIP to a folder. Close AO and its launcher.
2. Double-click **Install.cmd**. Select your AO installation folder, or use **Browse** to find it.
3. Click **Install**, then start AO with your normal launcher.
4. Select **Primary Display Driver** in AO's graphics configuration. This name refers to the Vulkan wrapper; your GPU does the rendering.

In-game options: **Ctrl+Shift+F12**. Tab changes category; Up/Down selects; Left/Right adjusts; Esc closes. Changes save automatically. Grass defaults to **on**, performance HUD to **off**. The experimental terrain replacement stays **off** on live; grass works over the original ground.

For an existing AO-VK Live installation, use **Update.cmd** and select the same AO folder. To remove it, use **Uninstall.cmd** and select that folder. Uninstall preserves game files, AO preferences, AO-VK settings, logs and shader cache. The installer records any extra loader copy so uninstall can remove it too.

If Windows denies access to your game folder, close setup and right-click the .cmd file, then choose **Run as administrator**. Setup does not automatically request elevation.

## Supported client

- Windows classic live **18.8.62_EP1**, 32-bit; the installer verifies six original game DLLs before installing.
- A GPU/driver supporting Vulkan 1.3 and the renderer's required features. Gameplay testing so far used an RTX 2070 Super.
- **New Engine, TestLive, other client builds and Linux/Wine are not validated by this live package.**

Do not replace `randy31.dll`. The package adds `ddraw.dll`, `ao-vk.dll` and a small installation record. If an installed launcher addon relocates the game executable, setup also handles its detected AppData loader location. Settings remain in the real AO folder.

Another wrapper such as dgVoodoo cannot use the same `ddraw.dll` filename. Setup refuses to overwrite unknown files. If an official patch changes the supported client, uninstall AO-VK Live until that build has been checked.

## Source and releases

The source ZIP contains this repository's source and bundled dependencies, with no game files, personal settings or logs. Build instructions are in [docs/build-windows.md](docs/build-windows.md). Technical compatibility notes are in [docs/live-client.md](docs/live-client.md).

Release maintainers: see [docs/publishing.md](docs/publishing.md) for packaging and uploading ready-built downloads. SHA-256 checksums accompany release archives.

## Credits and license

The renderer is derived from **wannabuh's AO-VK**. This project adds classic live loading and compatibility, standalone options, live terrain grass support, Halloween fog and weather controls, shutdown fixes, and a Windows installer. It is an unofficial community project and is not affiliated with Funcom or the upstream author.

The upstream [MIT license](LICENSE) and copyright are preserved. Third-party licenses and credits are included in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md) and `licenses/`. No Funcom game DLLs are distributed.
