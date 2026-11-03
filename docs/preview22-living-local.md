# Preview 22 local Living integration candidate

The isolated source starts at published Preview 21 Setup 3 commit
`85862ddf596646295e2bea9bd3045e879f835d60`, including its installer progress,
optional memory-support and artwork-license fixes. Its validated renderer origin
is `1887c4704b1293d91d12458130306cb9f40f908e`. Preview 21 already incorporated the
V5/V5.1 engine from `9b8a42fcb2259973387ba556d7810506c3c40aad`, including the
animation lifetime, suspension, sky-camera, city expansion and wardrobe fixes.
This change preserves that engine and changes how its resources are prepared
and selected. It does not replace the Preview 21 renderer with the old experiment.

The existing `ao_living_assets`, `prepare_local`, `prepare_static`, `cache_v3`,
`city_wardrobes` and ground reader implement the local preparation. The new
wrapper uses only the selected installation's database and installed, hash-checked
HD maps. There is no new full-database extractor, private pack download, native
game-object construction, gameplay entity or server interaction. The AOSharp
database-object interfaces were inspected, but their resource lifetimes and
renderer handoff are not validated; the candidate does not invoke them.

The supported resource index is pinned to
`9f3633726602ead5003bd3aec82f67f63462ff1080a5676f91c2f149ddfbf6d9`. Different
indexes are unsupported. All original archives are opened read-only. Generated
models, materials, animation palettes, ground support and authored route files
remain under `<AO>/randy-vk/living-local/generation-*`. A receipt records SHA256
identities, and `active.txt` selects one immutable generation atomically. The
renderer checks the installation index, route, ground, review policy and only the
current playfield's mesh files on loading. It never falls back to developer
directories or a leftover private Living pack. Fixture overrides are limited to
the reserved fixture playfield. Missing or invalid meshes skip their actors and
attached children; normal AO draws and machinery fallback remain intact.

Preparation keeps one previous generation for rollback. Older flat generations
are removed only when every recorded file is still hash-matched and no unknown
files or links are present. User-modified generations are preserved. Full
preparation requires 2 GiB free for staging and produces about 1.05 GiB for all
seven wardrobe themes, 21 city/settlement areas and one outdoor wildlife habitat.
This disk size is shared across clients, not allocated per AO process. GPU and
decoded per-playfield source limits remain those of the recovered engine.

The installer remains transactional for the normal renderer/material payload.
Local preparation follows that transaction as optional work. A conversion failure
leaves the normal renderer installed and the previous local cache selected; setup
reports the failure. This is not an atomic rollback of both transactions together.
All Living defaults are Off. Graphics presets preserve population preferences.
Routes remain static-reviewed, not live-certified. The separate Newland test
command explicitly prepares playfield 566 with review enabled; it does not modify
INI preferences or launch AO. The player then enables Ambient People Low.

`RVK_PhasefrontTraffic` and `RVK_OrbitalCraft` are reserved Off/Low/Medium settings,
hidden and effectively Off. The original index contains showroom holograms,
skeletal hoverbike families and appearance resources, but the requested marketing
family/variant/rider correspondence is not yet verified. No hoverboards are used
as aircraft. No additional showcase spacecraft or guessed flight routes are
enabled. `aerial_clearance.py` supplies a conservative continuous swept-path
authoring check, including complete geometry, source identity, terrain coverage,
vehicle envelope, bounds and Phasefront altitude gates. No real route is approved
by those synthetic tests. The existing transporter (1010001:6579) is preserved.

Automated checks distinguish installed-resource conversion in RAM, synthetic
filesystem preparation transactions, controlled renderer fixtures and actual
gameplay. The first three have evidence beside this source; combined gameplay,
actual local-cache GPU rendering, clean-install runtime behavior, multi-client
memory, dynamic obstacle clearance and live performance remain open. Parallax
V9 matched twelve approved fixture renders; All-Off matched Preview 21 pixels.

Package audits exclude original models, textures, animations, terrain exports,
game DLLs, private caches and hardware pipeline caches. Full HD uses exact pinned
Preview 21 material companions; new worker artwork needs a separate accepted
material freeze. No texture worker directory is read or modified by packaging.

This is a local test candidate, not public release certification. The installed
EULA and published AO terms restrict modifications and data collection. No written
Funcom authorization or official modding permission was established. Keeping
resources local removes redistribution from this design but does not settle those
other restrictions. See https://www.anarchy-online.com/terms-of-service and the
installation's `anarchy.eula`. Nothing has been published, installed into F: or
launched in the game by this task.
