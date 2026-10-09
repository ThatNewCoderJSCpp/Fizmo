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

    static double hash(std::int64_t i, std::uint32_t seed) noexcept;

    static double noise(double t, std::uint32_t seed) noexcept;

    void half_extents(double& hx, double& hy) const noexcept;

    void clamp_to_bounds(double& x, double& y) const noexcept;

public:
    CameraController2D() = default;
    explicit CameraController2D(Camera2D& camera);

    void attach(Camera2D& camera);
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
    CameraController2D& set_shake(double max_offset, double max_angle_degrees, double frequency, double decay_per_second) noexcept;

    void add_trauma(double amount) noexcept { m_trauma = std::max(0.0, std::min(1.0, m_trauma + amount)); }
    void shake(double intensity) noexcept { add_trauma(intensity); }
    double trauma() const noexcept { return m_trauma; }
    double zoom_target() const noexcept { return m_zoom_target; }

    vector2d focus() const noexcept { return { m_x, m_y }; }
    vector2d shake_offset() const noexcept { return { m_shake_x, m_shake_y }; }

    void zoom_to_fit(const WorldRect& r, double padding = 0.0) noexcept;

    void snap() noexcept;

    void update(double dt) noexcept;
};

} // namespace graphics
} // namespace fizmo

#endif // FIZMO_CAMERA_CONTROLLER_2D_HPP
