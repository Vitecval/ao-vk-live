# Billboard soft-alpha classification

The retail captures `rvk-frame-161912.txt`, `161914.txt` and `161916.txt`
show the Dreadloch billboard base `1010004:258421` at draw 141 and its two
screen-effect planes, `258411` and `258412`, at draws 1118 and 1119. The
texture objects, serials, upload counts, material values and blend/depth state
remain stable in all three captures. These captures provide no evidence of an
alternating texture upload or missing advert artwork.

Both effect textures report hard-alpha edges 0 and soft-alpha share 1. Their
transparency was nevertheless sufficient for Draw's foliage-material test:
`!texture->m_opaque`. This applied leaf backlighting and foliage distance LOD
to soft screen effects. Canopy generation already excluded the same soft cards
with a minimum hard-alpha-edge threshold of 0.002, so the two policies disagreed.

`rvk/foliage_alpha.h` now supplies that shared threshold. Draw requires actual
hard cutout edges before applying foliage shading or foliage LOD, and canopy
classification uses the same helper. Genuine leaf cutouts retain the existing
boundary. No advert texture, screen overlay, blend operation, depth state,
geometry, interface color, terrain shader or user preference is changed by
this classification correction.

`foliage_alpha_check` covers the captured zero-edge soft cards, the exact
threshold, ordinary leaf edges, opaque cards and invalid metrics. The
controlled HDR Vulkan screen-card fixture passes: soft cards remain pixel
identical with LeafLight and distance LOD toggled (maximum channel deltas 0
and 0, foliage counts 0 and 0). Genuine hard-edge leaf cards retain backlighting
(maximum channel delta 123), with 599 foliage draws and all 599 taking the
forced distance LOD. The fixture primes the frame camera before testing LOD.

This is a demonstrated classification defect and a plausible contributor to
the reported flicker. The three captured frames alone do not establish that
every visible fluctuation has this cause. The original near-coplanar screen
planes still use their authored LEQUAL depth test and depth writes; no unrelated
depth-bias workaround or global effect suppression was introduced. Retail
billboard stability after installation remains to be observed.
