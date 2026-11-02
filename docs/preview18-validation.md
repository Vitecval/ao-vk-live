# Preview 18 release scope and validation

Preview 18 includes the Shared Ground+ per-cell texture correction when AO's
internal lighting switch is off. The combined terrain texture-stage layout is
recognized independently of that switch, and the replacement patches retain
their tile table, lightmap and material records. The lighting switch itself
is preserved. Standard Shared Ground and GroundOn remain separate modes.

Full HD retains every Preview 17 map and adds the six completed 100-identity
production batches plus 18 finalized worker identities: five architecture/prop
textures, six industrial textures and seven equipment atlases. Source masters,
UV mappings, original alpha, material-map hashes and companion relationships
are retained. Native generation sizes and resampled export sizes are recorded
separately. No new artwork was generated during release preparation.

Four finalized natural-ground repaints remain inactive because the current
renderer deliberately bypasses individual natural-tile repaints to preserve
terrain transitions. The unfinished earlier batch and a held moss transition
remain preserved outside the release. The finalized equipment family takes
precedence over its overlapping unfinished candidate. These are not claimed
as newly active terrain art or completed whole-building families.

Lite contains the same renderer, shaders, embedded terrain resources and core
ground materials as Full HD. Optional world/equipment maps are Full HD content;
the renderer and original AO terrain do not depend on that library.

The accompanying release REPORT.md and validation.json record the checks
actually performed, including terrain, lighting, foliage, threading, materials,
setup, source provenance and archive integrity. Offline rendering and loader
checks do not certify live mesh fit, every zone, moving distant LOD, long sessions
or live-game correction of the reported screenshot. Live verification remains
outstanding. Database inventory coverage is not visible-world coverage, and no
Shadowlands completion percentage is claimed.
