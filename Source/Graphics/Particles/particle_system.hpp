#ifndef FIZMO_PARTICLES_SYSTEM_HPP
#define FIZMO_PARTICLES_SYSTEM_HPP

#include "particle_emitter.hpp"
#include "../camera_3d.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace particles {

namespace detail {
class WorkerPool;
} // namespace detail

struct CameraView {
    bool                active = false;
    bool                is_2d = false;
    linalg::vector3d    position{};
    linalg::vector3d    forward{ 0.0, 0.0, -1.0 };
    linalg::vector3d    right{ 1.0, 0.0, 0.0 };
    linalg::vector3d    up{ 0.0, 1.0, 0.0 };
    bool                has_frustum = false;
    graphics::Frustum3D frustum{};
    bool                orthographic = false;
    double              pixel_scale = 0.0;
    double              near_clip = 0.0;
    double              rect_x0 = 0.0;
    double              rect_y0 = 0.0;
    double              rect_x1 = 0.0;
    double              rect_y1 = 0.0;

    static CameraView none() noexcept { return CameraView{}; }
    static CameraView from_camera(const graphics::Camera3D& camera) noexcept;
    static CameraView at(const linalg::vector3d& position, const linalg::vector3d& forward, const linalg::vector3d& up = { 0.0, 1.0, 0.0 }) noexcept;
    static CameraView from_rect(double x0, double y0, double x1, double y1) noexcept;
};

struct LodLevel {
    float distance = 0.0f;
    float spawn_scale = 1.0f;
    int   update_interval = 1;
    bool  simple_render = false;
};

struct SystemSettings {
    std::size_t   budget = 100000;
    float         soft_budget = 0.85f;
    float         min_spawn_scale = 0.0f;
    bool          adaptive = false;
    float         target_ms = 2.0f;
    float         min_quality = 0.25f;
    double        fixed_step = 1.0 / 60.0;
    int           max_substeps = 4;
    double        max_frame_dt = 0.25;
    int           threads = 1;
    LodLevel      lod[4] = { { 25.0f, 1.0f, 1, false }, { 50.0f, 0.6f, 2, false }, { 100.0f, 0.3f, 3, true }, { 200.0f, 0.12f, 4, true } };
    float         cull_distance = 300.0f;
    float         cull_margin = 0.5f;
    int           offscreen_interval = 6;
    float         max_catch_up = 2.0f;
    int           catch_up_steps = 6;
    float         min_pixel_size = 0.75f;
    bool          extrapolate = true;
    std::uint32_t seed = 0x5EEDu;
};

struct ParticleStats {
    std::size_t   particles = 0;
    std::size_t   emitters = 0;
    std::size_t   active = 0;
    std::size_t   sleeping = 0;
    std::size_t   offscreen = 0;
    std::size_t   paused = 0;
    std::size_t   distance_culled = 0;
    std::size_t   updated = 0;
    std::size_t   spawned = 0;
    std::size_t   limited = 0;
    std::size_t   budget = 0;
    double        step_ms = 0.0;
    float         quality = 1.0f;
    float         budget_scale = 1.0f;
    int           substeps = 0;
    std::uint64_t ticks = 0;
};

class ParticleSystem {
public:
    explicit ParticleSystem(const SystemSettings& settings = SystemSettings{});
    ~ParticleSystem();
    ParticleSystem(const ParticleSystem&) = delete;
    ParticleSystem& operator=(const ParticleSystem&) = delete;

    const SystemSettings& settings() const noexcept { return m_settings; }
    void set_settings(const SystemSettings& settings);
    void set_budget(std::size_t budget) noexcept { m_settings.budget = budget; }
    void set_threads(int threads);
    int threads() const noexcept;

    Emitter& create_emitter(const EmitterSettings& settings);
    Emitter& create_emitter(const EmitterSettings& settings, const linalg::vector3d& position);
    bool destroy(Emitter* emitter);
    bool destroy(Emitter& emitter) { return destroy(&emitter); }

    int register_effect(const EmitterSettings& settings, std::size_t preallocate = 0);
    const EmitterSettings* effect_settings(int effect) const noexcept;
    std::size_t effect_pool_size(int effect) const noexcept;
    Emitter* spawn_effect(int effect, const linalg::vector3d& position, const linalg::vector3d& forward = { 0.0, 0.0, 1.0 },
                          const linalg::vector3d& up = { 0.0, 1.0, 0.0 }, const linalg::vector3d& velocity = { 0.0, 0.0, 0.0 });
    Emitter* spawn_effect_2d(int effect, double x, double y, double angle_degrees = 0.0);

    void step(double dt, const CameraView& view = CameraView::none());
    void clear() noexcept;

    void set_origin(const linalg::vector3d& origin);
    const linalg::vector3d& origin() const noexcept { return m_origin; }

    void add_plane(const linalg::vector3d& normal, const linalg::vector3d& point, std::uint32_t layers = 0xFFFFFFFFu);
    void add_ground(double height, std::uint32_t layers = 0xFFFFFFFFu) { add_plane({ 0.0, 1.0, 0.0 }, { 0.0, height, 0.0 }, layers); }
    void add_sphere(const linalg::vector3d& center, double radius, std::uint32_t layers = 0xFFFFFFFFu);
    void add_box(const linalg::vector3d& lo, const linalg::vector3d& hi, std::uint32_t layers = 0xFFFFFFFFu);
    void clear_colliders() noexcept;
    const ColliderSet& colliders() const noexcept { return m_colliders; }

    std::size_t particle_count() const noexcept;
    std::size_t emitter_count() const noexcept { return m_live.size(); }
    std::size_t root_count() const noexcept { return m_live.size(); }
    const Emitter* root(std::size_t i) const noexcept { return i < m_live.size() ? m_live[i].get() : nullptr; }
    Emitter* root(std::size_t i) noexcept { return i < m_live.size() ? m_live[i].get() : nullptr; }

    const ParticleStats& stats() const noexcept { return m_stats; }
    float quality() const noexcept { return m_quality; }
    void set_quality(float q) noexcept { m_quality = q; }
    double render_ahead() const noexcept { return m_render_ahead; }
    const CameraView& last_view() const noexcept { return m_view; }

private:
    struct Effect {
        EmitterSettings                       settings;
        std::vector<std::unique_ptr<Emitter>> free;
        std::size_t                           made = 0;
    };

    struct ChunkTask {
        Emitter*    emitter = nullptr;
        std::size_t begin = 0;
        std::size_t end = 0;
        Box3F*      box = nullptr;
    };

    struct FamilyTask {
        Emitter* root = nullptr;
        float    dt = 0.0f;
        float    catch_up = 0.0f;
    };

    struct TierEntry {
        Emitter* root = nullptr;
        float    expected = 0.0f;
    };

    std::unique_ptr<Emitter> make_emitter(const EmitterSettings& settings, std::uint32_t seed);
    void adopt(Emitter& e);
    void evaluate_view(const CameraView& view);
    void apply_budget();
    void tick(float dt);
    void assign_quotas();
    void release_finished();
    void rebuild_colliders();
    void number_family(Emitter& e, int depth);

    static void integrate_thunk(void* self, std::size_t i);
    static void finalize_thunk(void* self, std::size_t i);

    SystemSettings                        m_settings;
    std::unique_ptr<detail::WorkerPool>   m_pool;
    std::vector<std::unique_ptr<Emitter>> m_live;
    std::vector<Effect>                   m_effects;
    linalg::vector3d                      m_origin{};

    std::vector<linalg::vector3d>         m_plane_normals;
    std::vector<double>                   m_plane_offsets;
    std::vector<linalg::vector3d>         m_sphere_centers;
    std::vector<double>                   m_sphere_radii;
    std::vector<linalg::vector3d>         m_box_lo;
    std::vector<linalg::vector3d>         m_box_hi;
    std::vector<std::uint32_t>            m_plane_layers;
    std::vector<std::uint32_t>            m_sphere_layers;
    std::vector<std::uint32_t>            m_box_layers;
    ColliderSet                           m_colliders;

    std::vector<ChunkTask>                m_chunks;
    std::vector<FamilyTask>               m_families;
    std::vector<Emitter*>                 m_updating;
    std::vector<TierEntry>                m_tiers;

    CameraView                            m_view;
    ParticleStats                         m_stats;
    double                                m_accumulator = 0.0;
    double                                m_render_ahead = 0.0;
    float                                 m_quality = 1.0f;
    double                                m_ema_ms = 0.0;
    std::uint64_t                         m_tick = 0;
    std::uint32_t                         m_next_order = 0;
    std::uint32_t                         m_next_id = 0;
    std::uint32_t                         m_spawn_counter = 0;
};

} // namespace particles
} // namespace fizmo

#endif // FIZMO_PARTICLES_SYSTEM_HPP
