# AO-VK Live 0.3.0-preview.1 — Optional modern ICC materials

Vulkan graphics for the classic Anarchy Online live client, based on wannabuh's AO-VK.

- New optional ICC material preview: six architectural textures with higher-resolution albedo, subtle normal maps and roughness/metallic/occlusion maps. This is a small showcase, not a whole-game remaster.
- Saved toggle: Ctrl+Shift+F12 → Materials → Modern texture pack (ICC preview). Off by default; updates loaded materials when toggled, preserves other overrides and honors the master Enhancements switch.
- Installer verifies, updates and removes the pack's owned files transactionally while preserving settings and other packs. Original game resource databases remain unchanged.
- Includes source artwork, generation prompts and a reproducible material-map bake in the source download. The player ZIP contains the ready-to-use maps.

- Preserves the original renderer on disk and works with the normal launcher.
- Modern lighting, shadows, procedural grass and other AO-VK enhancements.
- Standalone options on Ctrl+Shift+F12, with automatically saved settings.
- Weather category: select Automatic, Clear, Overcast, Rain or Thunderstorm and adjust rain/storm strength.
- Optional Halloween fog inspired by AO's historic black fog, with 30 m visibility by default. Also includes pale Mist and Custom RGB fog, clear distance, nearby dimming, drift and an indoor opt-in. Interface and held labels stay readable.
- Both atmosphere selectors default to Automatic. Fog works independently of the game's seasonal fog flag and the experimental terrain replacement.
- Grass supported on live's single-pass terrain; grass on and performance HUD off by default.
- Experimental terrain replacement disabled to avoid missing ground on live.
- Folder-selecting Windows installer, update and uninstall; compatible with the detected AppData executable-copy layout.
- Shutdown repair: stop the render worker and frame-generation presentation when the game window closes, with a queued stop command that avoids a lost wake-up. Normal-exit harness checks passed with frame generation on/off and both renderer paths; confirmation in a real game session is still needed.
- Minimize/restore repair: pause world rendering while minimized, refresh presentation when restored, and retry deferred zero-area swapchains. Six harness configurations completed 24 restore cycles, including frame generation, direct/threaded rendering and minimized startup, with rendered images and clean exit.
- Report graphics acquire/submit/present errors and stop recording frames after device loss. This does not implement recovery from a full driver reset; restart the affected client if that occurs. Live confirmation of the minimize fix is still needed.

Download the **windows-x86.zip**, extract it, close AO, and run **Install.cmd**. Select your AO folder and click Install. Launch normally and select **Primary Display Driver**. Use Update.cmd for an earlier AO-VK Live build, or Uninstall.cmd to remove it while preserving settings.

Targets the audited **classic live 18.8.62_EP1** client. New Engine, TestLive, other versions and Linux/Wine are not validated. The preview has headless GPU checks, isolated retail-loader checks, normal-exit tests, and a positive live-client smoke test on RTX 2070 Super. Broader hardware, outdoor-zone comparisons and long-session testing are still needed. This is an unsigned, unofficial community build. MIT license and upstream/third-party credits are included. No game files are bundled.
