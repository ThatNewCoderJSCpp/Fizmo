# Particles (fizmo::particles)

Headers: `Source/Graphics/Particles/particles.hpp` (included by `Includes/custom_graphics.hpp`, or on its own through `Includes/particles.hpp`). The sources are part of the `fizmo::graphics` component.

| File | Contents |
|---|---|
| `particle_settings.hpp` | `EmitterSettings`, curves, enums, presets, `to_text` / `from_text` |
| `particle_emitter.hpp` | `Emitter` (one structure-of-arrays particle pool), colliders, `CollisionBatch` |
| `particle_system.hpp` | `ParticleSystem` (budget, LOD, culling, threads, fixed step, pooled effects), `CameraView` |
| `particle_draw.hpp` | Draw lists for 3D and 2D, and helpers that draw them with `graphics::Renderer` |

The older `graphics::ParticleEmitter` / `ParticleConfig` (`Source/Graphics/particles_2d.hpp`) is unchanged and still works. New code should use `fizmo::particles`.

Simulation data is `float`. Positions are stored relative to the system origin (`ParticleSystem::set_origin`, a double vector), so large worlds keep full precision near the camera. Rebasing shifts every particle once.

## Quick start

```cpp
using namespace fizmo;
using namespace fizmo::particles;

ParticleSystem fx;
fx.add_ground(0.0);

Emitter& torch = fx.create_emitter(preset(ParticlePreset::Fire), { 2.0, 0.0, -4.0 });
fx.create_emitter(preset(ParticlePreset::Embers), { 2.0, 0.2, -4.0 });

const int boom = fx.register_effect(preset(ParticlePreset::Explosion), 16);

DrawList3D draw_list;
draw_list.reserve(50000);

void frame(graphics::Renderer& r, const graphics::Camera3D& camera, double dt) {
    if (hit) fx.spawn_effect(boom, hit_point);
    torch.set_position(player_hand);

    const CameraView view = CameraView::from_camera(camera);
    fx.step(dt, view);

    r.begin_3d(camera);
    build_draw_list(fx, view, draw_list);
    draw(r, draw_list);
    r.end_3d();
}
```

For 2D (y down, pixels): `preset(ParticlePreset::Fire, ParticleDim::D2, 50.0f)`, `fx.step(dt, view_of(renderer))` (or `CameraView::from_rect(x0, y0, x1, y1)`), and `draw_particles_2d(renderer, fx, list2d)`.

## Emitter API

| Call | Meaning |
|---|---|
| `set_position(x, y, z)` / `set_position(vector3d)` | Move. Spawns between the old and new position are spread along the path, so fast emitters leave continuous trails. |
| `teleport(p)` | Move without a trail and without a velocity spike |
| `set_orientation(forward, up)`, `set_basis(right, up, forward)`, `set_angle(degrees)` (2D) | Orientation. The emission direction and shapes are in the emitter's local frame. |
| `set_velocity(v)` / `clear_velocity()` | Velocity used by `inherit_velocity`. Without it the velocity is measured from movement. |
| `play()`, `stop()`, `stop_and_clear()`, `pause(bool)`, `restart()`, `clear()` | Playback. `stop` lets the live particles finish. |
| `burst(n)` | Spawn `n` now (returns how many fit). This is immediate, so it is limited by `max_particles` but not by the system budget. Use settings `bursts` for budgeted bursts. |
| `count()`, `total_count()` (with sub-emitters), `capacity()`, `bounds()`, `potential_bounds()` | Queries. Bounds are world-space doubles. |
| `playing()`, `finished()`, `sleeping()` | `finished` = not playing, no particles, no pending sub-emitter events |
| `step(dt)` | Standalone update when no `ParticleSystem` is used |
| `warm_up(seconds, max_steps)` | Simulate ahead in a few coarse steps |
| `set_texture(const graphics::Texture*)` | Texture for the fizmo renderer (the settings only store `texture` / `texture_id` for engines) |
| `set_emission_mesh(xyz, triangle_count)` | Triangles (9 floats each, local space) for `EmissionShape::Mesh` |
| `set_collision_callback(fn, user)` | `void fn(void* user, CollisionBatch& batch)`, called once per batch of up to 4096 particles |
| `view()` | Raw `ParticleView` arrays (px, py, pz, vx, vy, vz, age, inv_life, size, rotation, rnd) |
| `child(i)`, `child_count()` | Sub-emitters (their own `Emitter`s) |

## ParticleSystem API

| Call | Meaning |
|---|---|
| `ParticleSystem(SystemSettings)`, `set_settings`, `set_budget`, `set_threads(n)` (0 = hardware) | Setup |
| `create_emitter(settings[, position])` | Persistent emitter. With `auto_play` it starts (and prewarms) on the next `step`, so you can position it first. |
| `destroy(emitter)` | Remove |
| `register_effect(settings, preallocate)` → id | A pooled one-shot effect type |
| `spawn_effect(id, position, forward, up, velocity)`, `spawn_effect_2d(id, x, y, angle)` | Fire and forget. Finished instances go back to the pool, so spawning costs no allocation. |
| `step(dt, view)` | Update everything |
| `add_ground(y)`, `add_plane(normal, point)`, `add_sphere(c, r)`, `add_box(lo, hi)`, `clear_colliders()` | World colliders (with a `layers` mask matched against `collision_mask`) |
| `set_origin(origin)` | Floating origin / rebase |
| `particle_count()`, `stats()`, `quality()`, `root_count()`, `root(i)` | Queries. `ParticleStats` has particles, active / sleeping / offscreen / paused / distance-culled emitters, spawned, limited (spawns refused by the budget), `budget_scale`, `quality`, `substeps` and `step_ms`. |

`CameraView`: `from_camera(Camera3D)` (frustum, LOD distance, pixel size), `at(position, forward, up)` (distance LOD only), `from_rect(x0, y0, x1, y1)` (2D), `none()` (no culling, everything at full detail).

## Settings (EmitterSettings)

All fields are plain values. `to_text(settings)` writes `key: value` lines and `from_text(text, settings, &error)` reads them back exactly. Enum values are written by name. Unknown keys are ignored. Bad values are reported in `error`, and the other keys still load. Bursts are written as `burst.N: time count cycles interval probability`, and sub-emitters as `sub.N.trigger`, `sub.N.sub_count`, `sub.N.sub_probability`, `sub.N.sub_inherit_velocity` and `sub.N.<any field>`. Sub-emitters can nest up to 4 levels.

| Group | Fields |
|---|---|
| General | `name`, `dimension` (`3d`/`2d`), `space` (`world`/`local`), `max_particles`, `priority` (higher wins under budget), `seed` (0 = derived), `lod_bias` (2 = treated as half as far away) |
| Timing | `duration` (0 = endless), `loop`, `start_delay`, `prewarm`, `auto_play`, `rate` (per second), `rate_per_distance` (per unit moved), `bursts` (`time`, `count`, `cycles` (0 = forever), `interval`, `probability`) |
| Spawn | `lifetime` (range), `shape` (`point`, `sphere`, `hemisphere`, `cone`, `box`, `circle`, `ring`, `line`, `mesh`), `shape_radius`, `shape_thickness` (1 = volume, 0 = surface), `shape_arc` (degrees), `shape_size` (box / line extents), `direction` (local), `direction_mode` (`axis`, `outward`, `random`), `spread` (cone half angle in degrees), `speed` (range), `inherit_velocity`, `size`, `rotation`, `spin` (degrees, degrees/s), `color`, `color_alt` (each particle picks a colour between the two) |
| Forces | `gravity`, `gravity_scale` (negative = buoyant), `drag` (1/s, exact exponential), `wind` (velocity that drag pulls toward, so wind needs `drag > 0`), `turbulence`, `turbulence_frequency`, `turbulence_speed` |
| Over life | `size_over_life`, `alpha_over_life`, `speed_over_life` (scales movement), `spin_over_life` (`LifeCurve`, up to 8 keys), `color_over_life` (`LifeGradient`, up to 8 keys). The text form is `t v; t v` and `t r g b a; ...`. |
| Render | `render_mode` (`billboard`, `stretched`, `horizontal`, `vertical`), `stretch` (seconds of velocity added to the length), `blend` (`alpha`, `additive`, `premultiplied`), `lit`, `soft`, `soft_distance`, `near_fade` (fade out within this distance of the near plane), `sort` (`auto`, `none`, `approximate`, `exact`), `frames_x`, `frames_y`, `frame_count`, `flipbook` (`over_life`, `fps`, `random`), `frame_fps`, `random_start_frame`, `texture_id`, `texture` |
| Collision | `collide`, `bounce`, `friction`, `kill_on_collision`, `collision_radius` (fraction of size), `min_bounce_speed` (slower impacts stop bouncing), `collision_mask` |
| Culling | `offscreen` (`auto`, `simulate`, `throttle`, `pause`), `cull_radius` (0 = estimated from speed, lifetime, gravity and shape) |
| Sub-emitters | `sub_emitters` (`trigger`: `death`, `collision` or `birth`; `count` per event; `probability`; `inherit_velocity`; `settings`) |

The 3D shape frame: the emission axis is `direction` (default +Y). `circle`, `ring` and the `cone` base lie in the plane across that axis, and `hemisphere` faces it. In 2D everything is in the XY plane, and `to_2d(settings, pixels_per_unit)` converts 3D settings (scales lengths, flips y for a y-down screen, turns `sphere` into `circle`).

Sub-emitters are event driven: each event spawns `count` particles (each with `probability` times the LOD / budget spawn scale) at the parent particle's position, with `inherit_velocity` times its velocity added. Their own `rate` and `bursts` are ignored. They always simulate in world space.

Collisions work in world space (`space = world`). They cover the system's planes, spheres and boxes, and the per-emitter callback. In the callback you can move particles, change velocities and set `hit[i]` to `kHitBounce` (counts as a collision for sub-emitters) or `kHitKill`. The callback runs on worker threads when threads are enabled.

## Presets

`preset(ParticlePreset, ParticleDim = D3, pixels_per_unit = 50)`. 3D presets use metres with y up. 2D presets use pixels with y down.

| Preset | Look | Notes |
|---|---|---|
| `sparks` | 40-spark burst, additive, stretched | bounce on colliders, smoke puff sub-emitter on collision |
| `smoke` | soft rising grey puffs | lit, sorted, turbulence, near fade |
| `fire` | additive flames | rises, turbulence |
| `explosion` | 60 fireballs outward, additive | sparks on birth, smoke on death, priority 2 |
| `dust` | slow motes in an 8x2x8 box | prewarmed, unsorted, priority -1 |
| `rain` | 600/s streaks over 30x30 | kills on collision, splash sub-emitter, prewarmed |
| `snow` | 150/s flakes drifting with wind | kills on collision |
| `magic_sparkle` | additive sparkles on a sphere shell | |
| `debris` | 25 tumbling chips | bounce with friction, lit |
| `bubbles` | rising bubbles | |
| `steam` | cone of white puffs | |
| `embers` | slow glowing specks | |
| `impact` | blood-free hit puff | chips sub-emitter on birth |

## Rendering

`build_draw_list(system, view, list)` fills a `DrawList3D`. You get one `DrawBatch3D` per emitter with particles on screen, holding the blend, lit, soft, texture (`texture`, `texture_id`, `texture_name`) and the range in the list's arrays. `draw(renderer, list)` sends each batch to the fizmo renderer as one draw:

* `DrawPath::Instanced`: camera-facing billboards with no rotation, flipbook or stretch use one `Renderer::draw_instances` call. That is one `Instance3D` (24 bytes) per particle, on a shared 4-vertex quad built from the camera basis (`list.billboard`), which is only rebuilt when the camera turns. All view-plane billboards share one orientation, so this needs no special shader.
* `DrawPath::Triangles`: rotation, flipbook frames, stretched (velocity-aligned), horizontal (flat, ground-aligned with Y up) and vertical (cylindrical) billboards are expanded on the CPU into one `draw_triangles_3d` call (6 `Vertex3D` per particle).
* `DrawPath::Sprites` (`DrawOptions::sprites = true`): an engine-neutral `ParticleSprite` per particle (position, size, stretch axis, rotation, uv rectangle, rgba, depth). Use it with a custom GPU billboard shader.

`particle_material(batch)` gives the `Material3D` used: blend alpha or additive, depth test on, depth write off, cull none, shadows off, fog on, and the batch's texture or `default_particle_texture()`. `premultiplied` is drawn as alpha, with the colours premultiplied on the CPU, because `Material3D` has no premultiplied blend. `soft` / `soft_distance` are passed through for engines with a depth-aware shader. The fizmo renderer does not read them, so use `near_fade` to stop particles cutting into the camera.

2D: `build_draw_list_2d(system, view, list2d)` gives `Vertex2D` triangles and one `DrawBatch2D` per emitter (`graphics::BlendMode::Normal` or `Add`). `draw(renderer, list2d)` draws each batch with one `draw_vertices` call inside a `BlendScope`, so the current 2D camera transform applies.

## Optimizations and their knobs

| Optimization | What it does | Knobs |
|---|---|---|
| Structure of arrays | 12 float arrays plus one byte array per emitter, allocated once at `max_particles`. Tight loops with separate passes per feature, so unused features cost nothing. No per-particle heap memory or `std::function`. | `max_particles` |
| Swap-remove | Dead particles are replaced by the last particle (O(1)), in one pass | |
| Hard budget | Before each tick every updating emitter gets a spawn quota (in priority order, then creation order). The sum of live particles can never exceed `budget`. | `SystemSettings::budget` |
| Soft budget by priority | Each step the expected steady-state count (rate x lifetime x LOD scale) is summed per priority tier. Tiers that don't fit in `budget x quality x soft_budget` get their spawn rates (and burst sizes) scaled down, lowest priority first. Frames don't drop and you don't hit the hard cap all at once. | `soft_budget` (0.85), `min_spawn_scale`, `EmitterSettings::priority` |
| Adaptive quality | Optional. Measures `step` time (EMA) and lowers `quality` (which scales the budget) when over `target_ms`, then recovers slowly. | `adaptive`, `target_ms`, `min_quality` |
| Distance LOD | 4 levels by distance from the camera to the emitter's bounds (divided by `lod_bias`). Each level scales spawns, updates every N ticks with the summed dt, and can switch to simple rendering (instanced, no rotation or flipbook). | `lod[4] = { distance, spawn_scale, update_interval, simple_render }` (defaults 25/50/100/200 m, 1/0.6/0.3/0.12, every 1/2/3/4 ticks), `cull_distance` (300) |
| Staggered updates | Emitters on interval N update on different ticks (by creation order), so load spreads evenly | automatic |
| View culling | Emitter bounds (live particle box, plus the reach of a playing emitter) are tested against the frustum (3D) or view rect (2D). Off-screen emitters are not drawn. `auto`: looping emitters pause, and one-shots are throttled (update every `offscreen_interval` ticks, far-LOD spawn rate) so they still finish. | `offscreen`, `cull_margin`, `offscreen_interval` (6), `cull_radius` |
| Catch-up / warm-up | A paused emitter that comes into view simulates the missed time in at most `catch_up_steps` coarse steps (one-shots up to `max_catch_up` s). Looping emitters gone longer than a lifetime are cleared and warmed up for one lifetime instead, so the cost is bounded. `prewarm` does the same on `play`. | `catch_up_steps` (6), `max_catch_up` (2 s), `prewarm` |
| Sleeping | Stopped emitters with no particles, and paused ones, are skipped completely | |
| Fixed timestep | `fixed_step` (1/60) with an accumulator and at most `max_substeps` (4) per frame. Extra time is dropped rather than causing a spiral. Leftover time is used to extrapolate positions when drawing, so 144 Hz looks smooth with a 60 Hz simulation. `fixed_step = 0` uses variable steps. | `fixed_step`, `max_substeps`, `max_frame_dt`, `extrapolate`, `DrawOptions::max_extrapolation` |
| Threads | Each tick has two parallel phases: integration of 4096-particle chunks from all emitters, then per-family finalize (kill, sub-emitter events, spawn). Results are bit-identical for any thread count (tested). Work items are plain function pointers, so there is no allocation. | `threads` (1; 0 = hardware) |
| Cheap turbulence | A 16^3 tiling table of divergence-free (curl) noise, trilinear lookup, scrolling over time. It is evaluated on alternating halves of the particles each tick with a doubled impulse. | `turbulence`, `turbulence_frequency`, `turbulence_speed` |
| Baked curves | Size, alpha, speed, spin and colour curves are baked into 128-entry tables, and colour into packed RGBA tables (16 tint levels when `color != color_alt`). Rendering a particle is a few table lookups. | |
| Draw culling | Particles that are dead, transparent, behind the near plane or smaller than `min_pixel_size` pixels are skipped | `SystemSettings::min_pixel_size` (0.75), `DrawOptions::min_pixel_size` |
| Near fade | Particles fade out near the camera instead of filling the screen (big overdraw) | `near_fade` |
| Sorting only when needed | Additive emitters are never sorted. Alpha / premultiplied use an O(n) 16-bit radix sort on view depth (`approximate`, the `auto` default), or `exact` (std::sort). `none` turns sorting off (snow, dust). | `sort` |
| One draw per emitter | Instanced billboards or one triangle list per emitter. The billboard quad is only re-uploaded when the camera basis changes. | `DrawOptions::allow_instancing` |
| Effect pools | `register_effect` + `spawn_effect` reuse finished emitters (arrays, sub-emitters, curve tables) | `preallocate` |
| No allocation in steady state | Task lists, quotas, sort scratch and draw lists keep their capacity between frames (`DrawList3D::reserve`). The test checks 0 allocations per frame with threads, sub-emitters, collisions and pooled effects. | |

## Recommended budgets

| Target | `budget` | Notes |
|---|---|---|
| Desktop, 1 thread | 100k to 200k | about 2.5 ms update + 3 ms draw build for 200k on a 2.3 GHz Xeon |
| Desktop, 4+ threads | 300k+ | update scales with threads. Draw build is single threaded. |
| Laptop / integrated GPU | 50k | fill rate (overdraw) usually limits first: keep big smoke counts low and use `near_fade` |
| Mobile / Android | 15k to 30k | use `lod_bias < 1` for ambient effects and `sort: none` where you can |

Give gameplay-critical effects (muzzle flashes, hits) a higher `priority` than ambient ones (dust, rain: `-1`), so the ambient effects are thinned first.

## Benchmarks

`Testing/particle_tests.cpp` (`FIZMO_BUILD_PARTICLE_TESTS=ON`, target `fizmo-particle-tests`; pass `--no-bench` to skip). 4 emitters with gravity, drag, turbulence and a size curve, alpha blended (sorted), Release, Intel Xeon 2.3 GHz:

| Particles | Threads | step | draw list (sorted) |
|---|---|---|---|
| 50k | 1 | 0.61 ms | 0.60 ms |
| 200k | 1 | 2.48 ms | 2.93 ms |
| 200k | 2 | 1.49 ms | 2.88 ms |

Without turbulence the update is about 4.7 ns per particle. An over-subscribed scene (20 emitters asking for 2M particles with a 100k budget) stays at 85k live particles (the soft budget) for 1.1 ms per step.
