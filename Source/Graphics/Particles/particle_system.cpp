#include "fizmo_library.hpp"
#include "particle_system.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <limits>
#include <mutex>
#include <thread>

namespace fizmo {
namespace particles {

namespace detail {

class WorkerPool {
public:
    using Job = void (*)(void*, std::size_t);

    WorkerPool() = default;
    ~WorkerPool() { stop(); }
    WorkerPool(const WorkerPool&) = delete;
    WorkerPool& operator=(const WorkerPool&) = delete;

    void resize(int threads) {
        if (threads < 1) threads = 1;
        if (threads == size()) return;
        stop();
        m_stop = false;
        m_workers.reserve(static_cast<std::size_t>(threads - 1));
        for (int i = 1; i < threads; ++i) m_workers.emplace_back([this] { worker(); });
    }

    int size() const noexcept { return static_cast<int>(m_workers.size()) + 1; }

    void run(std::size_t count, Job fn, void* ctx) {
        if (count == 0) return;
        if (m_workers.empty() || count == 1) {
            for (std::size_t i = 0; i < count; ++i) fn(ctx, i);
            return;
        }
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_fn = fn;
            m_ctx = ctx;
            m_count = count;
            m_next.store(0);
            m_done.store(0);
            ++m_generation;
        }
        m_wake.notify_all();
        work(fn, ctx, count);
        std::unique_lock<std::mutex> lock(m_mutex);
        m_finished.wait(lock, [&] { return m_done.load() >= m_count && m_active == 0; });
        m_fn = nullptr;
    }

private:
    void work(Job fn, void* ctx, std::size_t count) {
        for (;;) {
            const std::size_t i = m_next.fetch_add(1);
            if (i >= count) break;
            fn(ctx, i);
            if (m_done.fetch_add(1) + 1 == count) {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_finished.notify_all();
            }
        }
    }

    void worker() {
        std::uint64_t seen = 0;
        for (;;) {
            Job fn = nullptr;
            void* ctx = nullptr;
            std::size_t count = 0;
            {
                std::unique_lock<std::mutex> lock(m_mutex);
                m_wake.wait(lock, [&] { return m_stop || (m_fn && m_generation != seen); });
                if (m_stop) return;
                seen = m_generation;
                fn = m_fn;
                ctx = m_ctx;
                count = m_count;
                ++m_active;
            }
            work(fn, ctx, count);
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                --m_active;
            }
            m_finished.notify_all();
        }
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_stop = true;
        }
        m_wake.notify_all();
        for (std::thread& t : m_workers) t.join();
        m_workers.clear();
    }

    std::vector<std::thread>  m_workers;
    std::mutex                m_mutex;
    std::condition_variable   m_wake;
    std::condition_variable   m_finished;
    Job                       m_fn = nullptr;
    void*                     m_ctx = nullptr;
    std::size_t               m_count = 0;
    std::atomic<std::size_t>  m_next{ 0 };
    std::atomic<std::size_t>  m_done{ 0 };
    std::uint64_t             m_generation = 0;
    int                       m_active = 0;
    bool                      m_stop = false;
};

} // namespace detail

namespace {

std::uint32_t hash32(std::uint32_t a, std::uint32_t b) noexcept {
    std::uint32_t h = a * 0x9E3779B1u ^ (b * 0x85EBCA77u + 0x165667B1u);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h ? h : 1u;
}

template <class F>
void for_family(Emitter& e, F& f) {
    f(e);
    for (std::size_t i = 0; i < e.child_count(); ++i) if (Emitter* c = e.child(i)) for_family(*c, f);
}

} // namespace

CameraView CameraView::from_camera(const graphics::Camera3D& camera) noexcept {
    CameraView v;
    v.active = true;
    v.position = camera.position();
    v.forward = camera.forward();
    v.right = camera.right();
    v.up = camera.up();
    v.has_frustum = true;
    v.frustum = camera.frustum();
    v.orthographic = camera.projection_type() == graphics::Projection3D::Orthographic;
    const double h = static_cast<double>(camera.viewport_height() ? camera.viewport_height() : 720u);
    if (v.orthographic) v.pixel_scale = camera.ortho_height() > 0.0 ? h / camera.ortho_height() : 0.0;
    else v.pixel_scale = h / (2.0 * std::tan(camera.fov_y() * 0.00872664625997165));
    v.near_clip = camera.near_plane();
    return v;
}

CameraView CameraView::at(const linalg::vector3d& position, const linalg::vector3d& forward, const linalg::vector3d& up) noexcept {
    CameraView v;
    v.active = true;
    v.position = position;
    const double fl = std::sqrt(forward.x * forward.x + forward.y * forward.y + forward.z * forward.z);
    v.forward = fl > 0.0 ? linalg::vector3d{ forward.x / fl, forward.y / fl, forward.z / fl } : linalg::vector3d{ 0.0, 0.0, -1.0 };
    linalg::vector3d r{ v.forward.y * up.z - v.forward.z * up.y, v.forward.z * up.x - v.forward.x * up.z, v.forward.x * up.y - v.forward.y * up.x };
    const double rl = std::sqrt(r.x * r.x + r.y * r.y + r.z * r.z);
    v.right = rl > 0.0 ? linalg::vector3d{ r.x / rl, r.y / rl, r.z / rl } : linalg::vector3d{ 1.0, 0.0, 0.0 };
    v.up = { v.right.y * v.forward.z - v.right.z * v.forward.y, v.right.z * v.forward.x - v.right.x * v.forward.z, v.right.x * v.forward.y - v.right.y * v.forward.x };
    return v;
}

CameraView CameraView::from_rect(double x0, double y0, double x1, double y1) noexcept {
    CameraView v;
    v.active = true;
    v.is_2d = true;
    v.rect_x0 = x0 < x1 ? x0 : x1;
    v.rect_x1 = x0 < x1 ? x1 : x0;
    v.rect_y0 = y0 < y1 ? y0 : y1;
    v.rect_y1 = y0 < y1 ? y1 : y0;
    v.position = { 0.5 * (x0 + x1), 0.5 * (y0 + y1), 0.0 };
    v.forward = { 0.0, 0.0, 1.0 };
    v.right = { 1.0, 0.0, 0.0 };
    v.up = { 0.0, -1.0, 0.0 };
    return v;
}

ParticleSystem::ParticleSystem(const SystemSettings& settings) : m_settings(settings), m_pool(new detail::WorkerPool()) {
    set_threads(settings.threads);
    m_live.reserve(64);
    m_chunks.reserve(256);
    m_families.reserve(64);
    m_updating.reserve(128);
    m_tiers.reserve(64);
}

ParticleSystem::~ParticleSystem() = default;

void ParticleSystem::set_settings(const SystemSettings& settings) {
    const int threads = settings.threads;
    m_settings = settings;
    set_threads(threads);
}

void ParticleSystem::set_threads(int threads) {
    if (threads <= 0) threads = static_cast<int>((std::max)(1u, std::thread::hardware_concurrency()));
    m_settings.threads = threads;
    m_pool->resize(threads);
}

int ParticleSystem::threads() const noexcept { return m_pool->size(); }

std::unique_ptr<Emitter> ParticleSystem::make_emitter(const EmitterSettings& settings, std::uint32_t seed) {
    std::unique_ptr<Emitter> e(new Emitter(settings, seed, Emitter::Deferred{}));
    e->set_origin(m_origin);
    e->set_colliders(&m_colliders);
    return e;
}

void ParticleSystem::number_family(Emitter& e, int depth) {
    e.m_id = ++m_next_id;
    e.m_order = m_next_order++;
    e.m_depth = depth;
    e.m_phase = e.m_order;
    for (std::size_t i = 0; i < e.child_count(); ++i) if (Emitter* c = e.child(i)) number_family(*c, depth + 1);
}

void ParticleSystem::adopt(Emitter& e) {
    number_family(e, 0);
    e.m_pending = 0.0f;
    e.m_offscreen_time = 0.0f;
    e.m_paused_offscreen = false;
    e.m_catch_up = 0.0f;
}

Emitter& ParticleSystem::create_emitter(const EmitterSettings& settings) {
    const std::uint32_t seed = settings.seed ? settings.seed : hash32(m_settings.seed, m_next_order + 1u);
    std::unique_ptr<Emitter> e = make_emitter(settings, seed);
    adopt(*e);
    e->m_play_on_step = settings.auto_play;
    m_live.push_back(std::move(e));
    return *m_live.back();
}

Emitter& ParticleSystem::create_emitter(const EmitterSettings& settings, const linalg::vector3d& position) {
    Emitter& e = create_emitter(settings);
    e.teleport(position);
    return e;
}

bool ParticleSystem::destroy(Emitter* emitter) {
    if (!emitter) return false;
    for (std::size_t i = 0; i < m_live.size(); ++i) {
        if (m_live[i].get() != emitter) continue;
        std::unique_ptr<Emitter> e = std::move(m_live[i]);
        m_live[i] = std::move(m_live.back());
        m_live.pop_back();
        if (e->m_effect >= 0 && static_cast<std::size_t>(e->m_effect) < m_effects.size()) {
            e->stop_and_clear();
            m_effects[static_cast<std::size_t>(e->m_effect)].free.push_back(std::move(e));
        }
        return true;
    }
    return false;
}

int ParticleSystem::register_effect(const EmitterSettings& settings, std::size_t preallocate) {
    Effect fx;
    fx.settings = settings;
    fx.settings.auto_play = false;
    fx.free.reserve(preallocate + 8);
    const int id = static_cast<int>(m_effects.size());
    for (std::size_t i = 0; i < preallocate; ++i) {
        std::unique_ptr<Emitter> e = make_emitter(fx.settings, hash32(m_settings.seed, 0x80000000u + static_cast<std::uint32_t>(i)));
        e->m_effect = id;
        e->m_auto_release = true;
        fx.free.push_back(std::move(e));
    }
    m_effects.push_back(std::move(fx));
    if (m_live.capacity() < m_live.size() + preallocate) m_live.reserve(m_live.size() + preallocate + 16);
    return id;
}

const EmitterSettings* ParticleSystem::effect_settings(int effect) const noexcept {
    return effect >= 0 && static_cast<std::size_t>(effect) < m_effects.size() ? &m_effects[static_cast<std::size_t>(effect)].settings : nullptr;
}

std::size_t ParticleSystem::effect_pool_size(int effect) const noexcept {
    return effect >= 0 && static_cast<std::size_t>(effect) < m_effects.size() ? m_effects[static_cast<std::size_t>(effect)].free.size() : 0;
}

Emitter* ParticleSystem::spawn_effect(int effect, const linalg::vector3d& position, const linalg::vector3d& forward, const linalg::vector3d& up, const linalg::vector3d& velocity) {
    if (effect < 0 || static_cast<std::size_t>(effect) >= m_effects.size()) return nullptr;
    Effect& fx = m_effects[static_cast<std::size_t>(effect)];
    std::unique_ptr<Emitter> e;
    const std::uint32_t seed = hash32(m_settings.seed ^ 0xA5A5A5A5u, ++m_spawn_counter);
    if (!fx.free.empty()) {
        e = std::move(fx.free.back());
        fx.free.pop_back();
        e->clear();
        e->set_origin(m_origin);
    } else {
        e = make_emitter(fx.settings, seed);
        e->m_effect = effect;
        e->m_auto_release = true;
        ++fx.made;
        if (fx.free.capacity() < fx.made) fx.free.reserve(fx.made + 8);
    }
    e->reseed(seed);
    e->set_colliders(&m_colliders);
    adopt(*e);
    e->teleport(position);
    e->set_orientation(forward, up);
    if (velocity.x != 0.0 || velocity.y != 0.0 || velocity.z != 0.0) e->set_velocity(velocity); else e->clear_velocity();
    e->play();
    m_live.push_back(std::move(e));
    return m_live.back().get();
}

Emitter* ParticleSystem::spawn_effect_2d(int effect, double x, double y, double angle_degrees) {
    Emitter* e = spawn_effect(effect, { x, y, 0.0 });
    if (e) {
        e->set_angle(angle_degrees);
    }
    return e;
}

void ParticleSystem::clear() noexcept {
    for (auto& e : m_live) e->clear();
}

std::size_t ParticleSystem::particle_count() const noexcept {
    std::size_t n = 0;
    for (const auto& e : m_live) n += e->total_count();
    return n;
}

void ParticleSystem::set_origin(const linalg::vector3d& origin) {
    m_origin = origin;
    for (auto& e : m_live) e->set_origin(origin);
    for (Effect& fx : m_effects) for (auto& e : fx.free) e->set_origin(origin);
    rebuild_colliders();
}

void ParticleSystem::add_plane(const linalg::vector3d& normal, const linalg::vector3d& point, std::uint32_t layers) {
    const double l = std::sqrt(normal.x * normal.x + normal.y * normal.y + normal.z * normal.z);
    const linalg::vector3d n = l > 0.0 ? linalg::vector3d{ normal.x / l, normal.y / l, normal.z / l } : linalg::vector3d{ 0.0, 1.0, 0.0 };
    m_plane_normals.push_back(n);
    m_plane_offsets.push_back(-(n.x * point.x + n.y * point.y + n.z * point.z));
    m_plane_layers.push_back(layers);
    rebuild_colliders();
}

void ParticleSystem::add_sphere(const linalg::vector3d& center, double radius, std::uint32_t layers) {
    m_sphere_centers.push_back(center);
    m_sphere_radii.push_back(radius);
    m_sphere_layers.push_back(layers);
    rebuild_colliders();
}

void ParticleSystem::add_box(const linalg::vector3d& lo, const linalg::vector3d& hi, std::uint32_t layers) {
    m_box_lo.push_back({ (std::min)(lo.x, hi.x), (std::min)(lo.y, hi.y), (std::min)(lo.z, hi.z) });
    m_box_hi.push_back({ (std::max)(lo.x, hi.x), (std::max)(lo.y, hi.y), (std::max)(lo.z, hi.z) });
    m_box_layers.push_back(layers);
    rebuild_colliders();
}

void ParticleSystem::clear_colliders() noexcept {
    m_plane_normals.clear(); m_plane_offsets.clear(); m_plane_layers.clear();
    m_sphere_centers.clear(); m_sphere_radii.clear(); m_sphere_layers.clear();
    m_box_lo.clear(); m_box_hi.clear(); m_box_layers.clear();
    m_colliders.clear();
}

void ParticleSystem::rebuild_colliders() {
    m_colliders.clear();
    const linalg::vector3d& o = m_origin;
    for (std::size_t i = 0; i < m_plane_normals.size(); ++i) {
        const linalg::vector3d& n = m_plane_normals[i];
        PlaneCollider p;
        p.normal = { static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z) };
        p.offset = static_cast<float>(m_plane_offsets[i] + n.x * o.x + n.y * o.y + n.z * o.z);
        p.layers = m_plane_layers[i];
        m_colliders.planes.push_back(p);
    }
    for (std::size_t i = 0; i < m_sphere_centers.size(); ++i) {
        const linalg::vector3d& c = m_sphere_centers[i];
        SphereCollider s;
        s.center = { static_cast<float>(c.x - o.x), static_cast<float>(c.y - o.y), static_cast<float>(c.z - o.z) };
        s.radius = static_cast<float>(m_sphere_radii[i]);
        s.layers = m_sphere_layers[i];
        m_colliders.spheres.push_back(s);
    }
    for (std::size_t i = 0; i < m_box_lo.size(); ++i) {
        BoxCollider b;
        b.lo = { static_cast<float>(m_box_lo[i].x - o.x), static_cast<float>(m_box_lo[i].y - o.y), static_cast<float>(m_box_lo[i].z - o.z) };
        b.hi = { static_cast<float>(m_box_hi[i].x - o.x), static_cast<float>(m_box_hi[i].y - o.y), static_cast<float>(m_box_hi[i].z - o.z) };
        b.layers = m_box_layers[i];
        m_colliders.boxes.push_back(b);
    }
}

void ParticleSystem::evaluate_view(const CameraView& view) {
    m_view = view;
    const SystemSettings& cfg = m_settings;
    m_stats.offscreen = m_stats.paused = m_stats.distance_culled = m_stats.sleeping = 0;

    for (auto& up : m_live) {
        Emitter& r = *up;
        if (r.m_play_on_step) { r.m_play_on_step = false; r.play(); }
        if (r.sleeping()) { ++m_stats.sleeping; continue; }

        int level = 0;
        bool visible = true;
        bool far_culled = false;
        if (view.active) {
            const WorldBounds b = r.potential_bounds();
            if (view.is_2d) {
                const double m = cfg.cull_margin;
                visible = b.hi.x >= view.rect_x0 - m && b.lo.x <= view.rect_x1 + m && b.hi.y >= view.rect_y0 - m && b.lo.y <= view.rect_y1 + m;
            } else {
                const linalg::vector3d c{ 0.5 * (b.lo.x + b.hi.x), 0.5 * (b.lo.y + b.hi.y), 0.5 * (b.lo.z + b.hi.z) };
                const double hx = 0.5 * (b.hi.x - b.lo.x), hy = 0.5 * (b.hi.y - b.lo.y), hz = 0.5 * (b.hi.z - b.lo.z);
                const double radius = std::sqrt(hx * hx + hy * hy + hz * hz);
                const double dx = c.x - view.position.x, dy = c.y - view.position.y, dz = c.z - view.position.z;
                const double bias = r.m_settings.lod_bias > 1e-3f ? r.m_settings.lod_bias : 1e-3f;
                const double d = (std::max)(0.0, std::sqrt(dx * dx + dy * dy + dz * dz) - radius) / bias;
                level = 3;
                for (int k = 0; k < 4; ++k) if (d <= cfg.lod[k].distance) { level = k; break; }
                far_culled = d > cfg.cull_distance;
                if (view.has_frustum) {
                    const double m = cfg.cull_margin;
                    visible = view.frustum.intersects_aabb({ b.lo.x - m, b.lo.y - m, b.lo.z - m }, { b.hi.x + m, b.hi.y + m, b.hi.z + m });
                }
            }
        }

        const LodLevel& lod = cfg.lod[level];
        float scale = lod.spawn_scale;
        int interval = lod.update_interval < 1 ? 1 : lod.update_interval;
        bool pause = false;
        const bool shown = visible && !far_culled;
        if (!shown) {
            ++m_stats.offscreen;
            if (far_culled) ++m_stats.distance_culled;
            OffscreenMode mode = r.m_settings.offscreen;
            if (mode == OffscreenMode::Auto) mode = (r.m_settings.loop && r.m_playing) ? OffscreenMode::Pause : OffscreenMode::Throttle;
            if (mode == OffscreenMode::Pause) pause = true;
            else if (mode == OffscreenMode::Throttle) {
                interval = (std::max)(interval, cfg.offscreen_interval < 1 ? 1 : cfg.offscreen_interval);
                scale = (std::min)(scale, cfg.lod[3].spawn_scale);
            }
        }
        if (pause) ++m_stats.paused;

        if (r.m_paused_offscreen && !pause) {
            r.m_catch_up = r.m_offscreen_time;
            r.m_offscreen_time = 0.0f;
        }
        r.m_paused_offscreen = pause;
        r.m_interval = interval;

        struct Apply {
            int level; float scale; bool shown; bool simple;
            void operator()(Emitter& e) const { e.set_lod(level, scale, shown, simple); }
        } apply{ level, scale, shown, lod.simple_render };
        for_family(r, apply);
    }
    apply_budget();
}

void ParticleSystem::apply_budget() {
    const SystemSettings& cfg = m_settings;
    m_tiers.clear();
    for (auto& up : m_live) {
        Emitter& r = *up;
        if (r.sleeping() || r.m_paused_offscreen) continue;
        float expected = 0.0f;
        struct Sum {
            float* out;
            void operator()(Emitter& e) const {
                const float steady = (e.m_playing ? e.m_settings.steady_count() : 0.0f) * e.m_spawn_scale;
                *out += steady > static_cast<float>(e.m_count) ? steady : static_cast<float>(e.m_count);
            }
        } sum{ &expected };
        for_family(r, sum);
        m_tiers.push_back({ &r, expected });
    }
    std::sort(m_tiers.begin(), m_tiers.end(), [](const TierEntry& a, const TierEntry& b) {
        if (a.root->priority() != b.root->priority()) return a.root->priority() > b.root->priority();
        return a.root->m_order < b.root->m_order;
    });

    float remaining = static_cast<float>(cfg.budget) * m_quality * cfg.soft_budget;
    float worst = 1.0f;
    std::size_t i = 0;
    while (i < m_tiers.size()) {
        std::size_t j = i;
        float need = 0.0f;
        const int pr = m_tiers[i].root->priority();
        while (j < m_tiers.size() && m_tiers[j].root->priority() == pr) { need += m_tiers[j].expected; ++j; }
        float s = 1.0f;
        if (need > remaining) s = need > 0.0f ? remaining / need : 0.0f;
        remaining = need > remaining ? 0.0f : remaining - need;
        if (s < cfg.min_spawn_scale) s = cfg.min_spawn_scale;
        if (s < worst) worst = s;
        if (s < 1.0f) {
            struct Scale {
                float s;
                void operator()(Emitter& e) const { e.m_spawn_scale *= s; }
            } scale{ s };
            for (std::size_t k = i; k < j; ++k) for_family(*m_tiers[k].root, scale);
        }
        i = j;
    }
    m_stats.budget_scale = worst;
}

void ParticleSystem::assign_quotas() {
    std::sort(m_updating.begin(), m_updating.end(), [](const Emitter* a, const Emitter* b) {
        if (a->priority() != b->priority()) return a->priority() > b->priority();
        if (a->m_depth != b->m_depth) return a->m_depth < b->m_depth;
        return a->m_order < b->m_order;
    });
    const std::size_t live = particle_count();
    std::size_t headroom = m_settings.budget > live ? m_settings.budget - live : 0;
    for (Emitter* e : m_updating) {
        const float dt = e->m_step.dt;
        std::size_t want = e->want_this_step(dt);
        if (e->m_is_child) {
            const std::size_t guess = (static_cast<std::size_t>(e->m_inbox.size()) / 4 + 32) * static_cast<std::size_t>(e->m_event_count < 1 ? 1 : e->m_event_count);
            if (want > guess) want = guess;
        }
        if (e->m_catch_up > 0.0f) want = e->m_capacity - e->m_count;
        const std::size_t q = want < headroom ? want : headroom;
        e->m_quota = q;
        headroom -= q;
    }
}

void ParticleSystem::integrate_thunk(void* self, std::size_t i) {
    ParticleSystem& s = *static_cast<ParticleSystem*>(self);
    const ChunkTask& t = s.m_chunks[i];
    t.emitter->integrate(t.begin, t.end, *t.box);
}

void ParticleSystem::finalize_thunk(void* self, std::size_t i) {
    ParticleSystem& s = *static_cast<ParticleSystem*>(self);
    const FamilyTask& f = s.m_families[i];
    if (f.catch_up > 0.0f) {
        const float seconds = (f.root->m_settings.loop ? f.catch_up : (std::min)(f.catch_up, s.m_settings.max_catch_up)) + f.dt;
        f.root->catch_up(seconds, s.m_settings.catch_up_steps, f.root->m_quota);
        f.root->m_catch_up = 0.0f;
    } else {
        f.root->finalize_family(0);
    }
}

void ParticleSystem::tick(float dt) {
    ++m_tick;
    m_chunks.clear();
    m_families.clear();
    m_updating.clear();

    for (auto& up : m_live) {
        Emitter& r = *up;
        if (r.sleeping()) continue;
        if (r.m_paused_offscreen) { r.m_offscreen_time += dt; continue; }
        r.m_pending += dt;
        const int interval = r.m_interval < 1 ? 1 : r.m_interval;
        if (r.m_catch_up <= 0.0f && (m_tick + r.m_phase) % static_cast<std::uint64_t>(interval) != 0) continue;
        const float h = r.m_pending;
        r.m_pending = 0.0f;

        FamilyTask task;
        task.root = &r;
        task.dt = h;
        task.catch_up = r.m_catch_up;
        m_families.push_back(task);

        struct Prepare {
            ParticleSystem* sys;
            float h;
            bool catching;
            void operator()(Emitter& e) const {
                e.prepare_step(h);
                sys->m_updating.push_back(&e);
                if (catching) return;
                const std::size_t n = e.m_count;
                for (std::size_t b = 0, c = 0; b < n; b += Emitter::kChunk, ++c) {
                    ChunkTask t;
                    t.emitter = &e;
                    t.begin = b;
                    t.end = (std::min)(n, b + Emitter::kChunk);
                    t.box = &e.m_chunk_bounds[c];
                    sys->m_chunks.push_back(t);
                }
            }
        } prep{ this, h, task.catch_up > 0.0f };
        for_family(r, prep);
    }

    assign_quotas();
    m_pool->run(m_chunks.size(), &ParticleSystem::integrate_thunk, this);
    m_pool->run(m_families.size(), &ParticleSystem::finalize_thunk, this);

    for (Emitter* e : m_updating) {
        m_stats.spawned += e->m_last_spawned;
        m_stats.limited += e->m_last_limited;
    }
    m_stats.updated += m_families.size();
}

void ParticleSystem::release_finished() {
    std::size_t i = 0;
    while (i < m_live.size()) {
        Emitter& e = *m_live[i];
        if (!e.m_auto_release || !e.finished()) { ++i; continue; }
        std::unique_ptr<Emitter> done = std::move(m_live[i]);
        m_live[i] = std::move(m_live.back());
        m_live.pop_back();
        if (done->m_effect >= 0 && static_cast<std::size_t>(done->m_effect) < m_effects.size()) {
            m_effects[static_cast<std::size_t>(done->m_effect)].free.push_back(std::move(done));
        }
    }
}

void ParticleSystem::step(double dt, const CameraView& view) {
    const auto start = std::chrono::steady_clock::now();
    if (!(dt > 0.0)) dt = 0.0;
    if (dt > m_settings.max_frame_dt) dt = m_settings.max_frame_dt;

    int steps = 1;
    double h = dt;
    if (m_settings.fixed_step > 0.0) {
        m_accumulator += dt;
        steps = static_cast<int>(m_accumulator / m_settings.fixed_step);
        const int cap = m_settings.max_substeps < 1 ? 1 : m_settings.max_substeps;
        if (steps > cap) {
            steps = cap;
            m_accumulator = 0.0;
        } else {
            m_accumulator -= steps * m_settings.fixed_step;
        }
        h = m_settings.fixed_step;
    } else if (dt <= 0.0) {
        steps = 0;
    }

    m_stats.spawned = 0;
    m_stats.limited = 0;
    m_stats.updated = 0;
    m_stats.substeps = steps;

    if (steps > 0) {
        evaluate_view(view);
        for (int s = 0; s < steps; ++s) tick(static_cast<float>(h));
        release_finished();
    } else {
        m_view = view;
    }

    m_render_ahead = (m_settings.fixed_step > 0.0 && m_settings.extrapolate) ? m_accumulator : 0.0;
    for (auto& up : m_live) {
        struct Lag {
            float lag;
            void operator()(Emitter& e) const { e.m_render_lag = lag; }
        } lag{ m_settings.extrapolate ? static_cast<float>(m_render_ahead) + up->m_pending : 0.0f };
        for_family(*up, lag);
    }

    std::size_t active = 0;
    for (auto& up : m_live) if (!up->sleeping()) ++active;
    m_stats.emitters = m_live.size();
    m_stats.active = active;
    m_stats.particles = particle_count();
    m_stats.budget = m_settings.budget;
    m_stats.ticks = m_tick;

    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
    m_stats.step_ms = ms;
    if (m_settings.adaptive && steps > 0) {
        const double per = ms / steps;
        m_ema_ms = m_ema_ms <= 0.0 ? per : m_ema_ms * 0.8 + per * 0.2;
        if (m_ema_ms > m_settings.target_ms) m_quality = (std::max)(m_settings.min_quality, m_quality * 0.92f);
        else if (m_ema_ms < 0.6 * m_settings.target_ms) m_quality = (std::min)(1.0f, m_quality * 1.03f);
    }
    m_stats.quality = m_quality;
}

} // namespace particles
} // namespace fizmo
