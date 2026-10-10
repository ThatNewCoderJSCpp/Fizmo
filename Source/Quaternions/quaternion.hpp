#ifndef FIZMO_QUATERNION_HPP
#define FIZMO_QUATERNION_HPP

#include <array>
#include <cmath>
#include <string>
#include <sstream>
#include <ostream>
#include <type_traits>
#include <utility>

#include "../Basic/constants.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "../Standard Overloads/trig.hpp"
#include "../Standard Overloads/abs.hpp"
#include "../Standard Overloads/log.hpp"
#include "../Standard Overloads/pow.hpp"
#include "../Matrices/square_matrices.hpp"
#include "../Vectors/vectors.hpp"

namespace fizmo {

template <typename T = long double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
class Quaternion {
public:
    constexpr Quaternion() noexcept : m_w(0), m_x(0), m_y(0), m_z(0) {}
    constexpr explicit Quaternion(T w, T x = T(0), T y = T(0), T z = T(0)) noexcept : m_w(w), m_x(x), m_y(y), m_z(z) {}
    constexpr Quaternion(const Quaternion&) noexcept = default;
    constexpr Quaternion(Quaternion&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator=(const Quaternion&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator=(Quaternion&&) noexcept = default;

    template <typename U, typename = typename std::enable_if<std::is_arithmetic<U>::value>::type>
    constexpr Quaternion(U val) noexcept : m_w(static_cast<T>(val)), m_x(0), m_y(0), m_z(0) {}

    constexpr T w() const noexcept { return m_w; }
    constexpr T x() const noexcept { return m_x; }
    constexpr T y() const noexcept { return m_y; }
    constexpr T z() const noexcept { return m_z; }
    OPTIONAL_CPP14_CONSTEXPR T& w() noexcept { return m_w; }
    OPTIONAL_CPP14_CONSTEXPR T& x() noexcept { return m_x; }
    OPTIONAL_CPP14_CONSTEXPR T& y() noexcept { return m_y; }
    OPTIONAL_CPP14_CONSTEXPR T& z() noexcept { return m_z; }

    constexpr T magnitude_squared() const noexcept { return m_w*m_w + m_x*m_x + m_y*m_y + m_z*m_z; }
    constexpr T magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr T norm() const noexcept { return magnitude(); }

    constexpr Quaternion conjugate() const noexcept { return Quaternion(m_w, -m_x, -m_y, -m_z); }
    static constexpr Quaternion conjugate(const Quaternion& q) noexcept { return q.conjugate(); }

    constexpr Quaternion inverse() const noexcept {
        const T ms = magnitude_squared();
        if (fizmo::abs_constexpr(ms) <= constants::MIDDLE_EPSILON<T>) return Quaternion(constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>());
        return conjugate() / ms;
    }

    static constexpr Quaternion inverse(const Quaternion& q) noexcept { return q.inverse(); }

    constexpr Quaternion unit() const noexcept {
        const T mag = magnitude();
        if (mag <= constants::MIDDLE_EPSILON<T>) return *this;
        return Quaternion(m_w / mag, m_x / mag, m_y / mag, m_z / mag);
    }

    constexpr Quaternion normalized() const noexcept { return unit(); }

    OPTIONAL_CPP14_CONSTEXPR Quaternion& normalize() noexcept {
        const T mag = magnitude();
        if (mag > constants::MIDDLE_EPSILON<T>) { m_w /= mag; m_x /= mag; m_y /= mag; m_z /= mag; }
        return *this;
    }

    constexpr bool is_unit(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return fizmo::abs_constexpr(magnitude_squared() - T(1)) <= eps; }
    constexpr bool is_zero(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return magnitude_squared() <= eps * eps; }
    constexpr bool is_pure(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return fizmo::abs_constexpr(m_w) <= eps; }
    constexpr bool is_real(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return fizmo::abs_constexpr(m_x) <= eps && fizmo::abs_constexpr(m_y) <= eps && fizmo::abs_constexpr(m_z) <= eps; }
    constexpr bool is_error() const noexcept { return std::isnan(m_w) || std::isnan(m_x) || std::isnan(m_y) || std::isnan(m_z); }
    constexpr bool is_infinite() const noexcept { return std::isinf(m_w) || std::isinf(m_x) || std::isinf(m_y) || std::isinf(m_z); }

    constexpr T dot(const Quaternion& rhs) const noexcept { return m_w*rhs.m_w + m_x*rhs.m_x + m_y*rhs.m_y + m_z*rhs.m_z; }
    static constexpr T dot(const Quaternion& a, const Quaternion& b) noexcept { return a.dot(b); }

    constexpr T scalar_part() const noexcept { return m_w; }
    constexpr Quaternion vector_part() const noexcept { return Quaternion(T(0), m_x, m_y, m_z); }

    constexpr Quaternion natural_log() const noexcept {
        const T mag = magnitude();
        if (mag <= constants::MIDDLE_EPSILON<T>) return Quaternion(constants::quiet_nan<T>());
        const T vec_mag = math::sqrt_constexpr(m_x*m_x + m_y*m_y + m_z*m_z);
        const T log_mag = math::log_constexpr(mag);
        if (vec_mag <= constants::MIDDLE_EPSILON<T>) return Quaternion(log_mag, T(0), T(0), T(0));
        const T scale = math::acos_constexpr(m_w / mag) / vec_mag;
        return Quaternion(log_mag, m_x * scale, m_y * scale, m_z * scale);
    }

    Quaternion natural_exponential() const noexcept {
        const T vec_mag = math::sqrt_constexpr(m_x*m_x + m_y*m_y + m_z*m_z);
        const T exp_w = std::exp(m_w);
        if (vec_mag <= constants::MIDDLE_EPSILON<T>) return Quaternion(exp_w, T(0), T(0), T(0));
        const T scale = exp_w * math::sin_constexpr(vec_mag) / vec_mag;
        return Quaternion(exp_w * math::cos_constexpr(vec_mag), m_x * scale, m_y * scale, m_z * scale);
    }

    Quaternion power(const Quaternion& p) const noexcept { return (natural_log() * p).natural_exponential(); }
    Quaternion power(T t) const noexcept { return (natural_log() * t).natural_exponential(); }

    static constexpr Quaternion from_axis_angle(T ax, T ay, T az, T angle) noexcept {
        const T len = math::sqrt_constexpr(ax*ax + ay*ay + az*az);
        if (len <= constants::MIDDLE_EPSILON<T>) return Quaternion(T(1), T(0), T(0), T(0));
        const T half = angle * T(0.5);
        const T s = math::sin_constexpr(half) / len;
        return Quaternion(math::cos_constexpr(half), ax * s, ay * s, az * s);
    }

    std::array<T, 3> to_axis() const noexcept {
        const T vec_mag = math::sqrt_constexpr(m_x*m_x + m_y*m_y + m_z*m_z);
        if (vec_mag <= constants::MIDDLE_EPSILON<T>) return {T(1), T(0), T(0)};
        return {m_x / vec_mag, m_y / vec_mag, m_z / vec_mag};
    }

    constexpr T to_angle() const noexcept { return T(2) * math::acos_constexpr(m_w / magnitude()); }

    constexpr std::array<T, 9> to_rotation_matrix() const noexcept {
        const Quaternion n = unit();
        const T ww = n.m_w*n.m_w, xx = n.m_x*n.m_x, yy = n.m_y*n.m_y, zz = n.m_z*n.m_z;
        const T wx = n.m_w*n.m_x, wy = n.m_w*n.m_y, wz = n.m_w*n.m_z;
        const T xy = n.m_x*n.m_y, xz = n.m_x*n.m_z, yz = n.m_y*n.m_z;
        return {
            ww+xx-yy-zz,    T(2)*(xy-wz),   T(2)*(xz+wy),
            T(2)*(xy+wz),   ww-xx+yy-zz,    T(2)*(yz-wx),
            T(2)*(xz-wy),   T(2)*(yz+wx),   ww-xx-yy+zz
        };
    }

    static constexpr Quaternion from_euler(T roll, T pitch, T yaw) noexcept {
        const T cr = math::cos_constexpr(roll  * T(0.5)), sr = math::sin_constexpr(roll  * T(0.5));
        const T cp = math::cos_constexpr(pitch * T(0.5)), sp = math::sin_constexpr(pitch * T(0.5));
        const T cy = math::cos_constexpr(yaw   * T(0.5)), sy = math::sin_constexpr(yaw   * T(0.5));
        return Quaternion(
            cr*cp*cy + sr*sp*sy,
            sr*cp*cy - cr*sp*sy,
            cr*sp*cy + sr*cp*sy,
            cr*cp*sy - sr*sp*cy
        );
    }

    std::array<T, 3> to_euler() const noexcept {
        const Quaternion n = unit();
        const T sinr_cosp = T(2)*(n.m_w*n.m_x + n.m_y*n.m_z);
        const T cosr_cosp = T(1) - T(2)*(n.m_x*n.m_x + n.m_y*n.m_y);
        const T sinp = T(2)*(n.m_w*n.m_y - n.m_z*n.m_x);
        const T siny_cosp = T(2)*(n.m_w*n.m_z + n.m_x*n.m_y);
        const T cosy_cosp = T(1) - T(2)*(n.m_y*n.m_y + n.m_z*n.m_z);
        return {
            math::atan2_constexpr(sinr_cosp, cosr_cosp),
            fizmo::abs_constexpr(sinp) >= T(1) ? static_cast<T>(std::copysign(constants::pi() / T(2), sinp)) : math::asin_constexpr(sinp),
            math::atan2_constexpr(siny_cosp, cosy_cosp)
        };
    }

    constexpr Quaternion rotate_vector(T vx, T vy, T vz) const noexcept {
        const Quaternion v(T(0), vx, vy, vz);
        return *this * v * inverse();
    }

    static constexpr Quaternion slerp(const Quaternion& a, const Quaternion& b, T t) noexcept {
        const T d = a.dot(b);
        const Quaternion b2 = d < T(0) ? Quaternion(-b.m_w, -b.m_x, -b.m_y, -b.m_z) : b;
        const T ad = fizmo::abs_constexpr(d);
        if (ad >= T(1) - constants::MIDDLE_EPSILON<T>) return (a * (T(1) - t) + b2 * t).unit();
        const T theta = math::acos_constexpr(ad);
        const T inv_sin = T(1) / math::sin_constexpr(theta);
        return a * (math::sin_constexpr((T(1) - t) * theta) * inv_sin) + b2 * (math::sin_constexpr(t * theta) * inv_sin);
    }

    static constexpr Quaternion nlerp(const Quaternion& a, const Quaternion& b, T t) noexcept {
        const Quaternion b2 = a.dot(b) < T(0) ? Quaternion(-b.m_w, -b.m_x, -b.m_y, -b.m_z) : b;
        return (a * (T(1) - t) + b2 * t).unit();
    }

    constexpr T angle_between(const Quaternion& other) const noexcept {
        const T d = fizmo::abs_constexpr(unit().dot(other.unit()));
        return T(2) * math::acos_constexpr(d < T(1) ? d : T(1));
    }

    constexpr Quaternion operator+(const Quaternion& rhs) const noexcept { return Quaternion(m_w+rhs.m_w, m_x+rhs.m_x, m_y+rhs.m_y, m_z+rhs.m_z); }
    constexpr Quaternion operator-(const Quaternion& rhs) const noexcept { return Quaternion(m_w-rhs.m_w, m_x-rhs.m_x, m_y-rhs.m_y, m_z-rhs.m_z); }
    constexpr Quaternion operator-() const noexcept { return Quaternion(-m_w, -m_x, -m_y, -m_z); }

    constexpr Quaternion operator*(const Quaternion& rhs) const noexcept {
        return Quaternion(
            m_w*rhs.m_w - m_x*rhs.m_x - m_y*rhs.m_y - m_z*rhs.m_z,
            m_w*rhs.m_x + m_x*rhs.m_w + m_y*rhs.m_z - m_z*rhs.m_y,
            m_w*rhs.m_y - m_x*rhs.m_z + m_y*rhs.m_w + m_z*rhs.m_x,
            m_w*rhs.m_z + m_x*rhs.m_y - m_y*rhs.m_x + m_z*rhs.m_w
        );
    }

    constexpr Quaternion operator*(T s) const noexcept { return Quaternion(m_w*s, m_x*s, m_y*s, m_z*s); }
    constexpr Quaternion operator/(T s) const noexcept { return Quaternion(m_w/s, m_x/s, m_y/s, m_z/s); }

    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator+=(const Quaternion& rhs) noexcept { m_w+=rhs.m_w; m_x+=rhs.m_x; m_y+=rhs.m_y; m_z+=rhs.m_z; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator-=(const Quaternion& rhs) noexcept { m_w-=rhs.m_w; m_x-=rhs.m_x; m_y-=rhs.m_y; m_z-=rhs.m_z; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator*=(const Quaternion& rhs) noexcept { return *this = *this * rhs; }
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator*=(T s) noexcept { m_w*=s; m_x*=s; m_y*=s; m_z*=s; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Quaternion& operator/=(T s) noexcept { m_w/=s; m_x/=s; m_y/=s; m_z/=s; return *this; }

    constexpr bool operator==(const Quaternion& rhs) const noexcept {
        return fizmo::abs_constexpr(m_w-rhs.m_w) <= constants::MIDDLE_EPSILON<T> &&
               fizmo::abs_constexpr(m_x-rhs.m_x) <= constants::MIDDLE_EPSILON<T> &&
               fizmo::abs_constexpr(m_y-rhs.m_y) <= constants::MIDDLE_EPSILON<T> &&
               fizmo::abs_constexpr(m_z-rhs.m_z) <= constants::MIDDLE_EPSILON<T>;
    }

    constexpr bool operator!=(const Quaternion& rhs) const noexcept { return !(*this == rhs); }

    constexpr bool approx_equal(const Quaternion& rhs, T eps = constants::MIDDLE_EPSILON<T>) const noexcept {
        return fizmo::abs_constexpr(m_w-rhs.m_w) <= eps && fizmo::abs_constexpr(m_x-rhs.m_x) <= eps &&
               fizmo::abs_constexpr(m_y-rhs.m_y) <= eps && fizmo::abs_constexpr(m_z-rhs.m_z) <= eps;
    }

    friend std::ostream& operator<<(std::ostream& os, const Quaternion& q) {
        const T eps = constants::MIDDLE_EPSILON<T>;
        auto fmt = [&](T v) { std::ostringstream ss; ss.precision(os.precision()); ss.flags(os.flags()); ss << v; return ss.str(); };
        bool any = false;
        auto append = [&](T v, const char* unit) {
            if (fizmo::abs_constexpr(v) <= eps) return;
            if (any) os << (v > T(0) ? " + " : " - ");
            else if (v < T(0)) os << "-";
            const T av = fizmo::abs_constexpr(v);
            if (*unit == '\0' || fizmo::abs_constexpr(av - T(1)) > eps) os << fmt(av);
            os << unit;
            any = true;
        };
        append(q.m_w, ""); append(q.m_x, "i"); append(q.m_y, "j"); append(q.m_z, "k");
        if (!any) os << "0";
        return os;
    }

    std::string stringify(unsigned int precision = 8) const {
        std::ostringstream os;
        os.precision(precision);
        os << *this;
        return os.str();
    }

    std::string to_string() const { return stringify(); }

private:
    T m_w, m_x, m_y, m_z;
};

template <typename T>
constexpr Quaternion<T> operator*(T s, const Quaternion<T>& q) noexcept { return q * s; }

namespace constants {
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Quaternion<T> QUATERNION_IDENTITY(T(1), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Quaternion<T> QUATERNION_I(T(0), T(1), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Quaternion<T> QUATERNION_J(T(0), T(0), T(1), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Quaternion<T> QUATERNION_K(T(0), T(0), T(0), T(1));
}

using QuatF = Quaternion<float>;
using QuatD = Quaternion<double>;
using QuatL = Quaternion<long double>;

template <typename T> struct is_quaternion : std::false_type {};
template <typename T> struct is_quaternion<Quaternion<T>> : std::true_type {};
template <typename T> constexpr bool is_quaternion_v = is_quaternion<T>::value;

inline vector3d quat_rotate(const QuatD& q, const vector3d& v) noexcept {
    double qx = q.x(), qy = q.y(), qz = q.z(), qw = q.w();
    double tx = 2.0 * (qy * v.z - qz * v.y);
    double ty = 2.0 * (qz * v.x - qx * v.z);
    double tz = 2.0 * (qx * v.y - qy * v.x);

    return {
        v.x + qw * tx + (qy * tz - qz * ty),
        v.y + qw * ty + (qz * tx - qx * tz),
        v.z + qw * tz + (qx * ty - qy * tx)
    };
}

inline vector3d quat_rotate_inv(const QuatD& q, const vector3d& v) noexcept {
    return quat_rotate(q.conjugate(), v);
}

inline vector3d mat3_mul(const math::Matrix3d& m, const vector3d& v) noexcept {
    auto r = m * std::array<double, 3>{v.x, v.y, v.z};
    return { r[0], r[1], r[2] };
}

inline math::Matrix3d quat_to_mat3(const QuatD& q) noexcept {
    return math::Matrix3d(q.normalized().to_rotation_matrix());
}

inline math::Matrix3d rotate_inertia(const math::Matrix3d& I, const QuatD& q) noexcept {
    math::Matrix3d R = quat_to_mat3(q);
    return R * I * R.transpose();
}

inline math::Matrix3d skew(const vector3d& v) noexcept {
    return math::Matrix3d::skew_symmetric(v.x, v.y, v.z);
}

} // namespace fizmo

#endif // FIZMO_QUATERNION_HPP