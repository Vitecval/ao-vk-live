# AO-VK Live v0.6.0-preview.17

An experimental Vulkan renderer for the **32-bit classic Anarchy Online live
client (18.8.62_EP1)**. This unofficial community preview may contain bugs.
New Engine and other client builds are not validated.

## Preview 17 terrain corrections

Preview 17 brings the completed terrain corrections into the next numbered
preview. Some were already included in the final Preview 16.1 preparation;
upgrading from that final package carries them forward without new artwork.

- Shared Ground+ uses GroundOn's native terrain geometry, cliffs, exposed rock,
  boulders, pebbles and distant terrain, with classic combined-material lighting.
- Preserves the classic ADD(lightmap, diffuse), then MODULATE(base) calculation,
  including albedo multiplication and clamp order. Authored cobbled paving and
  rocky-road colors are retained. Global brightness and exposure are unchanged.
- Corrects transparent first-chunk handling in both color and cutout depth paths,
  so camera chunk order cannot hide the native terrain.
- Excludes identified void cells from native terrain, rock and pebble geometry.
- Keeps additional terrain, puddle and water ripples disabled in Shared Ground+;
  ordinary water remains. GroundOn retains its separate native lighting and
  standard Shared Ground retains the original terrain submissions.

## Setup correction

Install, Update and Uninstall now select 32-bit Windows PowerShell to inspect the
32-bit client reliably. Direct script launches from 64-bit Windows PowerShell
forward automatically. This fixes false "game is running" refusals caused by
incomplete module enumeration when only another AO installation is running.
Checks still block a running target installation or a shared loader being changed.

## Improvements inherited from Preview 16.1

- Full HD retains all **621 additional replacement identities compared with
  Preview 15**, and all existing material packs. No artwork was regenerated.
- The retained library contains **4,389 unique repaint identities**, including
  **4,380** in the full-quality database inventory, plus **112 material-only
  identities**, in **13,399 runtime map files**. There are **35 fully inventoried
  recorded material families** and three partial families. These counts describe
  inventory; style selection and intentional original-art policies affect use.
- Carries forward renderer ownership, threaded cleanup, loader compatibility and
  transactional setup fixes covered by the release validation report.

Controlled regressions cover day/night and HDR/LDR terrain lighting, transparent
chunks, void cells, mode switching, material loading and threaded rendering.
See the accompanying release REPORT.md and validation.json for performed tests
and limitations. Live-zone visual parity, model fit, distant LOD transitions and
extended gameplay stability still require in-game review.

## Downloads

| Edition | Contents |
| --- | --- |
| **Lite** | Complete renderer and loader, embedded shaders and terrain resources, 44 core ground maps and installation utilities; excludes the optional Full HD texture library. |
| **Full HD** | Identical renderer, loader and core resources, plus the retained world/character/material texture library and its provenance. |

Download one Windows x86 ZIP and SHA256SUMS.txt. No compiler, Python or Vulkan
SDK is required to play. A compatible Vulkan 1.3 GPU/driver is required.
No game DLLs are distributed.

## Installation, update and uninstall

1. Verify the ZIP with `Get-FileHash -Algorithm SHA256` against SHA256SUMS.txt.
2. Extract the **entire ZIP**. Close AO and its launcher before setup.
3. Run **Install.cmd**, select the supported AO installation folder, then install.
4. Start AO normally and select **Primary Display Driver** in its graphics setup.
5. Open **Ctrl+Shift+F12** for renderer settings. Select Shared Ground+ to use
   native terrain with classic lighting. In Full HD, choose the desired Materials
   texture style to enable its optional replacements.

To update, extract the desired edition and run **Update.cmd** against the same
AO folder. This also switches editions. Setup checks running processes, verifies
owned files, stages the transaction and rolls back on a copy/commit failure.
Existing game files, saved graphics preferences and personal packs are preserved.
Modified or unrecognized owned files cause setup to refuse the operation.

Run **Uninstall.cmd** against the same folder to remove owned addon files and
the installation receipt. Game files, preferences, personal packs and renderer
settings/cache/logs are retained. Do not manually replace randy31.dll. Existing
wrappers occupying ddraw.dll require their own supported removal workflow first.
If Windows denies access, close setup and run the relevant .cmd as administrator;
setup does not request elevation automatically.

Based on [wannabuh's AO-VK](https://github.com/wannabuh/ao-vk). MIT license and
third-party attribution are included. Not affiliated with Funcom.
