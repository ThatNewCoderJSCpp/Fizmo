#ifndef FIZMO_TRAIL_2D_HPP
#define FIZMO_TRAIL_2D_HPP

#include "texture.hpp"
#include "draw_types_2d.hpp"
#include <algorithm>
#include <cmath>
#include <deque>
#include <vector>

namespace fizmo {
namespace graphics {

enum class TrailUV : std::uint8_t { Stretch = 0, Tile };

class Trail {
public:
    struct Point {
        float x = 0.0f, y = 0.0f;
        float age = 0.0f;
        float width_scale = 1.0f;
    };

private:
    std::deque<Point> m_points;
    float        m_lifetime     = 0.5f;
    float        m_min_distance = 2.0f;
    std::size_t  m_max_points   = 128;
    float        m_width_start  = 12.0f;
    float        m_width_end    = 0.0f;
    Color        m_color_start  = Color(255, 255, 255, 255);
    Color        m_color_end    = Color(255, 255, 255, 0);
    Texture      m_texture;
    TrailUV      m_uv_mode      = TrailUV::Stretch;
    float        m_tile_length  = 32.0f;
    BlendMode    m_blend        = BlendMode::Normal;
    float        m_scroll       = 0.0f;
    float        m_length       = 0.0f;

    static Color lerp(const Color& a, const Color& b, float t) noexcept;

public:
    Trail() = default;

    Trail& set_lifetime(float seconds) noexcept { m_lifetime = seconds; return *this; }
    Trail& set_min_distance(float d) noexcept { m_min_distance = std::max(0.0f, d); return *this; }
    Trail& set_max_points(std::size_t n) noexcept { m_max_points = std::max<std::size_t>(2, n); return *this; }
    Trail& set_width(float start, float end) noexcept { m_width_start = start; m_width_end = end; return *this; }
    Trail& set_colors(const Color& start, const Color& end) noexcept { m_color_start = start; m_color_end = end; return *this; }
    Trail& set_texture(const Texture& t, TrailUV mode = TrailUV::Stretch, float tile_length = 32.0f);
    Trail& set_blend_mode(BlendMode m) noexcept { m_blend = m; return *this; }
    Trail& set_scroll(float u_offset) noexcept { m_scroll = u_offset; return *this; }

    const Texture& texture() const noexcept { return m_texture; }
    BlendMode blend_mode() const noexcept { return m_blend; }
    const std::deque<Point>& points() const noexcept { return m_points; }
    std::size_t size() const noexcept { return m_points.size(); }
    float length() const noexcept { return m_length; }
    void clear() noexcept { m_points.clear(); m_length = 0.0f; }

    void add_point(float x, float y, float width_scale = 1.0f);

    void update(float dt);

    void build(std::vector<Vertex2D>& out);
};

using Ribbon = Trail;

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_TRAIL_2D_HPP
