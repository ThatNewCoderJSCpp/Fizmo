# Lighting (`fizmo::lighting`)

`#include <fizmo/../lighting.hpp>` (or `fizmo::lighting` from CMake). It needs nothing but the C++17 standard library, so engines can use it without the graphics stack.

The module describes a scene (meshes, materials, instances, lights, sky), then lights it at whatever quality you ask for, from a flat unlit look to spectral photon tracing. Every quality uses the same lights and materials, so a game can let players pick.

## Quality

`LightingSettings::preset(quality)` gives a starting point. Every field can be changed after.

| Quality | Shadows | Indirect light | Reflections | Glass | Cost |
|---|---|---|---|---|---|
| `off` | none | none | none | flat | nothing |
| `simple` | hard, ray-cast, sun only (like Voxelspire) | flat sky ambient | sky | see-through, no bending | one ray per light |
| `shadowed` | soft, every object, sized by the light | sky + ambient occlusion | sky | bends light (4 layers) | a few rays per light |
| `realistic` | soft | probe grid that bounces light (DDGI-style), updates as lights move | ray traced, glossy | bends light, colored shadows | probes + 1-2 rays |
| `path_traced` | exact | full multi-bounce | exact | exact, with RGB dispersion | many samples, clears over time |
| `photon` | exact | full | exact | spectral: caustics and real rainbows | most |

The knobs behind the presets:

- `shadows`: `none`, `hard`, `soft`. `shadow_casters`: `sun_only`, `flagged` (each light's `casts_shadows`), `everything`. `shadow_samples`, `shadow_bias`.
- `gi`: `none`, `ambient` (+ `ambient_occlusion_samples`), `baked`, `probes` (`probe_spacing`, `probe_rays`, `probe_bounces`, `probe_hysteresis`), `path_traced`.
- `reflections`: `none`, `environment`, `ray_traced` (`reflection_bounces`).
- `refraction`, `dispersion`, `transmission_depth`.
- `transport`: `rgb` or `spectral`. `max_bounces`, `samples_per_pixel`, `russian_roulette_depth`, `firefly_clamp`.
- `light_samples` and `all_lights_threshold`: with few lights every light is sampled; with many, lights are picked by how much they matter at each point.
- `caustics`, `photons`, `photon_bundle` (packs of wavelengths that split apart at the first dispersive surface), `photon_radius`, `photon_bounces`.
- `area_lights` (off treats shaped lights as points), `emissive_lights` (glowing materials light things), `use_ies`, `use_cookies`.
- `fog` (density, color, anisotropy, height falloff, ray-march steps) for god rays.
- `exposure` (or `exposure_from_ev100`), `tone_mapper` (`none`, `reinhard`, `aces`, `filmic`, `agx`), `white_balance` in kelvin, `saturation`, `contrast`.
- `denoise` (edge-aware, guided by albedo, normals and depth), `threads`, `seed`.

## Lights

`Light` has a `shape`:

| Shape | Factory | Notes |
|---|---|---|
| `point` | `Light::point_light` | a `radius` turns it into a small sphere for soft shadows |
| `spot` | `Light::spot_light` | `inner_cone` / `outer_cone` in degrees |
| `directional` | `Light::sun` | `angular_diameter` (0.53 for the real sun) softens shadows |
| `sphere` | `Light::sphere_light` | round bulbs |
| `disk` | `Light::disk_light` | downlights, one- or two-sided |
| `rectangle` | `Light::rectangle_light` | windows, TVs, panels |
| `tube` | `Light::tube_light` | fluorescent tubes, neon, LED strips |

Any material with `emission` is a light too: every glowing triangle can light the scene and is sampled like the others.

Each light also has:

- `mobility`: `dynamic` (computed every frame), `stationary` (direct light live, bounced light baked), `baked` (all baked).
- `unit`: `unitless`, `candela`, `lumens`, `lux`, `nits`, `watts`. In `unitless` a shaped light is as bright as a point light of the same number, so you can switch shapes without relighting.
- `color`, `temperature` (kelvin, multiplies the color), `intensity`.
- `range`, `falloff` (`physical`, `linear`, `smooth`, `constant`, `custom` with `falloff_exponent`).
- `casts_shadows`, `shadow_strength`, `shadow_bias`, `shadow_samples`.
- `diffuse`, `specular`, `indirect`, `volumetric` multipliers, `visible` (whether you can see the light's shape), `channels` (a light only lights objects that share a channel).
- `ies` (an `IesProfile`, loaded from LM-63 `.ies` files) and `cookie` (an image projected through the light).

## Materials

`Material` is a metal/roughness PBR material with `transmission`, `ior`, `abbe` (dispersion: lower is more rainbow), `thin_walled` (windows), Beer-Lambert `absorption_color` / `absorption_distance`, `opacity`, `emission` and `emission_strength`. Presets: `diffuse`, `plastic`, `metal`, `mirror`, `glass`, `frosted_glass`, `flint_glass`, `crystal`, `diamond`, `water`, `ice`, `window_pane`, `emissive`.

## Scenes and rendering

```cpp
using namespace fizmo::lighting;
Scene scene;
scene.environment() = Environment::physical_sky();
scene.add_object(Mesh::plane(10, 10), Material::diffuse(Rgb(0.8f)));
scene.add_object(Mesh::sphere(0.5f), Material::diamond(), Transform::translate({0, 0.5, 0}));
scene.add_light(Light::sun({0.3, -1, -0.4}, Rgb(1), 3.0f));
scene.add_light(Light::tube_light({0, 3, 0}, {1, 0, 0}, 1.2f, 0.02f, Rgb(1), 40.0f).with_temperature(4000));

Renderer renderer(LightingSettings::photon());
renderer.render(scene, Camera::look_at({0, 2, 5}, {0, 0.5, 0}), 640, 360);
auto rgba = renderer.rgba8();
```

`render` keeps adding samples while the camera, settings and scene stay the same, so the image gets cleaner each call. Changing a light only rebuilds the light data. Adding, removing or editing geometry rebuilds the BVH.

Moving things is cheaper: `scene.move_instance(index, transform)` queues the move, and the next `commit()` leaves every other instance's triangles alone. It rewrites only the moved ones and refits the BVH's boxes in place instead of rebuilding it. It falls back to a full rebuild when the refitted tree gets loose (its bounds double) or after 240 refits in a row, so quality doesn't drift.

Gameplay queries: `Renderer::irradiance`, `light_level` (is the player hidden in shadow?) and `lit_by` (can this light see this spot?).

## Baking

```cpp
BakedLighting baked = bake(scene, LightingSettings::shadowed(), options);
baked.save("level1.bake");
renderer.set_baked(std::make_shared<BakedLighting>(baked));
```

Static instances (`Instance::is_static`) get per-vertex light and, if the mesh has `lightmap_uvs`, a lightmap. A probe grid is baked for moving objects. With `gi = baked`, baked lights are skipped at runtime and come from the bake, stationary lights stay live for direct light, and dynamic lights stay fully live.

## The GPU renderer

`fizmo::graphics::PointLight3D` now has `shape` (`Point`, `Sphere`, `Tube`, `Rectangle`, `Disk`), `size_x`, `size_y`, `axis` and `two_sided`, with `PointLight3D::sphere`, `tube`, `rectangle` and `disk` factories. The shaders light with the closest point on the shape (representative point), so highlights stretch along tubes and panels. `fizmo::graphics::to_scene_lighting(scene, settings)` turns a `fizmo::lighting::Scene` into a `SceneLighting3D` for the real-time renderer.

Sun shadows are cached in layers. A shadow map is reused as is while no caster changes. When some casters move, the casters that have stayed put for `SunShadow3D::stable_frames` frames (4 by default) are drawn once into a cached layer. Each frame copies that layer and draws only the moving casters on top. Set `SunShadow3D::layered = false` to always redraw the whole map. For shadows that only reach part of the view, see [visibility](visibility.md): real shadows inside `distance`, blobs past it.

### Light spread and spill

`PointLight3D::falloff` picks how a real-time light fades. `LightFalloff3D::Window` (the default, unchanged) is the old compact pool that fades to zero at `radius`. `LightFalloff3D::Physical` uses inverse-square falloff from `reference_distance` (the light is `intensity` bright there), windowed smoothly to zero at `radius`, so light keeps spreading across a room instead of stopping in a blob. `spill` (0 to 4) wraps light past the terminator and onto surfaces facing slightly away, which reads like light bouncing off nearby walls. `fizmo::lighting::Light` has `spread` (`natural` or `blob`) and `spill` (default 0.35); `to_point_light` maps `natural` to `Physical` with that spill.

### Reflections

`Material3D::reflectivity` (0 to 1) makes a PBR mesh reflect: 1 is a mirror, lower values a polished or wet look, scaled by roughness. Reflective meshes are drawn with `Blend3D::Alpha` so they come after the scene copy; with `SceneLighting3D::surfaces.screen_reflections` on they reflect what's on screen, and otherwise the sky. Combined with a vertex or material alpha below 1 this gives glass that both shows through and reflects at grazing angles.

### Water rendering cost

Liquid and glossy surfaces only run the screen-space reflection march when the Fresnel term is above about 3% (looking straight down at water skips it), and the march uses fewer steps for pixels far away. Mirrors always march. `surfaces.reflection_steps` is still the upper limit.

## Path tracing and photons on the GPU

`fizmo::graphics::GpuPathTracer` runs the `path_traced` and `photon` tiers on the GPU with compute shaders, on Vulkan and on OpenGL 4.3. It uploads the same compiled scene the CPU renderer uses (triangles, BVH, materials, every light shape, the sky), so the two agree: the tests render one room with both and check they match.

```cpp
fizmo::graphics::Renderer renderer(window);
fizmo::graphics::GpuPathTracer tracer;
tracer.attach(renderer, 640, 360);
tracer.set_settings(fizmo::lighting::LightingSettings::photon());
tracer.set_scene(scene);
tracer.set_camera(fizmo::lighting::Camera::look_at({0, 2, 5}, {0, 0.5, 0}));

while (window.is_open()) {
    renderer.begin_frame();
    tracer.draw(renderer, 0, 0, 1280, 720);
    renderer.present();
}
```

- Every frame adds `set_samples_per_frame(n)` samples (1 by default) to an HDR accumulation and tone maps it into `target()`, a normal `RenderTarget`. `set_max_samples(n)` stops once the image is clean, `set_paused(true)` freezes it.
- Moving: while the camera and scene stay still, samples keep adding up forever. When the camera moves or the scene changes, each pixel finds where its surface was last frame (reprojection with the first hit's position and normal) and keeps up to `history_cap` samples from there (`set_temporal(true, 24)`, the default), so moving around and playing stay smooth instead of starting from noise. Pixels that weren't visible before start fresh. `set_temporal(false)` starts over on every change, and `clear_history()` drops the history once.
- Denoising: with `settings.denoise` on, `denoise_passes` (1 to 6) passes of an edge-aware à-trous filter run on the lighting with the surface colour divided out, guided by each pixel's normal, position and brightness, so textures stay sharp and edges don't bleed. How much it blurs is set by how noisy each pixel really is (SVGF style): the tracer keeps each pixel's brightness variance alongside its samples (carried through reprojection like the samples), and while a pixel has few samples it estimates the variance from its neighbours. Clean areas such as sharp shadows from small lights aren't blurred, so shadows stay put; noisy areas are smoothed more. Its weight fades out by about 512 samples, where the plain image is already clean.
- The scene is watched: committing changes to it (`geometry_version`, `lighting_version`) re-uploads it on the next frame.
- What it does: next event estimation with multiple importance sampling for point, spot, sun (with its disk size), sphere, disk, rectangle and tube lights and emissive triangles; GGX metal and plastic; glass with Fresnel, roughness, Beer-Lambert absorption and thin windows; dispersion per channel, or real spectral transport with `Transport::spectral`; importance-sampled skies; height fog; Russian roulette; the firefly clamp; and every tone mapper.
- Photons: with `caustics` on (the `photon` preset), each frame first shoots `set_photons_per_frame(n)` photons (by default `photons`, up to 262144) from the lights through glass and mirrors into a hashed grid on the GPU, and camera paths gather them where they land. That is how a glass ball focuses a spot light into a bright point, and how a flint prism throws a spectrum on the floor, which plain path tracing can't find. The gather radius shrinks a little every frame, like progressive photon mapping.
- `read_hdr(image)` copies the accumulated radiance back into a `fizmo::lighting::Image`, so it can be saved or compared.
- IES profiles and cookies are CPU only for now; the GPU ignores them.
- `GpuPathTracer::wants_gpu(settings)` is true for the tiers it handles. `supported()` and `error()` say whether this device can run it (it needs compute shaders and storage buffers).

The shader is `Source/Graphics/Shaders/path_tracer.comp`. After editing it, rebuild its header with `fizmo-shaders -o Source/Graphics/gpu_path_tracer_shader.hpp -n fizmo::graphics::path_tracer_shader --include '"../GPU/device.hpp"' Source/Graphics/Shaders/path_tracer.comp`.
