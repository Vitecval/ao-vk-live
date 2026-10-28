# AO-VK Live Preview 16.1

An experimental Vulkan renderer for the **32-bit classic Anarchy Online live
client (18.8.62_EP1)**. This unofficial community preview may contain bugs.
New Engine and other client builds are not validated.

## Changes

- Shared Ground+ reuses GroundOn's native terrain geometry, detailed cliffs,
  exposed rock, boulders, pebbles and distant terrain/LOD path.
- Corrects whole-surface terrain darkening by preserving authored paving colors
  and applying classic AO's combined terrain material/lighting calculation.
  Global exposure and brightness are unchanged. GroundOn remains a separate mode.
- Disables added terrain, puddle and water ripple effects in Shared Ground+;
  ordinary water rendering remains.
- Corrects native terrain coverage that depended on the first retail chunk's
  transparency, which could make the ground disappear as the camera turned.
  Excludes Shadowlands Brink void-marker cells from native terrain, rocks and
  pebbles. Controlled regressions pass; corrected live-zone verification remains
  ongoing.
- Retains renderer resource-ownership and cleanup fixes. Controlled terrain,
  material loading, mode-switching and a 4,800-frame threaded sanitizer stress
  test passed. This does not establish extended live-game stability or performance.
- Full HD adds **621 texture replacements compared with Preview 15**. The current
  full-quality database inventory contains **4,380 baked replacement IDs**, with
  **35 fully covered recorded material families**. These are inventory counts;
  style selection and deliberate original-art exclusions affect what is shown.

Live-game visual verification, model fit and extended real-world stability testing
remain ongoing. Controlled comparisons are not a claim of parity in every zone.

## Downloads

| Edition | Contents |
| --- | --- |
| **Lite** | Complete renderer and loader, embedded shaders/cliff/shared-terrain resources, core ground material maps and setup utilities; excludes the optional Full HD world/character replacement library. |
| **Full HD** | The same renderer and core resources, plus the optional enhanced texture library. |

Download one Windows x86 ZIP and `SHA256SUMS.txt`. No compiler, Python or Vulkan
SDK is required to play. A compatible Vulkan 1.3 GPU/driver is required; testing
has used an RTX 2070 Super. No game DLLs are distributed.

## Install, update or uninstall

1. Extract the **entire ZIP**. Close AO and its launcher.
2. Run **Install.cmd**, select the supported AO installation folder, and install.
3. Start AO through the normal launcher and select **Primary Display Driver** in
   AO's graphics configuration.
4. Open **Ctrl+Shift+F12** for renderer settings. Select **Shared Ground+** to test
   native terrain with classic lighting. In Full HD, select the desired Materials
   texture style to use its optional replacements.

For an existing AO-VK Live installation, extract the desired edition and run
**Update.cmd** against the same AO folder. This also switches editions. Run
**Uninstall.cmd** from an extracted package to remove owned addon files.
Original AO files and saved preferences are preserved; modified/unrecognized
files cause setup to refuse the operation. Personal packs are preserved. Do not
replace `randy31.dll` manually. Existing wrappers sharing `ddraw.dll` must be
removed through their own supported workflow before installation.

If Windows denies access, close setup and run the relevant `.cmd` file as
administrator. Setup does not request elevation automatically.

Verify each downloaded ZIP with `Get-FileHash -Algorithm SHA256` and compare its
result with the matching entry in `SHA256SUMS.txt` before installation.

Based on [wannabuh's AO-VK](https://github.com/wannabuh/ao-vk). MIT license and
third-party attribution are included. Not affiliated with Funcom.
