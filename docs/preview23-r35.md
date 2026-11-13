# Preview23 revision 35 — Automatic ambience and ICC ground

Revision 35 builds on commit `288454b` (revision 34). It is a local test update, not a published release.

## Changes

- Ambient density defaults to 1.5, including Ultra. Automatic no longer discards most emitters through habitat thresholds or lowers its population relative to manual styles.
- Automatic chooses each emitter from terrain colour and region: green ground gets pollen/seeds, blue and violet ground gets spirits/spores, strongly red ground gets embers, and neutral paving gets dust. Snow, volcanic, ash, marsh and alien regions retain suitable profiles with some local variety. Jobe favours spirit motes with a smaller pollen population.
- Nighttime woodland fireflies occupy only a small portion of the pollen population; explicit Fireflies also stays sparse. Existing login and ordinary indoor exclusions remain. The emitter buffer and single particle draw are unchanged; population is bounded, though more particles cost more than the previous sparse effect.
- Native terrain records material use for the distinct tile owners in each visible patch. This closes a cache eviction gap where the renderer sampled HD child images without marking their owner as drawn. A bounded loader retry also handles an evicted owner sampled through native terrain without another game surface bind.
- HD Ground retains the ICC HD maps when ICC Tiles is off. With ICC Tiles on, AnarchyHD paving uses subdued aggregate, narrow joints and restrained relief/reflection. This is a procedural material refinement; no replacement artwork files were generated. Enhanced Original remains separately selectable.

## Capture evidence

The F: capture `rvk-frame-165651.txt`, written on 12 November at 16:56:52 Paraguay time, uses native Shared Ground+ geometry in ICC (655). The same session log records 181 cold-material retirements despite healthy main GPU memory. The picker reports `1010006:21` awaiting GPU upload while all three HD files are available. The installed renderer hash was still the revision 33 binary, so revision 34's heap-selection and reload fixes had not reached F:.

## Verification

The x86 renderer and headless demo built successfully. Settings checks verify Ultra, density 1.5, brightness 1 and HD Ground. Render-budget checks exercise regional and colour-based selection and equal Automatic/manual population scaling.

`tests/preview23_ambient_icc_tests.py` exercises nine regional Automatic profiles, login suppression, Off, three ICC artwork/toggle cases and native material residency. It explicitly enables native terrain and HDR, and requires `geometry=native-patches` in captures. Its results pin the tested DLL's SHA256.

The 240-frame residency test imposes a 1 MB material-cache cap. The unused 1 MB control set is evicted; the active ICC tile remains Ready and its last-sampled frame is 241. This verifies cache accounting, not retail camera-movement stability under every condition. The tile toggle checks compare actual rendered pixels, and Jobe emits spirits and pollen. No Vulkan validation errors or device loss were reported.

Results, images and logs: `build/preview23/atmosphere-r35/checks`. Retail visibility, subjective paving appearance and frame rate still require a playtest.

## Installation state

The attempted F: update was blocked by filesystem permissions while creating the staged DLL. No installed file was replaced. The renderer, INI and receipt were compared with the local backup afterward and all three remain byte-identical. Evidence: `build/preview23/atmosphere-r35/INSTALL-BLOCKED.json`.

Run `tools/Install-Preview23-r35.cmd` locally after closing the F: client. It checks the tested renderer hash, backs up the current DLL/settings/receipt, installs the renderer, updates its receipt and sets Automatic, density 1.5, brightness 1 and HD Ground on. Other saved settings and artwork are retained. A successful run writes `INSTALL-RESULTS.json`; that file does not exist at handoff.
