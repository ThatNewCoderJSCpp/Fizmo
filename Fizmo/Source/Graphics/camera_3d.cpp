#include "fizmo_library.hpp"
#include "camera_3d.hpp"

namespace fizmo {
namespace graphics {

bool Frustum3D::intersects_aabb(const vector3d& box_min, const vector3d& box_max) const noexcept {
    for (const auto& pl : planes) {
        const vector3d positive{
            pl.normal.x >= 0.0 ? box_max.x : box_min.x,
            pl.normal.y >= 0.0 ? box_max.y : box_min.y,
            pl.normal.z >= 0.0 ? box_max.z : box_min.z
        };
        if (pl.signed_distance(positive) < 0.0) return false;
    }
    return true;
}

Camera3D::Camera3D(const vector3d& position, unsigned int vp_w, unsigned int vp_h,
         double fov_y_degrees, double near_plane, double far_plane) noexcept : m_position(position), m_vp_w(vp_w), m_vp_h(vp_h)
{
    set_perspective(fov_y_degrees, near_plane, far_plane);
}

double Camera3D::aspect() const noexcept {
    return (m_vp_w == 0 || m_vp_h == 0) ? 1.0 : static_cast<double>(m_vp_w) / static_cast<double>(m_vp_h);
}

void Camera3D::move_local(double right_amount, double up_amount, double forward_amount) noexcept {
    rebuild_view_if_dirty();
    m_position += m_right * right_amount + m_up * up_amount + m_forward * forward_amount;
    mark_view_dirty();
}

void Camera3D::move_flat(double forward_amount, double right_amount, double up_amount) noexcept {
    const vector3d ff = flat_forward();
    const vector3d fr = ff.cross(world_up());
    m_position += ff * forward_amount + fr * right_amount + world_up() * up_amount;
    mark_view_dirty();
}

void Camera3D::set_rotation(double yaw_degrees, double pitch_degrees, double roll_degrees) noexcept {
    m_orientation = (axis_angle(world_up(), yaw_degrees)
                   * axis_angle(vector3d{1.0, 0.0, 0.0}, pitch_degrees)
                   * axis_angle(vector3d{0.0, 0.0, 1.0}, roll_degrees)).normalized();
    mark_view_dirty();
}

double Camera3D::pitch() const noexcept {
    rebuild_view_if_dirty();
    return to_degrees(math::asin_constexpr(clamp(m_forward.y, -1.0, 1.0)));
}

double Camera3D::roll() const noexcept {
    rebuild_view_if_dirty();
    const vector3d level_right = flat_forward().cross(world_up());
    const vector3d back = -m_forward;
    return to_degrees(math::atan2_constexpr(level_right.cross(m_right).dot(back), level_right.dot(m_right)));
}

void Camera3D::add_yaw_pitch(double dyaw_degrees, double dpitch_degrees) noexcept {
    const double new_pitch = clamp(pitch() + dpitch_degrees, -m_pitch_limit, m_pitch_limit);
    set_rotation(yaw() + dyaw_degrees, new_pitch, roll());
}

void Camera3D::rotate(const vector3d& world_axis, double degrees) noexcept {
    m_orientation = (axis_angle(world_axis, degrees) * m_orientation).normalized();
    mark_view_dirty();
}

void Camera3D::rotate_local(const vector3d& local_axis, double degrees) noexcept {
    m_orientation = (m_orientation * axis_angle(local_axis, degrees)).normalized();
    mark_view_dirty();
}

void Camera3D::look_in(const vector3d& direction, const vector3d& up_hint) noexcept {
    const double len = direction.magnitude();
    if (!(len > EPS)) return;
    const vector3d f = direction / len;
    vector3d r = f.cross(up_hint);
    double r_len = r.magnitude();

    if (r_len <= EPS) { 
        const vector3d alt = abs_constexpr(f.z) < 0.9 ? vector3d{0.0, 0.0, 1.0} : vector3d{1.0, 0.0, 0.0};
        r = f.cross(alt);
        r_len = r.magnitude();
    }

    r = r / r_len;
    const vector3d u = r.cross(f);
    m_orientation = quat_from_basis(r, u, -f);
    mark_view_dirty();
}

auto Camera3D::flat_forward() const noexcept -> vector3d {
    rebuild_view_if_dirty();
    vector3d ff{m_forward.x, 0.0, m_forward.z};
    if (ff.magnitude_squared() <= EPS) {
        ff = m_forward.y < 0.0 ? vector3d{m_up.x, 0.0, m_up.z} : vector3d{-m_up.x, 0.0, -m_up.z};
    }
    return ff.unit_vector();
}

void Camera3D::set_perspective(double fov_y_degrees, double near_plane, double far_plane) noexcept {
    m_projection = Projection3D::Perspective;
    set_fov_y(fov_y_degrees);
    set_clip_planes(near_plane, far_plane);
    mark_projection_dirty();
}

void Camera3D::set_orthographic(double height, double near_plane, double far_plane) noexcept {
    m_projection = Projection3D::Orthographic;
    set_ortho_height(height);
    set_clip_planes(near_plane, far_plane);
    mark_projection_dirty();
}

double Camera3D::fov_x() const noexcept {
    const double half = to_radians(m_fov_y) * 0.5;
    return to_degrees(2.0 * math::atan2_constexpr(tan_of(half) * aspect(), 1.0));
}

void Camera3D::set_clip_planes(double near_plane, double far_plane) noexcept {
    if (near_plane > 0.0 && far_plane > near_plane) { m_near = near_plane; m_far = far_plane; mark_projection_dirty(); }
}

auto Camera3D::world_to_screen(const vector3d& p) const noexcept -> ScreenPoint3D {
    const vector4d clip = view_projection_matrix() * vector4d{p.x, p.y, p.z, 1.0};
    ScreenPoint3D sp;
    sp.in_front = clip.w > EPS;
    if (abs_constexpr(clip.w) <= EPS) return sp;
    const double inv_w = 1.0 / clip.w;
    const double nx = clip.x * inv_w, ny = clip.y * inv_w, nz = clip.z * inv_w;
    sp.x = (nx * 0.5 + 0.5) * static_cast<double>(m_vp_w);
    sp.y = (0.5 - ny * 0.5) * static_cast<double>(m_vp_h);
    sp.depth = nz;
    sp.on_screen = sp.in_front && nx >= -1.0 && nx <= 1.0 && ny >= -1.0 && ny <= 1.0 && nz >= depth_min() && nz <= 1.0;
    return sp;
}

auto Camera3D::screen_to_ray(double sx, double sy) const noexcept -> geometry::Ray3D {
    rebuild_view_if_dirty();
    const double nx = screen_to_ndc_x(sx), ny = screen_to_ndc_y(sy);

    if (m_projection == Projection3D::Perspective) {
        const double ty = tan_of(to_radians(m_fov_y) * 0.5);
        const vector3d dir = m_right * (nx * ty * aspect()) + m_up * (ny * ty) + m_forward;
        return { m_position, dir.unit_vector() };
    }

    const double hh = m_ortho_height * 0.5, hw = hh * aspect();
    return { m_position + m_right * (nx * hw) + m_up * (ny * hh), m_forward };
}

auto Camera3D::screen_to_world(double sx, double sy, double ndc_depth) const noexcept -> vector3d {
    const vector4d v = inverse_view_projection_matrix() * vector4d{screen_to_ndc_x(sx), screen_to_ndc_y(sy), ndc_depth, 1.0};
    if (abs_constexpr(v.w) <= EPS) return m_position;
    return vector3d{v.x / v.w, v.y / v.w, v.z / v.w};
}

auto Camera3D::quat_from_basis(const vector3d& x, const vector3d& y, const vector3d& z) noexcept -> QuatD {
    const double m00 = x.x, m01 = y.x, m02 = z.x;
    const double m10 = x.y, m11 = y.y, m12 = z.y;
    const double m20 = x.z, m21 = y.z, m22 = z.z;
    const double trace = m00 + m11 + m22;

    if (trace > 0.0) {
        const double s = math::sqrt_constexpr(trace + 1.0) * 2.0;
        return QuatD(0.25 * s, (m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s).normalized();
    }

    if (m00 > m11 && m00 > m22) {
        const double s = math::sqrt_constexpr(1.0 + m00 - m11 - m22) * 2.0;
        return QuatD((m21 - m12) / s, 0.25 * s, (m01 + m10) / s, (m02 + m20) / s).normalized();
    }

    if (m11 > m22) {
        const double s = math::sqrt_constexpr(1.0 + m11 - m00 - m22) * 2.0;
        return QuatD((m02 - m20) / s, (m01 + m10) / s, 0.25 * s, (m12 + m21) / s).normalized();
    }

    const double s = math::sqrt_constexpr(1.0 + m22 - m00 - m11) * 2.0;
    return QuatD((m10 - m01) / s, (m02 + m20) / s, (m12 + m21) / s, 0.25 * s).normalized();
}

void Camera3D::rebuild_view_if_dirty() const noexcept {
    if (!m_view_dirty) return;
    m_view_dirty = false;
    const auto R = m_orientation.to_rotation_matrix(); 
    m_right   = vector3d{R[0], R[3], R[6]};
    m_up      = vector3d{R[1], R[4], R[7]};
    const vector3d back{R[2], R[5], R[8]};
    m_forward = -back;
    const vector3d& p = m_position;

    m_view = math::Matrix4d{
        m_right.x, m_right.y, m_right.z, -m_right.dot(p),
        m_up.x,    m_up.y,    m_up.z,    -m_up.dot(p),
        back.x,    back.y,    back.z,    -back.dot(p),
        0.0,       0.0,       0.0,       1.0
    };

    m_inv_view = math::Matrix4d{
        m_right.x, m_up.x, back.x, p.x,
        m_right.y, m_up.y, back.y, p.y,
        m_right.z, m_up.z, back.z, p.z,
        0.0,       0.0,    0.0,    1.0
    };
}

void Camera3D::rebuild_projection_if_dirty() const noexcept {
    if (!m_proj_dirty) return;
    m_proj_dirty = false;

    const double n = m_near, f = m_far;
    const bool zero_to_one = m_depth_range == DepthRange::ZeroToOne;

    if (m_projection == Projection3D::Perspective) {
        const double sy = 1.0 / tan_of(to_radians(m_fov_y) * 0.5);
        const double sx = sy / aspect();
        const double A = zero_to_one ? f / (n - f)       : (f + n) / (n - f);
        const double B = zero_to_one ? f * n / (n - f)   : 2.0 * f * n / (n - f);

        m_proj = math::Matrix4d{
            sx,  0.0,  0.0, 0.0,
            0.0, sy,   0.0, 0.0,
            0.0, 0.0,  A,   B,
            0.0, 0.0, -1.0, 0.0
        };

        m_inv_proj = math::Matrix4d{
            1.0 / sx, 0.0,      0.0,      0.0,
            0.0,      1.0 / sy, 0.0,      0.0,
            0.0,      0.0,      0.0,     -1.0,
            0.0,      0.0,      1.0 / B,  A / B
        };
    } else {
        const double hh = m_ortho_height * 0.5, hw = hh * aspect();
        const double C = zero_to_one ? -1.0 / (f - n) : -2.0 / (f - n);
        const double D = zero_to_one ? -n / (f - n)   : -(f + n) / (f - n);

        m_proj = math::Matrix4d{
            1.0 / hw, 0.0,      0.0, 0.0,
            0.0,      1.0 / hh, 0.0, 0.0,
            0.0,      0.0,      C,   D,
            0.0,      0.0,      0.0, 1.0
        };

        m_inv_proj = math::Matrix4d{
            hw,  0.0, 0.0,      0.0,
            0.0, hh,  0.0,      0.0,
            0.0, 0.0, 1.0 / C, -D / C,
            0.0, 0.0, 0.0,      1.0
        };
    }
}

void Camera3D::rebuild_combined_if_dirty() const noexcept {
    rebuild_view_if_dirty();
    rebuild_projection_if_dirty();
    if (!m_combined_dirty) return;
    m_combined_dirty = false;
    m_view_proj     = m_proj * m_view;
    m_inv_view_proj = m_inv_view * m_inv_proj;
    extract_frustum();
}

void Camera3D::extract_frustum() const noexcept {
    const auto& m = m_view_proj.data;
    auto row = [&m](std::size_t r, std::size_t c) { return m[r * 4 + c]; };

    auto make = [&](double s0, std::size_t r0, double s1, std::size_t r1) {
        const double a = s0 * row(r0, 0) + s1 * row(r1, 0);
        const double b = s0 * row(r0, 1) + s1 * row(r1, 1);
        const double c = s0 * row(r0, 2) + s1 * row(r1, 2);
        const double d = s0 * row(r0, 3) + s1 * row(r1, 3);
        const double len = math::sqrt_constexpr(a * a + b * b + c * c);
        const double inv = len > EPS ? 1.0 / len : 0.0;
        return Plane3D{ vector3d{a * inv, b * inv, c * inv}, d * inv };
    };

    auto& p = m_frustum.planes;
    p[Frustum3D::Left]   = make(1.0, 3,  1.0, 0);
    p[Frustum3D::Right]  = make(1.0, 3, -1.0, 0);
    p[Frustum3D::Bottom] = make(1.0, 3,  1.0, 1);
    p[Frustum3D::Top]    = make(1.0, 3, -1.0, 1);
    p[Frustum3D::Near]   = m_depth_range == DepthRange::ZeroToOne ? make(0.0, 3, 1.0, 2) : make(1.0, 3, 1.0, 2);
    p[Frustum3D::Far]    = make(1.0, 3, -1.0, 2);
}

} // namespace graphics
} // namespace fizmo
