#include "fizmo_library.hpp"
#include "particles_2d.hpp"

namespace fizmo {
namespace graphics {

void ParticleEmitter::spawn_one() {
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

void ParticleEmitter::update(float dt) {
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

auto ParticleEmitter::color_at(float t) const noexcept -> Color {
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

float ParticleEmitter::size_at(const Particle& p) const {
    const float t = p.age / p.life;
    if (!m_ease_fn || m_cached_easing != m_config.size_easing) { m_ease_fn = easing_from(m_config.size_easing); m_cached_easing = m_config.size_easing; }
    const double e = m_ease_fn(t);
    return p.size0 + (p.size1 - p.size0) * static_cast<float>(e);
}

auto ParticleEmitter::world_position(const Particle& p) const noexcept -> vector2d {
    if (!m_config.local_space) return { p.x, p.y };
    const float rad = m_angle * 0.0174532925f;
    const float c = std::cos(rad), s = std::sin(rad);
    return { m_x + p.x * c - p.y * s, m_y + p.x * s + p.y * c };
}

auto ParticleEmitter::default_texture() -> const Texture& {
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

auto ParticleEmitter::square_texture() -> const Texture& {
    static const Texture tex = [] {
        images::BitmapImage img(2, 2);
        img.change_background(Color(255, 255, 255, 255));
        return Texture(std::move(img));
    }();
    return tex;
}

} // namespace graphics
} // namespace fizmo
