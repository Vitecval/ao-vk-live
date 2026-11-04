# Preview 22 local Living integration candidate

Candidate 5 exposes Phasefront Off / Low / Medium / High / Ultra (0–4).
High uses 12–24 second gaps and a 10% skip chance per craft; Ultra uses 2–6
second gaps without skipped visits. Within deterministic ten-minute blocks,
only complete passes fit; the remaining tail is idle. Fresh lane choices favor
checked low corridors (35% / 50% additional low-lane selections). Cruise speeds,
whole-craft clearance bounds, and the current LRSCENE8 cache are unchanged.
High permits three simultaneous Phasefronts and four total aircraft; Ultra
permits four Phasefronts and five total aircraft. Large craft remain capped at
one. Low and Medium retain their original schedules and caps.

Candidate 4 removes distance-based pedestrian braking. Walk pace stays steady
until arrival; the character settles to idle at the fixed destination within
140 ms, with the gait still driven by actual travel distance. Short low-flight
lanes in LRSCENE8 add occasional roughly 10-foot skims. Those use tighter 0.25 m
steering, 0.08 m bob and 0.025 rad bank limits, and 6–10 m/s cruise. A full swept
vehicle/rider box includes these bounds and a safety margin. Triangle SAT checks
both original scenery and all covered native terrain cells, including slopes.
Low lanes are omitted when continuous clearance cannot be established. The
existing mid-height and large-craft routes remain available in every city.

Candidate 3 adds a bounded pedestrian activity state machine. Original city
floor triangles and terrain support new branching walking graphs; the full
animated body envelope filters edges. Actors choose destinations, pause for
different lengths, avoid occupied rest destinations and vary their next branch.
When a branch is unavailable, they vary stops and direction on preserved routes.
Native male/female shrug, scratch and nod animations are blended during selected
pauses. Checked flat rest nodes permit sit-down, ground idle and stand-up clips.
Coats and heavy suits omit sitting where compatibility is not established.
LRM4 records separate activity clips and source vertex contacts across poses,
keeping sit/stand transitions grounded. No animation event reaches the game.

LRSCENE7 carries the authored walking graph and up to sixteen checked flight
lanes. Phasefront choices vary by session and pass: lane, direction, 8–16 m/s
cruise speed, start delay and occasional skipped flights. Passes include modest
speed variation, bank and steering offsets bounded by 1.2 m, inside the 3 m
prepared clearance margin. Usable lanes generally span roughly 11–49 m above
ground as city roofs permit; constrained cities retain their certified corridor.
The renderer does not invent unverified low routes through tall buildings.
Activity and flight clocks preserve the existing focus/suspension safeguards.

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
preparation requires 2 GiB free for staging and produces about 1.45 GiB for all
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

The second candidate adds 23 original appearances, with distinct body/head
geometry, hairstyles, modular clothing, actual robe geometry, light/medium armor,
Omni-only security outfits, elite armor and a complete battlesuit. Original skin
underlays match the body breed and gender. Complete native armor/robe materials
are retained. CAT flag 8 identifies alpha used for environment blending rather
than transparency, so reflective armor and vehicle materials remain opaque.
Static archived materials also honor their original blend/test states, preserving
opaque hulls while retaining genuine transparent and cutout surfaces.
Appearance choices are deterministic and penalize nearby identical outfits.
City faction pools exclude Omni security from Clan and neutral populations.
Every enlarged body is also filtered by conservative whole-route clearance using
all animation poses, scale, cubic control-point hulls and original scenery.
Instances with no compatible clear appearance are skipped. Normal preserves the
previous wardrobe system; High and Armored Civilians opt into the larger pool.

Phasefront item/nano/appearance chains now establish Wraith X2-45 (CIR 269458
and 270681), Spectre (270811) and Banshee (270907). The original seated body,
matching pose animation and head attractor are resolved together. A rigid
composite preserves that authored rider pose. The showroom holograms are not
used. Original material masks govern reflectivity; only native engine/Glow
geometry receives subtle additional emission. Small navigation lenses attach at
actual hull extremities. No mask is invented for an entire hull.

The large showcase craft is the complete original Mech drop ship (ABIFF
262333), approximately 88 m long. It follows a separate, higher corridor and a
rare deterministic schedule. At most one large craft and two Phasefronts can
submit locally; combined ordinary/Phasefront/large traffic is capped at three.
Original transporter traffic is preserved. Aircraft use size-aware distant LODs,
smooth distance and entry/exit fades, and no server entities or targeting.

Local preparation validates both aircraft corridors in all 21 supported cities.
Every placed mesh is decoded; unknown geometry blocks that city's new routes.
The continuous swept envelope includes complete vehicle/rider dimensions,
clearance margins, playfield bounds, exact native terrain triangles and complete
20–50 m Phasefront centerline altitude coverage. Renderer flight follows the same
straight segments, without spline overshoot or per-frame collision searches.
The broader static geometry check does not certify dynamic live-game obstacles.

Open the preserved expandable menu with Ctrl+Shift+F12. Run Prepare Living City Test.cmd for explicit static-route review in all cities,
or Prepare Newland Test.cmd for Newland alone. Neither command edits saved
preferences or launches AO. All new controls default Off (appearance variety
defaults Normal), and Preview 21's expandable menu and Parallax V9 are preserved.

Automated checks distinguish installed-resource conversion in RAM, synthetic
filesystem preparation transactions, controlled renderer fixtures and actual
gameplay. The first three have evidence beside this source; combined gameplay,
clean-install runtime behavior, multi-client
memory, dynamic obstacle clearance and live performance remain open. Parallax
V9 matched twelve approved fixture renders; All-Off matched Preview 21 pixels.

Package audits exclude original models, textures, animations, terrain exports,
game DLLs, private caches and hardware pipeline caches. Full HD uses exact pinned
Preview 21 material companions; new worker artwork needs a separate accepted
material freeze. No texture worker directory is read or modified by packaging.

This is a local test candidate, not public release certification. Original asset
conversion and renderer evidence run from read-only installed resources in RAM;
no original mesh/material/terrain files are written outside the selected AO cache.
Visual evidence is labeled as renderer fixtures rather than gameplay. Optional
held props and true orbital/mothership traffic remain outside this candidate;
unverified engine masks are omitted. No new worker artwork is integrated, no
GitHub publication is performed. Installation occurs only when the user requests it.
