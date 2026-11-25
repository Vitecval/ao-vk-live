# Grid label family readability

The reviewed readability policy now covers 71 actual destination and exit
textures: 25 normal Grid labels and 46 Fixer Grid labels. This includes all 22
additional IDs reported by the user. Native/current image pairs establish the
common dark-glyph, light-panel role; no runtime filename or brightness heuristic
is used. Original lettering, artwork, alpha and UV coordinates remain unchanged.

`ReadableGridSign` explicitly lists these identities and their texture-quality
aliases. The Grid glowpiece `82935` remains an effect. Six catalog names without
an actual installed RDB record remain outside the policy: `82941`, `82943`,
`159680`, `201649`, `287010` and `287011`.

The existing bounded self-lit treatment is unchanged: only these HDR 3D faces
use their sampled diffuse at 0.45 linear strength, without additional NightGlow,
emission halo or screen reflection. Other lamps and the surrounding scene retain
their selected lighting settings.

The full Vulkan test rendered all 71 actual diffuse maps with glow levels 0
and 4, plus two generic-identity controls (144 renders). Label contrast ranged
from 89 to 115 channel values. Maximum mean label response was 0.031862/255;
the 99th-percentile delta was 1/255. The unrelated warm emitter changed 1,935
pixels in every comparison. Applying the same image to a generic identity
retained a mean glow response of 27.9758/255, proving that glow was exercised
and remains active outside the label policy.

The demo and renderer hashes remained stable throughout this run. Results and
reviewed source hashes are under `build/environment-078-r65/grid-label-family-check`
and `workers/architecture/grid-label-family`. A final combined build can repeat
representative cases with `tests/grid_sign_render.py --family-evidence ...
--label-ids 285603 254910 159913 125777`; full family membership is recorded
separately in `POLICY-CHECK.json`. These controlled checks do not substitute for
the next retail Grid inspection.
