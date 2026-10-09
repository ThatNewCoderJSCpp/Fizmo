#ifndef FIZMO_CAMERA_CONTROLLER_2D_HPP
#define FIZMO_CAMERA_CONTROLLER_2D_HPP

#include "camera_2d.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <functional>

namespace fizmo {
namespace graphics {

struct WorldRect {
    double x = 0.0, y = 0.0, w = 0.0, h = 0.0;
    bool empty() const noexcept { return w <= 0.0 || h <= 0.0; }
};

class CameraController2D {
private:
    Camera2D*                      m_camera = nullptr;
    std::function<vector2d()>      m_target_fn;
    double                         m_tx = 0.0, m_ty = 0.0;
    double                         m_prev_tx = 0.0, m_prev_ty = 0.0;
    bool                           m_has_target = false;
    bool                           m_first = true;
    double                         m_x = 0.0, m_y = 0.0;
    double                         m_half_life = 0.12;
    double                         m_dead_w = 0.0, m_dead_h = 0.0;
    double                         m_lookahead = 0.0;
    double                         m_lookahead_half_life = 0.25;
    double                         m_look_x = 0.0, m_look_y = 0.0;
    double                         m_offset_x = 0.0, m_offset_y = 0.0;
    WorldRect                      m_bounds;
    double                         m_zoom_target = 1.0;
    double                         m_zoom_half_life = 0.15;
    double                         m_rotation = 0.0;
    double                         m_trauma = 0.0;
    double                         m_trauma_decay = 1.2;
    double                         m_shake_offset = 12.0;
    double                         m_shake_angle = 3.0;
    double                         m_shake_frequency = 18.0;
    double                         m_shake_time = 0.0;
    std::uint32_t                  m_seed = 0x2545F491u;
    double                         m_shake_x = 0.0, m_shake_y = 0.0, m_shake_rot = 0.0;

    static double blend(double half_life, double dt) noexcept {
        if (half_life <= 0.0) return 1.0;
        return 1.0 - std::exp2(-dt / half_life);
    }

    static double hash(std::int64_t i, std::uint32_t seed) noexcept {
        std::uint32_t x = static_cast<std::uint32_t>(i) * 0x9E3779B1u ^ seed;
        x ^= x >> 16; x *= 0x7FEB352Du; x ^= x >> 15; x *= 0x846CA68Bu; x ^= x >> 16;
        return static_cast<double>(x) / 4294967295.0 * 2.0 - 1.0;
    }

    static double noise(double t, std::uint32_t seed) noexcept {
        const double f = std::floor(t);
        const double u = t - f;
        const double s = u * u * (3.0 - 2.0 * u);
        const std::int64_t i = static_cast<std::int64_t>(f);
        return hash(i, seed) * (1.0 - s) + hash(i + 1, seed) * s;
    }

    void half_extents(double& hx, double& hy) const noexcept {
        const double z = m_camera && m_camera->zoom() > 0.0 ? m_camera->zoom() : 1.0;
        hx = (m_camera ? m_camera->viewport_width() : 0) * 0.5 / z;
        hy = (m_camera ? m_camera->viewport_height() : 0) * 0.5 / z;
    }

    void clamp_to_bounds(double& x, double& y) const noexcept {
        if (m_bounds.empty()) return;
        double hx = 0.0, hy = 0.0;
        half_extents(hx, hy);
        if (m_bounds.w <= 2.0 * hx) x = m_bounds.x + m_bounds.w * 0.5;
        else x = std::max(m_bounds.x + hx, std::min(m_bounds.x + m_bounds.w - hx, x));
        if (m_bounds.h <= 2.0 * hy) y = m_bounds.y + m_bounds.h * 0.5;
        else y = std::max(m_bounds.y + hy, std::min(m_bounds.y + m_bounds.h - hy, y));
    }

public:
    CameraController2D() = default;
    explicit CameraController2D(Camera2D& camera) : m_camera(&camera), m_x(camera.x()), m_y(camera.y()), m_zoom_target(camera.zoom()), m_rotation(camera.rotation()) {}

    void attach(Camera2D& camera) { m_camera = &camera; m_x = camera.x(); m_y = camera.y(); m_zoom_target = camera.zoom(); m_rotation = camera.rotation(); }
    Camera2D* camera() const noexcept { return m_camera; }

    CameraController2D& follow(double x, double y) noexcept { m_tx = x; m_ty = y; m_has_target = true; return *this; }
    CameraController2D& follow(std::function<vector2d()> fn) { m_target_fn = std::move(fn); m_has_target = static_cast<bool>(m_target_fn); return *this; }
    CameraController2D& stop_following() noexcept { m_has_target = false; m_target_fn = nullptr; return *this; }
    CameraController2D& set_smoothing(double half_life_seconds) noexcept { m_half_life = std::max(0.0, half_life_seconds); return *this; }
    CameraController2D& set_deadzone(double w, double h) noexcept { m_dead_w = std::max(0.0, w); m_dead_h = std::max(0.0, h); return *this; }
    CameraController2D& set_lookahead(double seconds, double half_life = 0.25) noexcept { m_lookahead = seconds; m_lookahead_half_life = half_life; return *this; }
    CameraController2D& set_offset(double x, double y) noexcept { m_offset_x = x; m_offset_y = y; return *this; }
    CameraController2D& set_bounds(const WorldRect& r) noexcept { m_bounds = r; return *this; }
    CameraController2D& clear_bounds() noexcept { m_bounds = {}; return *this; }
    CameraController2D& set_zoom(double z, double half_life = 0.15) noexcept { if (z > 0.0) m_zoom_target = z; m_zoom_half_life = half_life; return *this; }
    CameraController2D& set_rotation(double degrees) noexcept { m_rotation = degrees; return *this; }
    CameraController2D& set_shake(double max_offset, double max_angle_degrees, double frequency, double decay_per_second) noexcept {
        m_shake_offset = max_offset; m_shake_angle = max_angle_degrees; m_shake_frequency = frequency; m_trauma_decay = decay_per_second; return *this;
    }

    void add_trauma(double amount) noexcept { m_trauma = std::max(0.0, std::min(1.0, m_trauma + amount)); }
    void shake(double intensity) noexcept { add_trauma(intensity); }
    double trauma() const noexcept { return m_trauma; }
    double zoom_target() const noexcept { return m_zoom_target; }

    vector2d focus() const noexcept { return { m_x, m_y }; }
    vector2d shake_offset() const noexcept { return { m_shake_x, m_shake_y }; }

    void zoom_to_fit(const WorldRect& r, double padding = 0.0) noexcept {
        if (!m_camera || r.empty() || m_camera->viewport_width() == 0 || m_camera->viewport_height() == 0) return;
        const double zx = m_camera->viewport_width() / (r.w + 2.0 * padding);
        const double zy = m_camera->viewport_height() / (r.h + 2.0 * padding);
        m_zoom_target = std::min(zx, zy);
        m_tx = r.x + r.w * 0.5;
        m_ty = r.y + r.h * 0.5;
        m_has_target = true;
    }

    void snap() noexcept {
        if (!m_camera) return;
        if (m_target_fn) { const vector2d t = m_target_fn(); m_tx = t.x; m_ty = t.y; }
        m_x = m_tx + m_offset_x;
        m_y = m_ty + m_offset_y;
        m_prev_tx = m_tx;
        m_prev_ty = m_ty;
        m_look_x = m_look_y = 0.0;
        m_camera->set_zoom(m_zoom_target);
        clamp_to_bounds(m_x, m_y);
        m_camera->set_position(m_x, m_y);
        m_first = false;
    }

    void update(double dt) noexcept {
        if (!m_camera || dt <= 0.0) return;
        if (m_target_fn) { const vector2d t = m_target_fn(); m_tx = t.x; m_ty = t.y; }
        if (m_first && m_has_target) snap();

        const double z = m_camera->zoom() + (m_zoom_target - m_camera->zoom()) * blend(m_zoom_half_life, dt);
        if (z > 0.0) m_camera->set_zoom(z);

        if (m_has_target) {
            const double vx = (m_tx - m_prev_tx) / dt, vy = (m_ty - m_prev_ty) / dt;
            m_prev_tx = m_tx;
            m_prev_ty = m_ty;
            const double k = blend(m_lookahead_half_life, dt);
            m_look_x += (vx * m_lookahead - m_look_x) * k;
            m_look_y += (vy * m_lookahead - m_look_y) * k;
            double gx = m_tx + m_offset_x + m_look_x, gy = m_ty + m_offset_y + m_look_y;
            double desired_x = m_x, desired_y = m_y;
            const double hw = m_dead_w * 0.5, hh = m_dead_h * 0.5;
            if (gx > m_x + hw) desired_x = gx - hw; else if (gx < m_x - hw) desired_x = gx + hw;
            if (gy > m_y + hh) desired_y = gy - hh; else if (gy < m_y - hh) desired_y = gy + hh;
            const double b = blend(m_half_life, dt);
            m_x += (desired_x - m_x) * b;
            m_y += (desired_y - m_y) * b;
        }

        clamp_to_bounds(m_x, m_y);
        m_shake_time += dt;
        const double amount = m_trauma * m_trauma;
        const double t = m_shake_time * m_shake_frequency;
        m_shake_x = m_shake_offset * amount * noise(t, m_seed);
        m_shake_y = m_shake_offset * amount * noise(t, m_seed ^ 0xA5A5A5A5u);
        m_shake_rot = m_shake_angle * amount * noise(t, m_seed ^ 0x5A5A5A5Au);
        m_trauma = std::max(0.0, m_trauma - m_trauma_decay * dt);
        m_camera->set_position(m_x + m_shake_x, m_y + m_shake_y);
        m_camera->set_rotation(m_rotation + m_shake_rot);
    }
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA_CONTROLLER_2D_HPP
