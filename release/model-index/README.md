# Model-identification index

Both Lite and Full HD require `randy-vk/mesh-ids.bin`. Global window interiors
use it to positively identify approved building models. Some legacy Newland
windows do not require a model witness, which concealed the missing dependency
on machines that already had a development index.

This generated lookup table contains only numeric resource identities,
irreversible 64-bit geometry/raw hashes, and piece numbers/counts. It contains
no vertex positions, indices, game models, textures, artwork, resource names,
dialogue, account information, or local paths. It cannot reconstruct game meshes.
Full debug/resource names are replaced with an empty string, except for a single
fixed `scenery_spaceship` category label already defined by the renderer's MIT
source. That label preserves the 13 distant-city-ship records' rendering predicate;
it is not an original resource filename. All 53,383 recognition
records and ambiguity candidates are retained; filtering to approved buildings
would incorrectly accept geometry shared with excluded objects.

The source identification index was generated for the supported classic live
18.8.62_EP1 client. `mesh-ids.json` records its input SHA256 and the sanitized
output SHA256, counts, and size. The source index/game resources are not shipped.
The format and hash algorithm are implemented by this project's MIT-licensed
`rvk/meshids.cpp` and `rvk/mesh_id.h`. Reproduce the export with
`python tools/build-mesh-index.py <local-index> release/model-index/mesh-ids.bin`.

Setup verifies the pinned table before changing the client and installs it in
the same transaction as the renderer. A compatible pre-existing index, including
one with local debug names, is preserved and never adopted for uninstall.
Unknown/damaged pre-existing indexes are preserved with a clear error. An
installer-owned index is removed on uninstall; user-supplied indexes remain.
