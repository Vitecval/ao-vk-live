# Preview23 lighting correction

Revision 17's daylight mode drove a display gamma lift that raised dark and
middle values across the image. Its creation-screen fallback also used full
local-light intensity for every intro draw, overriding the selected 25% scale.
Retail screenshots exposed flattened outdoor contrast and a blown-out intro.

The correction keeps daylight separate from the legacy display-grade control.
It supplies a bounded hemispherical sky term, colored by the rendered sky, to dark exterior materials,
multiplied by their albedo. Sunlit exposure and the black point remain intact.
The directional shadow adjustment is limited to 10%. Outdoor terrain must have
been observed recently; login, creation-room and indoor draws get no daylight
fill. Characters in the studio use their current draw's lights at the user's
selected scale, while room geometry retains the normal light path.

Real Vulkan checks use the three light positions/colors/attenuation settings
from the retail intro scan, bright/gray/black/color cards, sunlit and shaded
normals, and indoor-context controls. Intro and indoor mode toggles are pixel
identical. The controlled outdoor fixture changes mean brightness by about 2.2%,
raises shaded cards by about 10.1%, and changes sunlit channels by at most 6/255,
with no additional clipping or loss of sunlit saturation. These checks do not
replace retail scene inspection or validation on additional GPU families.
