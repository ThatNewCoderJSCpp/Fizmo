#ifndef FIZMO_PARTICLES_2D_HPP
#define FIZMO_PARTICLES_2D_HPP

#include "texture.hpp"
#include "draw_types_2d.hpp"
#include "../Util Hpp/ease_tween.hpp"
#include "../Vectors/vectors.hpp"
#include "../Images/Bitmap/image.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <random>
#include <vector>

namespace fizmo {
namespace graphics {

struct FloatRange {
    float min = 0.0f;
    float max = 0.0f;

    constexpr FloatRange() noexcept = default;
    constexpr FloatRange(float v) noexcept : min(v), max(v) {}
    constexpr FloatRange(float lo, float hi) noexcept : min(lo), max(hi) {}
};

enum class EmitShape : std::uint8_t { Point = 0, Circle, Ring, Rect, Line };

struct ColorStop {
    float t = 0.0f;
    Color color;
};

struct ParticleConfig {
    float        rate          = 50.0f;
    unsigned int max_particles = 1000;
    FloatRange   lifetime      { 0.8f, 1.2f };
    FloatRange   speed         { 40.0f, 80.0f };
    float        direction     = -90.0f;
    float        spread        = 360.0f;
    float        gravity_x     = 0.0f;
    float        gravity_y     = 0.0f;
    float        damping       = 0.0f;
    FloatRange   start_size    { 8.0f, 12.0f };
    FloatRange   end_size      { 0.0f, 0.0f };
    FloatRange   rotation      { 0.0f, 0.0f };
    FloatRange   spin          { 0.0f, 0.0f };
    bool         align_to_velocity = false;
    std::vector<ColorStop> colors { { 0.0f, Color(255, 255, 255, 255) }, { 1.0f, Color(255, 255, 255, 0) } };
    Easing       size_easing   = Easing::Linear;
    EmitShape    shape         = EmitShape::Point;
    float        shape_x       = 0.0f;
    float        shape_y       = 0.0f;
    bool         local_space   = false;
    Texture      texture;
    TextureRect  source;
    BlendMode    blend         = BlendMode::Normal;
    float        stretch       = 0.0f;
};

struct Particle {
    float x = 0.0f, y = 0.0f;
    float vx = 0.0f, vy = 0.0f;
    float age = 0.0f, life = 1.0f;
    float size0 = 1.0f, size1 = 0.0f;
    float rotation = 0.0f, spin = 0.0f;
    std::uint32_t seed = 0;
};

class ParticleEmitter {
private:
    ParticleConfig        m_config;
    std::vector<Particle> m_particles;
    std::mt19937          m_rng;
    float                 m_x = 0.0f, m_y = 0.0f;
    float                 m_angle = 0.0f;
    float                 m_carry = 0.0f;
    bool                  m_emitting = true;
    double                m_duration = -1.0;
    double                m_elapsed = 0.0;
    std::function<void(Particle&, float)> m_affector;
    mutable Easing                        m_cached_easing = Easing::Linear;
    mutable std::function<double(double)> m_ease_fn;

    float rand01() { return std::uniform_real_distribution<float>(0.0f, 1.0f)(m_rng); }
    float pick(const FloatRange& r) { return r.min + (r.max - r.min) * rand01(); }

    void spawn_one();

public:
    explicit ParticleEmitter(ParticleConfig config = {}, std::uint32_t seed = 0x9E3779B9u) : m_config(std::move(config)), m_rng(seed) {}

    ParticleConfig& config() noexcept { return m_config; }
    const ParticleConfig& config() const noexcept { return m_config; }
    const std::vector<Particle>& particles() const noexcept { return m_particles; }
    std::size_t count() const noexcept { return m_particles.size(); }

    void set_position(float x, float y) noexcept { m_x = x; m_y = y; }
    float x() const noexcept { return m_x; }
    float y() const noexcept { return m_y; }
    void set_angle(float degrees) noexcept { m_angle = degrees; }
    float angle() const noexcept { return m_angle; }
    void start() noexcept { m_emitting = true; m_elapsed = 0.0; }
    void stop() noexcept { m_emitting = false; }
    bool emitting() const noexcept { return m_emitting; }
    void set_duration(double seconds) noexcept { m_duration = seconds; }
    bool finished() const noexcept { return !m_emitting && m_particles.empty(); }
    void clear() noexcept { m_particles.clear(); m_carry = 0.0f; }
    void set_affector(std::function<void(Particle&, float)> fn) { m_affector = std::move(fn); }

    void burst(unsigned int n) { for (unsigned int i = 0; i < n; ++i) spawn_one(); }

    void update(float dt);

    Color color_at(float t) const noexcept;

    float size_at(const Particle& p) const;

    vector2d world_position(const Particle& p) const noexcept;

    static const Texture& default_texture();

    static const Texture& square_texture();
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PARTICLES_2D_HPP
