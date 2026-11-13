# AO-VK Live Preview 23

Welcome back to Rubi-Ka! Preview 23 brings a much larger HD artwork library, clearer daylight, fuller distant views and more life in the world.

## What's new

- **A bigger HD world:** more detailed buildings, signs, interiors and ground, with sharper texture loading and softer material highlights. The library now covers 14,317 of 14,753 indexed surface IDs (about 97%). Some original graphics, masks and unfinished artwork remain.
- **Sunshine with depth:** Enhanced Daylight improves outdoor lighting while keeping shadows, colour and the normal day/night cycle. You can also switch instantly to day or night in the graphics menu.
- **Fuller horizons:** optional Far Scenery brings distant rocks and buildings into view. Ultra starts at 3x range.
- **Smoother terrain:** improved terrain transitions, corrected cliff boundaries, optional HD ground and adjustable surface detail. SharedGround+ keeps the ICC tile pattern crisp.
- **Atmosphere that fits the area:** optional light-source glow, softer skies, ship navigation lights and Automatic ambient life. Dust, pollen, fireflies, spores and other motes vary with the area and time of day, including regional Shadowlands colours and movement.
- **Grass on more surfaces:** reviewed grassy floating islands can grow grass as well as native terrain.
- **Performance and stability work:** enhanced smoke now respects its performance budget more closely. Unused HD material maps can be released when graphics memory is tight and loaded again when needed. Constant material controls use less memory without reducing artwork resolution.
- **A clearer installer:** setup shows what it is doing, checks the matching HD files, reuses verified downloads and recovers interrupted updates.

## Which download do I need?

**Lite** is the smaller download: the Vulkan renderer and its built-in visual improvements, using mostly the game's original artwork.

**Full HD** adds the complete current replacement library. Download the **Full HD Setup ZIP**; setup downloads and verifies its matching material packs automatically. You do not need to download the source-code archive or extract the material ZIPs yourself.

For an offline Full HD install, place all matching `full-materials-*.zip` files beside the extracted setup folder before starting setup. Keep files from the same release together.

## Installing for the first time

1. Use the supported **classic Anarchy Online client**. The separate New Engine client is not supported.
2. Close the game and launcher. Extract the Lite ZIP or Full HD Setup ZIP into a normal folder.
3. Open **Install.cmd** and select the game folder containing `Anarchy.exe` and `AnarchyOnline.exe`.
4. Choose **Install**. Full HD may take longer on its first run while it downloads and checks the artwork; the progress text shows the current task.
5. Start AO normally and select **Primary Display Driver**. In game, press **Ctrl+Shift+F12** for AO-VK settings. Try Ultra, then adjust to taste; modified presets display as Custom.

Already using AO-VK? Close the game, extract the new setup and open **Update.cmd**, pointing it to the same game folder. Keep the extracted setup if you want its **Uninstall.cmd** later.

## Good to know

Frame generation remains **experimental and off by default**. It can reduce real rendering FPS on some hardware even when the displayed generated-frame count is higher. Start with it off; FSR upscaling can be adjusted separately.

Ultra enables HD replacements, AnarchyHD world artwork, Improved ICC Tiles, Enhanced Daylight, Soft Skies, Area Lighting, Light Source Glow, Automatic Ambient Life and Water Look 1. SharedGround+ surface detail starts at 0.6 and ICC glossiness at 0.7. Clouds are off by default. Existing player preferences are preserved during updates.

Living assets are not included; they remain a separate add-on project. This is a preview release: please report visual issues with **Ctrl+Shift+F9**, and include your graphics card and settings when reporting performance problems.
