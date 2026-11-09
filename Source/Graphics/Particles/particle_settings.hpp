#ifndef FIZMO_PARTICLES_SETTINGS_HPP
#define FIZMO_PARTICLES_SETTINGS_HPP

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace fizmo {
namespace particles {

struct Float3 {
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    constexpr Float3() noexcept = default;
    constexpr Float3(float px, float py, float pz) noexcept : x(px), y(py), z(pz) {}

    bool operator==(const Float3& o) const noexcept { return x == o.x && y == o.y && z == o.z; }
    bool operator!=(const Float3& o) const noexcept { return !(*this == o); }
};

struct ColorF {
    float r = 1.0f;
    float g = 1.0f;
    float b = 1.0f;
    float a = 1.0f;

    constexpr ColorF() noexcept = default;
    constexpr ColorF(float pr, float pg, float pb, float pa = 1.0f) noexcept : r(pr), g(pg), b(pb), a(pa) {}

    static ColorF bytes(int pr, int pg, int pb, int pa = 255) noexcept { return { pr / 255.0f, pg / 255.0f, pb / 255.0f, pa / 255.0f }; }

    bool operator==(const ColorF& o) const noexcept { return r == o.r && g == o.g && b == o.b && a == o.a; }
    bool operator!=(const ColorF& o) const noexcept { return !(*this == o); }
};

struct RangeF {
    float lo = 0.0f;
    float hi = 0.0f;

    constexpr RangeF() noexcept = default;
    constexpr RangeF(float v) noexcept : lo(v), hi(v) {}
    constexpr RangeF(float a, float b) noexcept : lo(a), hi(b) {}

    float mid() const noexcept { return 0.5f * (lo + hi); }
    float largest() const noexcept { return lo > hi ? lo : hi; }

    bool operator==(const RangeF& o) const noexcept { return lo == o.lo && hi == o.hi; }
    bool operator!=(const RangeF& o) const noexcept { return !(*this == o); }
};

struct CurveKey {
    float t = 0.0f;
    float v = 1.0f;
};

struct LifeCurve {
    static constexpr int kMaxKeys = 8;

    CurveKey keys[kMaxKeys] = { { 0.0f, 1.0f } };
    int      count = 1;

    static LifeCurve constant(float v) noexcept { LifeCurve c; c.keys[0] = { 0.0f, v }; c.count = 1; return c; }
    static LifeCurve linear(float a, float b) noexcept { LifeCurve c = constant(a); c.add(1.0f, b); return c; }
    static LifeCurve three(float a, float mid_t, float m, float b) noexcept { LifeCurve c = constant(a); c.add(mid_t, m); c.add(1.0f, b); return c; }

    LifeCurve& add(float t, float v) noexcept;
    float evaluate(float t) const noexcept;
    bool is_constant() const noexcept;
    float peak() const noexcept;

    bool operator==(const LifeCurve& o) const noexcept;
    bool operator!=(const LifeCurve& o) const noexcept { return !(*this == o); }
};

struct GradientKey {
    float  t = 0.0f;
    ColorF color{};
};

struct LifeGradient {
    static constexpr int kMaxKeys = 8;

    GradientKey keys[kMaxKeys] = { { 0.0f, ColorF{} } };
    int         count = 1;

    static LifeGradient constant(const ColorF& c) noexcept { LifeGradient g; g.keys[0] = { 0.0f, c }; g.count = 1; return g; }
    static LifeGradient linear(const ColorF& a, const ColorF& b) noexcept { LifeGradient g = constant(a); g.add(1.0f, b); return g; }

    LifeGradient& add(float t, const ColorF& c) noexcept;
    ColorF evaluate(float t) const noexcept;
    bool is_constant() const noexcept;

    bool operator==(const LifeGradient& o) const noexcept;
    bool operator!=(const LifeGradient& o) const noexcept { return !(*this == o); }
};

enum class ParticleDim : std::uint8_t { D3 = 0, D2 };

enum class SimulationSpace : std::uint8_t { World = 0, Local };

enum class EmissionShape : std::uint8_t { Point = 0, Sphere, Hemisphere, Cone, Box, Circle, Ring, Line, Mesh };

enum class DirectionMode : std::uint8_t { Axis = 0, Outward, Random };

enum class ParticleBlend : std::uint8_t { Alpha = 0, Additive, Premultiplied };

enum class RenderMode : std::uint8_t { Billboard = 0, Stretched, Horizontal, Vertical };

enum class SortMode : std::uint8_t { Auto = 0, None, Approximate, Exact };

enum class FlipbookMode : std::uint8_t { OverLife = 0, Fps, Random };

enum class OffscreenMode : std::uint8_t { Auto = 0, Simulate, Throttle, Pause };

enum class SubEmitterTrigger : std::uint8_t { Death = 0, Collision, Birth };

enum class ParticlePreset : std::uint8_t {
    Sparks = 0,
    Smoke,
    Fire,
    Explosion,
    Dust,
    Rain,
    Snow,
    MagicSparkle,
    Debris,
    Bubbles,
    Steam,
    Embers,
    Impact,
    Count
};

const char* to_string(ParticleDim v) noexcept;
const char* to_string(SimulationSpace v) noexcept;
const char* to_string(EmissionShape v) noexcept;
const char* to_string(DirectionMode v) noexcept;
const char* to_string(ParticleBlend v) noexcept;
const char* to_string(RenderMode v) noexcept;
const char* to_string(SortMode v) noexcept;
const char* to_string(FlipbookMode v) noexcept;
const char* to_string(OffscreenMode v) noexcept;
const char* to_string(SubEmitterTrigger v) noexcept;
const char* to_string(ParticlePreset v) noexcept;

bool parse(std::string_view text, ParticleDim& out) noexcept;
bool parse(std::string_view text, SimulationSpace& out) noexcept;
bool parse(std::string_view text, EmissionShape& out) noexcept;
bool parse(std::string_view text, DirectionMode& out) noexcept;
bool parse(std::string_view text, ParticleBlend& out) noexcept;
bool parse(std::string_view text, RenderMode& out) noexcept;
bool parse(std::string_view text, SortMode& out) noexcept;
bool parse(std::string_view text, FlipbookMode& out) noexcept;
bool parse(std::string_view text, OffscreenMode& out) noexcept;
bool parse(std::string_view text, SubEmitterTrigger& out) noexcept;
bool parse(std::string_view text, ParticlePreset& out) noexcept;

struct Burst {
    float time        = 0.0f;
    int   count       = 10;
    int   cycles      = 1;
    float interval    = 1.0f;
    float probability = 1.0f;

    bool operator==(const Burst& o) const noexcept {
        return time == o.time && count == o.count && cycles == o.cycles && interval == o.interval && probability == o.probability;
    }
    bool operator!=(const Burst& o) const noexcept { return !(*this == o); }
};

struct SubEmitter;

struct EmitterSettings {
    std::string     name;
    ParticleDim     dimension = ParticleDim::D3;
    SimulationSpace space = SimulationSpace::World;
    std::uint32_t   max_particles = 1000;
    int             priority = 0;
    std::uint32_t   seed = 0;
    float           lod_bias = 1.0f;

    float duration = 0.0f;
    bool  loop = true;
    float start_delay = 0.0f;
    bool  prewarm = false;
    bool  auto_play = true;
    float rate = 20.0f;
    float rate_per_distance = 0.0f;
    std::vector<Burst> bursts;

    RangeF        lifetime{ 1.0f, 1.0f };
    EmissionShape shape = EmissionShape::Point;
    float         shape_radius = 0.5f;
    float         shape_thickness = 1.0f;
    float         shape_arc = 360.0f;
    Float3        shape_size{ 1.0f, 1.0f, 1.0f };
    Float3        direction{ 0.0f, 1.0f, 0.0f };
    DirectionMode direction_mode = DirectionMode::Axis;
    float         spread = 15.0f;
    RangeF        speed{ 1.0f, 2.0f };
    float         inherit_velocity = 0.0f;
    RangeF        size{ 0.2f, 0.3f };
    RangeF        rotation{ 0.0f, 0.0f };
    RangeF        spin{ 0.0f, 0.0f };
    ColorF        color{ 1.0f, 1.0f, 1.0f, 1.0f };
    ColorF        color_alt{ 1.0f, 1.0f, 1.0f, 1.0f };

    float  gravity_scale = 0.0f;
    Float3 gravity{ 0.0f, -9.81f, 0.0f };
    float  drag = 0.0f;
    Float3 wind{ 0.0f, 0.0f, 0.0f };
    float  turbulence = 0.0f;
    float  turbulence_frequency = 0.5f;
    float  turbulence_speed = 0.5f;

    LifeCurve    size_over_life = LifeCurve::constant(1.0f);
    LifeGradient color_over_life = LifeGradient::constant(ColorF{});
    LifeCurve    alpha_over_life = LifeCurve::linear(1.0f, 0.0f);
    LifeCurve    speed_over_life = LifeCurve::constant(1.0f);
    LifeCurve    spin_over_life = LifeCurve::constant(1.0f);

    RenderMode    render_mode = RenderMode::Billboard;
    float         stretch = 0.0f;
    ParticleBlend blend = ParticleBlend::Alpha;
    bool          lit = false;
    bool          soft = false;
    float         soft_distance = 0.5f;
    float         near_fade = 0.0f;
    float         pivot_x = 0.0f;
    float         pivot_y = 0.0f;
    SortMode      sort = SortMode::Auto;
    int           frames_x = 1;
    int           frames_y = 1;
    int           frame_count = 0;
    FlipbookMode  flipbook = FlipbookMode::OverLife;
    float         frame_fps = 10.0f;
    bool          random_start_frame = false;
    int           texture_id = 0;
    std::string   texture;

    bool          collide = false;
    float         bounce = 0.3f;
    float         friction = 0.2f;
    bool          kill_on_collision = false;
    float         collision_radius = 0.5f;
    float         min_bounce_speed = 0.0f;
    std::uint32_t collision_mask = 0xFFFFFFFFu;

    OffscreenMode offscreen = OffscreenMode::Auto;
    float         cull_radius = 0.0f;

    std::vector<SubEmitter> sub_emitters;

    EmitterSettings();
    EmitterSettings(const EmitterSettings&);
    EmitterSettings(EmitterSettings&&) noexcept;
    EmitterSettings& operator=(const EmitterSettings&);
    EmitterSettings& operator=(EmitterSettings&&) noexcept;
    ~EmitterSettings();

    float max_lifetime() const noexcept { return lifetime.largest(); }
    bool one_shot() const noexcept { return !loop && duration > 0.0f; }
    std::uint32_t frames() const noexcept;
    float estimated_reach() const noexcept;
    float steady_count() const noexcept;

    bool operator==(const EmitterSettings& o) const;
    bool operator!=(const EmitterSettings& o) const { return !(*this == o); }
};

struct SubEmitter {
    SubEmitterTrigger trigger = SubEmitterTrigger::Death;
    int               count = 1;
    float             probability = 1.0f;
    float             inherit_velocity = 0.0f;
    EmitterSettings   settings;

    bool operator==(const SubEmitter& o) const {
        return trigger == o.trigger && count == o.count && probability == o.probability && inherit_velocity == o.inherit_velocity && settings == o.settings;
    }
    bool operator!=(const SubEmitter& o) const { return !(*this == o); }
};

EmitterSettings preset(ParticlePreset which, ParticleDim dimension = ParticleDim::D3, float pixels_per_unit = 50.0f);

EmitterSettings to_2d(const EmitterSettings& settings_3d, float pixels_per_unit = 50.0f);

std::string to_text(const EmitterSettings& settings);

bool from_text(std::string_view text, EmitterSettings& out, std::string* error = nullptr);

} // namespace particles
} // namespace fizmo

#endif // FIZMO_PARTICLES_SETTINGS_HPP
