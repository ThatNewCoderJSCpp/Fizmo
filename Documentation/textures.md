# Textures

Image maps for materials. The same textures work in the real-time renderer (`fizmo::graphics`, Vulkan and OpenGL), the CPU path tracer (`fizmo::lighting::Renderer`) and the GPU path tracer (`fizmo::graphics::GpuPathTracer`).

There are two texture types:

- `graphics::Texture` (`Source/Graphics/texture.hpp`) is an RGBA8 image plus sampling settings. The raster renderer draws with it.
- `lighting::Texture` (`Source/Lighting/texture.hpp`) is an RGBA8 or float image the path tracers read. `fizmo::lighting` still needs nothing outside the standard library.

`graphics::TextureLibrary` loads files once and gives you both.

## Loading: `TextureLibrary`

```cpp
#include <fizmo/../custom_graphics.hpp>
namespace gfx = fizmo::graphics;

gfx::TextureLibrary textures;
auto albedo = textures.load("bricks_albedo.png");                        // sRGB, trilinear, repeat
auto normal = textures.load("bricks_normal.png", gfx::TextureRole::Normal); // linear
auto orm    = textures.load("bricks_orm.png", gfx::TextureRole::Packed);    // linear
auto pixels = textures.load("tiles.png", gfx::TextureSettings::pixel_art()); // nearest, no mips
```

- Formats: anything `fizmo::images::load_image` decodes: PNG/APNG, JPEG, BMP, GIF (first frame), WebP, TGA, QOI, PNM, ICO/CUR, TIFF and SVG.
- Cache: entries are keyed by the normalized absolute path plus the settings. Loading the same file with the same settings returns the same `shared_ptr`; different settings (say sRGB vs linear) make a separate entry. `stats()` counts hits, misses, reloads and failures. `last_error()` says why a load failed (a failed load returns `nullptr`).
- `TextureSettings`: `color_space` (`SRGB` or `Linear`), `filter` (`Nearest` or `Bilinear`), `wrap` (`Clamp`, `Repeat`, `MirrorRepeat`), `mipmaps`, `flip_y`. `color()` and `data()` are the two usual presets, `for_role(role)` picks the colour space for a role, `pixel_art()` is nearest with no mips. `with_*` setters chain.
- `TextureRole`: `Color` and `Emissive` are sRGB; `Normal`, `Roughness`, `Metallic`, `Occlusion`, `Packed` (ORM), `Opacity` and `Data` are linear. `default_color_space(role)` returns it.
- Reload: `changed(path)` compares the file's modification time and size with the loaded copy, `reload(path)` reloads every entry for that file, and `reload_changed()` reloads every changed file and returns how many entries it refreshed. The `Texture` object keeps its address, so materials that point at it pick up the new pixels; the renderer re-uploads on the next frame. Call these on the thread that renders.
- `add(name, image, settings)` puts an in-memory image in the cache under `name` (adding the same name again replaces its pixels).
- `load_lighting(path, settings)` returns the same file as a `lighting::Texture` for the path tracers. It is cached alongside, and reloads follow it. `lighting_map(path, role, channel)` wraps it in a `lighting::TextureMap`.
- `prune()` drops entries nobody else holds, `remove(path)` and `clear()` drop entries. Materials hold raw `const Texture*`, so keep the `shared_ptr` alive while a material uses it.
- Helpers: `pack_orm(occlusion, roughness, metallic)` packs three grey images (any may be `nullptr` for "1") into one ORM image (R = occlusion, G = roughness, B = metallic, like glTF). `solid(color, size)` makes a flat image and `flip_vertical(image)` flips one.
- Conversions: `to_lighting_texture(texture)`, `to_lighting_map(texture, settings_from, channel)` and `to_lighting(...)` for `ColorSpace`, `SampleFilter`, `WrapMode` and `UvTransform`.

`graphics::Texture` gained `color_space()` / `set_color_space()`, `mipmaps()` / `set_mipmaps()` (with `Bilinear` that is trilinear filtering), `shared_image()` / `set_image()` and `sample_wrapped(u, v)` (uv-space sampling with the texture's wrap mode).

## UV transforms

`graphics::UvTransform` (`scale_u`, `scale_v`, `offset_u`, `offset_v`, `rotation` in radians) and `lighting::UvTransform` (`scale`, `offset`, `rotation`) do the same thing:

```
uv' = offset + R(rotation) * (scale * uv)        R = [cos -sin; sin cos]
```

`tiling(u, v)` and `make(...)` build them, `matrix(out[6])` gives the 2x3 matrix and `graphics::UvTransform::compose(first, second, out)` chains two.

UV (0, 0) is the top-left texel row of the image, as in glTF. Texel centres sit at (i + 0.5) / size, like a GPU.

## Raster renderer: `Material3D`

```cpp
gfx::Material3D m = gfx::Material3D::physical(0.0f, 1.0f);
m.texture = albedo.get();                 // base colour (sRGB, alpha = coverage)
m.with_normal_map(*normal, 1.0f)          // tangent-space, OpenGL / glTF convention (+Y up)
 .with_orm_map(*orm)                      // R occlusion, G roughness, B metallic (multiplies the scalars)
 .with_emissive_map(*glow)                // multiplies emissive * emissive_strength
 .with_opacity_map(*mask)                 // red channel multiplies alpha
 .with_alpha_cutoff(0.5f)                 // alpha test: discard below 0.5, keep the rest opaque
 .with_tiling(4.0f, 4.0f);                // material-wide UV transform
m.normal_uv = gfx::UvTransform::make(1, 1, 0.5f, 0);  // extra per-map transforms: normal_uv, orm_uv, emissive_uv, opacity_uv
m.with_triplanar(0.5f, 4.0f);             // object-space triplanar projection instead of UVs
```

- Fields: `normal_map`, `orm_map`, `emissive_map`, `opacity_map` (all `const Texture*`), `normal_strength`, `alpha_cutoff` (0 = off), `orm_occlusion` (false ignores the red channel, for metallic-roughness textures without baked occlusion), `uv`, `normal_uv`, `orm_uv`, `emissive_uv`, `opacity_uv`, `triplanar`, `triplanar_scale`, `triplanar_sharpness`. `has_maps()` says whether any of it is in use.
- Each map's sampled UV is `map_uv(uv(vertex_uv))`. The base colour `texture` uses `uv` only.
- Filtering and wrapping come from each `Texture` (`filter()`, `wrap()`, `mipmaps()`). Textures with `mipmaps()` get a full mip chain built on the GPU (`CommandList::generate_mipmaps`) and a trilinear sampler.
- Normal maps need no tangents in the vertex format: the shader builds the tangent frame from screen-space derivatives of the position and the normal map's UV. Normal maps follow UVs even when `triplanar` is on.
- `orm_map` and `emissive_map` act in the PBR path (`pbr = true`; the `with_` setters turn it on). Normal and opacity maps and the alpha cut-out work for lit, unlit, legacy and PBR materials.
- Unset maps are bound as a 1x1 white texture and an identity record, and the shader skips them, so materials without maps draw exactly as before.
- Colour space: the raster renderer shades in display space like it always has, so colour maps are used as stored. Data maps (normal, ORM, opacity, emissive) are uploaded without alpha premultiplication.
- Supported by `draw_mesh`, `draw_triangles`, instanced meshes and planar reflections. Quad meshes and quad batches (`QuadMesh3D`, `CompactVertex3D`, which have no UVs) keep using only the albedo texture binding. The software (non-GPU) renderer ignores the new maps.
- `Model3D` (glTF) now uses `normalTexture` (with `scale`), `metallicRoughnessTexture` (with `occlusionTexture` when it is the same image), `emissiveTexture` and `alphaMode: MASK` (`alphaCutoff`). New `ModelMaterial` fields: `normal_texture`, `metallic_roughness_texture`, `occlusion_texture`, `emissive_texture`, `normal_scale`, `metallic_factor`, `roughness_factor`, `emissive_map_strength`. The old averaged `metallic`/`roughness` values are still filled in.

GPU layout: set 0 of the 3D mesh pipeline is binding 0 `tex`, 1 `MaterialMaps` (std140 uniform, 192 bytes: ten UV rows, `params` = normal strength, alpha cutoff, triplanar scale and sharpness, `flags`), 2 `normal_map`, 3 `orm_map`, 4 `emissive_map`, 5 `opacity_map`. `mesh3d.vert` and `instanced3d.vert` also pass the object-space position and normal (locations 7 and 8) for triplanar mapping.

## Path tracers: `lighting::Material`

```cpp
namespace lt = fizmo::lighting;
lt::Material m = lt::Material::plastic(lt::Rgb(1.0f), 1.0f);
m.base_color_map = textures.lighting_map("bricks_albedo.png", gfx::TextureRole::Color);
m.normal_map     = textures.lighting_map("bricks_normal.png", gfx::TextureRole::Normal);
m.roughness_map  = textures.lighting_map("bricks_orm.png", gfx::TextureRole::Packed, 1);  // green
m.metallic_map   = textures.lighting_map("bricks_orm.png", gfx::TextureRole::Packed, 2);  // blue
m.uv = lt::UvTransform::tiling(4.0f, 4.0f);
```

- Maps: `base_color_map` (RGB multiplies `base_color`, alpha multiplies `opacity`), `normal_map` (`strength` scales the tilt), `roughness_map`, `metallic_map`, `opacity_map`, `transmission_map` (one `channel` each, multiplying the scalar), `emission_map` (RGB multiplies `emission * emission_strength`). Plus `uv` (applied before each map's own `transform`) and `alpha_cutoff` (applied when the material has an alpha or opacity map).
- `TextureMap`: `texture`, `transform`, `filter` (`nearest`, `bilinear`), `wrap` (`repeat`, `clamp`, `mirror`), `channel`, `strength`, `triplanar`, `triplanar_scale`, `triplanar_sharpness`. `TextureMap::of(texture, channel)` is a shortcut.
- `lighting::Texture`: `from_rgba8`, `from_float`, `solid`, `checker`; `stored(x, y)` (raw), `texel(x, y)` (decoded to linear when the texture is sRGB), `sample(uv, filter, wrap)` (bilinear filtering happens after decoding), `average()`, `has_alpha()`, `assign_rgba8`/`assign_float` to replace pixels in place.
- `Mesh` has `uvs` (texture coordinates, separate from `lightmap_uvs`) and optional `tangents` (`Vec4f`, w = handedness, glTF style); `compute_tangents()` builds them from the UVs. All the built-in shapes now fill `uvs`. Without tangents the tracer derives a per-triangle tangent frame from the UVs.
- Triplanar maps project in the instance's object space and need no UVs (not for normal maps).
- `Material::has_maps()`, `has_opacity_maps()`, `average_base_color()`, `average_emitted()`, `cutout(alpha)`.

CPU path tracer (every quality): all maps work, sampled at each hit with barycentric UVs and bilinear or nearest filtering. Shadow rays that look through surfaces (every quality but `simple` for direct light, and the bakers) honour alpha and opacity maps, so cut-outs cast cut-out shadows. Materials without maps take exactly the old code path. Emissive triangles still light the scene as lights with the map's average emission; a ray that hits them directly sees the full texture.

GPU path tracer: base colour maps (with alpha / cut-out, UV transforms, wrap, nearest or bilinear filtering, sRGB decoding) are packed into the `tables` storage buffer (textures over 2048 pixels are box-filtered down). Triangles carry an offset to their UVs, materials an offset to their texels. The other maps fall back to their average value (roughness, metallic, transmission, opacity, emission); normal maps and triplanar projection are ignored there. Call `scene_changed()` after changing a material or reloading one of its textures.

## Tests

`-DFIZMO_BUILD_TEXTURE_TESTS=ON` builds `fizmo-texture-tests` (Testing/texture_tests.cpp): loading and caching, reload, colour spaces, UV maths, wrap modes, CPU path tracer checks (checker map, normal map, roughness map, alpha cut-out, emission map, triplanar, maps that change nothing), and with a display Vulkan/OpenGL raster checks (textured, tiled, cut-out, normal-mapped, ORM, emissive, mipmapped and triplanar quads) and a GPU path tracer checker. Run it under `xvfb-run -a` on machines without a display. `fizmo-texture-tests cpu|vulkan|opengl|tracer` runs one part.
