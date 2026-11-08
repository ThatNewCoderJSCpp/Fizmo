#ifndef FIZMO_PHYSICS_FLUID_2D_HPP
#define FIZMO_PHYSICS_FLUID_2D_HPP

#include "../Rigid Body/simulation_2d.hpp"
#include "fluid_common.hpp"
#include "fluid_pool_3d.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <vector>

namespace fizmo {
namespace physics {

enum class SandCell2D : std::uint8_t {
    empty = 0,
    wall,
    sand,
    water,
    oil,
    lava,
    steam,
    smoke,
    fire,
    ice,
    stone,
    wood,
    acid,
    count
};

enum class SandKind2D : std::uint8_t {
    empty = 0,
    solid,
    powder,
    liquid,
    gas,
    fire
};

struct SandCellInfo2D {
    SandKind2D kind = SandKind2D::empty;
    Real density = 0.0_r;
    int dispersion = 0;
    Real move_chance = 1.0_r;
    Real slide_chance = 1.0_r;
    Real flammability = 0.0_r;
    int life_min = 0;
    int life_max = 0;
    Real base_temperature = 20.0_r;
    bool emits_heat = false;
    Real viscosity = 1.0e-3_r;
    FluidColor color{};
    bool glows = false;
};

const SandCellInfo2D& sand_cell_info(SandCell2D cell) noexcept;
const char* to_string(SandCell2D cell) noexcept;
const char* to_string(SandKind2D kind) noexcept;
std::uint32_t sand_cell_color(SandCell2D cell, std::uint8_t shade = 128) noexcept;

enum class FluidTransfer2D : std::uint8_t {
    flip = 0,
    apic
};

const char* to_string(FluidTransfer2D transfer) noexcept;

std::uint32_t fluid_pack_color(const FluidColor& c) noexcept;
FluidColor fluid_unpack_color(std::uint32_t rgba) noexcept;

namespace detail {

bool fluid_point_in_shape_2d(const Shape2D& shape, const RVec2& local_point);
bool fluid_collider_contact_2d(const Collider2D& collider, const Transform2D& body_xf, const RVec2& point, Real radius, RVec2& normal, Real& depth);

} // namespace detail

struct FluidSandSettings2D {
    Real tick_rate = 60.0_r;
    bool heat = false;
    Real ambient_temperature = 20.0_r;
    Real heat_conduction = 0.15_r;
    Real reaction_rate = 1.0_r;
    Real fire_spread = 1.0_r;
    Real steam_condense = 1.0_r;
    SandCell2D fill_cell = SandCell2D::water;
    bool displace_by_bodies = true;
    int displace_search = 48;
    std::uint32_t seed = 1;
};

struct FluidWaveSettings2D {
    Real damping = 0.4_r;
    Real min_depth = 1e-4_r;
    Real swell_amplitude = 0.0_r;
    Real swell_wavelength = 10.0_r;
    Real swell_steepness = 0.3_r;
    int swell_octaves = 3;
    Real wind = 1.0_r;
    Real ripple_strength = 1.0_r;
    Real occupancy_response = 30.0_r;
    Real reference_time = 0.5_r;
};

struct FluidParticleSettings2D {
    Real radius = 0.0_r;
    std::size_t max_particles = 0;
    FluidTransfer2D transfer = FluidTransfer2D::flip;
    Real flip_ratio = 0.95_r;
    int pressure_iterations = 0;
    Real pressure_tolerance = 0.0_r;
    int separation_iterations = 0;
    Real separation = 1.0_r;
    Real cohesion = -1.0_r;
    Real drift_correction = 1.0_r;
    Real viscosity_scale = 1.0_r;
    int viscosity_iterations = 0;
    Real color_mixing = 0.0_r;
    int bubble_cells = 256;
    bool contain = true;
    bool kill_outside = true;
    Real sleep_speed = 0.03_r;
    Real wake_speed = 0.1_r;
    int sleep_frames = 60;
    Real density_drift = 1.0_r;
    Real settle_speed = 0.2_r;
    Real settle_rate = 1.5_r;
    Real damping = 0.0_r;
    int max_substeps = 0;
    Real cfl = 0.0_r;
};

struct FluidVolumeDef2D {
    FluidLevel level = FluidLevel::still;
    AABB bounds{ RVec2{ -5.0_r, -5.0_r }, RVec2{ 5.0_r, 0.0_r } };
    RVec2 up{};
    FluidMaterial material{};
    Real cell_size = 0.1_r;
    Real fill = 0.0_r;
    RVec2 current{};
    bool allow_sleep = true;
    Real sleep_threshold = 1e-4_r;
    Real heave_damping = 0.3_r;
    FluidStillSettings still{};
    FluidSandSettings2D sand{};
    FluidFlowingSettings flowing{};
    FluidWaveSettings2D waves{};
    FluidParticleSettings2D particles{};
    FluidCouplingSettings coupling{};

    static FluidVolumeDef2D box(FluidLevel level, const AABB& bounds, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r);
};

struct FluidEmitter2D {
    RVec2 position{};
    RVec2 velocity{};
    Real radius = 0.1_r;
    Real rate = 0.0_r;
    Real carry = 0.0_r;
    std::uint8_t material = 0;
    std::uint32_t color = 0;
    SandCell2D cell = SandCell2D::water;
    bool active = true;
};

struct FluidForces2D {
    Real submerged_area = 0.0_r;
    Real submerged_fraction = 0.0_r;
    Real body_area = 0.0_r;
    RVec2 center_of_buoyancy{};
    RVec2 buoyancy{};
    RVec2 drag{};
    Real angular_drag = 0.0_r;
    RVec2 fluid_velocity{};
    Real fluid_density = 0.0_r;
    Real heave_damping = 0.0_r;
    bool touching = false;
};

struct FluidStats2D {
    std::size_t volumes = 0;
    std::size_t stepped = 0;
    std::size_t sleeping = 0;
    std::size_t active_regions = 0;
    std::size_t total_regions = 0;
    std::size_t particles = 0;
    std::size_t awake_particles = 0;
    std::size_t fluid_cells = 0;
    std::size_t bodies = 0;
    std::size_t ticks = 0;
    int substeps = 0;
    int iterations = 0;
    double milliseconds = 0.0;
};

class FluidWorld2D;

class FluidVolume2D {
    friend class FluidWorld2D;

public:
    FluidVolume2D(const FluidVolumeDef2D& def, const RVec2& gravity, const FluidQualitySettings& quality);

    FluidLevel level() const noexcept { return m_def.level; }
    const FluidVolumeDef2D& def() const noexcept { return m_def; }
    FluidVolumeDef2D& edit_def() noexcept { wake(); return m_def; }
    const FluidMaterial& material() const noexcept { return m_materials[0]; }
    void set_material(const FluidMaterial& m);
    const FluidMaterial& material(std::uint8_t id) const noexcept { return m_materials[id < m_materials.size() ? id : 0]; }
    void set_material(std::uint8_t id, const FluidMaterial& m);
    std::uint8_t add_material(const FluidMaterial& m);
    std::size_t material_count() const noexcept { return m_materials.size(); }
    void set_current(const RVec2& v) noexcept { m_def.current = v; }
    std::uint32_t id() const noexcept { return m_id; }

    AABB bounds() const noexcept { return m_world_bounds; }
    RVec2 up() const noexcept { return m_up; }
    RVec2 right() const noexcept { return m_u; }
    RVec2 origin() const noexcept { return m_o; }
    RVec2 size() const noexcept { return m_size; }
    RVec2 to_local(const RVec2& world) const noexcept;
    RVec2 to_world(const RVec2& local) const noexcept;
    RVec2 local_dir(const RVec2& world) const noexcept;
    RVec2 world_dir(const RVec2& local) const noexcept;

    Real surface_height(const RVec2& point) const;
    Real surface_height(Real a) const;
    RVec2 surface_point(const RVec2& point) const;
    RVec2 surface_normal(const RVec2& point) const;
    Real depth_at(const RVec2& point) const;
    RVec2 velocity_at(const RVec2& point) const;
    Real density_at(const RVec2& point) const;
    bool inside(const RVec2& point) const;
    Real total_volume() const;
    Real time() const noexcept { return m_time; }

    void wake() noexcept;
    bool sleeping() const noexcept;
    std::size_t active_regions() const noexcept;
    std::size_t total_regions() const noexcept;
    void set_visible(bool v) noexcept { m_visible = v; }
    bool visible() const noexcept { return m_visible; }
    void set_frozen(bool f) noexcept { m_frozen = f; }
    bool frozen() const noexcept { return m_frozen; }

    void set_still_level(Real height_along_up);
    Real still_level() const noexcept { return m_still_level; }

    int cells_x() const noexcept { return m_nx; }
    int cells_y() const noexcept { return m_ny; }
    Real cell_size() const noexcept { return m_cs; }
    std::size_t cell_index(int i, int j) const noexcept { return static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(i); }
    bool valid_cell(int i, int j) const noexcept { return i >= 0 && j >= 0 && i < m_nx && j < m_ny; }
    bool cell_of(const RVec2& world, int& i, int& j) const noexcept;
    RVec2 cell_center(int i, int j) const noexcept;
    void set_solid(int i, int j, bool solid);
    void set_solid_box(const AABB& world_box, bool solid);
    void paint_solid(const RVec2& center, Real radius, bool solid = true);
    bool solid(int i, int j) const noexcept;
    const std::vector<std::uint8_t>& cell_solids() const noexcept { return m_solid; }
    const std::vector<std::uint8_t>& cell_occupancy() const noexcept { return m_occ; }
    void voxelize(Simulation2D& sim, bool static_only = true);
    RigidBody2D& build_static_body(Simulation2D& sim, RigidBody2D* reuse = nullptr, const Material& material = Material{});
    std::size_t solid_rectangles(std::vector<AABB>& out) const;

    void set_source(int i, int j, bool source = true);
    bool source(int i, int j) const noexcept;
    void set_amount(int i, int j, Real amount);
    Real amount(int i, int j) const noexcept;
    void fill_box(const AABB& world_box, Real amount = 1.0_r);
    const std::vector<Real>& cell_fill() const noexcept { return m_fill; }
    const std::vector<RVec2>& cell_velocities() const noexcept { return m_cell_vel; }

    SandCell2D sand_cell(int i, int j) const noexcept;
    void set_sand_cell(int i, int j, SandCell2D cell);
    std::size_t paint_cells(const RVec2& center, Real radius, SandCell2D cell, Real probability = 1.0_r, bool replace = true);
    std::size_t paint_cells_box(const AABB& world_box, SandCell2D cell, Real probability = 1.0_r, bool replace = true);
    std::size_t erase_cells(const RVec2& center, Real radius);
    std::size_t count_cells(SandCell2D cell) const noexcept;
    void add_heat(const RVec2& center, Real radius, Real degrees);
    Real temperature(int i, int j) const noexcept;
    const std::vector<std::uint8_t>& sand_cells() const noexcept { return m_sand; }
    const std::vector<std::uint8_t>& cell_life() const noexcept { return m_life; }
    const std::vector<std::uint8_t>& cell_shade() const noexcept { return m_shade; }
    const std::vector<float>& cell_temperatures() const noexcept { return m_temp; }

    int columns() const noexcept { return m_cols; }
    Real column_size() const noexcept { return m_dx; }
    std::size_t column_index(int i) const noexcept { return static_cast<std::size_t>(i); }
    bool column_of(const RVec2& world, int& i) const noexcept;
    void set_ground(int i, Real height);
    Real ground(int i) const noexcept;
    void set_wall(int i, bool wall);
    void set_wall_box(const AABB& world_box, bool wall);
    bool wall(int i) const noexcept;
    void set_depth(int i, Real depth);
    Real depth(int i) const noexcept;
    Real column_height(int i) const noexcept;
    void disturb(const RVec2& point, Real strength, Real radius = 0.0_r);
    const std::vector<Real>& heights() const;
    const std::vector<Real>& depths() const noexcept { return m_depth; }
    const std::vector<Real>& column_velocities() const noexcept { return m_col_vel; }
    Real swell_height(Real lu) const noexcept;

    std::size_t particle_count() const noexcept { return m_px.size(); }
    std::size_t awake_particles() const noexcept { return m_asleep ? 0 : m_px.size(); }
    const std::vector<RVec2>& positions() const;
    const std::vector<RVec2>& velocities() const;
    const std::vector<RVec2>& local_positions() const noexcept { return m_px; }
    const std::vector<RVec2>& local_velocities() const noexcept { return m_pv; }
    const std::vector<std::uint8_t>& material_ids() const noexcept { return m_pmat; }
    const std::vector<std::uint32_t>& colors() const noexcept { return m_pcol; }
    Real particle_radius() const noexcept { return m_radius; }
    Real particle_spacing() const noexcept { return m_spacing; }
    Real particle_mass(std::uint8_t material_id = 0) const noexcept;
    Real rest_density() const noexcept { return m_rest; }
    bool add_particle(const RVec2& position, const RVec2& velocity = RVec2{}, std::uint8_t material_id = 0, std::uint32_t color = 0);
    std::size_t add_particle_box(const AABB& world_box, const RVec2& velocity = RVec2{}, std::uint8_t material_id = 0, std::uint32_t color = 0);
    std::size_t add_fluid_circle(const RVec2& center, Real radius, const RVec2& velocity = RVec2{}, std::uint8_t material_id = 0, std::uint32_t color = 0);
    std::size_t remove_fluid_circle(const RVec2& center, Real radius);
    std::size_t remove_material(std::uint8_t material_id);
    void push(const RVec2& center, Real radius, const RVec2& velocity, Real strength = 1.0_r);
    void swirl(const RVec2& center, Real radius, Real spin, Real strength = 1.0_r);
    void explode(const RVec2& center, Real radius, Real strength);
    void clear_particles();
    std::size_t particles_in(const AABB& world_box) const;
    std::size_t add_emitter(const FluidEmitter2D& emitter);
    FluidEmitter2D& emitter(std::size_t i) { return m_emitters.at(i); }
    std::size_t emitter_count() const noexcept { return m_emitters.size(); }
    void clear_emitters() { m_emitters.clear(); }
    std::size_t particle_contacts() const noexcept { return m_contact_count; }
    const std::vector<std::uint8_t>& cell_types() const noexcept { return m_ctype; }
    const std::vector<Real>& cell_densities() const noexcept { return m_cdens; }
    const std::vector<Real>& grid_velocity_u() const noexcept { return m_gu; }
    const std::vector<Real>& grid_velocity_v() const noexcept { return m_gv; }
    const std::vector<Real>& pressure() const noexcept { return m_phi; }
    int last_substeps() const noexcept { return m_last_substeps; }
    int last_iterations() const noexcept { return m_last_iterations; }

private:
    struct SamplePoint {
        RVec2 local{};
        Real weight = 0.0_r;
    };

    struct BodyCache {
        std::uint64_t signature = 0;
        std::vector<SamplePoint> points;
        std::vector<std::uint32_t> collider_start;
        Real area = 0.0_r;
        Real spacing = 0.1_r;
        std::uint64_t seen = 0;
        Real ref = 0.0_r;
        bool has_ref = false;
    };

    struct NearBody {
        RigidBody2D* body = nullptr;
        AABB box{};
        AABB local{};
        bool dynamic = false;
        RVec2 impulse{};
        Real angular = 0.0_r;
        Real max_rel = 0.0_r;
    };

    struct SandRect {
        int x0 = 1;
        int y0 = 1;
        int x1 = 0;
        int y1 = 0;
        bool empty() const noexcept { return x0 > x1 || y0 > y1; }
        void add(int ax0, int ay0, int ax1, int ay1) noexcept {
            if (empty()) {
                x0 = ax0; y0 = ay0; x1 = ax1; y1 = ay1;
                return;
            }
            x0 = std::min(x0, ax0); y0 = std::min(y0, ay0);
            x1 = std::max(x1, ax1); y1 = std::max(y1, ay1);
        }
    };

    struct SandRng {
        std::uint32_t s = 1;
        std::uint32_t next() noexcept {
            s ^= s << 13;
            s ^= s >> 17;
            s ^= s << 5;
            return s;
        }
        Real unit() noexcept { return static_cast<Real>(next() >> 8) * (1.0_r / 16777216.0_r); }
        bool chance(Real p) noexcept { return p >= 1.0_r || (p > 0.0_r && unit() < p); }
    };

    struct Ambient {
        bool valid = false;
        Real level = 0.0_r;
        int left = -1;
        int right = -1;
    };

    void build_frame(const RVec2& gravity);
    void init_grid();
    void init_flowing();
    void init_sand();
    void init_waves();
    void init_particles();
    void refresh_materials();

    void step_level(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats);
    void step_flowing(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats);
    void step_sand(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats);
    void step_waves(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats);
    void step_particles(detail::FluidTaskPool& pool, Real dt, FluidStats2D& stats);

    void gather_bodies(Simulation2D& sim);
    void rasterize_bodies();
    void clear_occupancy();
    bool occupied(int i, int j) const noexcept { return m_occ[cell_index(i, j)] != 0; }

    void flowing_tick();
    void flowing_displace();
    Real flowing_stable(Real total) const noexcept;
    void mark_flow_dirty(int i, int j) noexcept;

    void sand_tick(detail::FluidTaskPool& pool);
    void sand_chunk(std::size_t chunk, std::uint32_t tick);
    void sand_cell_update(int x, int y, SandRng& rng, SandRect& spill, std::uint32_t tick);
    void sand_heat(int x, int y, SandRect& spill);
    void sand_displace();
    void sand_mark(int x, int y) noexcept;
    void sand_set(std::size_t c, SandCell2D cell, SandRng* rng) noexcept;
    void sand_swap(std::size_t a, std::size_t b) noexcept;
    bool sand_free(int x, int y) const noexcept;
    std::size_t sand_chunk_of(int x, int y) const noexcept { return static_cast<std::size_t>(y / kSandChunk) * static_cast<std::size_t>(m_kx) + static_cast<std::size_t>(x / kSandChunk); }

    void waves_substep(Real h);
    void waves_occupancy(Real dt);
    Real sample_columns(Real lu, const std::vector<Real>& field) const noexcept;
    Real column_head(std::size_t c) const noexcept { return m_ground[c] + m_depth[c] + m_occ_col[c]; }
    Real swell_velocity(Real lu, Real lh) const noexcept;

    void particle_substep(detail::FluidTaskPool& pool, Real h, int iterations);
    void sort_particles();
    void sweep_walls(detail::FluidTaskPool& pool);
    void separate_particles(detail::FluidTaskPool& pool, int iterations);
    void collide_particles(detail::FluidTaskPool& pool, Real h);
    void apply_body_impulses();
    void particles_to_grid();
    void classify_cells();
    void solid_face_velocities();
    void grid_viscosity(Real h);
    int pressure_solve(Real h, int max_iterations);
    void grid_to_particles(detail::FluidTaskPool& pool, Real h);
    void mix_colors(Real h);
    void run_emitters(Real dt);
    void compact_particles();
    void refresh_world_particles() const;
    int particle_cell(const RVec2& local) const noexcept;
    Real sample_grid_u(const RVec2& local) const noexcept;
    Real sample_grid_v(const RVec2& local) const noexcept;
    bool static_solid(int i, int j) const noexcept;
    bool any_solid(int i, int j) const noexcept;

    Real cell_frac(int i, int j) const noexcept;
    Real cell_rho(int i, int j) const noexcept;
    Real cell_mu(int i, int j) const noexcept;
    RVec2 cell_flow(int i, int j) const noexcept;
    bool cell_blocked(int i, int j) const noexcept;
    bool scan_column(int i, int j0, Real& level) const noexcept;
    Ambient ambient_for(const RVec2& lo, const RVec2& hi) const noexcept;

    const BodyCache& body_cache(const RigidBody2D& body, int samples);
    FluidForces2D measure(const RigidBody2D& body, const BodyCache& cache, const RVec2& gravity) const;
    bool local_footprint(const RigidBody2D& body, RVec2& lo, RVec2& hi) const;

    static constexpr int kSandChunk = 32;
    static constexpr int kFlowChunk = 16;
    static constexpr int kWaveTile = 32;

    FluidVolumeDef2D m_def;
    FluidQualitySettings m_quality;
    std::vector<FluidMaterial> m_materials;
    std::uint32_t m_id = 0;
    RVec2 m_o{};
    RVec2 m_u{ 1.0_r, 0.0_r };
    RVec2 m_up{ 0.0_r, 1.0_r };
    RVec2 m_size{};
    AABB m_world_bounds{};
    Real m_g = 9.81_r;
    RVec2 m_gl{ 0.0_r, -9.81_r };
    Real m_time = 0.0_r;
    Real m_accum = 0.0_r;
    Real m_lod_accum = 0.0_r;
    int m_lod_counter = 0;
    bool m_visible = true;
    bool m_frozen = false;
    bool m_all_dirty = true;
    Real m_still_level = 0.0_r;
    Real m_last_dt = 1.0_r / 60.0_r;

    int m_nx = 0, m_ny = 0;
    Real m_cs = 0.1_r;
    std::vector<std::uint8_t> m_solid;
    std::vector<std::uint8_t> m_occ;
    std::vector<RVec2> m_occ_vel;
    std::vector<std::uint32_t> m_occ_list;
    bool m_solids_changed = true;

    std::vector<Real> m_fill;
    std::vector<Real> m_next;
    std::vector<std::uint8_t> m_source;
    std::vector<RVec2> m_cell_vel;
    std::vector<Real> m_flow_rem;
    std::vector<Real> m_flow_scale;
    std::vector<Real> m_flow_hflux;
    int m_fkx = 0, m_fky = 0;
    std::vector<std::uint8_t> m_flow_changed;
    std::vector<std::uint8_t> m_flow_active;
    std::vector<std::uint8_t> m_flow_ready;
    std::vector<std::uint32_t> m_flow_list;

    std::vector<std::uint8_t> m_sand;
    std::vector<std::uint8_t> m_life;
    std::vector<std::uint8_t> m_shade;
    std::vector<std::uint8_t> m_clock;
    std::vector<float> m_temp;
    int m_kx = 0, m_ky = 0;
    std::vector<SandRect> m_rect_now;
    std::vector<SandRect> m_rect_next;
    std::vector<SandRect> m_rect_spill;
    std::vector<std::uint32_t> m_sand_phase[4];
    std::uint32_t m_sand_tick = 0;
    std::size_t m_sand_active = 0;
    SandRng m_brush_rng{};

    int m_cols = 0;
    Real m_dx = 0.25_r;
    std::vector<Real> m_ground;
    std::vector<Real> m_depth;
    std::vector<Real> m_flux;
    std::vector<Real> m_occ_col;
    std::vector<Real> m_occ_prev;
    std::vector<Real> m_occ_push;
    std::vector<std::uint8_t> m_wall_col;
    std::vector<Real> m_col_vel;
    std::vector<Real> m_col_scale;
    std::vector<Real> m_col_change;
    std::vector<std::uint8_t> m_tile_active;
    std::vector<std::uint16_t> m_tile_calm;
    mutable std::vector<Real> m_heights;
    mutable bool m_heights_dirty = true;

    Real m_radius = 0.03_r;
    Real m_spacing = 0.06_r;
    Real m_rest = 1.0_r;
    std::size_t m_max_particles = 0;
    std::size_t m_contact_count = 0;
    bool m_asleep = false;
    int m_calm_frames = 0;
    int m_last_substeps = 0;
    int m_last_iterations = 0;
    Real m_max_speed = 0.0_r;
    std::vector<RVec2> m_px;
    std::vector<RVec2> m_pv;
    std::vector<RVec2> m_cu;
    std::vector<RVec2> m_cv;
    std::vector<std::uint8_t> m_pmat;
    std::vector<std::uint32_t> m_pcol;
    std::vector<std::int32_t> m_pcontact;
    std::vector<RVec2> m_pdv;
    std::vector<std::uint32_t> m_pkey;
    std::vector<RVec2> m_tmp_a;
    std::vector<RVec2> m_tmp_b;
    std::vector<RVec2> m_tmp_c;
    std::vector<RVec2> m_tmp_d;
    std::vector<RVec2> m_tmp_e;
    std::vector<std::uint8_t> m_tmp_u8;
    std::vector<std::uint32_t> m_tmp_u32;
    std::vector<std::uint32_t> m_cell_start;
    std::vector<std::uint32_t> m_cell_fill_count;
    mutable std::vector<RVec2> m_wpx;
    mutable std::vector<RVec2> m_wpv;
    mutable bool m_world_dirty = true;

    std::vector<Real> m_gu;
    std::vector<Real> m_gv;
    std::vector<Real> m_gu0;
    std::vector<Real> m_gv0;
    std::vector<Real> m_guw;
    std::vector<Real> m_gvw;
    std::vector<Real> m_gscratch;
    std::vector<std::uint8_t> m_ctype;
    std::vector<Real> m_cdens;
    std::vector<Real> m_crho;
    std::vector<Real> m_cmu;
    std::vector<Real> m_cvisc;
    std::vector<Real> m_phi;
    std::vector<std::int32_t> m_fidx;
    std::vector<std::uint32_t> m_flist;
    std::vector<double> m_pcg_b;
    std::vector<double> m_pcg_x;
    std::vector<double> m_pcg_r;
    std::vector<double> m_pcg_z;
    std::vector<double> m_pcg_s;
    std::vector<double> m_pcg_q;
    std::vector<double> m_pcg_diag;
    std::vector<double> m_pcg_ax;
    std::vector<double> m_pcg_ay;
    std::vector<double> m_pcg_pre;
    std::vector<std::int32_t> m_pcg_nb;
    std::vector<std::uint32_t> m_visc_faces;
    std::vector<std::int32_t> m_visc_nb;
    std::vector<Real> m_visc_d;
    std::vector<float> m_csum;
    std::vector<std::uint32_t> m_bubble_label;
    std::vector<std::uint32_t> m_bubble_queue;
    std::size_t m_solid_count = 0;
    Real m_vlim_cells = 1.0_r;
    Real m_last_h = 0.0_r;

    std::vector<Real> m_mat_rho;
    std::vector<Real> m_mat_mu;
    std::vector<Real> m_mat_visc;
    std::vector<Real> m_mat_flip;
    std::vector<Real> m_mat_cohesion;
    std::vector<Real> m_mat_friction;
    std::vector<Real> m_mat_restitution;
    std::vector<std::uint32_t> m_mat_color;
    bool m_any_viscous = false;
    bool m_any_cohesion = false;

    std::vector<NearBody> m_near;
    std::vector<FluidEmitter2D> m_emitters;
    SandRng m_rng{};

    std::unordered_map<std::uint64_t, BodyCache> m_cache;
    std::uint64_t m_frame = 0;
};

struct FluidWorld2DConfig {
    RVec2 gravity = to_real(default_axes2d().gravity(constants::gravity()));
    FluidLevel default_level = FluidLevel::still;
    FluidQuality quality = FluidQuality::medium;
    Real fixed_dt = 1.0_r / 60.0_r;
    int threads = 1;
    Real lod_distance = 0.0_r;
    int lod_interval = 4;
    Real freeze_distance = 0.0_r;
    bool require_buoyancy_flag = false;
    bool step_offscreen_less = true;
};

class FluidWorld2D {
public:
    using Config = FluidWorld2DConfig;

    explicit FluidWorld2D(const Config& cfg = Config());
    FluidWorld2D(const FluidWorld2D&) = delete;
    FluidWorld2D& operator=(const FluidWorld2D&) = delete;

    Config& config() noexcept { return m_cfg; }
    const Config& config() const noexcept { return m_cfg; }
    void set_quality(FluidQuality q);
    void set_quality(const FluidQualitySettings& q) { m_quality = q; }
    const FluidQualitySettings& quality() const noexcept { return m_quality; }
    void set_threads(int threads);
    void set_gravity(const RVec2& g) noexcept { m_cfg.gravity = g; }
    void set_axes(const Axes2D& axes) noexcept { m_cfg.gravity = to_real(axes.gravity(constants::gravity())); }
    void set_focus(const RVec2& point) noexcept { m_focus = point; m_has_focus = true; }
    const RVec2& focus() const noexcept { return m_focus; }

    FluidVolume2D& add_volume(const FluidVolumeDef2D& def);
    FluidVolume2D& add_volume(const AABB& bounds, FluidLevel level, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r);
    FluidVolume2D& add_volume(const AABB& bounds, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r) { return add_volume(bounds, m_cfg.default_level, material, fill); }
    void remove_volume(FluidVolume2D& volume);
    std::size_t volume_count() const noexcept { return m_volumes.size(); }
    FluidVolume2D& volume(std::size_t i) { return *m_volumes.at(i); }
    const FluidVolume2D& volume(std::size_t i) const { return *m_volumes.at(i); }

    void step(Simulation2D& sim, Real dt);
    void step(Real dt);
    void step(Simulation2D& sim) { step(sim, sim.config().fixed_dt); }
    void step() { step(m_cfg.fixed_dt); }

    void explode(Simulation2D& sim, const RVec2& center, Real radius, Real strength);
    void explode(const RVec2& center, Real radius, Real strength);
    void push(const RVec2& center, Real radius, const RVec2& velocity, Real strength = 1.0_r);

    FluidVolume2D* volume_at(const RVec2& point);
    const FluidVolume2D* volume_at(const RVec2& point) const;
    Real surface_height(const RVec2& point) const;
    Real surface_height(Real a) const;
    Real depth_at(const RVec2& point) const;
    RVec2 velocity_at(const RVec2& point) const;
    bool inside(const RVec2& point) const;

    FluidForces2D forces_on(const RigidBody2D& body);
    const FluidForces2D* body_forces(const RigidBody2D& body) const;
    const FluidStats2D& stats() const noexcept { return m_stats; }

private:
    void couple(Simulation2D& sim, Real dt);
    void schedule(FluidVolume2D& v, Real dt, Real& use_dt, bool& run);
    void finish_stats(FluidVolume2D& v);

    Config m_cfg;
    FluidQualitySettings m_quality;
    std::vector<std::unique_ptr<FluidVolume2D>> m_volumes;
    detail::FluidTaskPool m_pool;
    FluidStats2D m_stats;
    RVec2 m_focus{};
    bool m_has_focus = false;
    std::uint32_t m_next_id = 1;
    std::unordered_map<std::uint64_t, FluidForces2D> m_body_forces;
};

struct FluidImageSettings2D {
    int width = 256;
    int height = 0;
    AABB region{ RVec2{ 0.0_r, 0.0_r }, RVec2{ 0.0_r, 0.0_r } };
    Real kernel_radius = 0.0_r;
    Real threshold = 0.35_r;
    Real softness = 0.3_r;
    bool flip_y = true;
    bool blend_colors = true;
    bool premultiplied = false;
    bool shade_edges = true;
    Real edge_darken = 0.25_r;
    Real opacity = 1.0_r;
    FluidColor background{ 0.0f, 0.0f, 0.0f, 0.0f };
    bool cell_noise = true;
};

class FluidRenderer2D {
public:
    const std::vector<std::uint8_t>& render(const FluidVolume2D& volume, const FluidImageSettings2D& settings = FluidImageSettings2D());
    const std::vector<std::uint8_t>& render_particles(const std::vector<RVec2>& world_positions, const std::vector<std::uint32_t>& colors, Real particle_radius, Real rest_count, const FluidImageSettings2D& settings);
    const std::vector<std::uint8_t>& render_cells(const FluidVolume2D& volume, const FluidImageSettings2D& settings = FluidImageSettings2D());
    std::size_t contour(Real threshold, std::vector<RVec2>& segments) const;

    const std::vector<std::uint8_t>& rgba() const noexcept { return m_rgba; }
    const std::vector<float>& density() const noexcept { return m_density; }
    int width() const noexcept { return m_w; }
    int height() const noexcept { return m_h; }
    AABB region() const noexcept { return m_region; }

private:
    void setup(const AABB& fallback, const FluidImageSettings2D& s);
    void splat(const std::vector<RVec2>& world_positions, const std::vector<std::uint32_t>& colors, Real kernel);
    void shade(const FluidImageSettings2D& s, Real level);
    void put(std::size_t p, float r, float g, float b, float a, const FluidImageSettings2D& s);

    std::vector<std::uint8_t> m_rgba;
    std::vector<float> m_density;
    std::vector<float> m_color;
    std::vector<float> m_kernel;
    int m_w = 0;
    int m_h = 0;
    bool m_flip = true;
    AABB m_region{};
    Real m_px = 1.0_r;
    Real m_py = 1.0_r;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_FLUID_2D_HPP
