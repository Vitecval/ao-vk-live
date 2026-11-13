# Preview23 revision 36 — sky detail and ambient visibility

Local test update following revision 35. This does not change the published downloads.

## Requested defaults

- ICC Tiles defaults to Off, including Ultra. HD Ground retains the detailed ICC paving when that separate option is off.
- Ambient density defaults to 2; brightness defaults to 1.5. The brightness scale is unchanged.
- The explicit Fireflies rate is multiplied by 0.165 instead of 0.22. Density 2 therefore has the expected population previously obtained at 1.5. Automatic woodland night insects likewise drop from 10% to 7.5% of their population. Other ambient kinds retain the higher density.
- Dust radius rises from 0.030 to 0.036; snow and ash from 0.018 to 0.024. These three kinds also receive modest particle-only scattering and opacity increases. World lighting is unchanged.

## New sky controls

`RVK_SkyClouds` — **Enhanced sky clouds (no ground dimming)**. A separate lightweight procedural layer in the existing sky pass adds drifting cloud masses, finer edges, high wisps and blue-lit night shading. It does not enable the legacy cloud renderer, cast cloud shadows, alter sunlight, or feed the scene brightness controller. The legacy renderer takes priority when CloudsOn is enabled. This layer is an artistic sky treatment, not a new volumetric weather simulation.

`RVK_SkyTraffic` — **Distant sky traffic: Off / Light / Busy**. Eight or twelve deterministic routes carry distant procedural craft of different sizes, including larger hulls with side sections, lit bridge details, engine points and blinking navigation lights. Perspective comes from analytic 3D hull intersections in the sky shader. This adds original cosmetic craft alongside existing game ships and their separately enhanced navigation lights; it does not import more Lost Eden assets. Routes move continuously and close across the two-hour shader clock wrap.

Both controls operate in the procedural outdoor sky and exclude login/creation and detected interiors. Ultra enables the new cloud layer and Light traffic, while retaining legacy CloudsOn Off. No additional render pass, material library, shadow casters, or dynamic lights are allocated for the new sky features. Shader work increases; these features are not free.

The user's reference establishes a direction for a richer sky, not a promise to reproduce its AI artwork or overall scene quality. Retail clouds, ship scale and visual density remain subjective playtest items.

## Checks

The x86 renderer, demo and settings checks built. `preview23_sky_life_tests.py` passed day/night clouds and traffic separately and together, legacy-cloud priority, login suppression, moving routes, star compatibility, Off, dust, snow, ash and fireflies. All sampled foreground pixels in the bottom 112 rows of the 768×512 controlled sky fixture remain identical with either enhancement enabled. The sky pixels change. This is a controlled fixture result, not a claim about every scene or retail frame rate.

`preview23_ambient_icc_tests.py` also passed regional Automatic profiles, login/Off suppression, ICC tile toggle/HD retention and forced GPU-cache pressure. At frame 240 the visible HD tile remains Ready and sampled while the unused control material is evicted. Settings checks verify ICC Tiles Off, density 2, brightness 1.5, new Ultra sky controls, and legacy CloudsOn Off.

The actual installer passed under Windows PowerShell 5.1 against a project-local dummy client, including real DLL replacement, receipt update and requested INI values. Other settings and receipt data survived. No F: files were changed by that fixture. Rendering logs report no Vulkan errors or device loss; these checks do not establish exhaustive validation-layer coverage.

Evidence is under `build/preview23/atmosphere-r36/checks`, `terrain-checks` and `INSTALLER-CHECK.json`. Both render result files pin DLL SHA256 `7f2a36f04a662201e6697b24974d61cd8e18ebe972952fe07da17040eabf7b04`.

## Install state

Revision 36 is installed in `F:\AO_SInstall\Anarchy Online`. `INSTALL-RESULTS.json` records a successful installation at `2026-11-13T01:49:46.9317079Z` (November 12, 22:49 Paraguay time). The earlier `INSTALL-BLOCKED.json` is historical and does not describe the current installation.

A subsequent read-only verification confirmed the installed DLL matches SHA256 `7f2a36f04a662201e6697b24974d61cd8e18ebe972952fe07da17040eabf7b04`, both renderer test records and the installer fixture passed against that hash, the installation receipt matches, and all requested INI values and migration flags are present. The three-file backup exists under `build/preview23/atmosphere-r36/install-backup-dbf13ca9fe4e4706b738da99e1839225`; its previous DLL matches the recorded revision 35 hash. No reinstall was needed. The game was not launched, and all `C:\Funcom` clients were left untouched.

For a future reinstall, run `tools/Install-Preview23-r36.cmd` locally with the F: client closed. It installs the frozen, hash-verified DLL from `build/preview23/atmosphere-r36/payload`, backs up the current DLL/receipt/settings, sets the requested ambient defaults and ICC Tiles Off, enables the new sky controls, and disables legacy CloudsOn. Existing artwork and other saved settings are retained. It logs to `install-last.log` and writes `INSTALL-RESULTS.json` only after installation verification succeeds.
