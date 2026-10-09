#ifndef FIZMO_CAMERA2D_HPP
#define FIZMO_CAMERA2D_HPP

#include "../Matrices/square_matrices.hpp"
#include <cmath>
#include <utility>
#include "../Standard Overloads/min_max.hpp"
#include "../Vectors/vectors.hpp"

namespace fizmo {
namespace graphics {

class Camera2D {
public:
    Camera2D() noexcept = default;
    Camera2D(double x, double y, unsigned int vp_w, unsigned int vp_h) noexcept : m_x(x), m_y(y), m_vp_w(vp_w), m_vp_h(vp_h) { mark_dirty(); }

public:
    unsigned int viewport_width()  const noexcept { return m_vp_w; }
    unsigned int viewport_height() const noexcept { return m_vp_h; }

    void set_viewport_size(unsigned int w, unsigned int h) noexcept {
        if (w != m_vp_w || h != m_vp_h) { m_vp_w = w; m_vp_h = h; mark_dirty(); }
    }

    double x() const noexcept { return m_x; }
    double y() const noexcept { return m_y; }

    void set_x(double x) noexcept { m_x = x; mark_dirty(); }
    void set_y(double y) noexcept { m_y = y; mark_dirty(); }

    void set_position(double x, double y) noexcept { m_x = x; m_y = y; mark_dirty(); }
    void translate(double dx, double dy) noexcept { m_x += dx; m_y += dy; mark_dirty(); }
    double zoom() const noexcept { return m_zoom; }
    void set_zoom(double z) noexcept { if (z > 0.0) { m_zoom = z; mark_dirty(); } }

    void zoom_by(double factor) noexcept {
        double z = m_zoom * factor;
        if (z > 0.0) { m_zoom = z; mark_dirty(); }
    }

    void zoom_at(double factor, double world_x, double world_y) noexcept {
        double new_zoom = m_zoom * factor;
        if (new_zoom <= 0.0) return;
        double ratio = m_zoom / new_zoom;
        m_x = world_x - (world_x - m_x) * ratio;
        m_y = world_y - (world_y - m_y) * ratio;
        m_zoom = new_zoom;
        mark_dirty();
    }

    double rotation() const noexcept { return m_rotation; }
    void set_rotation(double degrees) noexcept { m_rotation = degrees; mark_dirty(); }
    void rotate(double degrees) noexcept { m_rotation += degrees; mark_dirty(); }

    const math::Matrix3d& view_matrix() const noexcept {
        rebuild_if_dirty();
        return m_view;
    }

    const math::Matrix3d& inverse_view_matrix() const noexcept {
        rebuild_if_dirty();
        return m_inv_view;
    }

    vector2d screen_to_world(double sx, double sy) const noexcept {
        rebuild_if_dirty();
        const auto& m = m_inv_view;
        return {
            m.data[0] * sx + m.data[1] * sy + m.data[2],
            m.data[3] * sx + m.data[4] * sy + m.data[5]
        };
    }

    vector2d world_to_screen(double wx, double wy) const noexcept {
        rebuild_if_dirty();
        const auto& m = m_view;
        return {
            m.data[0] * wx + m.data[1] * wy + m.data[2],
            m.data[3] * wx + m.data[4] * wy + m.data[5]
        };
    }

    struct AABB {
        double min_x, min_y, max_x, max_y;
        double width()  const noexcept { return max_x - min_x; }
        double height() const noexcept { return max_y - min_y; }
        bool contains(double x, double y) const noexcept { return x >= min_x && x <= max_x && y >= min_y && y <= max_y; }
    };

    AABB visible_bounds() const noexcept {
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

    bool is_visible(double wx, double wy, double radius = 0.0) const noexcept {
        AABB b = visible_bounds();
        return wx + radius >= b.min_x && wx - radius <= b.max_x && wy + radius >= b.min_y && wy - radius <= b.max_y;
    }

private:
    double m_x        = 0.0;
    double m_y        = 0.0;
    double m_zoom     = 1.0;
    double m_rotation = 0.0;   // degrees
    unsigned int m_vp_w = 0;
    unsigned int m_vp_h = 0;

    mutable math::Matrix3d m_view     = math::Matrix3d::identity();
    mutable math::Matrix3d m_inv_view = math::Matrix3d::identity();
    mutable bool m_dirty = true;

    void mark_dirty() noexcept { m_dirty = true; }

    void rebuild_if_dirty() const noexcept {
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
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA2D_HPP