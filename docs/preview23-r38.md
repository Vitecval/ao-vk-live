# Preview23 revision 38 - curved cloud horizon

Local follow-up to r37, prompted by the user's ProcSky screenshot showing flat cloud masses and vertical curtains near the horizon. F: was confirmed to contain the r37 DLL. Published downloads are unchanged; all C:\Funcom clients remain untouched.

## Cloud changes

The r37 shader clamped ray elevation to 0.06 and reused that artificial elevation for cloud density. Below that angle, it repeated essentially the same cloud column vertically. Its capped traversal also stopped before completing the volume near the horizon.

r38 intersects a curved elevated shell instead, traverses the complete shell thickness, and derives cloud height from radial distance. This avoids the low-elevation clamp and gives finite, continuous horizon intersections. The cloud density has a taller profile, more fine erosion and less continuous coverage. Stronger distance haze and a smooth horizon fade blend distant clouds into the atmosphere. Ground lighting, traffic, existing sky settings and legacy-cloud priority are unchanged.

## Verification

The x86 renderer, demo and settings checker built. `build/preview23/atmosphere-r38/checks/RESULTS.json` and `stability-checks/RESULTS.json` pass against frozen payload SHA256 `a020ace3d77abbf3849590f7b771f3406fe93fc23315defbc1715734bd4d52ce`.

Checks cover day/night clouds and traffic, legacy priority, login exclusion, stars, moving routes, ambient particles, original-sheet replacement and settings. All sampled foreground pixels stay identical. Horizontal sky-camera offsets of 3,000 and 100,000 change zero sky pixels. Consecutive static TAA frames differ by mean 0.148 channel units out of 255. A new level-camera horizon fixture confirms clouds change sky above the geometric horizon and zero pixels below it. Visual inspection shows fuller cloud shapes without r37's stretched horizon columns in this fixture. Retail appearance, motion and performance still need a playtest.

The 768x512 day-cloud fixture reports scene 3.22 ms, total 3.96 ms on RTX 2070 Super. These short fixture timings are not a retail FPS guarantee.

## Installation

`tools/Install-Preview23-r38.cmd` targets only `F:\AO_SInstall\Anarchy Online`, checks both validation records against the payload, and backs up the DLL, INI and receipt. The installer fixture result is recorded in `INSTALLER-CHECK.json` after a project-local Windows PowerShell 5.1 test. F: remains outside this session's writable sandbox roots; no retail installation was attempted here. Run the launcher locally with the F: client closed.

## Zoning image feasibility - not implemented

A custom loading image is feasible, but playfield-change detection alone may arrive too late to cover the beginning of loading. `proxy/ddraw/rvk_statels.cpp` already observes playfield pointers, IDs and generations; the renderer also tracks frames without 3D. A complete implementation needs a reliable start signal before the engine stops presenting, an overlay held during the transition, and a readiness condition for revealing the new scene. No overlay can appear during a presentation stall unless it was displayed beforehand or has its own presentation mechanism.

The image should accompany invalidation of TAA, upscaler, motion and other temporal history so that old-zone data does not smear into the first revealed frame. Existing camera-jump checks help but are not a complete explicit zoning reset. This turn implements cloud improvements only; no custom image or zoning overlay was added.
