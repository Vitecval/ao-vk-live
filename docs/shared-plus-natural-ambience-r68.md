# Private test r68

Shared Terrain+ again draws AO's own terrain in every biome, as in 0.7.7.
Enhanced Ground remains an explicit separate choice. Shared+ does not replace
snow with Enhanced Ground patches. HD materials, authored paths, native terrain
coverage, classic lightmaps, replacement residency and the recent screen fixes
remain in place.

Cliff material directions interpolate height gradients continuously. Material
weights no longer depend on individual triangle slope, while steep-face detail
still uses side projection. Snow additionally uses a continuous fragment lighting
normal before normal-map perturbation. This smooths snowy hills and cliffs at
distance without changing AO's positions, indices or non-snow lighting. Powder
and the authored path masks remain active.

Six existing licensed field-recording deliveries replace the six synthetic beds
in the private audio pack. Forest uses recorded birds and crickets; city uses
park ambience. Grid uses quiet ventilation, Inferno recorded crackle, and desert
and cold use restrained vegetation air. Desert and cold share one recording;
the crackle assembly includes repeated short recordings. These are private
listening candidates whose suitability still needs user feedback.

Reworked Sound remains Off by default. The separate Ambience volume control
defaults to 0.6 and changes only this added layer, with a short gain ramp. Original
music, voices, combat effects and system volume remain as before. The user's
existing explicit On/Off preference is preserved when installing.

The complete separate listening room is
`SoundRework/previews/all-drafts-listening/index.html`, served locally on port8766.
It includes 4,788 actual audio files across drafts, stems, preparations, references
and samples; this is not a count of independent performances or approved runtime
assets. The six proposed runtime files and provenance are under
`build/environment-078-r68/audio/runtime/`.

Controlled results and exact test hashes are recorded in
`build/environment-078-r65/shared-plus-classic-r68/RESULTS.json` and
`shared-snow-lighting-check/RESULTS.json`. In-game appearance and listening remain
separate checks; numeric regression success is not an artistic approval.
