#ifndef FIZMO_VECTORS_AXES_HPP
#define FIZMO_VECTORS_AXES_HPP

#include <array>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "vectors.hpp"
#include "fast_math.hpp"
#include "../Matrices/square_matrices.hpp"
#include "../Quaternions/quaternion.hpp"

namespace fizmo {

enum class UpAxis : std::uint8_t {
    Y = 0,
    Z = 1
};

enum class Handedness : std::uint8_t {
    Right = 0,
    Left = 1
};

enum class AxesPreset : std::uint8_t {
    YUpRight = 0,
    YUpLeft,
    ZUpRight,
    ZUpLeft
};

enum class YAxis2D : std::uint8_t {
    Up = 0,
    Down = 1
};

class Axes3D {
public:
    static constexpr double STANDARD_GRAVITY = 9.80665;

    constexpr Axes3D() noexcept = default;
    constexpr Axes3D(UpAxis up, Handedness hand) noexcept : m_up(up), m_hand(hand) {}

    static constexpr Axes3D preset(AxesPreset p) noexcept {
        switch (p) {
        case AxesPreset::YUpLeft: return Axes3D(UpAxis::Y, Handedness::Left);
        case AxesPreset::ZUpRight: return Axes3D(UpAxis::Z, Handedness::Right);
        case AxesPreset::ZUpLeft: return Axes3D(UpAxis::Z, Handedness::Left);
        case AxesPreset::YUpRight: break;
        }
        return Axes3D(UpAxis::Y, Handedness::Right);
    }

    static constexpr Axes3D y_up() noexcept { return preset(AxesPreset::YUpRight); }
    static constexpr Axes3D y_up_left() noexcept { return preset(AxesPreset::YUpLeft); }
    static constexpr Axes3D z_up() noexcept { return preset(AxesPreset::ZUpRight); }
    static constexpr Axes3D z_up_left() noexcept { return preset(AxesPreset::ZUpLeft); }

    constexpr UpAxis up_axis() const noexcept { return m_up; }
    constexpr Handedness handedness() const noexcept { return m_hand; }
    constexpr bool mirrored() const noexcept { return m_hand == Handedness::Left; }
    constexpr double sign() const noexcept { return mirrored() ? -1.0 : 1.0; }

    constexpr AxesPreset preset() const noexcept {
        if (m_up == UpAxis::Y) return m_hand == Handedness::Right ? AxesPreset::YUpRight : AxesPreset::YUpLeft;
        return m_hand == Handedness::Right ? AxesPreset::ZUpRight : AxesPreset::ZUpLeft;
    }

    constexpr bool operator==(const Axes3D& o) const noexcept { return m_up == o.m_up && m_hand == o.m_hand; }
    constexpr bool operator!=(const Axes3D& o) const noexcept { return !(*this == o); }

    std::string name() const {
        switch (preset()) {
        case AxesPreset::YUpRight: return "y-up";
        case AxesPreset::YUpLeft: return "y-up-left";
        case AxesPreset::ZUpRight: return "z-up";
        case AxesPreset::ZUpLeft: return "z-up-left";
        }
        return "y-up";
    }

    static std::optional<Axes3D> parse(std::string_view text) noexcept {
        if (text == "y-up" || text == "y-up-right" || text == "opengl" || text == "godot") return y_up();
        if (text == "y-up-left" || text == "unity" || text == "directx") return y_up_left();
        if (text == "z-up" || text == "z-up-right" || text == "blender") return z_up();
        if (text == "z-up-left" || text == "unreal") return z_up_left();
        return std::nullopt;
    }

    linalg::vector3d to_render(const linalg::vector3d& w) const noexcept {
        switch (preset()) {
        case AxesPreset::YUpRight: return w;
        case AxesPreset::YUpLeft: return { w.x, w.y, -w.z };
        case AxesPreset::ZUpRight: return { w.x, w.z, -w.y };
        case AxesPreset::ZUpLeft: return { w.y, w.z, -w.x };
        }
        return w;
    }

    linalg::vector3d from_render(const linalg::vector3d& r) const noexcept {
        switch (preset()) {
        case AxesPreset::YUpRight: return r;
        case AxesPreset::YUpLeft: return { r.x, r.y, -r.z };
        case AxesPreset::ZUpRight: return { r.x, -r.z, r.y };
        case AxesPreset::ZUpLeft: return { -r.z, r.x, r.y };
        }
        return r;
    }

    linalg::Matrix3d to_render_matrix() const noexcept {
        const linalg::vector3d x = to_render({ 1.0, 0.0, 0.0 });
        const linalg::vector3d y = to_render({ 0.0, 1.0, 0.0 });
        const linalg::vector3d z = to_render({ 0.0, 0.0, 1.0 });
        return linalg::Matrix3d{ x.x, y.x, z.x, x.y, y.y, z.y, x.z, y.z, z.z };
    }

    linalg::Matrix3d from_render_matrix() const noexcept { return to_render_matrix().transpose(); }

    linalg::vector3d up() const noexcept { return from_render({ 0.0, 1.0, 0.0 }); }
    linalg::vector3d down() const noexcept { return from_render({ 0.0, -1.0, 0.0 }); }
    linalg::vector3d forward() const noexcept { return from_render({ 0.0, 0.0, -1.0 }); }
    linalg::vector3d back() const noexcept { return from_render({ 0.0, 0.0, 1.0 }); }
    linalg::vector3d right() const noexcept { return from_render({ 1.0, 0.0, 0.0 }); }
    linalg::vector3d left() const noexcept { return from_render({ -1.0, 0.0, 0.0 }); }

    linalg::vector3d gravity(double g = STANDARD_GRAVITY) const noexcept { return down() * g; }
    double height(const linalg::vector3d& p) const noexcept { return fast::dot(p, up()); }
    linalg::vector3d flatten(const linalg::vector3d& v) const noexcept { return v - up() * height(v); }

    linalg::QuatD from_yaw_pitch_roll(double yaw, double pitch, double roll, bool degrees = false) const noexcept {
        if (degrees) {
            yaw *= DEG;
            pitch *= DEG;
            roll *= DEG;
        }
        return world_from_render(render_yaw_pitch_roll(yaw * sign(), pitch, roll * sign()));
    }

    std::array<double, 3> to_yaw_pitch_roll(const linalg::QuatD& q, bool degrees = false) const noexcept {
        const linalg::Matrix3d r = render_from_world_matrix(fast::rotation_matrix(q));
        const auto& d = r.data;
        const double s = clamp1(-d[5]);
        const double pitch = std::asin(s);
        double yaw, roll;
        if (std::fabs(d[5]) < 1.0 - 1e-12) {
            yaw = std::atan2(d[2], d[8]);
            roll = std::atan2(d[3], d[4]);
        } else {
            yaw = std::atan2(-d[6], d[0]);
            roll = 0.0;
        }
        std::array<double, 3> out{ yaw * sign(), pitch, roll * sign() };
        if (degrees) for (double& a : out) a /= DEG;
        return out;
    }

    linalg::QuatD rotation(const linalg::vector3d& axis, double angle, bool degrees = false) const noexcept {
        if (degrees) angle *= DEG;
        return fast::from_axis_angle(axis.x, axis.y, axis.z, angle);
    }

    linalg::QuatD render_rotation(const linalg::QuatD& world) const noexcept {
        return quat_from_matrix(render_from_world_matrix(fast::rotation_matrix(world)));
    }

    linalg::QuatD world_rotation(const linalg::QuatD& render) const noexcept {
        return world_from_render(render);
    }

    linalg::vector3d direction(double yaw, double pitch, bool degrees = false) const noexcept {
        return fast::rotate(from_yaw_pitch_roll(yaw, pitch, 0.0, degrees), forward());
    }

    linalg::vector3d orbit_view(const linalg::vector3d& offset, double yaw, double pitch, bool degrees = false) const noexcept {
        if (degrees) {
            yaw *= DEG;
            pitch *= DEG;
        }
        const linalg::vector3d r = to_render(offset);
        const double cy = std::cos(yaw * sign()), sy = std::sin(yaw * sign());
        const double cp = std::cos(pitch), sp = std::sin(pitch);
        const double x1 = cy * r.x + sy * r.z;
        const double z1 = -sy * r.x + cy * r.z;
        return { x1, cp * r.y - sp * z1, sp * r.y + cp * z1 };
    }

    linalg::vector3d orbit_unview(const linalg::vector3d& view, double yaw, double pitch, bool degrees = false) const noexcept {
        if (degrees) {
            yaw *= DEG;
            pitch *= DEG;
        }
        const double cy = std::cos(yaw * sign()), sy = std::sin(yaw * sign());
        const double cp = std::cos(pitch), sp = std::sin(pitch);
        const double y1 = cp * view.y + sp * view.z;
        const double z1 = -sp * view.y + cp * view.z;
        return from_render({ cy * view.x - sy * z1, y1, sy * view.x + cy * z1 });
    }

    linalg::vector3d orbit_eye(const linalg::vector3d& target, double yaw, double pitch, double distance, bool degrees = false) const noexcept {
        return target + orbit_unview({ 0.0, 0.0, distance }, yaw, pitch, degrees);
    }

private:
    static constexpr double DEG = 0.017453292519943295;

    static double clamp1(double v) noexcept { return v < -1.0 ? -1.0 : (v > 1.0 ? 1.0 : v); }

    static linalg::QuatD render_yaw_pitch_roll(double yaw, double pitch, double roll) noexcept {
        const linalg::QuatD qy = fast::from_axis_angle(0.0, 1.0, 0.0, yaw);
        const linalg::QuatD qx = fast::from_axis_angle(1.0, 0.0, 0.0, pitch);
        const linalg::QuatD qz = fast::from_axis_angle(0.0, 0.0, 1.0, roll);
        return fast::mul(fast::mul(qy, qx), qz);
    }

    linalg::Matrix3d render_from_world_matrix(const linalg::Matrix3d& world) const noexcept {
        const linalg::Matrix3d m = to_render_matrix();
        return fast::mul(fast::mul(m, world), m.transpose());
    }

    linalg::QuatD world_from_render(const linalg::QuatD& render) const noexcept {
        const linalg::Matrix3d m = to_render_matrix();
        return quat_from_matrix(fast::mul(fast::mul(m.transpose(), fast::rotation_matrix(render)), m));
    }

    static linalg::QuatD quat_from_matrix(const linalg::Matrix3d& r) noexcept {
        const auto& d = r.data;
        const double m00 = d[0], m01 = d[1], m02 = d[2];
        const double m10 = d[3], m11 = d[4], m12 = d[5];
        const double m20 = d[6], m21 = d[7], m22 = d[8];
        const double trace = m00 + m11 + m22;
        if (trace > 0.0) {
            const double s = std::sqrt(trace + 1.0) * 2.0;
            return fast::normalize(linalg::QuatD(0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s));
        }
        if (m00 > m11 && m00 > m22) {
            const double s = std::sqrt(1.0 + m00 - m11 - m22) * 2.0;
            return fast::normalize(linalg::QuatD((m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s));
        }
        if (m11 > m22) {
            const double s = std::sqrt(1.0 + m11 - m00 - m22) * 2.0;
            return fast::normalize(linalg::QuatD((m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s));
        }
        const double s = std::sqrt(1.0 + m22 - m00 - m11) * 2.0;
        return fast::normalize(linalg::QuatD((m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s));
    }

    UpAxis m_up = UpAxis::Y;
    Handedness m_hand = Handedness::Right;
};

class Axes2D {
public:
    static constexpr double STANDARD_GRAVITY = 9.80665;

    constexpr Axes2D() noexcept = default;
    constexpr explicit Axes2D(YAxis2D y) noexcept : m_y(y) {}

    static constexpr Axes2D y_up() noexcept { return Axes2D(YAxis2D::Up); }
    static constexpr Axes2D y_down() noexcept { return Axes2D(YAxis2D::Down); }

    constexpr YAxis2D y_axis() const noexcept { return m_y; }
    constexpr bool y_points_down() const noexcept { return m_y == YAxis2D::Down; }
    constexpr double screen_y_sign() const noexcept { return y_points_down() ? 1.0 : -1.0; }

    constexpr bool operator==(const Axes2D& o) const noexcept { return m_y == o.m_y; }
    constexpr bool operator!=(const Axes2D& o) const noexcept { return !(*this == o); }

    std::string name() const { return y_points_down() ? "y-down" : "y-up"; }

    static std::optional<Axes2D> parse(std::string_view text) noexcept {
        if (text == "y-up" || text == "up" || text == "math") return y_up();
        if (text == "y-down" || text == "down" || text == "screen") return y_down();
        return std::nullopt;
    }

    linalg::vector2d up() const noexcept { return { 0.0, y_points_down() ? -1.0 : 1.0 }; }
    linalg::vector2d down() const noexcept { return { 0.0, y_points_down() ? 1.0 : -1.0 }; }
    linalg::vector2d right() const noexcept { return { 1.0, 0.0 }; }
    linalg::vector2d gravity(double g = STANDARD_GRAVITY) const noexcept { return down() * g; }

    linalg::vector2d to_screen_direction(const linalg::vector2d& world) const noexcept { return { world.x, world.y * screen_y_sign() }; }
    double to_screen_angle(double world_angle) const noexcept { return world_angle * screen_y_sign(); }

private:
    YAxis2D m_y = YAxis2D::Up;
};

inline Axes3D& default_axes_storage_3d() noexcept {
    static Axes3D axes;
    return axes;
}

inline Axes2D& default_axes_storage_2d() noexcept {
    static Axes2D axes;
    return axes;
}

inline const Axes3D& default_axes3d() noexcept { return default_axes_storage_3d(); }
inline const Axes2D& default_axes2d() noexcept { return default_axes_storage_2d(); }
inline void set_default_axes(const Axes3D& axes) noexcept { default_axes_storage_3d() = axes; }
inline void set_default_axes(const Axes2D& axes) noexcept { default_axes_storage_2d() = axes; }
inline void set_default_axes(AxesPreset preset) noexcept { default_axes_storage_3d() = Axes3D::preset(preset); }
inline void set_default_axes(YAxis2D y) noexcept { default_axes_storage_2d() = Axes2D(y); }

} // namespace fizmo

#endif // FIZMO_VECTORS_AXES_HPP
