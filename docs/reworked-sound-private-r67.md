# Reworked Sound in the private test

Horizon and the in-game settings expose **Audio → Reworked Sound**, an On/Off
choice that defaults to Off. It is independent of the graphics master switch.
The built-in presets leave it Off; an explicit choice is saved with the other
settings.

On adds one quiet regional ambience layer using six existing SoundRework loop
masters: Grid, city, desert, forest, Penumbra/cold, and Inferno. Unsupported areas,
ordinary interiors, login screens and Off use the original audio alone. Original
music, combat sounds, voices and ambient playback are not intercepted or replaced.
The private pack contains no music previews or new generated material.

The original 48 kHz stereo PCM24 masters remain untouched. Runtime delivery is
PCM16 at the same frame count and sample rate. Playback uses software gain, short
fades and a loop overlap; it never changes system or device volume. Missing files
or audio-device failures leave original playback intact.

Validation covered regional selection, exact PCM decoding of all six deliveries,
default Off, every built-in preset Off, saved On/Off across processes, graphics
independence, and the actual Horizon checkbox. The checks opened no audio device.
In-game listening, levels and zoning transitions still need retail confirmation.

Evidence: `build/environment-078-r65/sound-check/RESULTS.json`,
`SOUND-SETTINGS-RESULTS.json`, `AUDIO-ASSET-RESULTS.json`, and
`build/horizon-sound-ui/ui-test/FEATURE-UI-RESULTS.json`.
