#ifndef FIZMO_PHYSICS_FLUID_3D_HPP
#define FIZMO_PHYSICS_FLUID_3D_HPP

#include "../Rigid Body/simulation_3d.hpp"
#include "fluid_common.hpp"
#include "fluid_pool_3d.hpp"

#include <array>
#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace fizmo {
namespace physics {

namespace detail {

bool fluid_point_in_shape(const Shape3D& shape, const RVec3& local_point);
bool fluid_collider_contact(const Collider3D& collider, const Transform3D& body_xf, const RVec3& point, Real radius, RVec3& normal, Real& depth);

} // namespace detail

struct FluidBlockySettings {
    Real game_tick = 0.05_r;
    int tick_interval = 5;
    int spread = 0;
    int hole_search = 4;
    bool infinite_sources = true;
    bool viscosity_slows_ticks = true;
};

struct FluidWaveSettings {
    Real damping = 0.4_r;
    Real min_depth = 1e-4_r;
    Real swell_amplitude = 0.0_r;
    Real swell_wavelength = 10.0_r;
    Real swell_steepness = 0.3_r;
    int swell_octaves = 3;
    RVec3 wind{ 1.0_r, 0.0_r, 0.0_r };
    Real ripple_strength = 1.0_r;
};

enum class FluidSolver3D : std::uint8_t {
    flip = 0,
    apic,
    pbf
};

const char* to_string(FluidSolver3D solver) noexcept;

struct FluidParticleSettings {
    Real radius = 0.05_r;
    std::size_t max_particles = 0;
    int iterations = 0;
    Real xsph = -1.0_r;
    Real cohesion = -1.0_r;
    Real artificial_pressure = -1.0_r;
    bool vorticity = false;
    Real vorticity_strength = 0.05_r;
    bool contain = true;
    bool kill_outside = true;
    Real sleep_speed = 0.02_r;
    Real wake_speed = 0.08_r;
    int sleep_frames = 30;
    int max_substeps = 0;
    Real surface_cell = 0.0_r;
    FluidSolver3D solver = FluidSolver3D::flip;
    Real flip_ratio = 0.95_r;
    bool narrow_band = true;
    Real band_cells = 3.0_r;
    int pressure_iterations = 0;
    Real pressure_tolerance = 0.0_r;
    Real drift_correction = 1.0_r;
    Real viscosity_scale = 1.0_r;
    int viscosity_iterations = 0;
    Real surface_tension_scale = 1.0_r;
    Real settle_speed = 0.25_r;
    Real settle_rate = 3.0_r;
    Real damping = 0.0_r;
    Real cfl = 0.0_r;
    bool reseed = true;
    int spray_neighbours = 2;
    Real wall_friction = 0.4_r;
};

struct FluidSurfaceSettings3D {
    Real iso = 0.5_r;
    int smoothing = 1;
    bool foam = true;
    Real foam_speed = 1.2_r;
    Real foam_range = 1.5_r;
    bool spray = true;
    bool close_walls = true;
};

struct FluidSurfaceMesh3D {
    std::vector<RVec3> positions;
    std::vector<RVec3> normals;
    std::vector<std::uint32_t> indices;
    std::vector<Real> foam;
    std::vector<Real> speeds;
    std::vector<RVec3> spray;
    Real cell = 0.0_r;
    int detail = -1;
    std::uint64_t version = 0;
    double milliseconds = 0.0;

    std::size_t vertex_count() const noexcept { return positions.size(); }
    std::size_t triangle_count() const noexcept { return indices.size() / 3; }
    bool empty() const noexcept { return indices.empty(); }
    void clear() noexcept {
        positions.clear();
        normals.clear();
        indices.clear();
        foam.clear();
        speeds.clear();
        spray.clear();
    }
};

struct FluidVolumeDef {
    FluidLevel level = FluidLevel::still;
    AABB3D bounds{ RVec3{ -5.0_r, -5.0_r, -5.0_r }, RVec3{ 5.0_r, 0.0_r, 5.0_r } };
    RVec3 up{};
    FluidMaterial material{};
    Real cell_size = 0.25_r;
    Real fill = 0.0_r;
    RVec3 current{};
    bool allow_sleep = true;
    Real sleep_threshold = 1e-4_r;
    FluidStillSettings still{};
    FluidBlockySettings blocky{};
    FluidFlowingSettings flowing{};
    FluidWaveSettings waves{};
    FluidParticleSettings particles{};
    FluidCouplingSettings coupling{};
    FluidSurfaceSettings3D surface{};

    static FluidVolumeDef box(FluidLevel level, const AABB3D& bounds, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r);
};

struct FluidEmitter3D {
    RVec3 position{};
    RVec3 velocity{};
    RVec3 normal{};
    Real radius = 0.1_r;
    Real rate = 0.0_r;
    Real carry = 0.0_r;
    bool active = true;
};

struct FluidForces3D {
    Real submerged_volume = 0.0_r;
    Real submerged_fraction = 0.0_r;
    Real body_volume = 0.0_r;
    RVec3 center_of_buoyancy{};
    RVec3 buoyancy{};
    RVec3 drag{};
    RVec3 angular_drag{};
    RVec3 fluid_velocity{};
    bool touching = false;
};

struct FluidStats3D {
    std::size_t volumes = 0;
    std::size_t stepped = 0;
    std::size_t sleeping = 0;
    std::size_t active_regions = 0;
    std::size_t total_regions = 0;
    std::size_t particles = 0;
    std::size_t awake_particles = 0;
    std::size_t bodies = 0;
    std::size_t ticks = 0;
    int substeps = 0;
    int iterations = 0;
    double milliseconds = 0.0;
    std::size_t fluid_cells = 0;
    std::size_t deep_cells = 0;
};

class FluidWorld3D;

class FluidVolume3D {
    friend class FluidWorld3D;

public:
    FluidVolume3D(const FluidVolumeDef& def, const RVec3& gravity, const FluidQualitySettings& quality);

    FluidLevel level() const noexcept { return m_def.level; }
    const FluidVolumeDef& def() const noexcept { return m_def; }
    FluidVolumeDef& edit_def() noexcept { wake(); return m_def; }
    const FluidMaterial& material() const noexcept { return m_def.material; }
    void set_material(const FluidMaterial& m) noexcept { m_def.material = m; wake(); }
    void set_current(const RVec3& v) noexcept { m_def.current = v; }
    std::uint32_t id() const noexcept { return m_id; }

    AABB3D bounds() const noexcept { return m_world_bounds; }
    RVec3 up() const noexcept { return m_up; }
    RVec3 right() const noexcept { return m_u; }
    RVec3 forward() const noexcept { return m_w; }
    RVec3 origin() const noexcept { return m_o; }
    RVec3 size() const noexcept { return m_size; }
    RVec3 to_local(const RVec3& world) const noexcept;
    RVec3 to_world(const RVec3& local) const noexcept;
    RVec3 local_dir(const RVec3& world) const noexcept;
    RVec3 world_dir(const RVec3& local) const noexcept;

    Real surface_height(const RVec3& point) const;
    Real surface_height(Real a, Real b) const;
    RVec3 surface_point(const RVec3& point) const;
    RVec3 surface_normal(const RVec3& point) const;
    Real depth_at(const RVec3& point) const;
    RVec3 velocity_at(const RVec3& point) const;
    bool inside(const RVec3& point) const;
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

    void ignore_body(const RigidBody3D& body, bool ignore = true);
    bool ignores(const RigidBody3D& body) const noexcept { return m_ignored.count(body.id()) != 0; }

    void set_still_level(Real height_along_up);
    Real still_level() const noexcept { return m_still_level; }

    int cells_x() const noexcept { return m_nx; }
    int cells_y() const noexcept { return m_ny; }
    int cells_z() const noexcept { return m_nz; }
    Real cell_size() const noexcept { return m_cs; }
    std::size_t cell_index(int i, int j, int k) const noexcept { return (static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(i); }
    bool valid_cell(int i, int j, int k) const noexcept { return i >= 0 && j >= 0 && k >= 0 && i < m_nx && j < m_ny && k < m_nz; }
    bool cell_of(const RVec3& world, int& i, int& j, int& k) const noexcept;
    RVec3 cell_center(int i, int j, int k) const noexcept;
    void set_solid(int i, int j, int k, bool solid);
    void set_solid_box(const AABB3D& world_box, bool solid);
    bool solid(int i, int j, int k) const noexcept;
    void set_source(int i, int j, int k, bool source = true);
    bool source(int i, int j, int k) const noexcept;
    void set_amount(int i, int j, int k, Real amount);
    Real amount(int i, int j, int k) const noexcept;
    int block_level(int i, int j, int k) const noexcept;
    void fill_box(const AABB3D& world_box, Real amount = 1.0_r);
    void voxelize(Simulation3D& sim, bool static_only = true);
    const std::vector<Real>& cell_fill() const noexcept { return m_fill; }
    const std::vector<std::uint8_t>& cell_levels() const noexcept { return m_block; }
    const std::vector<std::uint8_t>& cell_solids() const noexcept { return m_solid; }
    const std::vector<RVec3>& cell_velocities() const noexcept { return m_cell_vel; }

    int columns_x() const noexcept { return m_cx; }
    int columns_z() const noexcept { return m_cz; }
    Real column_size() const noexcept { return m_dx; }
    std::size_t column_index(int i, int k) const noexcept { return static_cast<std::size_t>(k) * static_cast<std::size_t>(m_cx) + static_cast<std::size_t>(i); }
    bool column_of(const RVec3& world, int& i, int& k) const noexcept;
    void set_ground(int i, int k, Real height);
    Real ground(int i, int k) const noexcept;
    void set_wall(int i, int k, bool wall);
    void set_wall_box(const AABB3D& world_box, bool wall);
    bool wall(int i, int k) const noexcept;
    void set_depth(int i, int k, Real depth);
    Real depth(int i, int k) const noexcept;
    Real column_height(int i, int k) const noexcept;
    void disturb(const RVec3& point, Real strength, Real radius = 0.0_r);
    const std::vector<Real>& heights() const;
    const std::vector<RVec3>& normals() const;
    const std::vector<Real>& depths() const noexcept { return m_depth; }
    const std::vector<Real>& column_velocity_u() const noexcept { return m_vel_u; }
    const std::vector<Real>& column_velocity_w() const noexcept { return m_vel_w; }
    Real swell_height(Real lu, Real lw) const noexcept;

    std::size_t particle_count() const noexcept { return m_px.size(); }
    std::size_t awake_particles() const noexcept { return m_awake_count; }
    const std::vector<RVec3>& positions() const noexcept { return m_px; }
    const std::vector<RVec3>& velocities() const noexcept { return m_pv; }
    const std::vector<Real>& densities() const noexcept { return m_density; }
    Real particle_radius() const noexcept { return m_radius; }
    Real particle_mass() const noexcept { return m_pmass; }
    bool add_particle(const RVec3& position, const RVec3& velocity = RVec3{});
    std::size_t add_particle_box(const AABB3D& world_box, const RVec3& velocity = RVec3{});
    void clear_particles();
    std::size_t add_emitter(const FluidEmitter3D& emitter);
    FluidEmitter3D& emitter(std::size_t i) { return m_emitters.at(i); }
    std::size_t emitter_count() const noexcept { return m_emitters.size(); }
    void clear_emitters() { m_emitters.clear(); }
    std::size_t particles_in(const AABB3D& world_box) const;
    void build_surface_grid(Real cell = 0.0_r);
    int surface_columns_x() const noexcept { return m_sx; }
    int surface_columns_z() const noexcept { return m_sz; }
    Real surface_cell() const noexcept { return m_scell; }
    const std::vector<Real>& surface_grid() const noexcept { return m_sgrid; }
    std::size_t particle_contacts() const noexcept { return m_contact_count; }

    FluidSolver3D solver() const noexcept { return m_solver; }
    Real particle_spacing() const noexcept { return m_spacing; }
    const std::vector<RVec3>& local_positions() const noexcept { return m_fx; }
    const std::vector<RVec3>& local_velocities() const noexcept { return m_fv; }
    const std::vector<Real>& level_set() const noexcept { return m_lphi; }
    const std::vector<std::uint8_t>& cell_types() const noexcept { return m_ltype; }
    const std::vector<Real>& cell_densities() const noexcept { return m_ldens; }
    const std::vector<Real>& grid_velocity_u() const noexcept { return m_gu; }
    const std::vector<Real>& grid_velocity_v() const noexcept { return m_gv; }
    const std::vector<Real>& grid_velocity_w() const noexcept { return m_gw; }
    const std::vector<Real>& pressure() const noexcept { return m_lpress; }
    std::size_t liquid_cells() const noexcept { return m_liquid_count; }
    std::size_t deep_cells() const noexcept { return m_deep_count; }
    int last_substeps() const noexcept { return m_last_substeps; }
    int last_iterations() const noexcept { return m_last_iterations; }
    Real max_speed() const noexcept { return m_max_speed; }
    std::uint64_t surface_version() const noexcept { return m_version; }
    const FluidSurfaceMesh3D& surface_mesh(int detail = 1) const;
    void build_surface_mesh(FluidSurfaceMesh3D& out, int detail = 1) const;
    Real surface_mesh_cell(int detail = 1) const noexcept;

private:
    struct SamplePoint {
        RVec3 local{};
        Real weight = 0.0_r;
    };

    struct BodyCache {
        std::uint64_t signature = 0;
        std::vector<SamplePoint> points;
        std::vector<std::uint32_t> collider_start;
        std::vector<RVec3> collider_half;
        Real volume = 0.0_r;
        Real spacing = 0.1_r;
        std::uint64_t seen = 0;
    };

    struct ParticleContact {
        std::uint32_t particle = 0;
        std::uint32_t body = 0;
        std::uint32_t collider = 0;
        RVec3 normal{};
        Real depth = 0.0_r;
    };

    struct NearBody {
        RigidBody3D* body = nullptr;
        AABB3D box{};
        bool dynamic = false;
        RVec3 impulse{};
        RVec3 angular{};
        RVec3 point{};
        Real weight = 0.0_r;
        RVec3 local_lo{};
        RVec3 local_hi{};
        Real max_rel = 0.0_r;
    };

    struct CellBox {
        int lo[3] = { 0, 0, 0 };
        int hi[3] = { -1, -1, -1 };
        bool empty() const noexcept { return hi[0] < lo[0] || hi[1] < lo[1] || hi[2] < lo[2]; }
    };

    void build_frame(const RVec3& gravity);
    void init_cells();
    void init_waves();
    void init_particles();

    void step_level(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);
    void step_blocky(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);
    void step_flowing(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);
    void step_waves(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);
    void step_particles(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);

    void blocky_tick(detail::FluidTaskPool& pool);
    void flowing_tick(detail::FluidTaskPool& pool, Real tick_dt);
    void refresh_chunks();
    void mark_cell_dirty(int i, int j, int k) noexcept;
    std::size_t chunk_of(int i, int j, int k) const noexcept;
    bool blocky_supported(int i, int j, int k) const noexcept;
    Real flowing_stable(Real total) const noexcept;
    int blocky_reach() const noexcept;
    Real blocky_tick_dt() const noexcept;
    Real cell_surface_local(int i, int j, int k, Real lh) const noexcept;
    void apply_cell_displacement();

    void waves_substep(detail::FluidTaskPool& pool, Real h);
    void refresh_tiles();
    void mark_column_dirty(int i, int k) noexcept;
    Real wave_ambient(Real u0, Real w0, Real u1, Real w1) const noexcept;
    void add_occupancy(std::size_t c, int i, int k, Real amount) noexcept;
    std::size_t tile_of(int i, int k) const noexcept;
    Real column_head(std::size_t c) const noexcept { return m_ground[c] + m_depth[c] + m_occ_col[c]; }
    Real sample_waves(Real lu, Real lw, const std::vector<Real>& field) const noexcept;
    void swell_velocity(Real lu, Real lw, Real lh, Real& vu, Real& vh, Real& vw) const noexcept;

    void particle_substep(detail::FluidTaskPool& pool, Simulation3D* sim, Real h, int iterations);
    void build_particle_grid(detail::FluidTaskPool& pool, const std::vector<RVec3>& at);
    void reorder_particles(detail::FluidTaskPool& pool);
    void find_neighbours(detail::FluidTaskPool& pool);
    void run_emitters(Real dt);
    void collide_particles(detail::FluidTaskPool& pool, Real h, Real ri);
    void respond_particles(Real h);
    void grid_coords(const RVec3& world, int& x, int& y, int& z) const noexcept;
    std::uint32_t grid_index(int x, int y, int z) const noexcept { return static_cast<std::uint32_t>((y * m_gz + z) * m_gx + x); }
    template <class Fn>
    void visit_near(const RVec3& world, Fn&& fn) const;
    RVec3 particle_velocity_at(const RVec3& world) const;
    Real kernel_w(Real r2) const noexcept;
    Real kernel_grad(Real r) const noexcept;
    Real particle_density_at(const RVec3& world) const;
    void gather_bodies(Simulation3D& sim);

    bool flip_solver() const noexcept { return m_solver != FluidSolver3D::pbf; }
    void init_flip();
    void flip_resize_particles(std::size_t n);
    void flip_push(const RVec3& local, const RVec3& local_velocity);
    void step_flip(detail::FluidTaskPool& pool, Real dt, FluidStats3D& stats);
    void flip_substep(detail::FluidTaskPool& pool, Real h, int iterations);
    void flip_rasterize_bodies();
    void flip_active_box();
    void flip_advect_particles(detail::FluidTaskPool& pool, Real h);
    void flip_advect_grid(detail::FluidTaskPool& pool, Real h);
    void flip_sort();
    bool flip_band(Real h);
    void flip_mark_spray();
    void flip_p2g(detail::FluidTaskPool& pool);
    void flip_lerps(detail::FluidTaskPool& pool);
    void flip_classify();
    void flip_redistance();
    void flip_forces(Real h);
    void flip_viscosity(Real h);
    int flip_pressure(Real h, int max_iterations);
    void flip_g2p(detail::FluidTaskPool& pool, Real h);
    void flip_body_impulses();
    void flip_finish_frame();
    double flip_count_volume() const;
    void flip_correct_volume();
    void flip_refresh_materials();
    Real flip_sample_u(const RVec3& l, const std::vector<Real>& f) const noexcept;
    Real flip_sample_v(const RVec3& l, const std::vector<Real>& f) const noexcept;
    Real flip_sample_w(const RVec3& l, const std::vector<Real>& f) const noexcept;
    Real flip_sample_cell(const RVec3& l, const std::vector<Real>& f) const noexcept;
    RVec3 flip_velocity_local(const RVec3& l) const noexcept;
    Real flip_depth(const RVec3& world) const;
    Real flip_volume() const;
    std::size_t ucell(int i, int j, int k) const noexcept { return (static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(m_nx + 1) + static_cast<std::size_t>(i); }
    std::size_t vcell(int i, int j, int k) const noexcept { return (static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nz) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(i); }
    std::size_t wcell(int i, int j, int k) const noexcept { return (static_cast<std::size_t>(j) * static_cast<std::size_t>(m_nz + 1) + static_cast<std::size_t>(k)) * static_cast<std::size_t>(m_nx) + static_cast<std::size_t>(i); }
    int flip_cell_of(const RVec3& l) const noexcept;
    bool flip_static_solid(int i, int j, int k) const noexcept;

    void mesh_field_particles(const std::vector<RVec3>& local, const std::vector<RVec3>* vel, Real inv_rest, bool deep) const;
    void mesh_field_cells() const;
    void mesh_blur(int passes) const;
    void mesh_extract(FluidSurfaceMesh3D& out) const;

    const BodyCache& body_cache(const RigidBody3D& body, int samples);
    FluidForces3D measure(const RigidBody3D& body, const BodyCache& cache, const RVec3& gravity, Real ambient = -1.0e30_r, const RVec3* flow = nullptr) const;
    Real ambient_level(const RigidBody3D& body, RVec3* flow = nullptr) const;
    Real particle_ambient(const RigidBody3D& body) const;
    Real cell_ambient(const RigidBody3D& body, RVec3* flow) const;
    bool local_footprint(const RigidBody3D& body, RVec3& lo, RVec3& hi) const;
    void push_fluid(const RigidBody3D& body, const BodyCache& cache, Real dt);
    void begin_push();

    FluidVolumeDef m_def;
    FluidQualitySettings m_quality;
    std::uint32_t m_id = 0;
    RVec3 m_o{};
    RVec3 m_u{ 1.0_r, 0.0_r, 0.0_r };
    RVec3 m_up{ 0.0_r, 1.0_r, 0.0_r };
    RVec3 m_w{ 0.0_r, 0.0_r, 1.0_r };
    RVec3 m_size{};
    AABB3D m_world_bounds{};
    Real m_g = 9.81_r;
    Real m_time = 0.0_r;
    Real m_accum = 0.0_r;
    Real m_lod_accum = 0.0_r;
    int m_lod_counter = 0;
    bool m_visible = true;
    bool m_frozen = false;
    bool m_all_dirty = true;
    Real m_still_level = 0.0_r;
    Real m_last_dt = 1.0_r / 60.0_r;

    int m_nx = 0, m_ny = 0, m_nz = 0;
    Real m_cs = 0.25_r;
    std::vector<std::uint8_t> m_solid;
    std::vector<std::uint8_t> m_source;
    std::vector<Real> m_fill;
    std::vector<Real> m_next;
    std::vector<std::uint8_t> m_block;
    std::vector<std::uint8_t> m_block_next;
    std::vector<std::uint8_t> m_spread_mask;
    std::vector<Real> m_flux;
    std::vector<Real> m_scale;
    std::vector<RVec3> m_cell_vel;
    std::vector<Real> m_occ_cell;
    std::vector<std::uint32_t> m_occ_touched;
    std::vector<std::uint32_t> m_confined;
    int m_kx = 0, m_ky = 0, m_kz = 0;
    std::vector<std::uint8_t> m_chunk_changed;
    std::vector<std::uint8_t> m_chunk_active;
    std::vector<std::uint32_t> m_active_list;
    std::vector<std::uint8_t> m_chunk_flag_scratch;

    int m_cx = 0, m_cz = 0;
    Real m_dx = 0.25_r;
    std::vector<Real> m_ground;
    std::vector<Real> m_depth;
    std::vector<Real> m_occ_col;
    std::vector<Real> m_occ_prev;
    std::vector<std::uint8_t> m_wall;
    std::vector<Real> m_pipe;
    std::vector<Real> m_vel_u;
    std::vector<Real> m_vel_w;
    std::vector<Real> m_change;
    std::vector<std::uint32_t> m_col_touched;
    int m_tx = 0, m_tz = 0;
    std::vector<std::uint8_t> m_tile_changed;
    std::vector<std::uint8_t> m_tile_active;
    std::vector<std::uint32_t> m_tile_list;
    mutable std::vector<Real> m_heights;
    mutable std::vector<RVec3> m_normals;
    mutable bool m_heights_dirty = true;

    Real m_radius = 0.05_r;
    Real m_spacing = 0.1_r;
    Real m_h = 0.2_r;
    Real m_pmass = 1.0_r;
    Real m_poly6 = 0.0_r;
    Real m_spiky = 0.0_r;
    Real m_wq = 1.0_r;
    std::size_t m_max_particles = 0;
    std::size_t m_awake_count = 0;
    std::size_t m_contact_count = 0;
    std::vector<RVec3> m_px;
    std::vector<RVec3> m_pv;
    std::vector<RVec3> m_pp;
    std::vector<RVec3> m_pv0;
    std::vector<RVec3> m_dp;
    std::vector<RVec3> m_omega;
    std::vector<Real> m_lambda;
    std::vector<Real> m_density;
    std::vector<std::uint16_t> m_calm;
    std::vector<std::uint8_t> m_asleep;
    std::vector<std::uint8_t> m_live;
    std::vector<std::uint32_t> m_nbr;
    std::vector<std::uint16_t> m_nbr_count;
    std::vector<std::uint32_t> m_bucket_start;
    std::vector<std::uint32_t> m_bucket_count;
    std::vector<std::uint32_t> m_sorted;
    std::vector<std::uint32_t> m_pkey;
    std::vector<std::uint8_t> m_tmp_u8;
    std::vector<std::uint16_t> m_tmp_u16;
    std::vector<Real> m_tmp_r;
    int m_gx = 1, m_gy = 1, m_gz = 1;
    Real m_gcell = 0.2_r;
    std::vector<ParticleContact> m_contacts_scratch;
    std::vector<std::int32_t> m_contact_of;
    std::vector<NearBody> m_near;
    std::vector<FluidEmitter3D> m_emitters;
    int m_sx = 0, m_sz = 0;
    Real m_scell = 0.0_r;
    std::vector<Real> m_sgrid;
    static constexpr std::size_t kMaxNeighbours = 64;

    std::unordered_map<std::uint64_t, BodyCache> m_cache;
    std::unordered_set<std::uint64_t> m_ignored;
    std::unordered_map<std::uint64_t, Real> m_entry;
    std::uint64_t m_frame = 0;

    FluidSolver3D m_solver = FluidSolver3D::pbf;
    Real m_pr = 0.025_r;
    std::uint64_t m_version = 1;
    std::size_t m_liquid_count = 0;
    std::size_t m_deep_count = 0;
    int m_last_substeps = 0;
    int m_last_iterations = 0;
    Real m_max_speed = 0.0_r;
    bool m_flip_asleep = false;
    int m_calm_frames = 0;
    Real m_last_h = 0.0_r;
    Real m_vlim_cells = 2.0_r;
    Real m_flip_mix = 0.95_r;
    Real m_drift_scale = 1.0_r;
    Real m_calm_scale = 1.0_r;
    double m_target_volume = 0.0;
    bool m_target_valid = false;
    Real m_visc_coef = 0.0_r;
    Real m_cohesion = 0.0_r;
    std::size_t m_solid_count = 0;
    std::uint32_t m_seed = 1;
    int m_flim[3] = { 0, 0, 0 };
    CellBox m_box{};
    CellBox m_box_prev{};
    CellBox m_work{};
    std::vector<RVec3> m_fx;
    std::vector<RVec3> m_fv;
    std::vector<RVec3> m_fcu;
    std::vector<RVec3> m_fcv;
    std::vector<RVec3> m_fcw;
    std::vector<RVec3> m_fprev;
    std::vector<RVec3> m_fdv;
    std::vector<std::int32_t> m_fcontact;
    std::vector<std::uint32_t> m_fkey;
    std::vector<std::uint8_t> m_fspray;
    std::vector<RVec3> m_ftmp_a;
    std::vector<RVec3> m_ftmp_b;
    std::vector<RVec3> m_ftmp_c;
    std::vector<RVec3> m_ftmp_d;
    std::vector<RVec3> m_ftmp_e;
    std::vector<std::uint32_t> m_cstart;
    std::vector<std::uint32_t> m_cfill;
    std::vector<std::uint16_t> m_lcount;
    std::vector<Real> m_gu, m_gv, m_gw;
    std::vector<Real> m_gu0, m_gv0, m_gw0;
    std::vector<Real> m_guw, m_gvw, m_gww;
    std::vector<Real> m_gua, m_gva, m_gwa;
    std::vector<Real> m_gscratch;
    std::vector<std::uint8_t> m_fvalid_u, m_fvalid_v, m_fvalid_w;
    std::vector<std::int32_t> m_lerp_i;
    std::vector<Real> m_lerp_t;
    std::vector<std::uint8_t> m_lsolid_nbr;
    mutable std::vector<std::uint8_t> m_minside;
    std::vector<Real> m_lphi;
    std::vector<Real> m_lphi_adv;
    std::vector<std::uint8_t> m_ltype;
    std::vector<Real> m_ldens;
    std::vector<Real> m_lpress;
    std::vector<std::uint8_t> m_locc;
    std::vector<RVec3> m_locc_vel;
    std::vector<std::uint32_t> m_locc_list;
    std::vector<std::int32_t> m_lidx;
    std::vector<std::uint32_t> m_llist;
    std::vector<double> m_pcg_b, m_pcg_x, m_pcg_r, m_pcg_z, m_pcg_s, m_pcg_q;
    std::vector<double> m_pcg_diag, m_pcg_ax, m_pcg_ay, m_pcg_az, m_pcg_pre;
    std::vector<std::int32_t> m_pcg_nb;
    std::vector<std::uint32_t> m_visc_faces;
    std::vector<std::int32_t> m_visc_nb;
    std::vector<Real> m_visc_d;

    mutable FluidSurfaceMesh3D m_mesh;
    mutable std::vector<RVec3> m_mlocal;
    mutable std::vector<float> m_mfield;
    mutable std::vector<float> m_mscratch;
    mutable std::vector<float> m_mspeed;
    mutable std::vector<float> m_mweight;
    mutable std::vector<std::int32_t> m_mvid;
    mutable std::vector<std::uint32_t> m_mtouched;
    mutable int m_mlo[3] = { 0, 0, 0 };
    mutable int m_mdim[3] = { 0, 0, 0 };
    mutable int m_mnodes[3] = { 0, 0, 0 };
    mutable Real m_mh = 0.0_r;
    mutable Real m_miso = 0.5_r;
};

struct FluidWorld3DConfig {
    RVec3 gravity = to_real(default_axes3d().gravity(constants::gravity()));
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

class FluidWorld3D {
public:
    using Config = FluidWorld3DConfig;

    explicit FluidWorld3D(const Config& cfg = Config());
    FluidWorld3D(const FluidWorld3D&) = delete;
    FluidWorld3D& operator=(const FluidWorld3D&) = delete;

    Config& config() noexcept { return m_cfg; }
    const Config& config() const noexcept { return m_cfg; }
    void set_quality(FluidQuality q);
    void set_quality(const FluidQualitySettings& q) { m_quality = q; }
    const FluidQualitySettings& quality() const noexcept { return m_quality; }
    void set_threads(int threads);
    void set_gravity(const RVec3& g) noexcept { m_cfg.gravity = g; }
    void set_axes(const Axes3D& axes) noexcept { m_cfg.gravity = to_real(axes.gravity(constants::gravity())); }
    void set_focus(const RVec3& point) noexcept { m_focus = point; m_has_focus = true; }
    const RVec3& focus() const noexcept { return m_focus; }

    FluidVolume3D& add_volume(const FluidVolumeDef& def);
    FluidVolume3D& add_volume(const AABB3D& bounds, FluidLevel level, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r);
    FluidVolume3D& add_volume(const AABB3D& bounds, const FluidMaterial& material = FluidMaterial::water(), Real fill = 0.0_r) { return add_volume(bounds, m_cfg.default_level, material, fill); }
    void remove_volume(FluidVolume3D& volume);
    std::size_t volume_count() const noexcept { return m_volumes.size(); }
    FluidVolume3D& volume(std::size_t i) { return *m_volumes.at(i); }
    const FluidVolume3D& volume(std::size_t i) const { return *m_volumes.at(i); }

    void step(Simulation3D& sim, Real dt);
    void step(Real dt);
    void step(Simulation3D& sim) { step(sim, sim.config().fixed_dt); }
    void step() { step(m_cfg.fixed_dt); }

    FluidVolume3D* volume_at(const RVec3& point);
    const FluidVolume3D* volume_at(const RVec3& point) const;
    Real surface_height(const RVec3& point) const;
    Real surface_height(Real a, Real b) const;
    Real depth_at(const RVec3& point) const;
    RVec3 velocity_at(const RVec3& point) const;
    bool inside(const RVec3& point) const;

    FluidForces3D forces_on(const RigidBody3D& body);
    const FluidForces3D* body_forces(const RigidBody3D& body) const;
    const FluidStats3D& stats() const noexcept { return m_stats; }

private:
    void couple(Simulation3D& sim, Real dt);
    void schedule(FluidVolume3D& v, Real dt, Real& use_dt, bool& run);

    Config m_cfg;
    FluidQualitySettings m_quality;
    std::vector<std::unique_ptr<FluidVolume3D>> m_volumes;
    detail::FluidTaskPool m_pool;
    FluidStats3D m_stats;
    RVec3 m_focus{};
    bool m_has_focus = false;
    std::uint32_t m_next_id = 1;
    std::unordered_map<std::uint64_t, FluidForces3D> m_body_forces;
    std::vector<RigidBody3D*> m_bodies;
};

} // namespace physics
} // namespace fizmo

#endif // FIZMO_PHYSICS_FLUID_3D_HPP
