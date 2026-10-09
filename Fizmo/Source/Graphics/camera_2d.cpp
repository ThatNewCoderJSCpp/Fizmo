#include "fizmo_library.hpp"
#include "camera_2d.hpp"

namespace fizmo {
namespace graphics {

void Camera2D::zoom_at(double factor, double world_x, double world_y) noexcept {
    double new_zoom = m_zoom * factor;
    if (new_zoom <= 0.0) return;
    double ratio = m_zoom / new_zoom;
    m_x = world_x - (world_x - m_x) * ratio;
    m_y = world_y - (world_y - m_y) * ratio;
    m_zoom = new_zoom;
    mark_dirty();
}

auto Camera2D::screen_to_world(double sx, double sy) const noexcept -> vector2d {
    rebuild_if_dirty();
    const auto& m = m_inv_view;
    return {
        m.data[0] * sx + m.data[1] * sy + m.data[2],
        m.data[3] * sx + m.data[4] * sy + m.data[5]
    };
}

auto Camera2D::world_to_screen(double wx, double wy) const noexcept -> vector2d {
    rebuild_if_dirty();
    const auto& m = m_view;
    return {
        m.data[0] * wx + m.data[1] * wy + m.data[2],
        m.data[3] * wx + m.data[4] * wy + m.data[5]
    };
}

auto Camera2D::visible_bounds() const noexcept -> AABB {
    rebuild_if_dirty();
    auto [x0, y0] = screen_to_world(0.0,    0.0);
    auto [x1, y1] = screen_to_world(m_vp_w, 0.0);
    auto [x2, y2] = screen_to_world(m_vp_w, m_vp_h);
    auto [x3, y3] = screen_to_world(0.0,    m_vp_h);
    AABB b;
    b.min_x = min_constexpr(x0, x1, x2, x3);
    b.min_y = min_constexpr(y0, y1, y2, y3);
    b.max_x = max_constexpr(x0, x1, x2, x3);
    b.max_y = max_constexpr(y0, y1, y2, y3);
    return b;
}

bool Camera2D::is_visible(double wx, double wy, double radius) const noexcept {
    AABB b = visible_bounds();
    return wx + radius >= b.min_x && wx - radius <= b.max_x && wy + radius >= b.min_y && wy - radius <= b.max_y;
}

void Camera2D::rebuild_if_dirty() const noexcept {
    if (!m_dirty) return;
    m_dirty = false;
    double cx = m_vp_w * 0.5;
    double cy = m_vp_h * 0.5;
    auto t_neg_pos  = math::Matrix3d::translation_2d(-m_x, -m_y);
    auto r_neg_rot  = math::Matrix3d::rotation_2d(-m_rotation, true);

    auto s_zoom     = math::Matrix3d{
        m_zoom, 0.0,    0.0,
        0.0,    m_zoom, 0.0,
        0.0,    0.0,    1.0
    };

    auto t_center = math::Matrix3d::translation_2d(cx, cy);
    m_view = t_center * s_zoom * r_neg_rot * t_neg_pos;
    double inv_z = 1.0 / m_zoom;
    auto t_pos       = math::Matrix3d::translation_2d(m_x, m_y);
    auto r_pos_rot   = math::Matrix3d::rotation_2d(m_rotation, true);

    auto s_inv_zoom  = math::Matrix3d{
        inv_z, 0.0,   0.0,
        0.0,   inv_z, 0.0,
        0.0,   0.0,   1.0
    };

    auto t_neg_center = math::Matrix3d::translation_2d(-cx, -cy);
    m_inv_view = t_pos * r_pos_rot * s_inv_zoom * t_neg_center;
}

} // namespace graphics
} // namespace fizmo
