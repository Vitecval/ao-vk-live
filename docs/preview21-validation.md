# AO-VK Live Preview 21 — Window Interiors & Expanded HD Textures

Preview 21 merges the frozen Preview 20 source, approved Parallax Windows V9,
expanded accepted HD artwork, and releasable renderer stability/cache fixes
from Living Rubi-Ka V5/V5.1. Living asset-dependent features are excluded. Exact
source identities, hashes, tests and package inventories accompany the release.

Lite is one ZIP: extract and run Install.cmd. Full HD is one small Setup ZIP:
extract and run Install.cmd. Built-in Windows downloading retrieves every pinned
Preview 21 material companion automatically, with progress, SHA256 verification,
retry/resume, verified-cache reuse and offline support. All packs are prepared
before transactional installation; preferences and the previous installation are
preserved on failure. See docs/full-hd-setup.md. Nothing is published yet.

Window interiors retain V9 furniture, curtains, adjoining-room views, layered
warm/cool lighting, distance detail and slow light schedules. Medium uses the
approved V9 Subtle setting; Maximum uses the exact Strong setting.

The owner subsequently required every included feature to be uploadable to
GitHub. The current Living V5/V5.1 people/clothing, drones, traffic, wildlife,
machinery and terrain resources are marked private and non-distributable.
These asset-dependent features are therefore omitted. The releasable MIT runtime
source and generic GPU animation/resource/cache fixes remain, with a compile-time
release gate that prevents loading old private experiment assets left in a client.
Living controls are hidden and ineffective; saved experimental preferences and
owner assets are preserved for rollback, not activated by this release.

Classic+ and Modern use Medium window interiors; Ultra uses Maximum (the exact
approved V9 Strong setting). Existing saved window values survive installation.
Original disables enhancements while retaining the saved setup. Texture style
and HD surface choices remain independent of Classic+, Modern and Ultra presets.
The standalone options panel puts important controls first; Enter expands related
customization. The previously requested people preset defaults are superseded by
the owner's instruction to omit content that cannot accompany the public release.

Accepted HD artwork retains original IDs, UVs, alpha and companion material maps.
Original native terrain, per-cell colors and deliberately retained materials stay
protected. Building Weathering, GroundOn, Shared Ground+, PBR, weather and foliage
remain. The abandoned Enhanced Trees, Surface Decals, Forest Floor and Ground
Contact experiments are not imported.

The integrated build has a separate bounded pipeline-cache namespace, checks
GPU/device/driver compatibility and creates its startup pipelines eagerly. No
hardware-specific cache is shipped. A source-built headless helper prepares the
owner GPU cache during installation before any AO game launch. Cold preparation
can take one or two minutes; a warm cache is reused. Cold and warm fixture results are distinct;
neither establishes launcher-to-world startup time.

Live appearance, prolonged gameplay, actual zoning and multi-client behavior
still require owner evaluation. Controlled renders are not gameplay captures.

No private Living assets, original game DLLs, world exports or hardware caches
are included in either player archive. Publishing remains disabled. The previous
Living and Parallax experiments and verified F: backup are preserved separately.

New worker maps are imported from sealed, hashed artwork handoffs. Legacy
procedural bakers are not a substitute for those approved runtime maps.
