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

    void zoom_at(double factor, double world_x, double world_y) noexcept;

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

    vector2d screen_to_world(double sx, double sy) const noexcept;

    vector2d world_to_screen(double wx, double wy) const noexcept;

    struct AABB {
        double min_x, min_y, max_x, max_y;
        double width()  const noexcept { return max_x - min_x; }
        double height() const noexcept { return max_y - min_y; }
        bool contains(double x, double y) const noexcept { return x >= min_x && x <= max_x && y >= min_y && y <= max_y; }
    };

    AABB visible_bounds() const noexcept;

    bool is_visible(double wx, double wy, double radius = 0.0) const noexcept;

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

    void rebuild_if_dirty() const noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA2D_HPP