# Preview23 sunny Enhanced Daylights

Revision 18 removed the washed-out display grade but left the user's requested
sunny-day appearance too close to the dark teal baseline. Revision 19 changes
the outdoor lighting profile itself. It is selected by the existing Enhanced
Daylights option; the server and longer-day choices retain their lighting.

The daytime atmosphere uses a pale blue vault and a bright neutral horizon.
Its existing atmosphere readback, with the applied keys divided out, calibrates
the sky radiance instead of relying on gamma-coded keys to look bright in AO's
API colour space. Ratios are bounded and all existing sky resources are reused.
Zone teal is reduced during this profile. Cloud layers continue to draw over
the atmosphere; overcast and storms reduce the sunny profile. Dawn and night
fade it out with the actual sun elevation.

The actual sun's directional light is warm-neutral and stronger where the
game's sun was weak, capped at 2.25 times its input energy. Surface normals,
material colours and existing shadow visibility still shape it. Effect lights,
studio lights and the sky dome are excluded. A hemispherical sky term supplies
missing ambient illumination, rather than lifting the completed image.

AO's ground has sunlight baked into a separate lightmap. Its light pass gets
missing directional energy according to the sun angle and baked visibility;
dark baked areas and live caster/terrain shadows remain effective. This runs
for the classic combined/separate light passes and SharedGround+. It does not
repaint the base texture or change its patterns.

Outdoor context survives looking up or terrain culling. It resets on playfield
changes and confirmed indoor entry. The profile still requires a known sun and
never supplies daylight to login, creation-room or indoor scenes. Display grade,
gamma, exposure and the frame-generation/presentation implementation are not
changed by this revision.

Real Vulkan controls cover studio/intro, indoor, material colour cards, an
outdoor atmosphere with facades and ground, night, and an upward camera with no
terrain drawn for 47 frames after establishing outdoor context. The fixed
outdoor comparison has approximately four times the sky brightness and twice
the ground brightness of its deliberately dim server baseline, without white
clipping or loss of material saturation. These are fixture measurements, not
predicted ratios for every game scene. Actual baked-terrain tests compare both
terrain modes with a high daytime sun, live occluder, and a below-horizon night
control. Retail screenshot approval and other GPU families remain separate.

Reproduce the outdoor controls using rvk_demo --visual-case 7 (scene), 8 (night)
or 9 (look up), --visual-toggle 0/1, --fixed-clock 16.666667 and --frames 48.
The shared terrain fixture also accepts --day-cycle 0/2.
