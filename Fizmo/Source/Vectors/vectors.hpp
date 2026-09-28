#ifndef FIZMO_VECTORS_FLOATING_HPP
#define FIZMO_VECTORS_FLOATING_HPP

#include <cmath>
#include <string>
#include <sstream>
#include <ostream>
#include <type_traits>

#include "../Standard Overloads/abs.hpp"
#include "../Basic/constants.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "../Standard Overloads/trig.hpp"
#include "../Matrices/matrix.hpp"

namespace fizmo {

template <std::size_t K, std::size_t N, typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value && (K <= N)>::type>
struct KVector;

template <std::size_t N, typename T = double>
using Bivector = KVector<2, N, T>;

template <std::size_t N, typename T = double>
using Trivector = KVector<3, N, T>;

template <typename TX = double, typename TY = double, typename TZ = double, typename = typename std::enable_if<std::is_floating_point<TX>::value && std::is_floating_point<TY>::value && std::is_floating_point<TZ>::value>::type>
struct Vector3D;

template <typename TX = double, typename TY = double, typename TZ = double, typename TW = double, typename = typename std::enable_if<std::is_floating_point<TX>::value && std::is_floating_point<TY>::value && std::is_floating_point<TZ>::value && std::is_floating_point<TW>::value>::type>
struct Vector4D;

template <typename TX = double, typename TY = double, typename = typename std::enable_if<std::is_floating_point<TX>::value && std::is_floating_point<TY>::value>::type>
struct Vector2D {
public:
    TX x;
    TY y;

public:
    constexpr Vector2D() noexcept : x(TX(0)), y(TY(0)) {}
    constexpr Vector2D(const TX& x_, const TY& y_) noexcept : x(x_), y(y_) {}
    constexpr Vector2D(const Vector2D& v) noexcept = default;
    constexpr Vector2D(Vector2D&& v) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator=(const Vector2D&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator=(Vector2D&&) noexcept = default;
    constexpr explicit operator Vector3D<TX, TY>() const noexcept;
    constexpr explicit operator Vector4D<TX, TY>() const noexcept;

    constexpr static Vector2D from_magnitude_angle(
        const double magnitude,
        const double angle,
        const bool angle_in_degrees = true,
        const bool angle_counterclockwise = true) noexcept
    {
        double rad = angle;
        if (angle_in_degrees) rad *= constants::pi_180();
        if (!angle_counterclockwise) rad = -rad;
        return Vector2D(static_cast<TX>(magnitude * math::cos_constexpr(rad)), static_cast<TY>(magnitude * math::sin_constexpr(rad)));
    }

    constexpr auto magnitude_squared() const noexcept { return x * x + y * y; }
    constexpr auto magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }

    constexpr auto angle_between(const Vector2D& v, const bool return_in_degrees = true) const noexcept {
        const auto rad = math::acos_constexpr(dot(v) / (magnitude() * v.magnitude()));
        return return_in_degrees ? rad * constants::reciprocal_pi_180() : rad;
    }

    constexpr auto dot(const Vector2D& v) const noexcept { return x * v.x + y * v.y; }
    constexpr auto perp_dot(const Vector2D& v) const noexcept { return x * v.y - y * v.x; }
    constexpr Vector3D<TX, TY> cross(const Vector2D& v) const noexcept;
    constexpr Bivector<2, typename std::common_type<TX, TY>::type> wedge(const Vector2D& o) const noexcept;

    constexpr auto scalar_project(const Vector2D& v) const noexcept {
        const auto mag = v.magnitude();
        if (mag == 0) return decltype(dot(v) / mag)(0);
        return dot(v) / mag;
    }

    constexpr Vector2D unit_vector() const noexcept {
        const auto mag = magnitude();
        if (mag == 0) return *this;
        return Vector2D(static_cast<TX>(x / mag), static_cast<TY>(y / mag));
    }

    constexpr Vector2D direction() const noexcept { return unit_vector(); }
    constexpr Vector2D hadamard(const Vector2D& v) const noexcept { return Vector2D(x * v.x, y * v.y); }

    constexpr Vector2D project_onto(const Vector2D& v) const noexcept {
        const auto denom = v.magnitude_squared();
        if (denom == 0) return *this;
        const auto scale = dot(v) / denom;
        return Vector2D(static_cast<TX>(v.x * scale), static_cast<TY>(v.y * scale));
    }

    constexpr bool is_orthogonal(const Vector2D& v, const double eps = constants::middle_epsilon()) const noexcept { return abs_constexpr(dot(v)) <= eps; }
    constexpr bool is_parallel(const Vector2D& v, const double eps = constants::middle_epsilon()) const noexcept { return abs_constexpr(perp_dot(v)) <= eps; }
    constexpr bool is_antiparallel(const Vector2D& v, const double eps = constants::middle_epsilon()) const noexcept { return is_parallel(v, eps) && dot(v) < 0; }
    constexpr Vector2D operator*(const double s) const noexcept { return Vector2D(static_cast<TX>(x * s), static_cast<TY>(y * s)); }
    constexpr Vector2D operator/(const double s) const noexcept { return Vector2D(static_cast<TX>(x / s), static_cast<TY>(y / s)); }
    constexpr Vector2D operator+(const Vector2D& v) const noexcept { return Vector2D(x + v.x, y + v.y); }
    constexpr Vector2D operator-(const Vector2D& v) const noexcept { return Vector2D(x - v.x, y - v.y); }
    constexpr Vector2D operator-() const noexcept { return Vector2D(-x, -y); }
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator+=(const Vector2D& v) noexcept { x += v.x; y += v.y; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator-=(const Vector2D& v) noexcept { x -= v.x; y -= v.y; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator*=(const double s) noexcept { x = static_cast<TX>(x * s); y = static_cast<TY>(y * s); return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector2D& operator/=(const double s) noexcept { x = static_cast<TX>(x / s); y = static_cast<TY>(y / s); return *this; }
    constexpr bool operator==(const Vector2D& v) const noexcept { return x == v.x && y == v.y; }
    constexpr bool operator!=(const Vector2D& v) const noexcept { return !(*this == v); }

    friend std::ostream& operator<<(std::ostream& os, const Vector2D& v) {
        os << "<\n    " << v.x << ",\n    " << v.y << "\n>";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

template <typename TX, typename TY, typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr Vector2D<TX, TY> operator*(const T s, const Vector2D<TX, TY>& v) noexcept { return v * s; }

template <typename TX, typename TY, typename TZ, typename>
struct Vector3D {
public:
    TX x;
    TY y;
    TZ z;

public:
    constexpr Vector3D() noexcept : x(TX(0)), y(TY(0)), z(TZ(0)) {}
    constexpr Vector3D(const TX& x_, const TY& y_, const double z_) noexcept : x(x_), y(y_), z(z_) {}
    constexpr Vector3D(const Vector3D& v) noexcept = default;
    constexpr Vector3D(Vector3D&& v) noexcept = default;
    constexpr Vector3D(const Vector2D<TX, TY>& v) noexcept : x(v.x), y(v.y), z(0.0) {}
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator=(const Vector3D&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator=(Vector3D&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator=(const Vector2D<TX, TY>& v) noexcept { x = v.x; y = v.y; z = 0.0; return *this; }
    constexpr explicit operator Vector4D<TX, TY, TZ>() const noexcept;

    constexpr auto magnitude_squared() const noexcept { return x * x + y * y + z * z; }
    constexpr auto magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr auto dot(const Vector3D& v) const noexcept { return x * v.x + y * v.y + z * v.z; }
    constexpr Bivector<3, typename std::common_type<TX, TY, TZ>::type> wedge(const Vector3D& o) const noexcept;

    constexpr auto angle_between(const Vector3D& v, const bool return_in_degrees = true) const noexcept {
        const auto rad = math::acos_constexpr(dot(v) / (magnitude() * v.magnitude()));
        return return_in_degrees ? rad * (constants::reciprocal_pi_180()) : rad;
    }

    constexpr Vector3D cross(const Vector3D& v) const noexcept {
        return Vector3D(
            static_cast<TX>(y * v.z - z * v.y),
            static_cast<TY>(z * v.x - x * v.z),
            static_cast<double>(x * v.y - y * v.x)
        );
    }

    constexpr auto scalar_project(const Vector3D& v) const noexcept {
        const auto mag = v.magnitude();
        if (mag == 0) return decltype(dot(v) / mag)(0);
        return dot(v) / mag;
    }

    constexpr Vector3D unit_vector() const noexcept {
        const auto mag = magnitude();
        if (mag == 0) return *this;
        return Vector3D(static_cast<TX>(x / mag), static_cast<TY>(y / mag), z / mag);
    }

    constexpr Vector3D direction() const noexcept { return unit_vector(); }
    constexpr Vector3D hadamard(const Vector3D& v) const noexcept { return Vector3D(x * v.x, y * v.y, z * v.z); }

    constexpr Vector3D project_onto(const Vector3D& v) const noexcept {
        const auto denom = v.magnitude_squared();
        if (denom == 0) return *this;
        const auto scale = dot(v) / denom;
        return Vector3D(static_cast<TX>(v.x * scale), static_cast<TY>(v.y * scale), v.z * scale);
    }

    constexpr bool is_orthogonal(const Vector3D& v, const double eps = constants::middle_epsilon()) const noexcept { return abs_constexpr(dot(v)) <= eps; }

    constexpr bool is_parallel(const Vector3D& v, const double eps = constants::middle_epsilon()) const noexcept {
        const Vector3D c = cross(v);
        return abs_constexpr(c.x) <= eps && abs_constexpr(c.y) <= eps && abs_constexpr(c.z) <= eps;
    }

    constexpr bool is_antiparallel(const Vector3D& v, const double eps = 1e-9) const noexcept { return is_parallel(v, eps) && dot(v) < 0; }
    constexpr Vector3D operator*(const double s) const noexcept { return Vector3D(static_cast<TX>(x * s), static_cast<TY>(y * s), z * s); }
    constexpr Vector3D operator/(const double s) const noexcept { return Vector3D(static_cast<TX>(x / s), static_cast<TY>(y / s), z / s); }
    constexpr Vector3D operator+(const Vector3D& v) const noexcept { return Vector3D(x + v.x, y + v.y, z + v.z); }
    constexpr Vector3D operator-(const Vector3D& v) const noexcept { return Vector3D(x - v.x, y - v.y, z - v.z); }
    constexpr Vector3D operator-() const noexcept { return Vector3D(-x, -y, -z); }
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator+=(const Vector3D& v) noexcept { x += v.x; y += v.y; z += v.z; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator-=(const Vector3D& v) noexcept { x -= v.x; y -= v.y; z -= v.z; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator*=(const double s) noexcept { x = static_cast<TX>(x * s); y = static_cast<TY>(y * s); z *= s; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector3D& operator/=(const double s) noexcept { x = static_cast<TX>(x / s); y = static_cast<TY>(y / s); z /= s; return *this; }
    constexpr bool operator==(const Vector3D& v) const noexcept { return x == v.x && y == v.y && z == v.z; }
    constexpr bool operator!=(const Vector3D& v) const noexcept { return !(*this == v); }

    friend std::ostream& operator<<(std::ostream& os, const Vector3D& v) {
        os << "<\n    " << v.x << ",\n    " << v.y << ",\n    " << v.z << "\n>";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

template <typename TX, typename TY, typename TZ, typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr Vector3D<TX, TY, TZ> operator*(const T s, const Vector3D<TX, TY, TZ>& v) noexcept { return v * s; }

template <typename TX, typename TY, typename E>
constexpr Vector2D<TX, TY, E>::operator Vector3D<TX, TY>() const noexcept { return Vector3D<TX, TY>(x, y, 0.0); }

template <typename TX, typename TY, typename E>
constexpr Vector3D<TX, TY> Vector2D<TX, TY, E>::cross(const Vector2D<TX, TY, E>& v) const noexcept {
    return Vector3D<TX, TY>(TX(0), TY(0), perp_dot(v));
}

template <typename TX, typename TY, typename TZ, typename TW, typename>
struct Vector4D {
public:
    TX x;
    TY y;
    TZ z;
    TW w;

public:
    constexpr Vector4D() noexcept : x(TX(0)), y(TY(0)), z(TZ(0)), w(TW(0)) {}
    constexpr Vector4D(const TX& x_, const TY& y_, const double z_, const double w_) noexcept : x(x_), y(y_), z(z_), w(w_) {}
    constexpr Vector4D(const Vector4D& v) noexcept = default;
    constexpr Vector4D(Vector4D&& v) noexcept = default;
    constexpr Vector4D(const Vector3D<TX, TY>& v) noexcept : x(v.x), y(v.y), z(v.z), w(0.0) {}
    constexpr Vector4D(const Vector2D<TX, TY>& v) noexcept : x(v.x), y(v.y), z(0.0), w(0.0) {}
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator=(const Vector4D&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator=(Vector4D&&) noexcept = default;

    constexpr auto magnitude_squared() const noexcept { return x * x + y * y + z * z + w * w; }
    constexpr auto magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr auto dot(const Vector4D& v) const noexcept { return x * v.x + y * v.y + z * v.z + w * v.w; }
    constexpr Bivector<4, typename std::common_type<TX, TY, TZ, TW>::type> wedge(const Vector4D& o) const noexcept;

    constexpr auto angle_between(const Vector4D& v, const bool return_in_degrees = true) const noexcept {
        const auto rad = math::acos_constexpr(dot(v) / (magnitude() * v.magnitude()));
        return return_in_degrees ? rad * constants::reciprocal_pi_180() : rad;
    }

    constexpr auto scalar_project(const Vector4D& v) const noexcept {
        const auto mag = v.magnitude();
        if (mag == 0) return decltype(dot(v) / mag)(0);
        return dot(v) / mag;
    }

    constexpr Vector4D unit_vector() const noexcept {
        const auto mag = magnitude();
        if (mag == 0) return *this;
        return Vector4D(static_cast<TX>(x / mag), static_cast<TY>(y / mag), z / mag, w / mag);
    }

    constexpr Vector4D direction() const noexcept { return unit_vector(); }
    constexpr Vector4D hadamard(const Vector4D& v) const noexcept { return Vector4D(x * v.x, y * v.y, z * v.z, w * v.w); }

    constexpr Vector4D project_onto(const Vector4D& v) const noexcept {
        const auto denom = v.magnitude_squared();
        if (denom == 0) return *this;
        const auto scale = dot(v) / denom;
        return Vector4D(static_cast<TX>(v.x * scale), static_cast<TY>(v.y * scale), v.z * scale, v.w * scale);
    }

    constexpr bool is_orthogonal(const Vector4D& v, const double eps = constants::middle_epsilon()) const noexcept { return abs_constexpr(dot(v)) <= eps; }

    constexpr bool is_parallel(const Vector4D& v, const double eps = constants::middle_epsilon()) const noexcept {
        const auto d = dot(v);
        return abs_constexpr(magnitude_squared() * v.magnitude_squared() - d * d) <= eps;
    }

    constexpr bool is_antiparallel(const Vector4D& v, const double eps = constants::middle_epsilon()) const noexcept { return is_parallel(v, eps) && dot(v) < 0; }

    constexpr Vector4D operator*(const double s) const noexcept { return Vector4D(static_cast<TX>(x * s), static_cast<TY>(y * s), z * s, w * s); }
    constexpr Vector4D operator/(const double s) const noexcept { return Vector4D(static_cast<TX>(x / s), static_cast<TY>(y / s), z / s, w / s); }
    constexpr Vector4D operator+(const Vector4D& v) const noexcept { return Vector4D(x + v.x, y + v.y, z + v.z, w + v.w); }
    constexpr Vector4D operator-(const Vector4D& v) const noexcept { return Vector4D(x - v.x, y - v.y, z - v.z, w - v.w); }
    constexpr Vector4D operator-() const noexcept { return Vector4D(-x, -y, -z, -w); }
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator+=(const Vector4D& v) noexcept { x += v.x; y += v.y; z += v.z; w += v.w; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator-=(const Vector4D& v) noexcept { x -= v.x; y -= v.y; z -= v.z; w -= v.w; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator*=(const double s) noexcept { x = static_cast<TX>(x * s); y = static_cast<TY>(y * s); z *= s; w *= s; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Vector4D& operator/=(const double s) noexcept { x = static_cast<TX>(x / s); y = static_cast<TY>(y / s); z /= s; w /= s; return *this; }
    constexpr bool operator==(const Vector4D& v) const noexcept { return x == v.x && y == v.y && z == v.z && w == v.w; }
    constexpr bool operator!=(const Vector4D& v) const noexcept { return !(*this == v); }

    friend std::ostream& operator<<(std::ostream& os, const Vector4D& v) {
        os << "<\n    " << v.x << ",\n    " << v.y << ",\n    " << v.z << ",\n    " << v.w << "\n>";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

template <typename TX, typename TY, typename E>
constexpr Vector2D<TX, TY, E>::operator Vector4D<TX, TY>() const noexcept {
    return Vector4D<TX, TY>(x, y, 0.0, 0.0);
}

template <typename TX, typename TY, typename TZ, typename E>
constexpr Vector3D<TX, TY, TZ, E>::operator Vector4D<TX, TY, TZ>() const noexcept {
    return Vector4D<TX, TY, TZ>(x, y, z, 0.0);
}

template <typename TX, typename TY, typename TZ, typename TW, typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
constexpr Vector4D<TX, TY, TZ, TW> operator*(const T s, const Vector4D<TX, TY, TZ, TW>& v) noexcept { return v * s; }

template <std::size_t N, typename T = double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
struct VectorN {
public:
    std::array<T, N> data{};

public:
    constexpr VectorN() noexcept = default;
    constexpr explicit VectorN(const std::array<T, N>& arr) noexcept : data(arr) {}
    template <typename... Args, typename = typename std::enable_if<sizeof...(Args) == N>::type>
    constexpr VectorN(Args&&... args) noexcept : data{ static_cast<T>(args)... } {}
    constexpr VectorN(const VectorN&) noexcept = default;
    constexpr VectorN(VectorN&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator=(const VectorN&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator=(VectorN&&) noexcept = default;
    constexpr T& operator[](std::size_t i) noexcept { return data[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data[i]; }
    constexpr T& at(std::size_t i) noexcept { return data[i]; }
    constexpr const T& at(std::size_t i) const noexcept { return data[i]; }
    constexpr T& x() noexcept { static_assert(N >= 1, "No x component"); return data[0]; }
    constexpr const T& x() const noexcept { static_assert(N >= 1, "No x component"); return data[0]; }
    constexpr T& y() noexcept { static_assert(N >= 2, "No y component"); return data[1]; }
    constexpr const T& y() const noexcept { static_assert(N >= 2, "No y component"); return data[1]; }
    constexpr T& z() noexcept { static_assert(N >= 3, "No z component"); return data[2]; }
    constexpr const T& z() const noexcept { static_assert(N >= 3, "No z component"); return data[2]; }
    constexpr T& w() noexcept { static_assert(N >= 4, "No w component"); return data[3]; }
    constexpr const T& w() const noexcept { static_assert(N >= 4, "No w component"); return data[3]; }
    constexpr auto begin() noexcept { return data.begin(); }
    constexpr auto end() noexcept { return data.end(); }
    constexpr auto begin() const noexcept { return data.begin(); }
    constexpr auto end() const noexcept { return data.end(); }
    static constexpr VectorN zero() noexcept { return VectorN{}; }
    static constexpr VectorN filled(T val) noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = val;
        return v;
    }
    static constexpr VectorN basis(std::size_t axis) noexcept {
        VectorN v;
        if (axis < N) v.data[axis] = T(1);
        return v;
    }
    constexpr VectorN operator+(const VectorN& o) const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = data[i] + o.data[i];
        return v;
    }
    constexpr VectorN operator-(const VectorN& o) const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = data[i] - o.data[i];
        return v;
    }
    constexpr VectorN operator-() const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = -data[i];
        return v;
    }
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator+=(const VectorN& o) noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator-=(const VectorN& o) noexcept { return *this = *this - o; }
    constexpr VectorN operator*(T s) const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = data[i] * s;
        return v;
    }
    constexpr VectorN operator/(T s) const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = data[i] / s;
        return v;
    }
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator*=(T s) noexcept { return *this = *this * s; }
    OPTIONAL_CPP14_CONSTEXPR VectorN& operator/=(T s) noexcept { return *this = *this / s; }
    constexpr bool operator==(const VectorN& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const VectorN& o) const noexcept { return !(*this == o); }
    constexpr bool approx_equal(const VectorN& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < N; ++i) if (abs_constexpr(data[i] - o.data[i]) > eps) return false;
        return true;
    }
    constexpr T dot(const VectorN& o) const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < N; ++i) s += data[i] * o.data[i];
        return s;
    }
    constexpr Bivector<N, T> wedge(const VectorN& o) const noexcept;
    constexpr T magnitude_squared() const noexcept { return dot(*this); }
    constexpr T magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr T length() const noexcept { return magnitude(); }
    constexpr VectorN unit_vector() const noexcept {
        const T mag = magnitude();
        if (mag == T(0)) return *this;
        return *this / mag;
    }
    constexpr VectorN direction() const noexcept { return unit_vector(); }
    constexpr VectorN normalized() const noexcept { return unit_vector(); }
    constexpr VectorN hadamard(const VectorN& o) const noexcept {
        VectorN v;
        for (std::size_t i = 0; i < N; ++i) v.data[i] = data[i] * o.data[i];
        return v;
    }
    constexpr T scalar_project(const VectorN& onto) const noexcept {
        const T mag = onto.magnitude();
        return mag == T(0) ? T(0) : dot(onto) / mag;
    }
    constexpr VectorN project_onto(const VectorN& onto) const noexcept {
        const T denom = onto.magnitude_squared();
        if (denom == T(0)) return *this;
        return onto * (dot(onto) / denom);
    }
    constexpr VectorN project_from(const VectorN& onto) const noexcept { return *this - project_onto(onto); }
    constexpr VectorN reflect(const VectorN& n) const noexcept { return *this - n * (T(2) * dot(n)); }
    constexpr T angle_between(const VectorN& o, bool degrees = true) const noexcept {
        const T denom = magnitude() * o.magnitude();
        if (denom == T(0)) return T(0);
        const T cosine = abs_constexpr(dot(o) / denom) <= T(1) ? dot(o) / denom : (dot(o) > T(0) ? T(1) : T(-1));
        const T rad = math::acos_constexpr(cosine);
        return degrees ? rad * constants::reciprocal_pi_180() : rad;
    }
    constexpr bool is_zero(T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < N; ++i) if (abs_constexpr(data[i]) > eps) return false;
        return true;
    }
    constexpr bool is_orthogonal(const VectorN& o, T eps = constants::middle_epsilon()) const noexcept { return abs_constexpr(dot(o)) <= eps; }
    constexpr bool is_parallel(const VectorN& o, T eps = constants::middle_epsilon()) const noexcept {
        const T d = dot(o);
        return abs_constexpr(magnitude_squared() * o.magnitude_squared() - d * d) <= eps;
    }
    constexpr bool is_antiparallel(const VectorN& o, T eps = constants::middle_epsilon()) const noexcept { return is_parallel(o, eps) && dot(o) < T(0); }
    template <std::size_t M = N>
    constexpr typename std::enable_if<M == 3, VectorN>::type
    cross(const VectorN& o) const noexcept {
        return VectorN(
            data[1] * o.data[2] - data[2] * o.data[1],
            data[2] * o.data[0] - data[0] * o.data[2],
            data[0] * o.data[1] - data[1] * o.data[0]
        );
    }
    template <std::size_t M = N>
    constexpr typename std::enable_if<M == 7, VectorN>::type
    cross(const VectorN& o) const noexcept {
        const auto& a = data;
        const auto& b = o.data;
        return VectorN(
            a[1]*b[2] - a[2]*b[1] + a[3]*b[4] - a[4]*b[3] + a[6]*b[5] - a[5]*b[6],
            a[2]*b[0] - a[0]*b[2] + a[3]*b[5] - a[5]*b[3] + a[4]*b[6] - a[6]*b[4],
            a[0]*b[1] - a[1]*b[0] + a[3]*b[6] - a[6]*b[3] + a[5]*b[4] - a[4]*b[5],
            a[0]*b[4] - a[4]*b[0] + a[1]*b[5] - a[5]*b[1] + a[2]*b[6] - a[6]*b[2],
            a[4]*b[0] - a[0]*b[3] + a[2]*b[5] - a[5]*b[2] + a[6]*b[1] - a[1]*b[6],  
            a[0]*b[6] - a[6]*b[0] + a[4]*b[2] - a[2]*b[4] + a[3]*b[5] - a[5]*b[3],  
            a[5]*b[0] - a[0]*b[5] + a[1]*b[3] - a[3]*b[1] + a[4]*b[2] - a[2]*b[4]   
        );
    }
    constexpr T sum() const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < N; ++i) s += data[i];
        return s;
    }
    constexpr T product() const noexcept {
        T p = T(1);
        for (std::size_t i = 0; i < N; ++i) p *= data[i];
        return p;
    }
    static constexpr std::size_t size = N;
    static constexpr std::size_t dims() noexcept { return N; }
    friend std::ostream& operator<<(std::ostream& os, const VectorN& v) {
        os << "<";
        for (std::size_t i = 0; i < N; ++i) {
            os << "\n    " << v.data[i];
            if (i + 1 < N) os << ",";
        }
        os << "\n>";
        return os;
    }
    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

template <typename T>
struct is_math_vector : std::false_type {};

template <typename T>
struct is_real_math_vector : std::false_type {};

template <typename TX, typename TY>
struct is_math_vector<Vector2D<TX, TY>> : std::true_type {};

template <typename TX, typename TY>
struct is_real_math_vector<Vector2D<TX, TY>> : std::true_type {};

template <typename TX, typename TY, typename TZ>
struct is_math_vector<Vector3D<TX, TY, TZ>> : std::true_type {};

template <typename TX, typename TY, typename TZ>
struct is_real_math_vector<Vector3D<TX, TY, TZ>> : std::true_type {};

template <typename TX, typename TY, typename TZ, typename TW>
struct is_math_vector<Vector4D<TX, TY, TZ, TW>> : std::true_type {};

template <typename TX, typename TY, typename TZ, typename TW>
struct is_real_math_vector<Vector4D<TX, TY, TZ, TW>> : std::true_type {};

template <typename T>
inline constexpr bool is_real_math_vector_v = is_real_math_vector<T>::value;

template <typename T>
inline constexpr bool is_math_vector_v = is_math_vector<T>::value;

using vector2f = Vector2D<float, float>;
using vector3f = Vector3D<float, float, float>;
using vector4f = Vector4D<float, float, float, float>;
using vector2d = Vector2D<double>;
using vector3d = Vector3D<double>;
using vector4d = Vector4D<double>;

} // namespace fizmo

#endif // FIZMO_VECTORS_FLOATING_HPP