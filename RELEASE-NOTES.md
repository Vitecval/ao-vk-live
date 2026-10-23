# AO-VK Live 0.2.0-preview.2 — Halloween, weather and shutdown

Vulkan graphics for the classic Anarchy Online live client, based on wannabuh's AO-VK.

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

Download the **windows-x86.zip**, extract it, close AO, and run **Install.cmd**. Select your AO folder and click Install. Launch normally and select **Primary Display Driver**. Use Update.cmd for an earlier AO-VK Live build, or Uninstall.cmd to remove it while preserving settings.

Targets the audited **classic live 18.8.62_EP1** client. New Engine, TestLive, other versions and Linux/Wine are not validated. The preview has headless GPU checks, isolated retail-loader checks, normal-exit tests, and a positive live-client smoke test on RTX 2070 Super. Broader hardware, outdoor-zone comparisons and long-session testing are still needed. This is an unsigned, unofficial community build. MIT license and upstream/third-party credits are included. No game files are bundled.
