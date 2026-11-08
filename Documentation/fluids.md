# Fluids (fizmo::physics)

Header: `Source/Physics/Fluid/fluid_3d.hpp` (included by `Includes/physics.hpp`).

A `FluidWorld3D` owns any number of `FluidVolume3D`s. Each volume has its own `FluidLevel`, `FluidMaterial` and settings, and they all step together. Volumes are axis boxes (`FluidVolumeDef::bounds`). "Up" is taken from gravity (or `FluidVolumeDef::up`), so y-up, z-up and other `Axes3D` setups all work.

## Levels

| Level | Feels like | Model | Cost |
|---|---|---|---|
| `blocky` | Minecraft | Voxel cellular automaton: sources (level 8), flowing levels 7..1 that drop by one per block, falls before spreading, heads for the nearest drop within `hole_search` blocks, optional infinite-source rule (two sources next to each other make a new one). Ticks every `tick_interval` game ticks (5 by default, so water ticks at 4 Hz). | Very low; only chunks that changed (16^3) tick |
| `still` | Garry's Mod | A flat surface (optionally bobbing) with no flow at all. Only provides buoyancy, drag and queries. | Almost free. Also the LOD fallback |
| `flowing` | Modded Minecraft ("Flowing Fluids") | Finite volume per cell (0..`max_mass`), mass is conserved exactly (face fluxes). Levels even out between neighbours, water drains down holes, and slight compression plus a confined-cell pressure solve lets it rise in U-bends. No infinite sources unless a cell is marked `set_source`. | Low; sleeping 16^3 chunks |
| `waves` | Somewhat realistic | 2D shallow-water heightfield (virtual pipe model): depth plus 4 outflow fluxes per column, so it flows and carries ripples. Bodies push water (ripples, wakes). Optional wind swell (analytic, costs nothing to simulate). Boundary and `set_wall` columns never spill over. | Medium; 16x16 tiles sleep when calm |
| `realistic` | Super realistic | Position Based Fluids particles on a dense uniform grid: XSPH viscosity, cohesion (surface tension), artificial pressure, optional vorticity confinement. Pours over walls and splashes. Two-way collisions with rigid bodies. | Highest; calm particles sleep |

## Materials

`FluidMaterial` presets: `water`, `sea_water`, `oil`, `honey`, `lava`, `mud`, `mercury`, `custom(density, viscosity, surface_tension)`.

| Field | Meaning |
|---|---|
| `density` | kg/m3; buoyancy, drag and particle mass |
| `viscosity` | dynamic viscosity (Pa s); see below |
| `surface_tension` | N/m; particle cohesion |
| `color` (`r,g,b,a`) | for renderers; `opacity()` returns `a` |
| `linear_drag` | multiplier on viscous (Stokes) drag, 6 pi mu L v |
| `quadratic_drag` | Cd in 0.5 rho Cd A v^2 |
| `angular_drag` | 1/s spin damping, scaled by the viscosity factor |
| `friction`, `restitution` | used for particle contacts (combined with the body collider: friction geometric mean, restitution max) |
| `stiffness` | 0..1; PBF stiffness and flowing compression |
| `temperature`, `emissive` | for gameplay and renderers (lava) |

`viscosity_factor()` = 1 + log10(mu / mu_water) (water 1, honey about 5, lava about 6). It changes every level that flows: blocky ticks are that many times slower and spread fewer blocks, flowing flux rates are divided by it, waves are damped by 2 nu k^2 + 3 nu / depth^2, and particles get XSPH viscosity of 0.02 + 0.15 log10(mu / mu_water).

## Rigid-body coupling

`world.step(sim, dt)` (call it before `sim.step(dt)`) handles every dynamic body overlapping a volume, using only the public `Simulation3D` and `RigidBody3D` API:

* Submerged volume and centre of buoyancy: spheres are analytic (spherical cap). Other convex colliders use a cached lattice of sample points (`FluidQualitySettings::buoyancy_samples` per axis), weighted to the shape's exact volume. Samples are smoothed across the surface, so a box floats at exactly the right depth.
* Buoyancy `-gravity * rho * V_sub` is applied at the centre of buoyancy, so bodies right themselves.
* Drag on the velocity relative to the local fluid (currents push things): linear (viscous) plus quadratic, plus `coupling.linear_damping` times the displaced fluid mass (this stands in for wave-radiation damping). Angular drag too. Drag is applied as an exact exponential impulse, so honey and lava can't overshoot.
* Bodies push fluid: occupancy raises the heightfield (`waves`), displaces cell water (`flowing`), and particles collide (`realistic`). In `waves`, `flowing` and `realistic` the forces are measured against the ambient level around the body, so a body's own displacement doesn't feed back into its buoyancy.
* `realistic`: particles collide with every collider type (sphere, capsule, hull, cylinder/cone/rounded box via GJK, plane, heightfield, mesh). The response uses the restitution and friction described above. The body gets the reaction impulse (impacts, friction, weight of particles on top). Buoyancy and drag come from the particle surface.

`world.forces_on(body)` computes the forces without applying them, and `world.body_forces(body)` returns what the last step applied. Both give a `FluidForces3D` with submerged volume and fraction, centre of buoyancy, buoyancy, drag, angular drag and fluid velocity. `coupling.layers` filters bodies. `require_buoyancy_flag` limits coupling to bodies with `PhysicsFlags::Buoyancy`.

## Queries and render data

All volumes: `surface_height(point)` / `surface_height(a, b)` (the two horizontal world axes), `surface_point`, `surface_normal`, `depth_at`, `velocity_at` (includes `def.current`), `inside`, `total_volume`. The world versions search all volumes. `volume_at(point)` returns the volume containing a point.

* Cells: `cell_fill()` (0..1+ per cell; blocky level/8), `cell_levels()` (blocky 0..8), `cell_solids()`, `cell_velocities()`. Index with `cell_index(i, j, k)`, where j is the up axis.
* Heightfield: `heights()` (ground + depth + body displacement + swell), `normals()`, `depths()`, `column_velocity_u/w()`.
* Particles: `positions()`, `velocities()`, `densities()`, `particle_radius()`, and the coarse `surface_grid()` (`surface_columns_x/z`, `surface_cell`). Particle order can change between steps because arrays are re-sorted for cache locality.

## Performance

* Sleeping: chunks and tiles only step when they or a neighbour changed. Particles sleep after `sleep_frames` frames under `sleep_speed` and wake from fast neighbours or moving bodies. Fully asleep neighbourhoods skip all work.
* Adaptive substeps: waves are CFL-limited by sqrt(g h), particles by their maximum speed. Both are capped by `max_substeps`; beyond the cap the step slows down rather than going unstable. Cell levels run at a fixed tick with `max_ticks_per_frame`. `particle_work_budget` caps particles x iterations x substeps.
* `FluidQuality` presets (`low/medium/high/ultra`) set `resolution_scale` (waves column size and particle radius), substeps, solver iterations, buoyancy samples, max particles and budgets. Use `world.set_quality(q)` before adding volumes.
* Threads: `config.threads` (0 = hardware). Work is split over chunks, tiles and particle blocks with a fixed decomposition, so results are bit-identical for any thread count.
* No per-step allocations in steady state: SoA arrays, an in-place counting sort into a dense grid, and preallocated neighbour lists (64 per particle).
* LOD: `set_focus(point)` with `lod_distance` steps far volumes every `lod_interval` frames, using the accumulated dt. Beyond `freeze_distance` a volume is frozen and keeps answering queries. `set_visible(false)` counts as far. `set_frozen(true)` freezes a volume by hand. For the cheapest fallback, swap a distant `waves` or `realistic` body of water for a `still` volume.

Benchmarks: `Testing/fluid_tests.cpp` prints ms/step for each level at a typical size.

## Example

```cpp
using namespace fizmo::physics;
Simulation3D sim;
FluidWorld3D fluids;
fluids.set_quality(FluidQuality::medium);

FluidVolumeDef lake = FluidVolumeDef::box(FluidLevel::waves, AABB3D(RVec3{ -32, -4, -32 }, RVec3{ 32, 1, 32 }), FluidMaterial::water(), 0.8_r);
lake.cell_size = 0.25_r;
lake.waves.swell_amplitude = 0.15_r;
FluidVolume3D& water = fluids.add_volume(lake);
water.voxelize(sim);

FluidVolume3D& pool = fluids.add_volume(AABB3D(RVec3{ 40, 0, 0 }, RVec3{ 44, 2, 4 }), FluidLevel::still, FluidMaterial::honey(), 0.9_r);

RigidBody3D& crate = sim.add_body(RVec3{ 0, 2, 0 });
crate.add_collider(Shape3D::make_box(0.5_r, 0.5_r, 0.5_r), Material3D(600.0_r));

for (;;) {
    fluids.set_focus(camera_position);
    fluids.step(sim, 1.0_r / 60.0_r);
    sim.step(1.0_r / 60.0_r);
    upload_heightfield(water.heights(), water.columns_x(), water.columns_z());
}
```

# 2D fluids (fizmo::physics)

Header: `Source/Physics/Fluid/fluid_2d.hpp` (included by `Includes/physics.hpp`). The shared types (`FluidLevel`, `FluidQuality`, `FluidPreset`, `FluidColor`, `FluidMaterial`, `FluidQualitySettings`, `FluidStillSettings`, `FluidFlowingSettings`, `FluidCouplingSettings`) live in `fluid_common.hpp`, which `fluid_3d.hpp` includes, so 2D and 3D use the same materials, presets and quality settings.

A `FluidWorld2D` owns `FluidVolume2D`s, made from `FluidVolumeDef2D` (`FluidVolumeDef2D::box(level, bounds, material, fill)`). Call `fluids.step(sim, dt)` before `sim.step(dt)` with a `Simulation2D`.

## Frame, units and index order

* "Up" comes from gravity (or `FluidVolumeDef2D::up`) when the volume is made, snapped to +x, -x, +y or -y, and stays fixed. `right()` is +x when up is along y, +y otherwise.
* Local coordinates: x along `right()`, y along `up()`, origin at the bounds corner that is lowest along both. With the default y-up gravity, local = world - `bounds.min`.
* Cells: `cell_index(i, j) = j * cells_x() + i`, `i` along `right()`, `j = 0` is the bottom row. Columns (`waves`): index `i` along `right()`.
* `realistic` uses the full gravity vector (in local axes) every step, so tilting gravity sloshes the fluid. The other levels only use its size.
* 2D densities are per area. A body with `Material(500)` floats half under in water (1000).

## Levels

| Level | Feels like | Model |
|---|---|---|
| `blocky` | Powder Toy / Noita | Falling-sand cellular automaton with 13 cell materials, reactions and optional heat. 32x32 chunks with dirty rectangles. |
| `still` | Flat pool | Flat surface (optional bob). Buoyancy, drag and queries only. |
| `flowing` | Modded Minecraft | Finite fill per cell, mass exact. Falls first, then spreads sideways with momentum on the face fluxes, compression pushes up. 16x16 chunks sleep. |
| `waves` | Water line in a side view | 1D shallow-water columns (virtual pipes) with walls, ground, ripples from bodies and an optional analytic swell. 32-column tiles sleep. |
| `realistic` | Particle sandbox | FLIP/APIC particles on a MAC grid with a variable-density pressure solve. Many fluids at once, viscosity, cohesion, splashes and two-way bodies. |

## Realistic (FLIP / APIC)

Grid cell = `particles.radius / 0.3` (or `cell_size * resolution_scale` when the radius is 0). Particles are seeded on a lattice with spacing `2 r`, about 2.8 per cell (`rest_density()`).

Each substep: counting sort into cells, push-apart to `2 r` plus cohesion inside `3 r` (same material only), particle-to-grid (bilinear, or APIC affine), cell classification (small enclosed air pockets of at most `bubble_cells` cells are collapsed), moving-body cells get the body velocity, gravity on the grid, implicit viscosity (Gauss-Seidel, no-slip at solids), pressure (PCG with a MIC(0) preconditioner in double, warm-started, relative tolerance 1e-3, capped at `20 * solver_iterations` iterations), density correction (over-full cells push out, thin interior cells pull in), grid-to-particle (per-material FLIP blend, or APIC), a sub-grid drift so lighter particles rise through heavier ones in the same cell, then advection with the projected velocity, a swept test against painted walls, and collisions.

Substeps come from the 99.5th-percentile particle speed with `cfl` (default `4 * quality.cfl`, so 2 cells per substep at medium), capped by `max_substeps`. Faster particles are clamped to the cap.

* Materials: id 0 is `def.material`. `add_material(m)` returns ids up to 254. Each particle stores a material id and a packed RGBA8 colour (`fluid_pack_color`, R in the low byte).
* Viscosity: diffusion rate per second = `viscosity_scale * (1.8 (f - 1)^3 + nu / dx^2)`, where `f = viscosity_factor()`. Water is inviscid, oil about 12/s, mud 65/s, honey 130/s, lava 225/s. The FLIP blend drops to `flip_ratio * (1 - 0.12 (f - 1))`, so viscous fluids are calmer too.
* Cohesion: `surface_tension / 0.072 * 0.02` (at most 0.25) unless `particles.cohesion >= 0`.
* Layering: the pressure solve uses face densities, so oil floats on water and honey or lava sinks.
* `color_mixing` (per second) blends particle colours inside each cell, for paint-mixing games.
* Sleeping: when the 99th-percentile speed drops below `settle_speed / (largest viscosity factor)`, velocities are damped at `settle_rate`. After `sleep_frames` frames under `sleep_speed` the volume sleeps (no work). Brushes, emitters, painting and moving bodies wake it.

### Sandbox API

| Call | Effect |
|---|---|
| `add_fluid_circle(center, radius, velocity, material, color)` | Pour on a lattice. Skips spots that already have a particle within 0.7 spacing, so holding the brush in one place never over-fills. (`blocky`: paints `sand.fill_cell`; `flowing`: fills cells.) |
| `add_particle`, `add_particle_box` | Single particles or a lattice box |
| `remove_fluid_circle(center, radius)`, `remove_material(id)`, `clear_particles()` | Erase |
| `push(center, radius, velocity, strength)` | Velocity brush (also pushes `waves`) |
| `swirl(center, radius, spin, strength)` | Stir |
| `explode(center, radius, strength)` | Radial kick. `FluidWorld2D::explode(sim, ...)` also kicks bodies. (`blocky`: blasts cells to smoke and fire.) |
| `add_emitter(FluidEmitter2D)` | Nozzle: `rate` particles per second (cells per second for `blocky`) |
| `set_solid`, `set_solid_box`, `paint_solid(center, radius, solid)` | Static walls in the grid. Fluid inside fresh walls is pushed out or removed. |
| `build_static_body(sim)` | Makes (or refills) a static `RigidBody2D` with merged box colliders for the solid cells, so bodies collide with painted walls. `solid_rectangles(out)` gives the rectangles. `voxelize(sim)` marks cells covered by static bodies as solid. |

### Render data

`positions()` and `velocities()` (world, cached), `local_positions()`, `local_velocities()`, `material_ids()`, `colors()`, `particle_radius()`, `particle_spacing()`, `particle_mass(id)`. Grid: `cell_types()` (0 air, 1 fluid, 2 solid), `cell_densities()` (particles per cell), `grid_velocity_u()` (`(cells_x + 1) * cells_y`, index `j * (cells_x + 1) + i`, face at `x = i * dx`), `grid_velocity_v()` (`cells_x * (cells_y + 1)`, index `j * cells_x + i`), `pressure()`. Particle order changes every substep because the arrays are sorted by cell.

## Blocky (falling sand)

`SandCell2D` values and order (also the byte values in `sand_cells()`):

| Value | Cell | Kind |
|---|---|---|
| 0 | `empty` | empty |
| 1 | `wall` | solid |
| 2 | `sand` | powder |
| 3 | `water` | liquid |
| 4 | `oil` | liquid (flammable, floats on water) |
| 5 | `lava` | liquid (slow, hot, glows) |
| 6 | `steam` | gas (condenses back to water) |
| 7 | `smoke` | gas (fades) |
| 8 | `fire` | fire |
| 9 | `ice` | solid (melts) |
| 10 | `stone` | powder (steeper piles) |
| 11 | `wood` | solid (flammable) |
| 12 | `acid` | liquid (dissolves sand, stone, wood, ice) |

`sand_cell_info(cell)` gives kind, density, dispersion, flammability, colour and so on. Rules: powders fall, slide and sink through liquids; liquids fall, slide and spread up to `dispersion` cells when there is a drop ahead or the same liquid behind (so puddles settle and sleep); denser liquids sink through lighter ones; gases rise; fire flickers, clings to fuel, ignites wood and oil, turns to smoke, and water puts it out (steam). Lava with water makes stone and steam, ignites wood and oil and melts ice. Acid eats sand, stone, wood and ice.

`sand.heat = true` adds a temperature per cell (`cell_temperatures()`): conduction, lava, fire and ice as sources, and phase changes (water boils above 100, freezes below -2, ice melts above 1, lava cools to stone below 650, stone melts above 1100, wood and oil ignite above 300 and 250). Without heat, `add_heat(center, radius, degrees)` boils, melts, ignites or freezes directly.

Arrays (index `j * cells_x() + i`): `sand_cells()`, `cell_life()` (fire: bit 7 set while burning on fuel), `cell_shade()` (a random byte per cell that moves with it, for colour noise), `cell_temperatures()`. `sand_cell_color(cell, shade)` gives a packed colour.

Ticks run at `sand.tick_rate` (60 Hz) with `max_ticks_per_frame`. Only dirty rectangles inside 32x32 chunks are processed, rows bottom to top with alternating direction. With threads, chunks run in four checkerboard phases; results are identical for any thread count. Bodies block cells, and cells they cover are moved to the nearest free cell. Liquids and powders hold up bodies (powders act like a dense fluid with high drag).

## Flowing, waves and still

* `flowing`: `cell_fill()` (0..`max_mass`, a bit more when compressed), `cell_velocities()`, `set_source`, `set_amount`, `fill_box`. `FluidFlowingSettings::momentum` (used by 2D only) keeps part of each face flux between ticks, so water travels instead of slowly diffusing. Bodies move water out of the cells they cover. Limitation: pressure is not carried sideways through completely full rows, so two levels separated by a submerged obstacle even out only slowly.
* `waves`: `heights()` (ground + depth + body occupancy + swell), `depths()`, `column_velocities()`, `set_ground`, `set_wall`, `set_wall_box`, `set_depth`, `disturb`. Bodies add their submerged area as occupancy, measured against a reference level that follows the water beside the body over `waves.reference_time` and blended in at `waves.occupancy_response` per second. This keeps light bodies stable while still making ripples and wakes.
* `still`: `set_still_level`, optional bob.

## Rigid-body coupling

`world.step(sim, dt)` walks `sim.bodies()` (no allocation), filtered by `coupling.layers`:

* Buoyancy `-g * sum(rho_local * submerged area)` at the centre of buoyancy. Circles use the exact circular segment. Other shapes use a cached sample lattice scaled to the exact area.
* `flowing`, `blocky` and `realistic` measure against the surface beside the body, with the density of the fluid at each height, so a body floats on the layer it is lighter than. `waves` uses the column heights. `still` uses the flat level.
* Drag as in 3D (viscous + quadratic + `linear_damping` times displaced mass, applied as an exact exponential impulse) plus angular drag. `still`, `flowing` and `blocky` also get `heave_damping` (fraction of critical damping, default 0.3), because they have no waves to carry the bobbing energy away.
* `realistic`: bodies are drawn into the grid as moving solids, so they push fluid. Particles collide with every collider type (circle, polygon, capsule, edge) using combined friction and restitution, and the bodies get the reaction impulses (clamped so light bodies cannot overshoot).
* `world.forces_on(body)` and `world.body_forces(body)` return a `FluidForces2D` (submerged area and fraction, centre of buoyancy, buoyancy, drag, angular drag, fluid velocity, fluid density, heave damping).

## Rendering helpers

`FluidRenderer2D` makes an RGBA8 image (`width * height * 4` bytes) that can be uploaded as a texture. With `flip_y` (default) row 0 is the top of the region.

* `render(volume, settings)`: `realistic` volumes go to `render_particles`, everything else to `render_cells`.
* `render_particles(world_positions, packed_colors, particle_radius, rest_count, settings)` works for any particle set. Each particle is splatted with a `(1 - q^2)^2` kernel of radius `kernel_radius` (default `2.4 r`), normalised so fluid at rest has density 1 (`rest_count` is particles per `(2r)^2` area, 1 for a fluid volume). Colour is the kernel-weighted average of the particle colours, so mixed fluids blend. Alpha is `smoothstep(threshold +- softness / 2)` times the colour alpha times `opacity`, with optional darkening near the surface (`shade_edges`, `edge_darken`), composited over `background`, optionally premultiplied.
* `render_cells(volume, settings)` draws sand cells (with shade noise), flowing fill, or the water below the surface for `waves` and `still`. `width = 0` gives one pixel per cell or column.
* `density()` is the density field (same size and row order) and `contour(threshold, segments)` runs marching squares over it, giving world-space segment pairs for outlines.

## Performance

* No allocation once running: SoA arrays, counting sort into the grid, compact fluid-cell lists for the PCG and viscosity solves, reused scratch buffers.
* `config.threads`: the particle parts (separation by alternating row phases, collisions, grid-to-particle, advection) and sand chunks run on the pool. Results are bit-identical for any thread count.
* Sleeping: sand chunks with empty dirty rectangles, flowing chunks and wave tiles that stopped changing, and whole particle volumes when calm.
* Fixed ticks for cell levels, CFL substeps for waves and particles, all capped (`max_ticks_per_frame`, `max_substeps`). Every iterative solver has a fixed iteration cap.
* LOD like 3D: `set_focus`, `lod_distance`, `lod_interval`, `freeze_distance`, `set_visible`, `set_frozen`.

Benchmarks: `Testing/fluid_2d_tests.cpp` (run with `bench` for benchmarks only, `quick` for tests only).

Measured on a 2-core VM (float, `-O2`, medium quality, ms per 1/60 s frame). The particle benchmark is a full-height dam break in a 4 x 2 m box, the worst case for substeps; "settling" is the same tank a few seconds later.

| Case | 1 thread | 2 threads |
|---|---|---|
| realistic, 5k particles | 2.6 (settling 1.4) | |
| realistic, 20k particles | 20.1 (settling 13.4) | 16.3 (settling 10.7) |
| realistic, 50k particles | 68.9 (settling 46.0) | 59.8 (settling 36.9) |
| blocky, 512x512 cells, sand and water pouring | 3.5 | 3.3 |
| blocky, 512x512 cells, settled | 0.9 | |
| flowing, 256x128 cells | 0.15 | |
| waves, 4096 columns | 0.04 | |

The pressure solve and particle-to-grid transfer are single-threaded, so a second thread helps the particle level by about 20%. `FluidQuality::low` (fewer substeps and iterations, and a coarser grid when `particles.radius` is 0) trades accuracy for speed; a larger `particles.radius` is the biggest lever.

## Example

```cpp
using namespace fizmo::physics;
Simulation2D sim;
FluidWorld2D fluids;

FluidVolumeDef2D def = FluidVolumeDef2D::box(FluidLevel::realistic, AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 4.0_r, 2.0_r }));
def.particles.radius = 0.008_r;
FluidVolume2D& tank = fluids.add_volume(def);
const std::uint8_t oil = tank.add_material(FluidMaterial::oil());
tank.add_particle_box(AABB(RVec2{ 0.0_r, 0.0_r }, RVec2{ 1.5_r, 0.6_r }));
tank.paint_solid(RVec2{ 2.0_r, 0.2_r }, 0.15_r);
tank.build_static_body(sim);

FluidVolumeDef2D sand_def = FluidVolumeDef2D::box(FluidLevel::blocky, AABB(RVec2{ 5.0_r, 0.0_r }, RVec2{ 10.12_r, 5.12_r }));
sand_def.cell_size = 0.01_r;
FluidVolume2D& sandbox = fluids.add_volume(sand_def);

FluidRenderer2D renderer;
FluidImageSettings2D image;
image.width = 800;

for (;;) {
    if (mouse_down) tank.add_fluid_circle(mouse, 0.05_r, RVec2{}, right_button ? oil : 0);
    if (sand_brush) sandbox.paint_cells(mouse, 0.05_r, SandCell2D::sand, 0.5_r, false);
    fluids.step(sim, 1.0_r / 60.0_r);
    sim.step(1.0_r / 60.0_r);
    upload_rgba(renderer.render(tank, image), renderer.width(), renderer.height());
    upload_rgba(renderer.render_cells(sandbox, FluidImageSettings2D{ 0 }), renderer.width(), renderer.height());
}
```
