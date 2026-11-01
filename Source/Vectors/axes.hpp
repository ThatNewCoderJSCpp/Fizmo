#ifndef FIZMO_VECTORS_AXES_HPP
#define FIZMO_VECTORS_AXES_HPP

#include <array>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

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
    ZUpLeft,
    Custom
};

enum class YAxis2D : std::uint8_t {
    Up = 0,
    Down = 1
};

enum class Axis : std::uint8_t {
    X = 0,
    Y = 1,
    Z = 2
};

enum class AxisFlip : std::uint8_t {
    None = 0,
    X = 1,
    Y = 2,
    XY = 3,
    Z = 4,
    XZ = 5,
    YZ = 6,
    XYZ = 7
};

constexpr AxisFlip operator|(AxisFlip a, AxisFlip b) noexcept { return static_cast<AxisFlip>((static_cast<std::uint8_t>(a) | static_cast<std::uint8_t>(b)) & 7u); }
constexpr AxisFlip operator&(AxisFlip a, AxisFlip b) noexcept { return static_cast<AxisFlip>((static_cast<std::uint8_t>(a) & static_cast<std::uint8_t>(b)) & 7u); }
constexpr AxisFlip operator^(AxisFlip a, AxisFlip b) noexcept { return static_cast<AxisFlip>((static_cast<std::uint8_t>(a) ^ static_cast<std::uint8_t>(b)) & 7u); }
constexpr AxisFlip operator~(AxisFlip a) noexcept { return static_cast<AxisFlip>(~static_cast<std::uint8_t>(a) & 7u); }
constexpr AxisFlip& operator|=(AxisFlip& a, AxisFlip b) noexcept { return a = a | b; }
constexpr AxisFlip& operator&=(AxisFlip& a, AxisFlip b) noexcept { return a = a & b; }
constexpr AxisFlip& operator^=(AxisFlip& a, AxisFlip b) noexcept { return a = a ^ b; }

constexpr AxisFlip axis_flip(Axis a) noexcept { return static_cast<AxisFlip>(1u << static_cast<std::uint8_t>(a)); }
constexpr bool has_flip(AxisFlip flips, AxisFlip axis) noexcept { return (flips & axis) != AxisFlip::None; }
constexpr bool has_flip(AxisFlip flips, Axis axis) noexcept { return has_flip(flips, axis_flip(axis)); }
constexpr int flip_count(AxisFlip flips) noexcept {
    return (has_flip(flips, AxisFlip::X) ? 1 : 0) + (has_flip(flips, AxisFlip::Y) ? 1 : 0) + (has_flip(flips, AxisFlip::Z) ? 1 : 0);
}

inline std::string to_string(AxisFlip flips) {
    std::string out;
    if (has_flip(flips, AxisFlip::X)) out += "flip-x";
    if (has_flip(flips, AxisFlip::Y)) out += out.empty() ? "flip-y" : " flip-y";
    if (has_flip(flips, AxisFlip::Z)) out += out.empty() ? "flip-z" : " flip-z";
    return out.empty() ? std::string("none") : out;
}

namespace axes_detail {

inline std::vector<std::string> words(std::string_view text) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : text) {
        if (c == ' ' || c == '\t' || c == ',' || c == '+' || c == '|') {
            if (!cur.empty()) out.push_back(cur);
            cur.clear();
        } else {
            cur += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

inline std::optional<AxisFlip> flip_word(std::string_view w) noexcept {
    if (w == "flip-x" || w == "flip_x" || w == "flipx" || w == "-x" || w == "x") return AxisFlip::X;
    if (w == "flip-y" || w == "flip_y" || w == "flipy" || w == "-y" || w == "y") return AxisFlip::Y;
    if (w == "flip-z" || w == "flip_z" || w == "flipz" || w == "-z" || w == "z") return AxisFlip::Z;
    return std::nullopt;
}

inline double get(const linalg::vector3d& v, int i) noexcept { return i == 0 ? v.x : (i == 1 ? v.y : v.z); }

inline void set(linalg::vector3d& v, int i, double value) noexcept {
    if (i == 0) v.x = value;
    else if (i == 1) v.y = value;
    else v.z = value;
}

} // namespace axes_detail

inline std::optional<AxisFlip> parse_axis_flips(std::string_view text) {
    AxisFlip out = AxisFlip::None;
    for (const std::string& w : axes_detail::words(text)) {
        if (w == "none") continue;
        const std::optional<AxisFlip> f = axes_detail::flip_word(w);
        if (!f) return std::nullopt;
        out ^= *f;
    }
    return out;
}

class Axes3D {
public:
    static constexpr double STANDARD_GRAVITY = 9.80665;

    constexpr Axes3D() noexcept = default;
    constexpr Axes3D(UpAxis up, Handedness hand) noexcept : m_base(base_of(up, hand)) {}
    constexpr explicit Axes3D(AxisFlip flips) noexcept : m_flips(flips & AxisFlip::XYZ) {}
    constexpr explicit Axes3D(AxesPreset base, AxisFlip flips = AxisFlip::None) noexcept
        : m_base(base == AxesPreset::Custom ? AxesPreset::YUpRight : base), m_flips(flips & AxisFlip::XYZ) {}
    constexpr Axes3D(UpAxis up, Handedness hand, AxisFlip flips) noexcept : m_base(base_of(up, hand)), m_flips(flips & AxisFlip::XYZ) {}

    static constexpr Axes3D preset(AxesPreset p) noexcept { return Axes3D(p); }

    static constexpr Axes3D y_up() noexcept { return preset(AxesPreset::YUpRight); }
    static constexpr Axes3D y_up_left() noexcept { return preset(AxesPreset::YUpLeft); }
    static constexpr Axes3D z_up() noexcept { return preset(AxesPreset::ZUpRight); }
    static constexpr Axes3D z_up_left() noexcept { return preset(AxesPreset::ZUpLeft); }

    static constexpr Axes3D standard() noexcept { return y_up(); }
    static constexpr Axes3D opengl() noexcept { return y_up(); }
    static constexpr Axes3D godot() noexcept { return y_up(); }
    static constexpr Axes3D unity() noexcept { return y_up_left(); }
    static constexpr Axes3D directx() noexcept { return y_up_left(); }
    static constexpr Axes3D blender() noexcept { return z_up(); }
    static constexpr Axes3D unreal() noexcept { return z_up_left(); }
    static constexpr Axes3D source() noexcept { return Axes3D(AxesPreset::ZUpLeft, AxisFlip::Y); }

    constexpr AxesPreset base() const noexcept { return m_base; }
    constexpr AxisFlip flips() const noexcept { return m_flips; }
    constexpr bool flipped(Axis axis) const noexcept { return has_flip(m_flips, axis); }
    constexpr bool flipped(AxisFlip axis) const noexcept { return has_flip(m_flips, axis); }

    constexpr Axes3D& set_base(AxesPreset base) noexcept {
        m_base = base == AxesPreset::Custom ? AxesPreset::YUpRight : base;
        return *this;
    }
    constexpr Axes3D& set_flips(AxisFlip flips) noexcept {
        m_flips = flips & AxisFlip::XYZ;
        return *this;
    }
    constexpr Axes3D& flip(AxisFlip axes) noexcept {
        m_flips ^= axes;
        return *this;
    }
    constexpr Axes3D& flip(Axis axis) noexcept { return flip(axis_flip(axis)); }
    constexpr Axes3D& flip_x() noexcept { return flip(AxisFlip::X); }
    constexpr Axes3D& flip_y() noexcept { return flip(AxisFlip::Y); }
    constexpr Axes3D& flip_z() noexcept { return flip(AxisFlip::Z); }
    constexpr Axes3D with_flips(AxisFlip extra) const noexcept { return Axes3D(m_base, m_flips ^ extra); }
    constexpr Axes3D with_base(AxesPreset base) const noexcept { return Axes3D(base, m_flips); }

    constexpr UpAxis up_axis() const noexcept { return (m_base == AxesPreset::ZUpRight || m_base == AxesPreset::ZUpLeft) ? UpAxis::Z : UpAxis::Y; }
    constexpr Handedness handedness() const noexcept { return mirrored() ? Handedness::Left : Handedness::Right; }
    constexpr bool mirrored() const noexcept {
        const bool left = m_base == AxesPreset::YUpLeft || m_base == AxesPreset::ZUpLeft;
        return left != (flip_count(m_flips) % 2 == 1);
    }
    constexpr double sign() const noexcept { return mirrored() ? -1.0 : 1.0; }

    constexpr AxesPreset preset() const noexcept {
        for (AxesPreset p : { AxesPreset::YUpRight, AxesPreset::YUpLeft, AxesPreset::ZUpRight, AxesPreset::ZUpLeft }) {
            if (same_mapping(Axes3D(p))) return p;
        }
        return AxesPreset::Custom;
    }
    constexpr bool is_preset() const noexcept { return preset() != AxesPreset::Custom; }

    constexpr bool operator==(const Axes3D& o) const noexcept { return same_mapping(o); }
    constexpr bool operator!=(const Axes3D& o) const noexcept { return !(*this == o); }

    std::string name() const {
        switch (preset()) {
        case AxesPreset::YUpRight: return "y-up";
        case AxesPreset::YUpLeft: return "y-up-left";
        case AxesPreset::ZUpRight: return "z-up";
        case AxesPreset::ZUpLeft: return "z-up-left";
        case AxesPreset::Custom: break;
        }
        std::string out = base_name(m_base);
        if (flipped(AxisFlip::X)) out += " flip-x";
        if (flipped(AxisFlip::Y)) out += " flip-y";
        if (flipped(AxisFlip::Z)) out += " flip-z";
        return out;
    }

    static std::optional<Axes3D> parse(std::string_view text) {
        const std::vector<std::string> ws = axes_detail::words(text);
        if (ws.empty()) return std::nullopt;
        Axes3D out;
        std::size_t i = 0;
        if (const std::optional<Axes3D> b = parse_base(ws[0])) {
            out = *b;
            i = 1;
        }
        for (; i < ws.size(); ++i) {
            if (ws[i] == "none") continue;
            const std::optional<AxisFlip> f = axes_detail::flip_word(ws[i]);
            if (!f) return std::nullopt;
            out.flip(*f);
        }
        return out;
    }

    linalg::vector3d to_render(const linalg::vector3d& w) const noexcept {
        linalg::vector3d r{ 0.0, 0.0, 0.0 };
        for (int i = 0; i < 3; ++i) axes_detail::set(r, target(i), axis_sign(i) * axes_detail::get(w, i));
        return r;
    }

    linalg::vector3d from_render(const linalg::vector3d& r) const noexcept {
        linalg::vector3d w{ 0.0, 0.0, 0.0 };
        for (int i = 0; i < 3; ++i) axes_detail::set(w, i, axis_sign(i) * axes_detail::get(r, target(i)));
        return w;
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

    static constexpr AxesPreset base_of(UpAxis up, Handedness hand) noexcept {
        if (up == UpAxis::Y) return hand == Handedness::Right ? AxesPreset::YUpRight : AxesPreset::YUpLeft;
        return hand == Handedness::Right ? AxesPreset::ZUpRight : AxesPreset::ZUpLeft;
    }

    static constexpr int base_target(AxesPreset b, int i) noexcept {
        switch (b) {
        case AxesPreset::ZUpRight: return i == 0 ? 0 : (i == 1 ? 2 : 1);
        case AxesPreset::ZUpLeft: return i == 0 ? 2 : (i == 1 ? 0 : 1);
        case AxesPreset::YUpRight:
        case AxesPreset::YUpLeft:
        case AxesPreset::Custom: break;
        }
        return i;
    }

    static constexpr int base_sign(AxesPreset b, int i) noexcept {
        switch (b) {
        case AxesPreset::YUpLeft: return i == 2 ? -1 : 1;
        case AxesPreset::ZUpRight: return i == 1 ? -1 : 1;
        case AxesPreset::ZUpLeft: return i == 0 ? -1 : 1;
        case AxesPreset::YUpRight:
        case AxesPreset::Custom: break;
        }
        return 1;
    }

    constexpr int target(int i) const noexcept { return base_target(m_base, i); }
    constexpr int axis_sign(int i) const noexcept {
        const bool f = has_flip(m_flips, static_cast<AxisFlip>(1u << i));
        return f ? -base_sign(m_base, i) : base_sign(m_base, i);
    }

    constexpr bool same_mapping(const Axes3D& o) const noexcept {
        for (int i = 0; i < 3; ++i) {
            if (target(i) != o.target(i) || axis_sign(i) != o.axis_sign(i)) return false;
        }
        return true;
    }

    static std::string base_name(AxesPreset b) {
        switch (b) {
        case AxesPreset::YUpLeft: return "y-up-left";
        case AxesPreset::ZUpRight: return "z-up";
        case AxesPreset::ZUpLeft: return "z-up-left";
        case AxesPreset::YUpRight:
        case AxesPreset::Custom: break;
        }
        return "y-up";
    }

    static std::optional<Axes3D> parse_base(std::string_view w) noexcept {
        if (w == "y-up" || w == "y-up-right" || w == "opengl" || w == "godot" || w == "standard") return y_up();
        if (w == "y-up-left" || w == "unity" || w == "directx") return y_up_left();
        if (w == "z-up" || w == "z-up-right" || w == "blender") return z_up();
        if (w == "z-up-left" || w == "unreal") return z_up_left();
        if (w == "source") return source();
        return std::nullopt;
    }

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

    AxesPreset m_base = AxesPreset::YUpRight;
    AxisFlip m_flips = AxisFlip::None;
};

class Axes2D {
public:
    static constexpr double STANDARD_GRAVITY = 9.80665;

    constexpr Axes2D() noexcept = default;
    constexpr explicit Axes2D(YAxis2D y) noexcept : m_flips(y == YAxis2D::Down ? AxisFlip::Y : AxisFlip::None) {}
    constexpr explicit Axes2D(AxisFlip flips) noexcept : m_flips(flips & AxisFlip::XY) {}
    constexpr Axes2D(YAxis2D y, AxisFlip flips) noexcept : m_flips((y == YAxis2D::Down ? AxisFlip::Y : AxisFlip::None) ^ (flips & AxisFlip::XY)) {}

    static constexpr Axes2D y_up() noexcept { return Axes2D(YAxis2D::Up); }
    static constexpr Axes2D y_down() noexcept { return Axes2D(YAxis2D::Down); }
    static constexpr Axes2D standard() noexcept { return y_up(); }
    static constexpr Axes2D math() noexcept { return y_up(); }
    static constexpr Axes2D screen() noexcept { return y_down(); }

    constexpr AxisFlip flips() const noexcept { return m_flips; }
    constexpr bool flipped(Axis axis) const noexcept { return has_flip(m_flips, axis); }
    constexpr bool flipped(AxisFlip axis) const noexcept { return has_flip(m_flips, axis); }

    constexpr Axes2D& set_flips(AxisFlip flips) noexcept {
        m_flips = flips & AxisFlip::XY;
        return *this;
    }
    constexpr Axes2D& flip(AxisFlip axes) noexcept {
        m_flips ^= (axes & AxisFlip::XY);
        return *this;
    }
    constexpr Axes2D& flip(Axis axis) noexcept { return flip(axis_flip(axis)); }
    constexpr Axes2D& flip_x() noexcept { return flip(AxisFlip::X); }
    constexpr Axes2D& flip_y() noexcept { return flip(AxisFlip::Y); }
    constexpr Axes2D with_flips(AxisFlip extra) const noexcept { return Axes2D(m_flips ^ extra); }

    constexpr YAxis2D y_axis() const noexcept { return flipped(AxisFlip::Y) ? YAxis2D::Down : YAxis2D::Up; }
    constexpr bool y_points_down() const noexcept { return y_axis() == YAxis2D::Down; }
    constexpr bool x_points_left() const noexcept { return flipped(AxisFlip::X); }
    constexpr bool mirrored() const noexcept { return flip_count(m_flips) % 2 == 1; }
    constexpr double screen_y_sign() const noexcept { return y_points_down() ? 1.0 : -1.0; }
    constexpr double screen_x_sign() const noexcept { return x_points_left() ? -1.0 : 1.0; }

    constexpr bool operator==(const Axes2D& o) const noexcept { return m_flips == o.m_flips; }
    constexpr bool operator!=(const Axes2D& o) const noexcept { return !(*this == o); }

    std::string name() const {
        std::string out = y_points_down() ? "y-down" : "y-up";
        if (x_points_left()) out += " flip-x";
        return out;
    }

    static std::optional<Axes2D> parse(std::string_view text) {
        const std::vector<std::string> ws = axes_detail::words(text);
        if (ws.empty()) return std::nullopt;
        Axes2D out;
        std::size_t i = 0;
        if (ws[0] == "y-up" || ws[0] == "up" || ws[0] == "math") {
            i = 1;
        } else if (ws[0] == "y-down" || ws[0] == "down" || ws[0] == "screen") {
            out = y_down();
            i = 1;
        }
        for (; i < ws.size(); ++i) {
            if (ws[i] == "none") continue;
            const std::optional<AxisFlip> f = axes_detail::flip_word(ws[i]);
            if (!f || *f == AxisFlip::Z) return std::nullopt;
            out.flip(*f);
        }
        return out;
    }

    linalg::vector2d up() const noexcept { return { 0.0, y_points_down() ? -1.0 : 1.0 }; }
    linalg::vector2d down() const noexcept { return { 0.0, y_points_down() ? 1.0 : -1.0 }; }
    linalg::vector2d right() const noexcept { return { screen_x_sign(), 0.0 }; }
    linalg::vector2d left() const noexcept { return { -screen_x_sign(), 0.0 }; }
    linalg::vector2d gravity(double g = STANDARD_GRAVITY) const noexcept { return down() * g; }

    linalg::vector2d to_screen_direction(const linalg::vector2d& world) const noexcept { return { world.x * screen_x_sign(), world.y * screen_y_sign() }; }
    linalg::vector2d from_screen_direction(const linalg::vector2d& screen) const noexcept { return { screen.x * screen_x_sign(), screen.y * screen_y_sign() }; }
    double to_screen_angle(double world_angle) const noexcept { return world_angle * screen_y_sign() * screen_x_sign(); }
    double from_screen_angle(double screen_angle) const noexcept { return screen_angle * screen_y_sign() * screen_x_sign(); }

private:
    AxisFlip m_flips = AxisFlip::None;
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
