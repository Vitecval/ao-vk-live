# Horizon 0.7.7 / Preview 24 r63

- Fresh startup and return-to-login use the same authored studio lighting.
  Temporary unavailable engine state preserves the confirmed presentation
  context. Login temporal smoothing marks additive effects rather than opaque
  scenery as reactive, reducing visible shimmer.
- Temple stone in the captured clan-temple playfield has a fully matte response
  with controlled diffuse highlights. Character and effect materials retain
  their existing response.
- Shared Ground+ rock decorations use their own opaque/depth state so they
  remain visible as the camera angle changes.
- Slow graphics preparation shows a responsive progress window with elapsed
  time and completed work. The matching shader-preparation helper is bundled.
- Day/Night Cycle adds PermaDay and PermaNight. Their fixed clocks persist
  across restarts; switching away restores saved manual clock preferences.
- A successful issue report visibly locks its completed form and changes the
  action to Send a New Report, which clears the form for a new submission.

Full reuses the existing 19 material companions and 14,345 prepared material
files. Lite downloads no texture archives. The pending HD armor and creature
rework is reserved for 0.7.8 and is excluded from this release.

Validation includes controlled login sequences, temporal image comparisons,
unchanged temple/world/interface reference images, native settings and process
restarts, progress-window responsiveness, report UI checks and isolated setup
lifecycle checks. Hardware and retail-scene coverage remain limited to the
tested configurations; these checks do not promise a general FPS improvement.
