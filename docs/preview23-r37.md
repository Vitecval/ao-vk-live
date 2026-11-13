# Preview23 revision 37 - sky stability and cloud depth

Local follow-up to r36. Published downloads are unchanged. Target client remains `F:\AO_SInstall\Anarchy Online`; leave all `C:\Funcom` clients alone.

## Retail evidence

The user's November 12 captures `rvk-frame-225513`, `225514`, `225516` and `225519` in the F: client confirm clouds 1 and traffic 1 in Mort (playfield 560). They also show the original `VisualSingleCloud_t` sheets rendering over the enhanced sky. The r36 enhancement was enabled, but its soft procedural overlay coexisted with the original sheets.

## Changes

- Sky rays now use a rotation-only view and the near plane, avoiding large-coordinate cancellation and division at the far plane. The sky projection follows the same TAA jitter as scene geometry. These address plausible sources of shaking; retail camera movement still needs a playtest.
- Enhanced clouds use a 48-sample elevated density volume with extinction, directional internal shading, rounded density variation, warm low-sun lighting, blue night lighting and distance haze. Samples are deterministic without screen-space dithering. This is an original approximation toward fuller modern clouds, not Enshrouded's renderer or assets.
- When enhanced sky clouds are active outdoors, their replacement suppresses original cloud sheets. Disabling the enhancement restores the original sheets. Legacy CloudsOn retains priority. Login, interiors and external draws are excluded.
- These clouds remain sky-only: no ground shadows, sunlight changes or exposure-controller inputs. They add shader cost; retail performance is not established by fixtures.

## Verification and installation

The x86 renderer, demo and settings checker built successfully. Both `checks/RESULTS.json` and `stability-checks/RESULTS.json` pass against frozen DLL SHA256 `f3b4a919bfa149afb78af8d7e2edc1fd26111d79c4acccf9ec738678d9f13e8b` under `build/preview23/atmosphere-r37`.

Day/night clouds and traffic, legacy priority, login suppression, moving traffic, stars and ambient regressions pass. All sampled foreground pixels in the bottom 112 fixture rows remain unchanged. Horizontal sky-camera offsets of 3,000 and 100,000 produce zero changed sky pixels. The retail-sheet fixture suppresses all ten original sheets. Consecutive static TAA frames differ by a mean 0.156 channel units out of 255. These controlled checks do not establish retail camera stability or exhaustive Vulkan validation coverage.

Visual inspection rejected the first sampling pattern; the final deterministic 48-step version removes that pattern. The final 768x512 day-cloud fixture averaged scene 3.04 ms and total 3.62 ms on RTX 2070 Super, versus r36's recorded scene 1.63 ms and total 1.98 ms. These short runs are indicative, not a controlled retail FPS comparison.

`INSTALLER-CHECK.json` records the actual installer passing under Windows PowerShell 5.1 against a project-local dummy client, including DLL replacement and settings/receipt preservation. `tools/Install-Preview23-r37.cmd` targets only F:, validates both render and stability results against the frozen payload, and backs up the DLL, settings and receipt before replacing them.

**Not installed into F: in this session:** F: is outside the writable sandbox roots and sandbox approvals are disabled. Read-only verification still finds the r36 hash `7f2a36f04a662201e6697b24974d61cd8e18ebe972952fe07da17040eabf7b04`. Run the local r37 launcher with the F: client closed. No C:\Funcom client was modified, and no retail client was launched or stopped.
