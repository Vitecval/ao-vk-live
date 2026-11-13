# AO-VK Live Preview 20 â€” Expanded HD Textures & Building Weathering

Experimental preview for the classic Anarchy Online live client, Windows x86.

Preview 20 expands the optional Full HD library with **866 verified original texture-ID bindings** from completed worker batches, including seven exact-original companion aliases. Architecture, industrial, natural, character and equipment artwork retain their original UV layout and alpha. All approved Preview 19 runtime maps are preserved. Full HD now has **7,318 unique repaint identities** and **22,903 runtime maps**.

**Building Weathering** is independently configurable under **Ctrl+Shift+F12 â†’ Materials**. Off is the default. Low adds light aging; Medium adds visible localized grime and runoff; High adds stronger material-specific oxidation and coating wear. The reviewed list covers 92 concrete, steel and painted architectural materials. Maintained modern architecture receives less aging. Unsupported materials retain their original appearance. Lite also supports weathering; two masked emissive ICC panels require the Full HD emission maps.

**GroundOn and Shared Ground+** retain the production terrain geometry and fixes. Shared Ground+ preserves AO's original per-cell terrain colors, UVs and classic lighting. Existing grass, foliage, weather, water and shadows remain available.

## Choose an edition

- **Lite:** complete renderer, loader, embedded resources, core ground maps and Building Weathering.
- **Full HD:** the same renderer and loader, plus the optional replacement library and its normal/ORM companions.

## Install or update

1. Download one edition and verify its ZIP against `SHA256SUMS.txt`.
2. Extract the entire ZIP. Close clients and launchers that use the selected installation.
3. Run **Install.cmd**, or **Update.cmd** for an existing AO-VK installation, and select the game folder. Keep a verified preservation backup before replacing an experimental build.
4. Start AO normally. Use **Primary Display Driver** in the classic client's graphics configuration.
5. Open **Ctrl+Shift+F12**. Choose **World texture style â†’ AnarchyHD** for Full HD artwork and enable **Character & equipment materials** for that library. For Shared Ground+, choose it under Materials and leave the separate GroundOn override off.
6. Compare Building Weathering Off/Low/Medium/High from the same camera and lighting. No game client is started or closed by this release-preparation task.

The transactional installer verifies its payload and preserves original game DLLs, settings and personal packs. It refuses unknown wrappers. Keep `randy31.dll` intact. **Uninstall.cmd** removes owned addon files while preserving game and personal files.

## Known limitations

This is an experimental preview. Coverage is an inventory of database texture identities, not the percentage of the visible world upgraded. Some faithful runtime exports are below 2K. Only reviewed materials weather; signs, glass, terrain, characters and protected emissive regions are excluded. Offline matched Vulkan comparisons and GPU timings do not certify every live city, moving model, lighting condition, extended session or gameplay FPS. Final in-game evaluation remains pending. Held and conflicting families keep their previous approved or original artwork.

[Project page](https://asyncstuff.com/plugins/ao-vk/) Â· [Bug reports](https://github.com/Vitecval/ao-vk-live/issues)
