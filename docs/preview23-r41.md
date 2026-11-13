# Preview23 revision 41 - cinematic screenshot artwork and loading phrases

The failed `Create Loadscreen Art` chat ended with the request to enhance the original screenshots in the Nascence bridge style while retaining their layouts. A later local r40 set and its renderer changes were recoverable, but F: still contained r39 when inspected. The recovered r40 artwork was comparatively flat; r41 replaces it with the richer lighting and material finish in the two style examples supplied in this chat.

## Artwork

All 14 final images are in `artwork/loading` and `artwork/loading-r41`. They were created using the built-in image generation tool with the matching original screenshot as the edit target and the user's two attached examples as style references. The original framing and major landmarks remain recognizable. This is generative enhancement, not pixel-identical retouching: surface details and small shapes can differ. Slight armorer-room alterations were explicitly accepted by the user.

Async2 is omitted from scenery views and retained in the Pumpkin-Head fight. The two ship references remain excluded from the loading rotation. Branding is part of the artwork; phrases are rendered separately. The earlier images are retained under `build/preview23/atmosphere-r41/previous-loading`.

`build/preview23/atmosphere-r41/ARTWORK-PROVENANCE.json` maps every final image to its original screenshot and records both hashes. Original screenshots and the supplied style references are preserved beside it. `PROMPTS.json` and `GENERATION-RECORD.json` record the prompt set and targeted corrections. No CLI/API image-generation fallback was used.

## Loading phrases

All 50 supplied phrases are compiled verbatim, without their list numbers, in `rvk/zoning_phrases.h`. Each loading transition chooses one phrase from a shuffled deck. Every phrase is used before reshuffling; adjacent loads cannot repeat the same phrase, including across deck boundaries. The phrase stays fixed throughout the load and uses an independent order from the artwork rotation.

Unicode text is painted with `DrawTextW` in a measured, wrapping footer. The artwork is aspect-fitted into the remaining space, keeping its AO logo visible. Image and text share the existing 180 ms fade-in and 320 ms fade-out. The zoning toggle, four-fresh-world-frame reveal condition, no-focus behavior, background suppression and temporal-history resets remain in place. Only one artwork image is decoded at a time.

The r40 Shared Ground+ density of 0.15, optional Skip HUGE shadows control and angular sky-traffic hulls are included unchanged.

## Verification and installation

The x86 renderer builds successfully. The phrase rotation check covers 100 seeds and ten complete decks per seed. The actual GDI painter renders all 50 phrases at 640x480, 1280x720, 1920x1080, 2560x1080 and 720x1280. The window fixture checks all 14 final images, single-image decoding, focus suppression and clean shutdown. All final images pass the existing exposure checks. Settings and saved selections pass in an isolated folder, including process restart. The actual Windows PowerShell 5.1 installer passes against a project-local dummy installation and verifies the complete artwork payload.

Evidence, exact payload hashes and representative phrase screenshots are under `build/preview23/atmosphere-r41`. Prior sky/pebble/shadow GPU regression results are identified explicitly as inherited r40 evidence; those GPU fixtures were not rerun for this zoning-only code change. Retail loading timing and appearance still require an in-game check.

`tools/Install-Preview23-r41.cmd` targets only `F:\AO_SInstall\Anarchy Online`. It checks the frozen payload and validation records, requires that client and launcher to be closed, backs up replaced files, verifies receipt hashes and supports rollback on failure. See `INSTALL-RESULTS.json` for the actual installation state; a fixture result alone does not establish retail installation.

Installed and hash-verified in `F:\AO_SInstall\Anarchy Online` on 2026-11-13. The receipt matches the renderer and all 14 artwork files. The installer retained a backup and did not launch the game. The six running `C:\Funcom` clients were outside the target installation and were not changed.
