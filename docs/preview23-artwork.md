# Preview 23 artwork and retained originals

The expanded library contains **13,103 unique original texture IDs** with HD
diffuse replacements and **40,397 runtime map files**. It includes **4,476 IDs**
beyond the Preview 21 library: the current offline production, 50 reviewed older
artwork IDs recovered from broad retention holds, and 20 new generation
completions. Existing ICC style alternatives remain available and do not inflate
the unique-ID count. Normal, ORM and emission maps are not additional texture IDs.

Of 14,753 identified game-surface IDs (including walls), 88.82% have replacement
artwork. The older website scope is **12,497 / 14,145 (88.35%)**.
The remaining 18 generation requests were rejected by the image service. Their
originals remain in use; none is represented as completed HD artwork.

## Why originals remain

There are **1,632 retained original IDs**, separate from those 18 unfinished
requests. **789 are terrain inputs**: per-cell colours and transition masks must
blend with neighbouring tiles. Manufactured paving exceptions and shared terrain
detail remain available. The remaining originals include exact fonts, numbers,
signs, diagnostic graphics, uniform palette inputs, opacity/cutout maps, glow and
reflection responses, and older items whose source or shared usage is unresolved.

The individual [retention list](preview23-retained-originals.csv) gives each ID,
name, reason and evidence level. A historical retention label is not proof that a
texture is a functional control. Some older cases remain conservative fidelity
holds, and nine ordinary-looking surfaces are still flagged for possible
overretention. Those limitations are explicit in the evidence; they have not been
reclassified as technically impossible to repaint.

## Integration checks and limits

Complete selected material sets and exact typed IDs are preserved. A narrow list
allows newly completed materials through older missing-resource holds; terrain
transition controls and existing reviewed-family guards remain in place. Runtime
decode remains demand-driven and keeps the existing address-space limits.

Controlled native frontend tests exercise real Vulkan map upload and selection,
including world, equipment, ICC, wall, skin and quality-alias paths. Those checks
do not certify every retail mesh, zone or prolonged play session. No game database
or unmodified source-image dump is part of the player package. Artwork remains
reference-derived and carries the existing artwork license and attribution files.

## Requested pedestal finish

Texture `1010004:200347` has a newer owner-requested modern satin-metal finish,
using `1010004:14941` as the material reference. Its roughness and metalness match
that reference (82/255 and 158/255). The generated diffuse and matching normal/ORM
maps preserve the original 1:2 aspect and alpha. This refines an already counted
ID, so the coverage figures above do not increase. Native x86 Vulkan upload
passed; the owner still needs to assess its appearance on the actual pedestal.
