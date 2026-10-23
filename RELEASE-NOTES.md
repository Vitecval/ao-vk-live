# AO-VK Live 0.1.0 — classic live preview

Vulkan graphics for the classic Anarchy Online live client, based on wannabuh's AO-VK.

- Preserves the original renderer on disk and works with the normal launcher.
- Modern lighting, shadows, procedural grass and other AO-VK enhancements.
- Standalone options on Ctrl+Shift+F12, with automatically saved settings.
- Grass supported on live's single-pass terrain; grass on and performance HUD off by default.
- Experimental terrain replacement disabled to avoid missing ground on live.
- Folder-selecting Windows installer, update and uninstall; compatible with the detected AppData executable-copy layout.

Download the **windows-x86.zip**, extract it, close AO, and run **Install.cmd**. Select your AO folder and click Install. Launch normally and select **Primary Display Driver**. Use Update.cmd for an earlier AO-VK Live build, or Uninstall.cmd to remove it while preserving settings.

Targets the audited **classic live 18.8.62_EP1** client. New Engine, TestLive, other versions and Linux/Wine are not validated. Gameplay tested on RTX 2070 Super; additional hardware and long-session testing are welcome. This is an unsigned, unofficial community build. MIT license and upstream/third-party credits are included. No game files are bundled.
