#ifndef FIZMO_VECTORS_FAST_MATH_HPP
#define FIZMO_VECTORS_FAST_MATH_HPP

#include <cmath>
#include <type_traits>

#include "../Quaternions/quaternion.hpp"

namespace fizmo {
namespace fast {

template <class T>
using if_native = typename std::enable_if<std::is_floating_point<T>::value, int>::type;

template <class T, if_native<T> = 0> T dot(const linalg::Vector2D<T>& a, const linalg::Vector2D<T>& b) { return a.x * b.x + a.y * b.y; }
template <class T, if_native<T> = 0> T dot(const linalg::Vector3D<T>& a, const linalg::Vector3D<T>& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
template <class T, if_native<T> = 0> T dot(const linalg::Vector4D<T>& a, const linalg::Vector4D<T>& b) { return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w; }

template <class T, if_native<T> = 0> T perp_dot(const linalg::Vector2D<T>& a, const linalg::Vector2D<T>& b) { return a.x * b.y - a.y * b.x; }

template <class T, if_native<T> = 0>
linalg::Vector3D<T> cross(const linalg::Vector3D<T>& a, const linalg::Vector3D<T>& b) {
    return linalg::Vector3D<T>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

template <class V> auto length_squared(const V& v) -> decltype(dot(v, v)) { return dot(v, v); }
template <class V> auto length(const V& v) -> decltype(dot(v, v)) { return std::sqrt(dot(v, v)); }

template <class V>
V normalize(const V& v) {
    const auto l = length(v);
    return l > decltype(l)(0) ? v / l : v;
}

template <class T, if_native<T> = 0>
T dot(const linalg::Quaternion<T>& a, const linalg::Quaternion<T>& b) { return a.w() * b.w() + a.x() * b.x() + a.y() * b.y() + a.z() * b.z(); }

template <class T, if_native<T> = 0>
linalg::Quaternion<T> normalize(const linalg::Quaternion<T>& q) {
    const T l = std::sqrt(dot(q, q));
    if (!(l > T(0))) return q;
    return linalg::Quaternion<T>(q.w() / l, q.x() / l, q.y() / l, q.z() / l);
}

template <class T, if_native<T> = 0>
linalg::Quaternion<T> mul(const linalg::Quaternion<T>& a, const linalg::Quaternion<T>& b) {
    return linalg::Quaternion<T>(
        a.w() * b.w() - a.x() * b.x() - a.y() * b.y() - a.z() * b.z(),
        a.w() * b.x() + a.x() * b.w() + a.y() * b.z() - a.z() * b.y(),
        a.w() * b.y() - a.x() * b.z() + a.y() * b.w() + a.z() * b.x(),
        a.w() * b.z() + a.x() * b.y() - a.y() * b.x() + a.z() * b.w()
    );
}

template <class T, if_native<T> = 0>
linalg::Quaternion<T> from_axis_angle(T ax, T ay, T az, T angle) {
    const T len = std::sqrt(ax * ax + ay * ay + az * az);
    if (!(len > T(0))) return linalg::Quaternion<T>(T(1));
    const T half = angle * T(0.5);
    const T s = std::sin(half) / len;
    return linalg::Quaternion<T>(std::cos(half), ax * s, ay * s, az * s);
}

template <class T, if_native<T> = 0>
linalg::Quaternion<T> from_euler(T roll, T pitch, T yaw) {
    const T cr = std::cos(roll * T(0.5)), sr = std::sin(roll * T(0.5));
    const T cp = std::cos(pitch * T(0.5)), sp = std::sin(pitch * T(0.5));
    const T cy = std::cos(yaw * T(0.5)), sy = std::sin(yaw * T(0.5));
    return linalg::Quaternion<T>(cr * cp * cy + sr * sp * sy, sr * cp * cy - cr * sp * sy, cr * sp * cy + sr * cp * sy, cr * cp * sy - sr * sp * cy);
}

template <class T, if_native<T> = 0>
linalg::Quaternion<T> nlerp(const linalg::Quaternion<T>& a, const linalg::Quaternion<T>& b, T t) {
    const T s = dot(a, b) < T(0) ? -t : t;
    const T u = T(1) - t;
    return normalize(linalg::Quaternion<T>(a.w() * u + b.w() * s, a.x() * u + b.x() * s, a.y() * u + b.y() * s, a.z() * u + b.z() * s));
}

template <class T, if_native<T> = 0>
linalg::Matrix3x3<T> rotation_matrix(const linalg::Quaternion<T>& q) {
    const linalg::Quaternion<T> n = normalize(q);
    const T w = n.w(), x = n.x(), y = n.y(), z = n.z();
    const T ww = w * w, xx = x * x, yy = y * y, zz = z * z;
    const T xy = x * y, xz = x * z, yz = y * z, wx = w * x, wy = w * y, wz = w * z;
    return linalg::Matrix3x3<T>(
        ww + xx - yy - zz, T(2) * (xy - wz),  T(2) * (xz + wy),
        T(2) * (xy + wz),  ww - xx + yy - zz, T(2) * (yz - wx),
        T(2) * (xz - wy),  T(2) * (yz + wx),  ww - xx - yy + zz
    );
}

template <class T, if_native<T> = 0>
linalg::Vector3D<T> rotate(const linalg::Quaternion<T>& q, const linalg::Vector3D<T>& v) {
    const linalg::Vector3D<T> u(q.x(), q.y(), q.z());
    const linalg::Vector3D<T> t = cross(u, v) * T(2);
    return v + t * q.w() + cross(u, t);
}

template <class T, if_native<T> = 0>
linalg::Matrix3x3<T> mul(const linalg::Matrix3x3<T>& a, const linalg::Matrix3x3<T>& b) {
    linalg::Matrix3x3<T> r;

    for (std::size_t i = 0; i < 3; ++i) {
        for (std::size_t j = 0; j < 3; ++j) {
            r.data[i * 3 + j] = a.data[i * 3] * b.data[j] + a.data[i * 3 + 1] * b.data[3 + j] + a.data[i * 3 + 2] * b.data[6 + j];
        }
    }

    return r;
}

template <class T, if_native<T> = 0>
linalg::Vector3D<T> mul(const linalg::Matrix3x3<T>& m, const linalg::Vector3D<T>& v) {
    const auto& d = m.data;
    return linalg::Vector3D<T>(d[0] * v.x + d[1] * v.y + d[2] * v.z, d[3] * v.x + d[4] * v.y + d[5] * v.z, d[6] * v.x + d[7] * v.y + d[8] * v.z);
}

template <class T, if_native<T> = 0>
linalg::Vector3D<T> mul_transpose(const linalg::Matrix3x3<T>& m, const linalg::Vector3D<T>& v) {
    const auto& d = m.data;
    return linalg::Vector3D<T>(d[0] * v.x + d[3] * v.y + d[6] * v.z, d[1] * v.x + d[4] * v.y + d[7] * v.z, d[2] * v.x + d[5] * v.y + d[8] * v.z);
}

template <class T, if_native<T> = 0>
T trace(const linalg::Matrix3x3<T>& m) { return m.data[0] + m.data[4] + m.data[8]; }

template <class T, if_native<T> = 0>
T determinant(const linalg::Matrix3x3<T>& m) {
    const auto& d = m.data;
    return d[0] * (d[4] * d[8] - d[5] * d[7]) - d[1] * (d[3] * d[8] - d[5] * d[6]) + d[2] * (d[3] * d[7] - d[4] * d[6]);
}

template <class T, if_native<T> = 0>
linalg::Matrix3x3<T> inverse(const linalg::Matrix3x3<T>& m) {
    const T det = determinant(m);
    if (det == T(0) || !std::isfinite(det)) return m.pinverse();
    const auto& d = m.data;
    const T k = T(1) / det;
    return linalg::Matrix3x3<T>(
        (d[4] * d[8] - d[5] * d[7]) * k, (d[2] * d[7] - d[1] * d[8]) * k, (d[1] * d[5] - d[2] * d[4]) * k,
        (d[5] * d[6] - d[3] * d[8]) * k, (d[0] * d[8] - d[2] * d[6]) * k, (d[2] * d[3] - d[0] * d[5]) * k,
        (d[3] * d[7] - d[4] * d[6]) * k, (d[1] * d[6] - d[0] * d[7]) * k, (d[0] * d[4] - d[1] * d[3]) * k
    );
}

template <class T, if_native<T> = 0>
linalg::Matrix3x3<T> rotate_inertia(const linalg::Matrix3x3<T>& inertia, const linalg::Quaternion<T>& q) {
    const linalg::Matrix3x3<T> r = rotation_matrix(q);
    return mul(mul(r, inertia), r.transpose());
}

} // namespace fast
} // namespace fizmo

#endif // FIZMO_VECTORS_FAST_MATH_HPP
