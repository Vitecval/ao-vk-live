# Preview 24 local revision 46: stronger photographic rocks

This local trial strengthens revision 45's Enhanced Rocks. The public Preview 24 packages and tag are unchanged. Build, graphics, installation and delivery evidence are kept in `build/preview24/rocks-r46`.

## Appearance

Revision 45 multiplied the original rock texture by photographic luminance, with relatively restrained normal relief. That left the original painted cracks, stretch and baked shading dominant. At full strength, revision 46 blends 94% toward a photographic stone surface. Its tint comes from the material's mean colour, rather than from every source texel; dark means are raised together by luminance to retain their hue. Original lighting, alpha and the original green moss regions remain in their respective paths.

The main photo repeats at 3.6 m rather than 6 m. Near the eye, an additional 7.31-times-finer scale adds grain and small normal variation. A slow world-space colour variation breaks up uniform coverage. Normal relief gain rises from 0.42 to 1.05, with a bounded slope to avoid extreme bends. The photographic occlusion has more influence and roughness retains a wider matte-stone range, with zero metallic response.

The same 0–1 Materials slider controls the change. Zero keeps the original surface; Original mode still disables its effective value. Green moss suppresses 98% of the new treatment. Overall detail still fades from 50 to 140 m; fine grain fades from 18 to 55 m and is also suppressed by pixel footprint. Explicit gradients keep sampling stable during movement. The existing normal variance filter handles the stronger relief's specular aliasing.

## Coverage and cost

All 143 identified rock texture identities from revision 45 remain covered, including the seven requested examples. The colour path also accepts verified original rocks with baked vertex lighting; it no longer depends on enabling dynamic per-pixel lighting for those surfaces. Where a mesh has no vertex normals, screen derivatives of its world position supply the projection basis. Baked lighting is preserved without forcing PBR or injecting new lights into those passes. Characters, foliage, ground clutter, unknown identities and projected texture coordinates remain excluded.

The photographic maps are the same embedded CC0 cliff material already used by terrain: 16 MiB including mips, shared by all rocks. No extra textures, downloads, tessellation, ray tracing or geometry passes are added. A nearby blended face uses two more filtered texture reads per active projection plane for fine colour and normals; these reads disappear after the close-detail fade. Actual renderer measurements are recorded in the GPU report. They are synthetic 768×512 measurements, not retail frame-rate promises. Original mesh silhouettes remain a limit on close-up realism.

## Verification and local installation

The graphics fixture checks visible detail, baked-lighting rocks without vertex normals, character exclusion, unknown lit and unlit surface exclusion, movement, and shared/native terrain separation. The shadow fixture checks depth clipping, moving poses and static depth reuse. Source snapshots and the previous renderer are retained for comparison. Reports distinguish graphics validation from an actual retail playtest.

`tools/Install-Preview24-r46.cmd` installs the verified renderer into the closed F: test installation, with ownership checks, verified backups and the existing recoverable file transaction. Preferences and prepared material files are preserved. `tools/Revert-Preview24-r46.cmd` restores the exact revision 45 installation while its ownership record still matches. Isolated install, repeat install, changed-file refusal and exact revert checks are recorded separately. Neither helper launches the game.
