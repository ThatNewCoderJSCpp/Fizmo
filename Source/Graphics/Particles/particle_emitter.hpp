#ifndef FIZMO_PARTICLES_EMITTER_HPP
#define FIZMO_PARTICLES_EMITTER_HPP

#include "particle_settings.hpp"
#include "../../Vectors/vectors.hpp"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace fizmo {
namespace graphics {
class Texture;
} // namespace graphics

namespace particles {

class Emitter;
class ParticleSystem;

struct Box3F {
    Float3 lo{ 0.0f, 0.0f, 0.0f };
    Float3 hi{ 0.0f, 0.0f, 0.0f };
    bool   valid = false;

    void reset() noexcept { valid = false; }
    void add(const Float3& p) noexcept;
    void add(const Box3F& b) noexcept;
    void pad(float r) noexcept { lo.x -= r; lo.y -= r; lo.z -= r; hi.x += r; hi.y += r; hi.z += r; }
    Float3 center() const noexcept { return { 0.5f * (lo.x + hi.x), 0.5f * (lo.y + hi.y), 0.5f * (lo.z + hi.z) }; }
};

struct WorldBounds {
    linalg::vector3d lo{};
    linalg::vector3d hi{};
    bool             valid = false;
};

struct PlaneCollider {
    Float3        normal{ 0.0f, 1.0f, 0.0f };
    float         offset = 0.0f;
    std::uint32_t layers = 0xFFFFFFFFu;
};

struct SphereCollider {
    Float3        center{};
    float         radius = 1.0f;
    std::uint32_t layers = 0xFFFFFFFFu;
};

struct BoxCollider {
    Float3        lo{ -0.5f, -0.5f, -0.5f };
    Float3        hi{ 0.5f, 0.5f, 0.5f };
    std::uint32_t layers = 0xFFFFFFFFu;
};

struct ColliderSet {
    std::vector<PlaneCollider>  planes;
    std::vector<SphereCollider> spheres;
    std::vector<BoxCollider>    boxes;

    bool empty() const noexcept { return planes.empty() && spheres.empty() && boxes.empty(); }
    void clear() noexcept { planes.clear(); spheres.clear(); boxes.clear(); }
};

enum : std::uint8_t { kHitNone = 0, kHitBounce = 1, kHitKill = 2 };

struct CollisionBatch {
    float*              px = nullptr;
    float*              py = nullptr;
    float*              pz = nullptr;
    float*              vx = nullptr;
    float*              vy = nullptr;
    float*              vz = nullptr;
    const float*        size = nullptr;
    std::uint8_t*       hit = nullptr;
    std::size_t         count = 0;
    float               dt = 0.0f;
    linalg::vector3d    origin{};
    const Emitter*      emitter = nullptr;
};

using CollisionCallback = void (*)(void* user, CollisionBatch& batch);

struct ParticleView {
    const float* px = nullptr;
    const float* py = nullptr;
    const float* pz = nullptr;
    const float* vx = nullptr;
    const float* vy = nullptr;
    const float* vz = nullptr;
    const float* age = nullptr;
    const float* inv_life = nullptr;
    const float* size = nullptr;
    const float* rotation = nullptr;
    const float* rnd = nullptr;
    std::size_t  count = 0;
};

struct SpawnEvent {
    float px = 0.0f, py = 0.0f, pz = 0.0f;
    float vx = 0.0f, vy = 0.0f, vz = 0.0f;
};

struct BakedCurves {
    static constexpr int kSize = 128;
    static constexpr int kTints = 16;

    float         size[kSize];
    std::uint32_t rgba[kTints * kSize];
    int           tint_levels = 1;
    float  alpha[kSize];
    float  speed[kSize];
    float  spin[kSize];
    ColorF color[kSize];

    const std::uint32_t* tint(float rnd) const noexcept {
        if (tint_levels <= 1) return rgba;
        const int l = static_cast<int>(rnd * static_cast<float>(tint_levels - 1) + 0.5f);
        return rgba + (l < 0 ? 0 : (l >= tint_levels ? tint_levels - 1 : l)) * kSize;
    }

    static int index(float t) noexcept {
        const float s = t * static_cast<float>(kSize - 1);
        const int i = static_cast<int>(s);
        return i < 0 ? 0 : (i >= kSize ? kSize - 1 : i);
    }
};

struct EmitterFlags {
    bool gravity = false;
    bool drag = false;
    bool turbulence = false;
    bool speed_curve = false;
    bool spin = false;
    bool rotation = false;
    bool collide = false;
    bool death_events = false;
    bool collision_events = false;
    bool birth_events = false;
    bool flipbook = false;
    bool instanced_ok = false;
};

class Emitter {
public:
    static constexpr std::size_t kChunk = 4096;

    explicit Emitter(const EmitterSettings& settings = EmitterSettings{}, std::uint32_t seed = 0);
    ~Emitter();
    Emitter(const Emitter&) = delete;
    Emitter& operator=(const Emitter&) = delete;

    const EmitterSettings& settings() const noexcept { return m_settings; }
    void set_settings(const EmitterSettings& settings);

    void set_position(double x, double y, double z = 0.0) noexcept;
    void set_position(const linalg::vector3d& p) noexcept { set_position(p.x, p.y, p.z); }
    linalg::vector3d position() const noexcept;
    void teleport(const linalg::vector3d& p) noexcept;

    void set_orientation(const linalg::vector3d& forward, const linalg::vector3d& up) noexcept;
    void set_basis(const linalg::vector3d& right, const linalg::vector3d& up, const linalg::vector3d& forward) noexcept;
    void set_angle(double degrees) noexcept;
    void set_transform(const linalg::vector3d& position, const linalg::vector3d& forward, const linalg::vector3d& up) noexcept {
        set_position(position);
        set_orientation(forward, up);
    }
    linalg::vector3d right() const noexcept { return { m_basis[0].x, m_basis[0].y, m_basis[0].z }; }
    linalg::vector3d up() const noexcept { return { m_basis[1].x, m_basis[1].y, m_basis[1].z }; }
    linalg::vector3d forward() const noexcept { return { m_basis[2].x, m_basis[2].y, m_basis[2].z }; }

    void set_velocity(const linalg::vector3d& v) noexcept;
    void clear_velocity() noexcept { m_velocity_set = false; }
    linalg::vector3d velocity() const noexcept { return { m_velocity.x, m_velocity.y, m_velocity.z }; }

    void play() noexcept;
    void stop() noexcept { m_playing = false; }
    void stop_and_clear() noexcept { m_playing = false; clear(); }
    void pause(bool paused) noexcept { m_paused = paused; }
    void restart() noexcept { clear(); play(); }
    std::size_t burst(int count);
    void clear() noexcept;

    bool playing() const noexcept { return m_playing; }
    bool paused() const noexcept { return m_paused; }
    bool finished() const noexcept;
    bool sleeping() const noexcept { return finished() || m_paused; }

    std::size_t count() const noexcept { return m_count; }
    std::size_t capacity() const noexcept { return m_capacity; }
    std::size_t total_count() const noexcept;
    float time() const noexcept { return m_time; }

    WorldBounds bounds() const noexcept;
    WorldBounds potential_bounds() const noexcept;
    const Box3F& local_bounds() const noexcept { return m_bounds; }

    void step(float dt);
    void warm_up(float seconds, int max_steps = 8);

    void set_texture(const graphics::Texture* texture) noexcept { m_texture = texture; }
    const graphics::Texture* texture() const noexcept { return m_texture; }

    void set_collision_callback(CollisionCallback fn, void* user) noexcept { m_callback = fn; m_callback_user = user; }
    void set_colliders(const ColliderSet* colliders) noexcept;

    void set_emission_mesh(const float* xyz, std::size_t triangle_count);

    void set_origin(const linalg::vector3d& origin);
    const linalg::vector3d& origin() const noexcept { return m_origin; }

    int lod_level() const noexcept { return m_lod; }
    float spawn_scale() const noexcept { return m_spawn_scale; }
    bool visible() const noexcept { return m_visible; }
    bool simple_render() const noexcept { return m_simple_render; }
    std::uint32_t id() const noexcept { return m_id; }
    float render_lag() const noexcept { return m_render_lag; }
    int priority() const noexcept { return m_settings.priority; }

    std::size_t child_count() const noexcept { return m_subs.size(); }
    Emitter* child(std::size_t i) noexcept { return i < m_subs.size() ? m_subs[i].child.get() : nullptr; }
    const Emitter* child(std::size_t i) const noexcept { return i < m_subs.size() ? m_subs[i].child.get() : nullptr; }

    ParticleView view() const noexcept;
    const BakedCurves& curves() const noexcept { return *m_curves; }
    const EmitterFlags& flags() const noexcept { return m_flags; }
    void basis_world(float out[9]) const noexcept;
    Float3 position_local() const noexcept { return m_pos; }

    std::uint64_t spawned_total() const noexcept { return m_spawned_total; }
    std::uint64_t died_total() const noexcept { return m_died_total; }
    std::size_t last_spawned() const noexcept { return m_last_spawned; }
    std::size_t last_quota_limited() const noexcept { return m_last_limited; }

    ColorF particle_color(float t, float rnd) const noexcept;
    float particle_size(std::size_t i) const noexcept;
    std::uint32_t particle_frame(std::size_t i) const noexcept;

    void reseed(std::uint32_t seed) noexcept;

private:
    friend class ParticleSystem;

    struct Deferred {};
    Emitter(const EmitterSettings& settings, std::uint32_t seed, Deferred);

    struct Rng {
        std::uint64_t state = 0x853C49E6748FEA9Bull;
        std::uint64_t inc = 0xDA3E39CB94B95BDBull;

        void seed(std::uint64_t s, std::uint64_t seq) noexcept;
        std::uint32_t next() noexcept;
        float uniform() noexcept { return static_cast<float>(next() >> 8) * (1.0f / 16777216.0f); }
        float range(const RangeF& r) noexcept { return r.lo + (r.hi - r.lo) * uniform(); }
    };

    struct SubLink {
        SubEmitterTrigger        trigger = SubEmitterTrigger::Death;
        int                      count = 1;
        float                    probability = 1.0f;
        float                    inherit = 0.0f;
        std::unique_ptr<Emitter> child;
    };

    struct StepParams {
        float  dt = 0.0f;
        float  drag_factor = 1.0f;
        Float3 accel{};
        Float3 wind{};
        Float3 noise_offset{};
        float  noise_frequency = 1.0f;
        float  turbulence = 0.0f;
        std::size_t parity = 0;
    };

    void allocate(std::size_t capacity);
    void bake();
    void build_children();
    void reset_timeline() noexcept;

    void prepare_step(float dt) noexcept;
    void integrate(std::size_t begin, std::size_t end, Box3F& chunk_bounds) noexcept;
    void finalize(std::size_t quota);
    void finalize_family(std::size_t quota_self);
    void step_family(float dt, std::size_t quota);
    void catch_up(float seconds, int max_steps, std::size_t quota);

    void collide(std::size_t begin, std::size_t end) noexcept;
    void kill_dead();
    std::size_t emit(float dt, std::size_t quota);
    std::size_t spawn_events(std::size_t quota);
    bool spawn_one(float frac, float dt, const SpawnEvent* at);
    void push_event(SubEmitterTrigger trigger, std::size_t i);
    void sample_shape(Float3& pos, Float3& dir);

    Float3 to_world_dir(const Float3& v) const noexcept;
    std::size_t want_this_step(float dt) const noexcept;
    void set_lod(int level, float spawn_scale, bool visible, bool simple) noexcept {
        m_lod = level; m_spawn_scale = spawn_scale; m_visible = visible; m_simple_render = simple;
    }

    EmitterSettings              m_settings;
    EmitterFlags                 m_flags;
    std::unique_ptr<BakedCurves> m_curves;
    std::unique_ptr<float[]>     m_block;
    std::unique_ptr<std::uint8_t[]> m_hit;
    float*                       m_px = nullptr;
    float*                       m_py = nullptr;
    float*                       m_pz = nullptr;
    float*                       m_vx = nullptr;
    float*                       m_vy = nullptr;
    float*                       m_vz = nullptr;
    float*                       m_age = nullptr;
    float*                       m_inv_life = nullptr;
    float*                       m_size = nullptr;
    float*                       m_rot = nullptr;
    float*                       m_spin = nullptr;
    float*                       m_rnd = nullptr;
    std::size_t                  m_count = 0;
    std::size_t                  m_capacity = 0;

    Rng                          m_rng;
    std::uint32_t                m_seed = 0;
    linalg::vector3d             m_origin{};
    Float3                       m_pos{};
    Float3                       m_prev_pos{};
    bool                         m_has_prev = false;
    Float3                       m_basis[3] = { { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } };
    Float3                       m_velocity{};
    bool                         m_velocity_set = false;
    Float3                       m_dir{ 0.0f, 1.0f, 0.0f };
    Float3                       m_dir_t1{ 1.0f, 0.0f, 0.0f };
    Float3                       m_dir_t2{ 0.0f, 0.0f, 1.0f };

    bool                         m_playing = false;
    bool                         m_paused = false;
    float                        m_time = 0.0f;
    float                        m_delay_left = 0.0f;
    float                        m_rate_carry = 0.0f;
    float                        m_distance_carry = 0.0f;
    float                        m_noise_time = 0.0f;
    std::vector<int>             m_burst_cycles;
    std::size_t                  m_manual_burst = 0;

    StepParams                   m_step;
    std::vector<Box3F>           m_chunk_bounds;
    Box3F                        m_bounds;

    std::vector<SubLink>         m_subs;
    std::vector<SpawnEvent>      m_inbox;
    std::size_t                  m_inbox_count = 0;
    bool                         m_is_child = false;
    int                          m_event_count = 1;
    float                        m_event_probability = 1.0f;

    std::vector<Float3>          m_mesh;
    std::vector<float>           m_mesh_cdf;

    const graphics::Texture*     m_texture = nullptr;
    const ColliderSet*           m_colliders = nullptr;
    CollisionCallback            m_callback = nullptr;
    void*                        m_callback_user = nullptr;

    std::uint32_t                m_id = 0;
    int                          m_lod = 0;
    float                        m_spawn_scale = 1.0f;
    bool                         m_visible = true;
    bool                         m_simple_render = false;
    int                          m_interval = 1;
    std::uint32_t                m_phase = 0;
    float                        m_pending = 0.0f;
    float                        m_offscreen_time = 0.0f;
    bool                         m_paused_offscreen = false;
    std::size_t                  m_quota = 0;
    std::uint32_t                m_order = 0;
    int                          m_depth = 0;
    int                          m_effect = -1;
    bool                         m_auto_release = false;
    bool                         m_play_on_step = false;
    float                        m_catch_up = 0.0f;
    float                        m_render_lag = 0.0f;

    std::uint64_t                m_spawned_total = 0;
    std::uint64_t                m_died_total = 0;
    std::size_t                  m_last_spawned = 0;
    std::size_t                  m_last_limited = 0;
};

} // namespace particles
} // namespace fizmo

#endif // FIZMO_PARTICLES_EMITTER_HPP
