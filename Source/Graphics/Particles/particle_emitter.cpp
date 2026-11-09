#include "fizmo_library.hpp"
#include "particle_emitter.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>

namespace fizmo {
namespace particles {

namespace {

constexpr float kPi = 3.14159265358979f;
constexpr float kTwoPi = 6.28318530717959f;
constexpr float kDegToRad = 0.0174532925199433f;
constexpr std::size_t kUnlimited = std::numeric_limits<std::size_t>::max();

inline float lo_of(float a, float b) noexcept { return a < b ? a : b; }
inline float hi_of(float a, float b) noexcept { return a > b ? a : b; }
inline float clamp01(float t) noexcept { return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t); }

inline Float3 add(const Float3& a, const Float3& b) noexcept { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
inline Float3 sub(const Float3& a, const Float3& b) noexcept { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
inline Float3 mul(const Float3& a, float s) noexcept { return { a.x * s, a.y * s, a.z * s }; }
inline float dot(const Float3& a, const Float3& b) noexcept { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Float3 cross(const Float3& a, const Float3& b) noexcept { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
inline float length(const Float3& a) noexcept { return std::sqrt(dot(a, a)); }
inline Float3 normalized(const Float3& a, const Float3& fallback) noexcept {
    const float l = length(a);
    return l > 1e-12f ? mul(a, 1.0f / l) : fallback;
}

std::uint32_t mix32(std::uint32_t a, std::uint32_t b) noexcept {
    std::uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u + (a << 6) + (a >> 2));
    h ^= h >> 16; h *= 0x85EBCA6Bu; h ^= h >> 13; h *= 0xC2B2AE35u; h ^= h >> 16;
    return h;
}

struct NoiseTable {
    static constexpr int kN = 16;
    float v[kN * kN * kN * 3];

    NoiseTable() noexcept {
        static float psi[kN * kN * kN * 3];
        std::uint32_t s = 0x2545F491u;
        for (float& p : psi) {
            s ^= s << 13; s ^= s >> 17; s ^= s << 5;
            p = static_cast<float>(s & 0xFFFFFF) / 8388608.0f - 1.0f;
        }
        auto at = [](int x, int y, int z, int c) { return psi[((((z & 15) * kN + (y & 15)) * kN + (x & 15)) * 3) + c]; };
        double sum = 0.0;
        for (int z = 0; z < kN; ++z) for (int y = 0; y < kN; ++y) for (int x = 0; x < kN; ++x) {
            const float dpz_dy = 0.5f * (at(x, y + 1, z, 2) - at(x, y - 1, z, 2));
            const float dpy_dz = 0.5f * (at(x, y, z + 1, 1) - at(x, y, z - 1, 1));
            const float dpx_dz = 0.5f * (at(x, y, z + 1, 0) - at(x, y, z - 1, 0));
            const float dpz_dx = 0.5f * (at(x + 1, y, z, 2) - at(x - 1, y, z, 2));
            const float dpy_dx = 0.5f * (at(x + 1, y, z, 1) - at(x - 1, y, z, 1));
            const float dpx_dy = 0.5f * (at(x, y + 1, z, 0) - at(x, y - 1, z, 0));
            float* o = v + ((z * kN + y) * kN + x) * 3;
            o[0] = dpz_dy - dpy_dz;
            o[1] = dpx_dz - dpz_dx;
            o[2] = dpy_dx - dpx_dy;
            sum += o[0] * o[0] + o[1] * o[1] + o[2] * o[2];
        }
        const float scale = sum > 0.0 ? static_cast<float>(1.0 / std::sqrt(sum / (kN * kN * kN))) : 1.0f;
        for (float& f : v) f *= scale;
    }
};

const NoiseTable& noise_table() {
    static const NoiseTable table;
    return table;
}

inline void sample_noise(const float* table, float x, float y, float z, float& ox, float& oy, float& oz) noexcept {
    x += 65536.0f; y += 65536.0f; z += 65536.0f;
    const int bx = static_cast<int>(x), by = static_cast<int>(y), bz = static_cast<int>(z);
    const float tx = x - static_cast<float>(bx), ty = y - static_cast<float>(by), tz = z - static_cast<float>(bz);
    const int ix = bx & 15, iy = by & 15, iz = bz & 15;
    const int jx = (ix + 1) & 15, jy = (iy + 1) & 15, jz = (iz + 1) & 15;
    const float* c000 = table + ((iz * 16 + iy) * 16 + ix) * 3;
    const float* c100 = table + ((iz * 16 + iy) * 16 + jx) * 3;
    const float* c010 = table + ((iz * 16 + jy) * 16 + ix) * 3;
    const float* c110 = table + ((iz * 16 + jy) * 16 + jx) * 3;
    const float* c001 = table + ((jz * 16 + iy) * 16 + ix) * 3;
    const float* c101 = table + ((jz * 16 + iy) * 16 + jx) * 3;
    const float* c011 = table + ((jz * 16 + jy) * 16 + ix) * 3;
    const float* c111 = table + ((jz * 16 + jy) * 16 + jx) * 3;
    float r[3];
    for (int c = 0; c < 3; ++c) {
        const float a = c000[c] + (c100[c] - c000[c]) * tx;
        const float b = c010[c] + (c110[c] - c010[c]) * tx;
        const float d = c001[c] + (c101[c] - c001[c]) * tx;
        const float e = c011[c] + (c111[c] - c011[c]) * tx;
        const float f = a + (b - a) * ty;
        const float g = d + (e - d) * ty;
        r[c] = f + (g - f) * tz;
    }
    ox = r[0]; oy = r[1]; oz = r[2];
}

constexpr int kArrays = 12;

} // namespace

void Box3F::add(const Float3& p) noexcept {
    if (!valid) { lo = hi = p; valid = true; return; }
    lo.x = lo_of(lo.x, p.x); lo.y = lo_of(lo.y, p.y); lo.z = lo_of(lo.z, p.z);
    hi.x = hi_of(hi.x, p.x); hi.y = hi_of(hi.y, p.y); hi.z = hi_of(hi.z, p.z);
}

void Box3F::add(const Box3F& b) noexcept {
    if (!b.valid) return;
    add(b.lo);
    add(b.hi);
}

void Emitter::Rng::seed(std::uint64_t s, std::uint64_t seq) noexcept {
    state = 0u;
    inc = (seq << 1u) | 1u;
    next();
    state += s;
    next();
}

std::uint32_t Emitter::Rng::next() noexcept {
    const std::uint64_t old = state;
    state = old * 6364136223846793005ull + inc;
    const std::uint32_t xorshifted = static_cast<std::uint32_t>(((old >> 18u) ^ old) >> 27u);
    const std::uint32_t rot = static_cast<std::uint32_t>(old >> 59u);
    return (xorshifted >> rot) | (xorshifted << ((32u - rot) & 31u));
}

Emitter::Emitter(const EmitterSettings& settings, std::uint32_t seed) : m_curves(new BakedCurves()) {
    reseed(seed ? seed : (settings.seed ? settings.seed : 0x1234567u));
    set_settings(settings);
    if (m_settings.auto_play && !m_is_child) play();
}

Emitter::Emitter(const EmitterSettings& settings, std::uint32_t seed, Deferred) : m_curves(new BakedCurves()) {
    reseed(seed ? seed : (settings.seed ? settings.seed : 0x1234567u));
    set_settings(settings);
}

Emitter::~Emitter() = default;

void Emitter::set_colliders(const ColliderSet* colliders) noexcept {
    m_colliders = colliders;
    for (SubLink& s : m_subs) if (s.child) s.child->set_colliders(colliders);
}

void Emitter::reseed(std::uint32_t seed) noexcept {
    m_seed = seed;
    m_rng.seed(seed, mix32(seed, 0xB5297A4Du));
    for (std::size_t i = 0; i < m_subs.size(); ++i) if (m_subs[i].child) m_subs[i].child->reseed(mix32(seed, static_cast<std::uint32_t>(i + 1)));
}

void Emitter::set_settings(const EmitterSettings& settings) {
    m_settings = settings;
    if (m_is_child) {
        m_settings.space = SimulationSpace::World;
        m_settings.loop = false;
        m_settings.prewarm = false;
    }
    if (m_settings.max_particles < 1) m_settings.max_particles = 1;
    if (m_settings.dimension == ParticleDim::D2) {
        m_settings.direction.z = 0.0f;
        m_settings.gravity.z = 0.0f;
        m_settings.wind.z = 0.0f;
        m_settings.shape_size.z = 0.0f;
    }
    allocate(m_settings.max_particles);
    bake();
    build_children();
    reset_timeline();
}

void Emitter::allocate(std::size_t capacity) {
    if (capacity == m_capacity && m_block) return;
    const std::size_t stride = (capacity + 15) & ~static_cast<std::size_t>(15);
    std::unique_ptr<float[]> block(new float[stride * kArrays]);
    std::unique_ptr<std::uint8_t[]> hit(new std::uint8_t[stride]);
    std::memset(hit.get(), 0, stride);
    float* base[kArrays];
    for (int a = 0; a < kArrays; ++a) base[a] = block.get() + stride * static_cast<std::size_t>(a);
    const std::size_t keep = (std::min)(m_count, capacity);
    if (m_block && keep) {
        float* old[kArrays] = { m_px, m_py, m_pz, m_vx, m_vy, m_vz, m_age, m_inv_life, m_size, m_rot, m_spin, m_rnd };
        for (int a = 0; a < kArrays; ++a) std::memcpy(base[a], old[a], keep * sizeof(float));
    }
    m_block = std::move(block);
    m_hit = std::move(hit);
    m_px = base[0]; m_py = base[1]; m_pz = base[2];
    m_vx = base[3]; m_vy = base[4]; m_vz = base[5];
    m_age = base[6]; m_inv_life = base[7]; m_size = base[8];
    m_rot = base[9]; m_spin = base[10]; m_rnd = base[11];
    m_capacity = capacity;
    m_count = keep;
    m_chunk_bounds.assign((capacity + kChunk - 1) / kChunk + 1, Box3F{});
}

void Emitter::bake() {
    const EmitterSettings& s = m_settings;
    BakedCurves& c = *m_curves;
    for (int i = 0; i < BakedCurves::kSize; ++i) {
        const float t = static_cast<float>(i) / static_cast<float>(BakedCurves::kSize - 1);
        c.size[i] = s.size_over_life.evaluate(t);
        c.alpha[i] = s.alpha_over_life.evaluate(t);
        c.speed[i] = s.speed_over_life.evaluate(t);
        c.spin[i] = s.spin_over_life.evaluate(t);
        c.color[i] = s.color_over_life.evaluate(t);
    }

    c.tint_levels = s.color == s.color_alt ? 1 : BakedCurves::kTints;
    for (int l = 0; l < c.tint_levels; ++l) {
        const float r = c.tint_levels > 1 ? static_cast<float>(l) / static_cast<float>(c.tint_levels - 1) : 0.0f;
        for (int i = 0; i < BakedCurves::kSize; ++i) {
            const float t = static_cast<float>(i) / static_cast<float>(BakedCurves::kSize - 1);
            ColorF col = particle_color(t, r);
            if (s.blend == ParticleBlend::Premultiplied) { col.r *= col.a; col.g *= col.a; col.b *= col.a; }
            auto byte = [](float v) { return static_cast<std::uint32_t>((v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v)) * 255.0f + 0.5f); };
            c.rgba[l * BakedCurves::kSize + i] = byte(col.r) | (byte(col.g) << 8) | (byte(col.b) << 16) | (byte(col.a) << 24);
        }
    }

    const bool d2 = s.dimension == ParticleDim::D2;
    m_flags.gravity = s.gravity_scale != 0.0f && (s.gravity.x != 0.0f || s.gravity.y != 0.0f || s.gravity.z != 0.0f);
    m_flags.drag = s.drag > 0.0f;
    m_flags.turbulence = s.turbulence != 0.0f;
    m_flags.speed_curve = !s.speed_over_life.is_constant() || s.speed_over_life.keys[0].v != 1.0f;
    m_flags.spin = s.spin.lo != 0.0f || s.spin.hi != 0.0f;
    m_flags.rotation = m_flags.spin || s.rotation.lo != 0.0f || s.rotation.hi != 0.0f;
    m_flags.collide = s.collide && s.space == SimulationSpace::World;
    m_flags.death_events = m_flags.collision_events = m_flags.birth_events = false;
    for (const SubEmitter& sub : s.sub_emitters) {
        if (sub.trigger == SubEmitterTrigger::Death) m_flags.death_events = true;
        if (sub.trigger == SubEmitterTrigger::Collision) m_flags.collision_events = true;
        if (sub.trigger == SubEmitterTrigger::Birth) m_flags.birth_events = true;
    }
    m_flags.flipbook = s.frames() > 1;
    m_flags.instanced_ok = s.render_mode == RenderMode::Billboard && !m_flags.rotation && !m_flags.flipbook && s.pivot_x == 0.0f && s.pivot_y == 0.0f;

    m_dir = normalized(s.direction, d2 ? Float3{ 0.0f, -1.0f, 0.0f } : Float3{ 0.0f, 1.0f, 0.0f });
    if (d2) {
        m_dir_t1 = { -m_dir.y, m_dir.x, 0.0f };
        m_dir_t2 = { 0.0f, 0.0f, 0.0f };
    } else {
        const Float3 pick = std::fabs(m_dir.y) < 0.9f ? Float3{ 0.0f, 1.0f, 0.0f } : Float3{ 1.0f, 0.0f, 0.0f };
        m_dir_t1 = normalized(cross(pick, m_dir), Float3{ 1.0f, 0.0f, 0.0f });
        m_dir_t2 = cross(m_dir, m_dir_t1);
    }
    m_burst_cycles.assign(s.bursts.size(), 0);
}

void Emitter::build_children() {
    m_subs.clear();
    if (m_depth >= 4) return;
    m_subs.reserve(m_settings.sub_emitters.size());
    for (std::size_t i = 0; i < m_settings.sub_emitters.size(); ++i) {
        const SubEmitter& sub = m_settings.sub_emitters[i];
        SubLink link;
        link.trigger = sub.trigger;
        link.count = sub.count;
        link.probability = sub.probability;
        link.inherit = sub.inherit_velocity;
        EmitterSettings cs = sub.settings;
        cs.auto_play = false;
        cs.space = SimulationSpace::World;
        cs.dimension = m_settings.dimension;
        link.child.reset(new Emitter(cs, mix32(m_seed, static_cast<std::uint32_t>(i + 1)), Deferred{}));
        Emitter& c = *link.child;
        c.m_is_child = true;
        c.m_depth = m_depth + 1;
        c.m_origin = m_origin;
        c.m_colliders = m_colliders;
        c.m_event_count = sub.count < 0 ? 0 : sub.count;
        c.m_event_probability = sub.probability;
        c.set_settings(cs);
        c.m_playing = false;
        c.m_inbox.assign((std::max)(static_cast<std::size_t>(16), static_cast<std::size_t>(cs.max_particles)), SpawnEvent{});
        c.m_inbox_count = 0;
        m_subs.push_back(std::move(link));
    }
}

void Emitter::reset_timeline() noexcept {
    m_time = 0.0f;
    m_delay_left = m_settings.start_delay;
    m_rate_carry = 0.0f;
    m_distance_carry = 0.0f;
    std::fill(m_burst_cycles.begin(), m_burst_cycles.end(), 0);
}

void Emitter::set_position(double x, double y, double z) noexcept {
    m_pos = { static_cast<float>(x - m_origin.x), static_cast<float>(y - m_origin.y), static_cast<float>(z - m_origin.z) };
    if (m_settings.dimension == ParticleDim::D2) m_pos.z = 0.0f;
    if (!m_has_prev) { m_prev_pos = m_pos; m_has_prev = true; }
}

void Emitter::teleport(const linalg::vector3d& p) noexcept {
    m_has_prev = false;
    set_position(p);
}

linalg::vector3d Emitter::position() const noexcept {
    return { m_origin.x + m_pos.x, m_origin.y + m_pos.y, m_origin.z + m_pos.z };
}

void Emitter::set_basis(const linalg::vector3d& r, const linalg::vector3d& u, const linalg::vector3d& f) noexcept {
    m_basis[0] = { static_cast<float>(r.x), static_cast<float>(r.y), static_cast<float>(r.z) };
    m_basis[1] = { static_cast<float>(u.x), static_cast<float>(u.y), static_cast<float>(u.z) };
    m_basis[2] = { static_cast<float>(f.x), static_cast<float>(f.y), static_cast<float>(f.z) };
}

void Emitter::set_orientation(const linalg::vector3d& forward, const linalg::vector3d& up_hint) noexcept {
    const Float3 f0{ static_cast<float>(forward.x), static_cast<float>(forward.y), static_cast<float>(forward.z) };
    const Float3 u0{ static_cast<float>(up_hint.x), static_cast<float>(up_hint.y), static_cast<float>(up_hint.z) };
    Float3 u = normalized(u0, Float3{ 0.0f, 1.0f, 0.0f });
    Float3 f = normalized(f0, Float3{ 0.0f, 0.0f, 1.0f });
    Float3 r = cross(u, f);
    if (length(r) < 1e-6f) r = cross(std::fabs(f.y) < 0.9f ? Float3{ 0.0f, 1.0f, 0.0f } : Float3{ 1.0f, 0.0f, 0.0f }, f);
    r = normalized(r, Float3{ 1.0f, 0.0f, 0.0f });
    u = cross(f, r);
    m_basis[0] = r;
    m_basis[1] = u;
    m_basis[2] = f;
}

void Emitter::set_angle(double degrees) noexcept {
    const float a = static_cast<float>(degrees) * kDegToRad;
    const float c = std::cos(a), s = std::sin(a);
    m_basis[0] = { c, s, 0.0f };
    m_basis[1] = { -s, c, 0.0f };
    m_basis[2] = { 0.0f, 0.0f, 1.0f };
}

void Emitter::basis_world(float out[9]) const noexcept {
    for (int i = 0; i < 3; ++i) { out[i * 3] = m_basis[i].x; out[i * 3 + 1] = m_basis[i].y; out[i * 3 + 2] = m_basis[i].z; }
}

void Emitter::set_velocity(const linalg::vector3d& v) noexcept {
    m_velocity = { static_cast<float>(v.x), static_cast<float>(v.y), static_cast<float>(v.z) };
    m_velocity_set = true;
}

void Emitter::play() noexcept {
    if (m_is_child) return;
    const bool was = m_playing;
    m_playing = true;
    m_paused = false;
    reset_timeline();
    if (!was && m_settings.prewarm && m_settings.loop && m_count == 0) warm_up(m_settings.max_lifetime());
}

void Emitter::clear() noexcept {
    m_count = 0;
    m_bounds.reset();
    m_inbox_count = 0;
    for (SubLink& s : m_subs) if (s.child) s.child->clear();
}

bool Emitter::finished() const noexcept {
    if (m_playing || m_count > 0 || m_inbox_count > 0) return false;
    for (const SubLink& s : m_subs) if (s.child && !s.child->finished()) return false;
    return true;
}

std::size_t Emitter::total_count() const noexcept {
    std::size_t n = m_count;
    for (const SubLink& s : m_subs) if (s.child) n += s.child->total_count();
    return n;
}

void Emitter::set_origin(const linalg::vector3d& origin) {
    const Float3 delta{ static_cast<float>(m_origin.x - origin.x), static_cast<float>(m_origin.y - origin.y), static_cast<float>(m_origin.z - origin.z) };
    m_origin = origin;
    m_pos = add(m_pos, delta);
    m_prev_pos = add(m_prev_pos, delta);
    if (m_settings.space == SimulationSpace::World) {
        for (std::size_t i = 0; i < m_count; ++i) { m_px[i] += delta.x; m_py[i] += delta.y; m_pz[i] += delta.z; }
        if (m_bounds.valid) { m_bounds.lo = add(m_bounds.lo, delta); m_bounds.hi = add(m_bounds.hi, delta); }
    }
    for (std::size_t i = 0; i < m_inbox_count; ++i) { m_inbox[i].px += delta.x; m_inbox[i].py += delta.y; m_inbox[i].pz += delta.z; }
    for (SubLink& s : m_subs) if (s.child) s.child->set_origin(origin);
}

void Emitter::set_emission_mesh(const float* xyz, std::size_t triangle_count) {
    m_mesh.clear();
    m_mesh_cdf.clear();
    if (!xyz || triangle_count == 0) return;
    m_mesh.reserve(triangle_count * 3);
    m_mesh_cdf.reserve(triangle_count);
    float total = 0.0f;
    for (std::size_t t = 0; t < triangle_count; ++t) {
        const Float3 a{ xyz[t * 9 + 0], xyz[t * 9 + 1], xyz[t * 9 + 2] };
        const Float3 b{ xyz[t * 9 + 3], xyz[t * 9 + 4], xyz[t * 9 + 5] };
        const Float3 c{ xyz[t * 9 + 6], xyz[t * 9 + 7], xyz[t * 9 + 8] };
        m_mesh.push_back(a); m_mesh.push_back(b); m_mesh.push_back(c);
        total += 0.5f * length(cross(sub(b, a), sub(c, a)));
        m_mesh_cdf.push_back(total);
    }
}

Float3 Emitter::to_world_dir(const Float3& v) const noexcept {
    return add(add(mul(m_basis[0], v.x), mul(m_basis[1], v.y)), mul(m_basis[2], v.z));
}

void Emitter::sample_shape(Float3& pos, Float3& dir) {
    const EmitterSettings& s = m_settings;
    const bool d2 = s.dimension == ParticleDim::D2;
    const float arc = (s.shape_arc <= 0.0f ? 360.0f : (s.shape_arc > 360.0f ? 360.0f : s.shape_arc)) * kDegToRad;
    const float r = s.shape_radius;
    const float th = clamp01(s.shape_thickness);
    pos = { 0.0f, 0.0f, 0.0f };
    Float3 normal = m_dir;
    bool cone = false;
    float cone_frac = 0.0f;
    Float3 radial = m_dir_t1;

    auto disc = [&](float radius, float thickness, Float3& out) {
        const float a = m_rng.uniform() * arc;
        const float inner = radius * (1.0f - thickness);
        const float rr = std::sqrt(inner * inner + m_rng.uniform() * (radius * radius - inner * inner));
        const float ca = std::cos(a), sa = std::sin(a);
        if (d2) {
            const Float3 ax{ 1.0f, 0.0f, 0.0f }, ay{ 0.0f, 1.0f, 0.0f };
            out = add(mul(ax, ca * rr), mul(ay, sa * rr));
            radial = { ca, sa, 0.0f };
        } else {
            radial = add(mul(m_dir_t1, ca), mul(m_dir_t2, sa));
            out = mul(radial, rr);
        }
        return rr;
    };

    switch (s.shape) {
        case EmissionShape::Point: break;
        case EmissionShape::Sphere:
        case EmissionShape::Hemisphere: {
            if (d2) {
                disc(r, th, pos);
                if (s.shape == EmissionShape::Hemisphere && dot(pos, m_dir) < 0.0f) pos = sub(pos, mul(m_dir, 2.0f * dot(pos, m_dir)));
            } else {
                const float z = 1.0f - 2.0f * m_rng.uniform();
                const float a = m_rng.uniform() * kTwoPi;
                const float sr = std::sqrt(hi_of(0.0f, 1.0f - z * z));
                Float3 u{ sr * std::cos(a), sr * std::sin(a), z };
                const float inner = r * (1.0f - th);
                const float i3 = inner * inner * inner, o3 = r * r * r;
                const float rr = std::cbrt(i3 + m_rng.uniform() * (o3 - i3));
                if (s.shape == EmissionShape::Hemisphere && dot(u, m_dir) < 0.0f) u = sub(u, mul(m_dir, 2.0f * dot(u, m_dir)));
                pos = mul(u, rr);
            }
            normal = normalized(pos, m_dir);
            break;
        }
        case EmissionShape::Cone: {
            const float rr = disc(r, th, pos);
            if (d2) {
                const float t = (m_rng.uniform() * 2.0f - 1.0f) * r;
                pos = mul(m_dir_t1, t);
                radial = t < 0.0f ? mul(m_dir_t1, -1.0f) : m_dir_t1;
                cone_frac = r > 0.0f ? std::fabs(t) / r : 0.0f;
            } else {
                cone_frac = r > 0.0f ? rr / r : 0.0f;
            }
            cone = r > 0.0f;
            break;
        }
        case EmissionShape::Box: {
            const Float3 h{ 0.5f * s.shape_size.x, 0.5f * s.shape_size.y, d2 ? 0.0f : 0.5f * s.shape_size.z };
            pos = { (m_rng.uniform() * 2.0f - 1.0f) * h.x, (m_rng.uniform() * 2.0f - 1.0f) * h.y, (m_rng.uniform() * 2.0f - 1.0f) * h.z };
            if (th <= 0.0f) {
                const int axis = static_cast<int>(m_rng.uniform() * (d2 ? 2.0f : 3.0f)) % (d2 ? 2 : 3);
                const float side = m_rng.uniform() < 0.5f ? -1.0f : 1.0f;
                if (axis == 0) pos.x = side * h.x; else if (axis == 1) pos.y = side * h.y; else pos.z = side * h.z;
            }
            normal = normalized(pos, m_dir);
            break;
        }
        case EmissionShape::Circle:
            disc(r, th, pos);
            normal = normalized(pos, m_dir);
            break;
        case EmissionShape::Ring:
            disc(r, 0.0f, pos);
            normal = normalized(pos, m_dir);
            break;
        case EmissionShape::Line: {
            const float t = m_rng.uniform() - 0.5f;
            pos = { t * s.shape_size.x, 0.0f, 0.0f };
            normal = m_dir;
            break;
        }
        case EmissionShape::Mesh: {
            if (m_mesh_cdf.empty()) break;
            const float pick = m_rng.uniform() * m_mesh_cdf.back();
            const std::size_t tri = static_cast<std::size_t>(std::upper_bound(m_mesh_cdf.begin(), m_mesh_cdf.end(), pick) - m_mesh_cdf.begin());
            const std::size_t k = (std::min)(tri, m_mesh_cdf.size() - 1) * 3;
            const Float3& a = m_mesh[k];
            const Float3& b = m_mesh[k + 1];
            const Float3& c = m_mesh[k + 2];
            float u = m_rng.uniform(), v = m_rng.uniform();
            if (u + v > 1.0f) { u = 1.0f - u; v = 1.0f - v; }
            pos = add(a, add(mul(sub(b, a), u), mul(sub(c, a), v)));
            normal = normalized(cross(sub(b, a), sub(c, a)), m_dir);
            break;
        }
    }

    const float spread = clamp01(s.spread / 180.0f) * kPi;
    switch (s.direction_mode) {
        case DirectionMode::Outward:
            dir = normal;
            break;
        case DirectionMode::Random:
            if (d2) {
                const float a = m_rng.uniform() * kTwoPi;
                dir = { std::cos(a), std::sin(a), 0.0f };
            } else {
                const float z = 1.0f - 2.0f * m_rng.uniform();
                const float a = m_rng.uniform() * kTwoPi;
                const float sr = std::sqrt(hi_of(0.0f, 1.0f - z * z));
                dir = { sr * std::cos(a), sr * std::sin(a), z };
            }
            break;
        default:
            if (cone) {
                const float tilt = spread * cone_frac;
                dir = add(mul(m_dir, std::cos(tilt)), mul(radial, std::sin(tilt)));
            } else if (d2) {
                const float a = (m_rng.uniform() * 2.0f - 1.0f) * spread;
                dir = add(mul(m_dir, std::cos(a)), mul(m_dir_t1, std::sin(a)));
            } else if (spread > 0.0f) {
                const float ct = 1.0f - m_rng.uniform() * (1.0f - std::cos(spread));
                const float st = std::sqrt(hi_of(0.0f, 1.0f - ct * ct));
                const float a = m_rng.uniform() * kTwoPi;
                dir = add(mul(m_dir, ct), mul(add(mul(m_dir_t1, std::cos(a)), mul(m_dir_t2, std::sin(a))), st));
            } else {
                dir = m_dir;
            }
            break;
    }
}

bool Emitter::spawn_one(float frac, float dt, const SpawnEvent* at) {
    if (m_count >= m_capacity) return false;
    const EmitterSettings& s = m_settings;
    Float3 lp, ld;
    sample_shape(lp, ld);
    const float speed = m_rng.range(s.speed);
    Float3 p, v;
    if (at) {
        p = add(Float3{ at->px, at->py, at->pz }, to_world_dir(lp));
        v = add(mul(to_world_dir(ld), speed), Float3{ at->vx, at->vy, at->vz });
    } else if (s.space == SimulationSpace::World) {
        const Float3 base = add(m_prev_pos, mul(sub(m_pos, m_prev_pos), frac));
        p = add(base, to_world_dir(lp));
        v = add(mul(to_world_dir(ld), speed), mul(m_velocity, s.inherit_velocity));
    } else {
        p = lp;
        v = mul(ld, speed);
    }
    const float age0 = (1.0f - frac) * dt;
    p = add(p, mul(v, age0));
    if (s.dimension == ParticleDim::D2) { p.z = 0.0f; v.z = 0.0f; }

    const std::size_t i = m_count++;
    m_px[i] = p.x; m_py[i] = p.y; m_pz[i] = p.z;
    m_vx[i] = v.x; m_vy[i] = v.y; m_vz[i] = v.z;
    m_age[i] = age0;
    const float life = m_rng.range(s.lifetime);
    m_inv_life[i] = 1.0f / hi_of(life, 1e-4f);
    m_size[i] = m_rng.range(s.size);
    m_rot[i] = m_rng.range(s.rotation) * kDegToRad;
    m_spin[i] = m_rng.range(s.spin) * kDegToRad;
    m_rnd[i] = m_rng.uniform();
    m_hit[i] = kHitNone;
    m_bounds.add(p);
    ++m_spawned_total;
    if (m_flags.birth_events) push_event(SubEmitterTrigger::Birth, i);
    return true;
}

void Emitter::push_event(SubEmitterTrigger trigger, std::size_t i) {
    for (SubLink& link : m_subs) {
        if (link.trigger != trigger || !link.child) continue;
        Emitter& c = *link.child;
        if (c.m_inbox_count >= c.m_inbox.size()) continue;
        SpawnEvent& e = c.m_inbox[c.m_inbox_count++];
        e.px = m_px[i]; e.py = m_py[i]; e.pz = m_pz[i];
        e.vx = m_vx[i] * link.inherit; e.vy = m_vy[i] * link.inherit; e.vz = m_vz[i] * link.inherit;
        if (m_settings.space == SimulationSpace::Local) {
            const Float3 wp = add(m_pos, to_world_dir(Float3{ e.px, e.py, e.pz }));
            const Float3 wv = to_world_dir(Float3{ e.vx, e.vy, e.vz });
            e.px = wp.x; e.py = wp.y; e.pz = wp.z;
            e.vx = wv.x; e.vy = wv.y; e.vz = wv.z;
        }
    }
}

std::size_t Emitter::burst(int count) {
    if (count <= 0) return 0;
    std::size_t n = 0;
    for (int k = 0; k < count; ++k) if (spawn_one(1.0f, 0.0f, nullptr)) ++n;
    return n;
}

void Emitter::prepare_step(float dt) noexcept {
    const EmitterSettings& s = m_settings;
    StepParams& p = m_step;
    p.dt = dt;
    p.drag_factor = m_flags.drag ? std::exp(-s.drag * dt) : 1.0f;
    Float3 accel = m_flags.gravity ? mul(s.gravity, s.gravity_scale * dt) : Float3{};
    Float3 wind = s.wind;
    if (s.space == SimulationSpace::Local) {
        accel = { dot(accel, m_basis[0]), dot(accel, m_basis[1]), dot(accel, m_basis[2]) };
        wind = { dot(wind, m_basis[0]), dot(wind, m_basis[1]), dot(wind, m_basis[2]) };
    }
    p.accel = accel;
    p.wind = m_flags.drag ? wind : Float3{};
    m_noise_time += dt * s.turbulence_speed;
    if (m_noise_time > 4096.0f) m_noise_time -= 4096.0f;
    p.noise_offset = { m_noise_time * 0.37f + static_cast<float>(m_seed & 15u), m_noise_time * 0.61f, m_noise_time * 0.53f + static_cast<float>((m_seed >> 4) & 15u) };
    p.noise_frequency = s.turbulence_frequency;
    p.turbulence = s.turbulence * dt;
    p.parity ^= 1u;
}

void Emitter::integrate(std::size_t begin, std::size_t end, Box3F& box) noexcept {
    box.reset();
    if (begin >= end) return;
    const StepParams p = m_step;
    const float dt = p.dt;
    float* __restrict px = m_px;
    float* __restrict py = m_py;
    float* __restrict pz = m_pz;
    float* __restrict vx = m_vx;
    float* __restrict vy = m_vy;
    float* __restrict vz = m_vz;
    float* __restrict age = m_age;
    const float* __restrict inv = m_inv_life;

    for (std::size_t i = begin; i < end; ++i) age[i] += dt;

    if (m_flags.turbulence) {
        const float* table = noise_table().v;
        const float f = p.noise_frequency;
        const float k = p.turbulence * 2.0f;
        const bool d2 = m_settings.dimension == ParticleDim::D2;
        for (std::size_t i = begin + ((begin + p.parity) & 1u); i < end; i += 2) {
            float nx, ny, nz;
            sample_noise(table, px[i] * f + p.noise_offset.x, py[i] * f + p.noise_offset.y, (d2 ? 0.0f : pz[i] * f) + p.noise_offset.z, nx, ny, nz);
            vx[i] += nx * k;
            vy[i] += ny * k;
            if (!d2) vz[i] += nz * k;
        }
    }

    const float df = p.drag_factor;
    const float wx = p.wind.x, wy = p.wind.y, wz = p.wind.z;
    const float ax = p.accel.x, ay = p.accel.y, az = p.accel.z;
    if (m_flags.speed_curve) {
        const float* lut = m_curves->speed;
        for (std::size_t i = begin; i < end; ++i) {
            vx[i] = wx + (vx[i] - wx) * df + ax;
            vy[i] = wy + (vy[i] - wy) * df + ay;
            vz[i] = wz + (vz[i] - wz) * df + az;
            const float m = lut[BakedCurves::index(age[i] * inv[i])] * dt;
            px[i] += vx[i] * m;
            py[i] += vy[i] * m;
            pz[i] += vz[i] * m;
        }
    } else {
        for (std::size_t i = begin; i < end; ++i) {
            vx[i] = wx + (vx[i] - wx) * df + ax;
            vy[i] = wy + (vy[i] - wy) * df + ay;
            vz[i] = wz + (vz[i] - wz) * df + az;
            px[i] += vx[i] * dt;
            py[i] += vy[i] * dt;
            pz[i] += vz[i] * dt;
        }
    }

    if (m_flags.spin) {
        float* __restrict rot = m_rot;
        const float* __restrict spin = m_spin;
        if (m_settings.spin_over_life.is_constant()) {
            const float m = m_settings.spin_over_life.keys[0].v * dt;
            for (std::size_t i = begin; i < end; ++i) rot[i] += spin[i] * m;
        } else {
            const float* lut = m_curves->spin;
            for (std::size_t i = begin; i < end; ++i) rot[i] += spin[i] * lut[BakedCurves::index(age[i] * inv[i])] * dt;
        }
    }

    if (m_flags.collide && m_colliders && !m_colliders->empty()) collide(begin, end);

    if (m_callback) {
        CollisionBatch b;
        b.px = px + begin; b.py = py + begin; b.pz = pz + begin;
        b.vx = vx + begin; b.vy = vy + begin; b.vz = vz + begin;
        b.size = m_size + begin;
        b.hit = m_hit.get() + begin;
        b.count = end - begin;
        b.dt = dt;
        b.origin = m_origin;
        b.emitter = this;
        m_callback(m_callback_user, b);
        if (m_settings.kill_on_collision) {
            for (std::size_t i = begin; i < end; ++i) if (m_hit[i]) m_age[i] = 2.0f / inv[i];
        } else {
            for (std::size_t i = begin; i < end; ++i) if (m_hit[i] == kHitKill) m_age[i] = 2.0f / inv[i];
        }
    }

    float lx = px[begin], ly = py[begin], lz = pz[begin];
    float hx = lx, hy = ly, hz = lz;
    for (std::size_t i = begin; i < end; ++i) {
        lx = px[i] < lx ? px[i] : lx; hx = px[i] > hx ? px[i] : hx;
        ly = py[i] < ly ? py[i] : ly; hy = py[i] > hy ? py[i] : hy;
        lz = pz[i] < lz ? pz[i] : lz; hz = pz[i] > hz ? pz[i] : hz;
    }
    box.lo = { lx, ly, lz };
    box.hi = { hx, hy, hz };
    box.valid = true;
}

void Emitter::collide(std::size_t begin, std::size_t end) noexcept {
    const EmitterSettings& s = m_settings;
    const ColliderSet& cs = *m_colliders;
    const std::uint32_t mask = s.collision_mask;
    const float bounce = s.bounce;
    const float keep = 1.0f - clamp01(s.friction);
    const float min_bounce = s.min_bounce_speed;
    const float rs = s.collision_radius * 0.5f;
    const bool kill = s.kill_on_collision;
    const float* inv = m_inv_life;

    auto respond = [&](std::size_t i, const Float3& n, float depth) {
        m_px[i] += n.x * depth; m_py[i] += n.y * depth; m_pz[i] += n.z * depth;
        const float vn = m_vx[i] * n.x + m_vy[i] * n.y + m_vz[i] * n.z;
        if (vn < 0.0f) {
            const float tx = m_vx[i] - n.x * vn, ty = m_vy[i] - n.y * vn, tz = m_vz[i] - n.z * vn;
            const float out = -vn * bounce < min_bounce ? 0.0f : -vn * bounce;
            m_vx[i] = tx * keep + n.x * out;
            m_vy[i] = ty * keep + n.y * out;
            m_vz[i] = tz * keep + n.z * out;
            m_hit[i] = kill ? kHitKill : kHitBounce;
            if (kill) m_age[i] = 2.0f / inv[i];
        }
    };

    for (const PlaneCollider& pl : cs.planes) {
        if (!(pl.layers & mask)) continue;
        const Float3 n = pl.normal;
        for (std::size_t i = begin; i < end; ++i) {
            const float r = m_size[i] * rs;
            const float d = m_px[i] * n.x + m_py[i] * n.y + m_pz[i] * n.z + pl.offset - r;
            if (d < 0.0f) respond(i, n, -d);
        }
    }
    for (const SphereCollider& sp : cs.spheres) {
        if (!(sp.layers & mask)) continue;
        for (std::size_t i = begin; i < end; ++i) {
            const float r = m_size[i] * rs + sp.radius;
            const float dx = m_px[i] - sp.center.x, dy = m_py[i] - sp.center.y, dz = m_pz[i] - sp.center.z;
            const float d2 = dx * dx + dy * dy + dz * dz;
            if (d2 >= r * r) continue;
            const float d = std::sqrt(d2);
            const Float3 n = d > 1e-6f ? Float3{ dx / d, dy / d, dz / d } : Float3{ 0.0f, 1.0f, 0.0f };
            respond(i, n, r - d);
        }
    }
    for (const BoxCollider& bx : cs.boxes) {
        if (!(bx.layers & mask)) continue;
        for (std::size_t i = begin; i < end; ++i) {
            const float r = m_size[i] * rs;
            const float x = m_px[i], y = m_py[i], z = m_pz[i];
            if (x < bx.lo.x - r || x > bx.hi.x + r || y < bx.lo.y - r || y > bx.hi.y + r || z < bx.lo.z - r || z > bx.hi.z + r) continue;
            const float pen[6] = { x - (bx.lo.x - r), (bx.hi.x + r) - x, y - (bx.lo.y - r), (bx.hi.y + r) - y, z - (bx.lo.z - r), (bx.hi.z + r) - z };
            int best = 0;
            for (int k = 1; k < 6; ++k) if (pen[k] < pen[best]) best = k;
            static const Float3 normals[6] = { { -1, 0, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 1, 0 }, { 0, 0, -1 }, { 0, 0, 1 } };
            respond(i, normals[best], pen[best]);
        }
    }
}

void Emitter::kill_dead() {
    std::size_t n = m_count;
    std::size_t i = 0;
    const bool events = m_flags.death_events;
    while (i < n) {
        if (m_age[i] * m_inv_life[i] < 1.0f) { ++i; continue; }
        if (events) push_event(SubEmitterTrigger::Death, i);
        --n;
        ++m_died_total;
        if (i != n) {
            m_px[i] = m_px[n]; m_py[i] = m_py[n]; m_pz[i] = m_pz[n];
            m_vx[i] = m_vx[n]; m_vy[i] = m_vy[n]; m_vz[i] = m_vz[n];
            m_age[i] = m_age[n]; m_inv_life[i] = m_inv_life[n]; m_size[i] = m_size[n];
            m_rot[i] = m_rot[n]; m_spin[i] = m_spin[n]; m_rnd[i] = m_rnd[n];
            m_hit[i] = m_hit[n];
        }
    }
    m_count = n;
}

std::size_t Emitter::want_this_step(float dt) const noexcept {
    const std::size_t free_slots = m_capacity - m_count;
    if (m_is_child) return free_slots;
    if (!m_playing || m_paused) return 0;
    const EmitterSettings& s = m_settings;
    float n = s.rate * m_spawn_scale * dt * 1.5f + 2.0f;
    if (s.rate_per_distance > 0.0f) n += s.rate_per_distance * m_spawn_scale * length(sub(m_pos, m_prev_pos)) + 1.0f;
    for (std::size_t b = 0; b < s.bursts.size(); ++b) {
        const Burst& bu = s.bursts[b];
        const int done = b < m_burst_cycles.size() ? m_burst_cycles[b] : 0;
        if (bu.cycles > 0 && done >= bu.cycles) continue;
        const float when = bu.time + static_cast<float>(done) * bu.interval;
        if (when < m_time + dt + m_delay_left) n += static_cast<float>(bu.count);
    }
    const std::size_t want = static_cast<std::size_t>(n);
    return want < free_slots ? want : free_slots;
}

std::size_t Emitter::emit(float dt, std::size_t quota) {
    if (!m_playing || m_paused || dt <= 0.0f) return 0;
    const EmitterSettings& s = m_settings;
    float edt = dt;
    if (m_delay_left > 0.0f) {
        if (dt <= m_delay_left) { m_delay_left -= dt; return 0; }
        edt = dt - m_delay_left;
        m_delay_left = 0.0f;
    }

    const float t0 = m_time;
    float t1 = t0 + edt;
    const float duration = s.duration;
    bool wrapped = false, ended = false;
    float active = edt;
    if (duration > 0.0f && t1 >= duration) {
        active = hi_of(0.0f, duration - t0);
        if (s.loop) wrapped = true; else ended = true;
    }

    const float scale = m_spawn_scale;
    m_rate_carry += s.rate * scale * (s.loop ? edt : active);
    if (s.rate_per_distance > 0.0f) m_distance_carry += s.rate_per_distance * scale * length(sub(m_pos, m_prev_pos));
    std::size_t n = static_cast<std::size_t>(m_rate_carry);
    m_rate_carry -= static_cast<float>(n);
    const std::size_t nd = static_cast<std::size_t>(m_distance_carry);
    m_distance_carry -= static_cast<float>(nd);
    n += nd;

    auto fire_bursts = [&](float to) {
        for (std::size_t b = 0; b < s.bursts.size(); ++b) {
            const Burst& bu = s.bursts[b];
            int& done = m_burst_cycles[b];
            while (bu.cycles <= 0 || done < bu.cycles) {
                const float when = bu.time + static_cast<float>(done) * hi_of(bu.interval, 1e-3f);
                if (when >= to || (duration > 0.0f && when >= duration)) break;
                if (bu.probability >= 1.0f || m_rng.uniform() < bu.probability) {
                    const float c = static_cast<float>(bu.count) * scale;
                    std::size_t k = static_cast<std::size_t>(std::lround(c));
                    if (k == 0 && bu.count > 0 && scale > 0.0f) k = 1;
                    n += k;
                }
                ++done;
                if (bu.cycles <= 0 && done > 100000) break;
            }
        }
    };
    fire_bursts(wrapped || ended ? duration : t1);

    if (wrapped) {
        t1 = std::fmod(t1, duration);
        std::fill(m_burst_cycles.begin(), m_burst_cycles.end(), 0);
        fire_bursts(t1);
    }
    m_time = ended ? duration : t1;
    if (ended) m_playing = false;

    const std::size_t limit = (std::min)(quota, m_capacity - m_count);
    if (n > limit) { m_last_limited += n - limit; n = limit; }
    std::size_t made = 0;
    for (std::size_t k = 0; k < n; ++k) {
        const float frac = (static_cast<float>(k) + 0.5f) / static_cast<float>(n);
        if (spawn_one(frac, dt, nullptr)) ++made;
    }
    return made;
}

std::size_t Emitter::spawn_events(std::size_t quota) {
    std::size_t made = 0;
    const float chance = m_event_probability * m_spawn_scale;
    for (std::size_t e = 0; e < m_inbox_count; ++e) {
        for (int k = 0; k < m_event_count; ++k) {
            if (chance < 1.0f && m_rng.uniform() >= chance) continue;
            if (made >= quota) { ++m_last_limited; continue; }
            if (spawn_one(1.0f, 0.0f, &m_inbox[e])) ++made; else ++m_last_limited;
        }
    }
    m_inbox_count = 0;
    return made;
}

void Emitter::finalize(std::size_t quota) {
    m_bounds.reset();
    const std::size_t chunks = (m_count + kChunk - 1) / kChunk;
    for (std::size_t c = 0; c < chunks; ++c) m_bounds.add(m_chunk_bounds[c]);

    if (m_flags.collision_events) {
        for (std::size_t i = 0; i < m_count; ++i) if (m_hit[i]) push_event(SubEmitterTrigger::Collision, i);
    }
    kill_dead();
    if (m_flags.collide || m_callback) std::memset(m_hit.get(), 0, m_count);

    m_last_limited = 0;
    const float dt = m_step.dt;
    if (!m_velocity_set && dt > 0.0f && m_has_prev) m_velocity = mul(sub(m_pos, m_prev_pos), 1.0f / dt);
    std::size_t made = 0;
    if (m_is_child) made = spawn_events(quota);
    else made = emit(dt, quota);
    m_prev_pos = m_pos;
    m_last_spawned = made;
    if (m_count == 0) m_bounds.reset();
}

void Emitter::finalize_family(std::size_t) {
    finalize(m_quota);
    for (SubLink& s : m_subs) if (s.child) s.child->finalize_family(0);
}

void Emitter::step_family(float dt, std::size_t quota) {
    prepare_step(dt);
    const std::size_t chunks = (m_count + kChunk - 1) / kChunk;
    for (std::size_t c = 0; c < chunks; ++c) integrate(c * kChunk, (std::min)(m_count, (c + 1) * kChunk), m_chunk_bounds[c]);
    finalize(quota);
    for (SubLink& s : m_subs) if (s.child) s.child->step_family(dt, quota == kUnlimited ? kUnlimited : s.child->m_quota);
}

void Emitter::step(float dt) {
    if (dt <= 0.0f || m_paused) return;
    step_family(dt, kUnlimited);
}

void Emitter::catch_up(float seconds, int max_steps, std::size_t quota) {
    if (seconds <= 0.0f) return;
    const float life = m_settings.max_lifetime();
    if (m_playing && m_settings.loop && seconds > life) {
        clear();
        seconds = life;
    }
    int steps = static_cast<int>(std::ceil(seconds * 30.0f));
    if (steps > max_steps) steps = max_steps;
    if (steps < 1) steps = 1;
    const float h = seconds / static_cast<float>(steps);
    for (int k = 0; k < steps; ++k) step_family(h, quota == kUnlimited ? kUnlimited : quota / static_cast<std::size_t>(steps) + 1);
}

void Emitter::warm_up(float seconds, int max_steps) {
    if (seconds <= 0.0f) return;
    int steps = static_cast<int>(std::ceil(seconds * 30.0f));
    if (steps > max_steps) steps = max_steps;
    if (steps < 1) steps = 1;
    const float h = seconds / static_cast<float>(steps);
    for (int k = 0; k < steps; ++k) step_family(h, kUnlimited);
}

WorldBounds Emitter::bounds() const noexcept {
    WorldBounds out;
    Box3F b = m_bounds;
    for (const SubLink& s : m_subs) if (s.child && s.child->m_count > 0) {
        const WorldBounds cb = s.child->bounds();
        if (cb.valid) {
            b.add(Float3{ static_cast<float>(cb.lo.x - m_origin.x), static_cast<float>(cb.lo.y - m_origin.y), static_cast<float>(cb.lo.z - m_origin.z) });
            b.add(Float3{ static_cast<float>(cb.hi.x - m_origin.x), static_cast<float>(cb.hi.y - m_origin.y), static_cast<float>(cb.hi.z - m_origin.z) });
        }
    }
    if (!b.valid) return out;
    const float pad = 0.5f * m_settings.size.largest() * m_settings.size_over_life.peak() * (m_settings.render_mode == RenderMode::Stretched ? 2.0f : 1.0f);
    b.pad(pad);
    if (m_settings.space == SimulationSpace::Local) {
        const Float3 c = b.center();
        const Float3 h{ 0.5f * (b.hi.x - b.lo.x), 0.5f * (b.hi.y - b.lo.y), 0.5f * (b.hi.z - b.lo.z) };
        const Float3 wc = add(m_pos, to_world_dir(c));
        const float ex = std::fabs(m_basis[0].x) * h.x + std::fabs(m_basis[1].x) * h.y + std::fabs(m_basis[2].x) * h.z;
        const float ey = std::fabs(m_basis[0].y) * h.x + std::fabs(m_basis[1].y) * h.y + std::fabs(m_basis[2].y) * h.z;
        const float ez = std::fabs(m_basis[0].z) * h.x + std::fabs(m_basis[1].z) * h.y + std::fabs(m_basis[2].z) * h.z;
        b.lo = { wc.x - ex, wc.y - ey, wc.z - ez };
        b.hi = { wc.x + ex, wc.y + ey, wc.z + ez };
    }
    out.lo = { m_origin.x + b.lo.x, m_origin.y + b.lo.y, m_origin.z + b.lo.z };
    out.hi = { m_origin.x + b.hi.x, m_origin.y + b.hi.y, m_origin.z + b.hi.z };
    out.valid = true;
    return out;
}

WorldBounds Emitter::potential_bounds() const noexcept {
    WorldBounds out = bounds();
    const float r = m_settings.cull_radius > 0.0f ? m_settings.cull_radius : m_settings.estimated_reach();
    const linalg::vector3d p = position();
    const linalg::vector3d lo{ p.x - r, p.y - r, p.z - r }, hi{ p.x + r, p.y + r, p.z + r };
    if (!out.valid) { out.lo = lo; out.hi = hi; out.valid = true; return out; }
    if (m_playing) {
        out.lo = { (std::min)(out.lo.x, lo.x), (std::min)(out.lo.y, lo.y), (std::min)(out.lo.z, lo.z) };
        out.hi = { (std::max)(out.hi.x, hi.x), (std::max)(out.hi.y, hi.y), (std::max)(out.hi.z, hi.z) };
    }
    return out;
}

ParticleView Emitter::view() const noexcept {
    ParticleView v;
    v.px = m_px; v.py = m_py; v.pz = m_pz;
    v.vx = m_vx; v.vy = m_vy; v.vz = m_vz;
    v.age = m_age; v.inv_life = m_inv_life; v.size = m_size;
    v.rotation = m_rot; v.rnd = m_rnd;
    v.count = m_count;
    return v;
}

ColorF Emitter::particle_color(float t, float rnd) const noexcept {
    const EmitterSettings& s = m_settings;
    const int k = BakedCurves::index(t);
    const ColorF& g = m_curves->color[k];
    const float a = m_curves->alpha[k];
    return {
        (s.color.r + (s.color_alt.r - s.color.r) * rnd) * g.r,
        (s.color.g + (s.color_alt.g - s.color.g) * rnd) * g.g,
        (s.color.b + (s.color_alt.b - s.color.b) * rnd) * g.b,
        (s.color.a + (s.color_alt.a - s.color.a) * rnd) * g.a * a
    };
}

float Emitter::particle_size(std::size_t i) const noexcept {
    if (i >= m_count) return 0.0f;
    return m_size[i] * m_curves->size[BakedCurves::index(m_age[i] * m_inv_life[i])];
}

std::uint32_t Emitter::particle_frame(std::size_t i) const noexcept {
    const std::uint32_t frames = m_settings.frames();
    if (frames <= 1 || i >= m_count) return 0;
    const float t = clamp01(m_age[i] * m_inv_life[i]);
    std::uint32_t f = 0;
    switch (m_settings.flipbook) {
        case FlipbookMode::OverLife: f = static_cast<std::uint32_t>(t * static_cast<float>(frames)); break;
        case FlipbookMode::Fps: {
            const float start = m_settings.random_start_frame ? m_rnd[i] * static_cast<float>(frames) : 0.0f;
            f = static_cast<std::uint32_t>(start + m_age[i] * m_settings.frame_fps) % frames;
            break;
        }
        case FlipbookMode::Random: f = static_cast<std::uint32_t>(m_rnd[i] * static_cast<float>(frames)); break;
    }
    return f >= frames ? frames - 1 : f;
}

} // namespace particles
} // namespace fizmo
