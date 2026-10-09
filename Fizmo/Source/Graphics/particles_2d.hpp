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

    void spawn_one() {
        if (m_particles.size() >= m_config.max_particles) return;
        Particle p;
        float ox = 0.0f, oy = 0.0f;
        const float two_pi = 6.28318530718f;

        switch (m_config.shape) {
            case EmitShape::Circle: {
                const float a = rand01() * two_pi, r = std::sqrt(rand01()) * m_config.shape_x;
                ox = std::cos(a) * r; oy = std::sin(a) * r;
                break;
            }
            case EmitShape::Ring: {
                const float a = rand01() * two_pi;
                ox = std::cos(a) * m_config.shape_x; oy = std::sin(a) * m_config.shape_x;
                break;
            }
            case EmitShape::Rect:
                ox = (rand01() - 0.5f) * m_config.shape_x;
                oy = (rand01() - 0.5f) * m_config.shape_y;
                break;
            case EmitShape::Line: {
                const float t = rand01() - 0.5f;
                ox = t * m_config.shape_x; oy = t * m_config.shape_y;
                break;
            }
            default: break;
        }

        const float rad = m_angle * 0.0174532925f;
        const float c = std::cos(rad), s = std::sin(rad);
        p.x = (m_config.local_space ? 0.0f : m_x) + ox * c - oy * s;
        p.y = (m_config.local_space ? 0.0f : m_y) + ox * s + oy * c;
        const float dir = (m_config.direction + m_angle + (rand01() - 0.5f) * m_config.spread) * 0.0174532925f;
        const float speed = pick(m_config.speed);
        p.vx = std::cos(dir) * speed;
        p.vy = std::sin(dir) * speed;
        p.life = std::max(0.001f, pick(m_config.lifetime));
        p.size0 = pick(m_config.start_size);
        p.size1 = pick(m_config.end_size);
        p.rotation = pick(m_config.rotation);
        p.spin = pick(m_config.spin);
        p.seed = static_cast<std::uint32_t>(m_rng());
        m_particles.push_back(p);
    }

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

    void update(float dt) {
        if (dt <= 0.0f) return;

        if (m_emitting) {
            m_elapsed += dt;
            if (m_duration >= 0.0 && m_elapsed >= m_duration) m_emitting = false;
            m_carry += m_config.rate * dt;
            const unsigned int n = static_cast<unsigned int>(m_carry);
            m_carry -= static_cast<float>(n);
            for (unsigned int i = 0; i < n; ++i) spawn_one();
        }

        const float drag = m_config.damping > 0.0f ? std::exp(-m_config.damping * dt) : 1.0f;
        std::size_t i = 0;

        while (i < m_particles.size()) {
            Particle& p = m_particles[i];
            p.age += dt;
            if (p.age >= p.life) { p = m_particles.back(); m_particles.pop_back(); continue; }
            p.vx = (p.vx + m_config.gravity_x * dt) * drag;
            p.vy = (p.vy + m_config.gravity_y * dt) * drag;
            p.x += p.vx * dt;
            p.y += p.vy * dt;
            p.rotation += p.spin * dt;
            if (m_affector) m_affector(p, dt);
            ++i;
        }
    }

    Color color_at(float t) const noexcept {
        const auto& stops = m_config.colors;
        if (stops.empty()) return Color(255, 255, 255, 255);
        if (t <= stops.front().t) return stops.front().color;
        for (std::size_t k = 1; k < stops.size(); ++k) {
            if (t > stops[k].t) continue;
            const ColorStop& a = stops[k - 1];
            const ColorStop& b = stops[k];
            const float span = b.t - a.t;
            const float f = span > 0.0f ? (t - a.t) / span : 1.0f;
            auto mix = [f](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(x + (static_cast<float>(y) - x) * f)); };
            return Color(mix(a.color.red(), b.color.red()), mix(a.color.green(), b.color.green()), mix(a.color.blue(), b.color.blue()), mix(a.color.alpha(), b.color.alpha()));
        }
        return stops.back().color;
    }

    float size_at(const Particle& p) const {
        const float t = p.age / p.life;
        if (!m_ease_fn || m_cached_easing != m_config.size_easing) { m_ease_fn = easing_from(m_config.size_easing); m_cached_easing = m_config.size_easing; }
        const double e = m_ease_fn(t);
        return p.size0 + (p.size1 - p.size0) * static_cast<float>(e);
    }

    vector2d world_position(const Particle& p) const noexcept {
        if (!m_config.local_space) return { p.x, p.y };
        const float rad = m_angle * 0.0174532925f;
        const float c = std::cos(rad), s = std::sin(rad);
        return { m_x + p.x * c - p.y * s, m_y + p.x * s + p.y * c };
    }

    static const Texture& default_texture() {
        static const Texture tex = [] {
            const unsigned int n = 32;
            images::BitmapImage img(n, n);
            for (unsigned int y = 0; y < n; ++y) for (unsigned int x = 0; x < n; ++x) {
                const double dx = (x + 0.5) / n * 2.0 - 1.0, dy = (y + 0.5) / n * 2.0 - 1.0;
                const double d = std::sqrt(dx * dx + dy * dy);
                const double a = d >= 1.0 ? 0.0 : std::pow(1.0 - d, 1.5);
                img.set_pixel(x, y, Color(255, 255, 255, static_cast<std::uint8_t>(std::lround(a * 255.0))));
            }
            return Texture(std::move(img), SampleFilter::Bilinear);
        }();
        return tex;
    }

    static const Texture& square_texture() {
        static const Texture tex = [] {
            images::BitmapImage img(2, 2);
            img.change_background(Color(255, 255, 255, 255));
            return Texture(std::move(img));
        }();
        return tex;
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_PARTICLES_2D_HPP
