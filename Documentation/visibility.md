# Visibility, level of detail and shadow level (`fizmo::graphics`)

`#include "Graphics/visibility_3d.hpp"` (it comes with the graphics headers). Two classes help a 3D renderer draw less: `Visibility3D` decides what to draw, how detailed, and with what kind of shadow, and `LodQuadtree3D` tracks ground tiles for terrain and voxel worlds.

## Visibility3D

```cpp
using namespace fizmo::graphics;
Visibility3D vis;
auto wall = vis.add({{-5, 0, -11}, {5, 4, -10}}, VIS_CASTER | VIS_OCCLUDER);
auto crate = vis.add(crate_box, VIS_CASTER, LodLevels3D::of({30.0, 90.0}));
vis.cull(camera);
if (vis.visible(crate)) draw(crate_meshes[vis.lod(crate)], vis.shadow(crate) == ShadowMode3D::Real);
for (const BlobShadow3D& b : vis.blobs()) draw_blob(b.center, b.normal, b.radius, b.opacity);
```

Objects live in a dynamic AABB tree, so `cull` walks the tree with the camera's frustum and skips whole branches out of view. `update(handle, box)` is cheap when the box hasn't changed. `cull` then runs these tests:

- **Frustum:** the box is outside the camera's view.
- **Distance:** past `max_distance`, or the camera's far plane when that's 0.
- **Size:** smaller than `min_screen_pixels` on screen.
- **Occlusion:** hidden behind an occluder.
  - Occluders are objects flagged `VIS_OCCLUDER`. Their box, or a smaller one from `set_occluder_box`, must be solid.
  - The nearest `max_occluders` of them are rasterized into a small depth buffer, `occlusion_width` × `occlusion_height`. The buffer is then eroded by a pixel, so a gap narrower than a pixel can't hide anything by mistake.
  - Anything whose whole screen rectangle is behind that depth is hidden.
  - `VIS_NO_OCCLUSION` opts an object out, and `VIS_ALWAYS` skips every test.

Level of detail comes from `LodLevels3D` switch distances, with `lod_hysteresis` slack so an object near an edge doesn't flicker. `lod_bias` scales the distances.

Shadow level, from `settings.shadows`:

| Distance | `ShadowMode3D` | Meaning |
|---|---|---|
| under `real_distance` | `Real` | Draw into the shadow map. Off-screen casters count too, since their shadows can fall into view. |
| under `blob_distance` | `Blob` | No shadow map; a soft blob is placed under the object (`blobs()`), fading out toward `blob_distance` |
| beyond | `None` | No shadow |

Set `SunShadow3D::distance` to `real_distance`, so the shadow map spends its pixels on the near range. `settings.ground` can place blobs on real ground: a callback that casts down from the object. `reuse_move` and `reuse_turn` let `cull` reuse last frame's answer while the camera is almost still and nothing changed. `stats()` reports each test's count.

## LodQuadtree3D

Voxelspire-style tiles. Tiles are split while the camera is closer than `base_distance × 2^(level − 1)`, with `hysteresis`, so the finest tiles (level 0) are under the camera. `update(eye)` returns `true` when the tile set changed, and `added()` and `removed()` list just the tiles to build or free. `tile_at(point)` and `level_at(point)` look tiles up. `up_axis` picks which axis is up (0, 1 or 2).

Tests: `Testing/visibility_tests.cpp` (`FIZMO_BUILD_VISIBILITY_TESTS`).
