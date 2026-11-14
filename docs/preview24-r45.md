# Preview 24 local revision 45: rock surfaces and flight material residency

This is a local development trial after the published Preview 24. Published release ZIPs, tags and checksums are unchanged. Evidence is in `build/preview24/rocks-r45`; installation is recorded separately from renderer validation.

## Flight log

The latest F: renderer log was last written on 2026-11-13 at 21:07:46 local time. It contains playfield 4310, 122 GPU-pressure eviction cycles, 2,392 retired cold HD sets and 5,576.1 MiB retired in total. One representative driver's main-heap reading is 4,977 MiB used of a 5,691 MiB budget, while VMA has 2,561 MiB allocated inside 4,917 MiB of blocks. Discarding individual images need not release those blocks, so repeatedly trimming above 85% can churn otherwise useful residency. Seven recorded heap samples exceeded the old threshold; none exceeded 92%.

The trim trigger is now 92%, with an 88% retirement target. Largest-local-heap selection, active/recent-map protection, 180-frame cold eligibility, bounded retirement batches and safe GPU retirement are retained. This reduces a demonstrated source of reloads when flying away and returning. It does not establish that every instance of the reported flicker has the same cause. Building-family ready/missing lines are diagnostics, not a global draw gate. Large suspended-focus frame times must not be treated as flight stalls. The log's account name is not recorded; this is the latest F: session, rather than an independently identified Async1 account log.

## Enhanced Rocks

The Materials section has an Enhanced Rocks strength slider from 0 to 1, enabled by default. Original mode disables its effective value while retaining the user's preference. Identification uses 143 natural-rock texture identities from the existing world catalogue, including 1010004:5955, :9863, :205836, :214375, :205675, :205838 and :202640. Aliased world-texture tables are normalized; character texture tables are excluded. Additional natural rocks with unidentified texture names may need catalogue additions after a playtest.

Original rock meshes share the already embedded CC0 photographic cliff albedo, normal and ORM set. It uses 16 MiB including filtered mip chains, once for the renderer, with no per-rock replacement images, downloads or geometry. This also works without external Full texture packs. Source biome colours and alpha are preserved; green moss suppresses most added detail. Dielectric roughness and normal relief improve definition without making rock metallic. Explicit world-space triplanar gradients, footprint filtering and a 50–140 m detail fade limit distant shimmer and sampling cost. Terrain and object strengths remain separate; object-only resource loading cannot activate terrain cliff texturing or its pebble pass.

## Verification and local use

The actual Vulkan tests exercise visible detail, tile-rock coverage, character and unknown-surface exclusions, camera movement, and both shared and native terrain with cliff detail disabled. The camera comparison permits one output-channel quantization step; it does not allow lost or popping material regions. Shadow tests cover mixed world clipping ranges, moving caster poses and exact static-depth reuse. Sky/ambient fixtures check the expanded frame-light buffer layout. CPU checks cover all seven requested identities, character-table rejection, observed memory-pressure boundaries and recent-map protection. Settings checks cover schema, effective Original mode, restoration and process restart.

The synthetic rock fixture measured approximately 0.12 ms additional GPU time at 768×512, with about 63,000 affected pixels. This is not a retail FPS estimate; larger rock coverage and higher resolutions cost more. See the final report for the exact build and measurements. Retail flying and rock appearance still need a fresh in-game comparison.

`tools/Install-Preview24-r45.cmd` installs only the verified candidate renderer and its ownership receipt into the closed F: client. It checks ownership, backs up both files, stages and verifies copies, and uses the existing recoverable transaction. Settings and prepared texture files are preserved. `tools/Revert-Preview24-r45.cmd` restores the exact previous renderer and receipt only while the current installation still matches this trial. Installation, repeat installation, changed-file refusal and exact revert are tested in an isolated fixture. The GPU's local pipeline cache can be prepared before installation to avoid first-start compilation. Neither installer launches the game.
