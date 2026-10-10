#include "fizmo_library.hpp"
#include "trail_2d.hpp"

namespace fizmo {
namespace graphics {

auto Trail::lerp(const Color& a, const Color& b, float t) noexcept -> Color {
    auto m = [t](std::uint8_t x, std::uint8_t y) { return static_cast<std::uint8_t>(std::lround(x + (static_cast<float>(y) - x) * t)); };
    return Color(m(a.red(), b.red()), m(a.green(), b.green()), m(a.blue(), b.blue()), m(a.alpha(), b.alpha()));
}

auto Trail::set_texture(const Texture& t, TrailUV mode, float tile_length) -> Trail& { m_texture = t; m_uv_mode = mode; m_tile_length = tile_length > 0.0f ? tile_length : 32.0f; return *this; }

void Trail::add_point(float x, float y, float width_scale) {
    if (!m_points.empty()) {
        const Point& head = m_points.front();
        if (std::hypot(x - head.x, y - head.y) < m_min_distance) {
            if (m_points.size() > 1) { m_points.front().x = x; m_points.front().y = y; m_points.front().width_scale = width_scale; }
            return;
        }
    }
    m_points.push_front({ x, y, 0.0f, width_scale });
    while (m_points.size() > m_max_points) m_points.pop_back();
}

void Trail::update(float dt) {
    for (Point& p : m_points) p.age += dt;
    if (m_lifetime > 0.0f) while (!m_points.empty() && m_points.back().age >= m_lifetime) m_points.pop_back();
}

void Trail::build(std::vector<Vertex2D>& out) {
    out.clear();
    m_length = 0.0f;
    const std::size_t n = m_points.size();
    if (n < 2) return;
    std::vector<float> dist(n, 0.0f);
    for (std::size_t i = 1; i < n; ++i) dist[i] = dist[i - 1] + std::hypot(m_points[i].x - m_points[i - 1].x, m_points[i].y - m_points[i - 1].y);
    m_length = dist[n - 1];
    if (m_length <= 0.0f) return;
    std::vector<Vertex2D> left(n), right(n);

    for (std::size_t i = 0; i < n; ++i) {
        const Point& p = m_points[i];
        const Point& a = m_points[i == 0 ? 0 : i - 1];
        const Point& b = m_points[i + 1 < n ? i + 1 : n - 1];
        float tx = b.x - a.x, ty = b.y - a.y;
        const float tl = std::hypot(tx, ty);
        if (tl > 1e-6f) { tx /= tl; ty /= tl; } else { tx = 1.0f; ty = 0.0f; }
        const float along = dist[i] / m_length;
        const float life = m_lifetime > 0.0f ? std::min(1.0f, p.age / m_lifetime) : 0.0f;
        const float t = std::max(along, life);
        const float half = 0.5f * (m_width_start + (m_width_end - m_width_start) * t) * p.width_scale;
        const Color c = lerp(m_color_start, m_color_end, t);
        const float u = m_uv_mode == TrailUV::Tile ? dist[i] / m_tile_length + m_scroll : along + m_scroll;
        left[i]  = Vertex2D(p.x - ty * half, p.y + tx * half, u, 0.0f, c);
        right[i] = Vertex2D(p.x + ty * half, p.y - tx * half, u, 1.0f, c);
    }

    out.reserve((n - 1) * 6);
    for (std::size_t i = 0; i + 1 < n; ++i) {
        out.push_back(left[i]); out.push_back(right[i]); out.push_back(right[i + 1]);
        out.push_back(left[i]); out.push_back(right[i + 1]); out.push_back(left[i + 1]);
    }
}

} // namespace graphics
} // namespace fizmo
