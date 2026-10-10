#ifndef FIZMO_OCTONION_HPP
#define FIZMO_OCTONION_HPP

#include "quaternion.hpp"

namespace fizmo {

template <typename T = long double, typename = typename std::enable_if<std::is_floating_point<T>::value>::type>
class Octonion {
public:
    constexpr Octonion() noexcept : m_data{} {}
    constexpr explicit Octonion(T e0, T e1=T(0), T e2=T(0), T e3=T(0), T e4=T(0), T e5=T(0), T e6=T(0), T e7=T(0)) noexcept
        : m_data{e0, e1, e2, e3, e4, e5, e6, e7} {}
    constexpr explicit Octonion(const std::array<T, 8>& arr) noexcept : m_data(arr) {}
    constexpr Octonion(const Octonion&) noexcept = default;
    constexpr Octonion(Octonion&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator=(const Octonion&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator=(Octonion&&) noexcept = default;

    constexpr explicit Octonion(const Quaternion<T>& q) noexcept
        : m_data{q.w(), q.x(), q.y(), q.z(), T(0), T(0), T(0), T(0)} {}

    template <typename U, typename = typename std::enable_if<std::is_arithmetic<U>::value>::type>
    constexpr Octonion(U val) noexcept : m_data{static_cast<T>(val), T(0), T(0), T(0), T(0), T(0), T(0), T(0)} {}

    constexpr T operator[](std::size_t i) const noexcept { return m_data[i]; }
    OPTIONAL_CPP14_CONSTEXPR T& operator[](std::size_t i) noexcept { return m_data[i]; }
    constexpr const std::array<T, 8>& data() const noexcept { return m_data; }

    constexpr T magnitude_squared() const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < 8; ++i) s += m_data[i] * m_data[i];
        return s;
    }
    constexpr T magnitude() const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr T norm() const noexcept { return magnitude(); }

    constexpr Octonion conjugate() const noexcept {
        return Octonion(m_data[0], -m_data[1], -m_data[2], -m_data[3], -m_data[4], -m_data[5], -m_data[6], -m_data[7]);
    }

    static constexpr Octonion conjugate(const Octonion& o) noexcept { return o.conjugate(); }

    constexpr Octonion inverse() const noexcept {
        const T ms = magnitude_squared();
        if (fizmo::abs_constexpr(ms) <= constants::MIDDLE_EPSILON<T>)
            return Octonion(constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>(),
                            constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>(), constants::quiet_nan<T>());
        return conjugate() / ms;
    }

    static constexpr Octonion inverse(const Octonion& o) noexcept { return o.inverse(); }

    constexpr Octonion unit() const noexcept {
        const T mag = magnitude();
        if (mag <= constants::MIDDLE_EPSILON<T>) return *this;
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = m_data[i] / mag;
        return Octonion(arr);
    }

    constexpr Octonion normalized() const noexcept { return unit(); }

    OPTIONAL_CPP14_CONSTEXPR Octonion& normalize() noexcept {
        const T mag = magnitude();
        if (mag > constants::MIDDLE_EPSILON<T>) for (auto& v : m_data) v /= mag;
        return *this;
    }

    constexpr bool is_zero(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return magnitude_squared() <= eps * eps; }
    constexpr bool is_unit(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return fizmo::abs_constexpr(magnitude_squared() - T(1)) <= eps; }
    constexpr bool is_pure(T eps = constants::MIDDLE_EPSILON<T>) const noexcept { return fizmo::abs_constexpr(m_data[0]) <= eps; }

    constexpr bool is_error() const noexcept {
        for (std::size_t i = 0; i < 8; ++i) if (std::isnan(m_data[i])) return true;
        return false;
    }

    constexpr bool is_infinite() const noexcept {
        for (std::size_t i = 0; i < 8; ++i) if (std::isinf(m_data[i])) return true;
        return false;
    }

    constexpr T dot(const Octonion& rhs) const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < 8; ++i) s += m_data[i] * rhs.m_data[i];
        return s;
    }

    static constexpr T dot(const Octonion& a, const Octonion& b) noexcept { return a.dot(b); }

    constexpr T scalar_part() const noexcept { return m_data[0]; }

    constexpr Octonion vector_part() const noexcept {
        return Octonion(T(0), m_data[1], m_data[2], m_data[3], m_data[4], m_data[5], m_data[6], m_data[7]);
    }

    constexpr Octonion operator+(const Octonion& rhs) const noexcept {
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = m_data[i] + rhs.m_data[i];
        return Octonion(arr);
    }

    constexpr Octonion operator-(const Octonion& rhs) const noexcept {
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = m_data[i] - rhs.m_data[i];
        return Octonion(arr);
    }

    constexpr Octonion operator-() const noexcept {
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = -m_data[i];
        return Octonion(arr);
    }

    constexpr Octonion operator*(T s) const noexcept {
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = m_data[i] * s;
        return Octonion(arr);
    }

    constexpr Octonion operator/(T s) const noexcept {
        std::array<T, 8> arr{};
        for (std::size_t i = 0; i < 8; ++i) arr[i] = m_data[i] / s;
        return Octonion(arr);
    }

    constexpr Octonion operator*(const Octonion& b) const noexcept {
        const auto& a = m_data;
        const auto& v = b.m_data;
        return Octonion(
            a[0]*v[0] - a[1]*v[1] - a[2]*v[2] - a[3]*v[3] - a[4]*v[4] - a[5]*v[5] - a[6]*v[6] - a[7]*v[7],
            a[0]*v[1] + a[1]*v[0] + a[2]*v[3] - a[3]*v[2] + a[4]*v[5] - a[5]*v[4] - a[6]*v[7] + a[7]*v[6],
            a[0]*v[2] - a[1]*v[3] + a[2]*v[0] + a[3]*v[1] + a[4]*v[6] + a[5]*v[7] - a[6]*v[4] - a[7]*v[5],
            a[0]*v[3] + a[1]*v[2] - a[2]*v[1] + a[3]*v[0] + a[4]*v[7] - a[5]*v[6] + a[6]*v[5] - a[7]*v[4],
            a[0]*v[4] - a[1]*v[5] - a[2]*v[6] - a[3]*v[7] + a[4]*v[0] + a[5]*v[1] + a[6]*v[2] + a[7]*v[3],
            a[0]*v[5] + a[1]*v[4] - a[2]*v[7] + a[3]*v[6] - a[4]*v[1] + a[5]*v[0] - a[6]*v[3] + a[7]*v[2],
            a[0]*v[6] + a[1]*v[7] + a[2]*v[4] - a[3]*v[5] - a[4]*v[2] + a[5]*v[3] + a[6]*v[0] - a[7]*v[1],
            a[0]*v[7] - a[1]*v[6] + a[2]*v[5] + a[3]*v[4] - a[4]*v[3] - a[5]*v[2] + a[6]*v[1] + a[7]*v[0]
        );
    }

    OPTIONAL_CPP14_CONSTEXPR Octonion& operator+=(const Octonion& rhs) noexcept { return *this = *this + rhs; }
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator-=(const Octonion& rhs) noexcept { return *this = *this - rhs; }
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator*=(const Octonion& rhs) noexcept { return *this = *this * rhs; }
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator*=(T s) noexcept { for (auto& v : m_data) v *= s; return *this; }
    OPTIONAL_CPP14_CONSTEXPR Octonion& operator/=(T s) noexcept { for (auto& v : m_data) v /= s; return *this; }

    constexpr bool operator==(const Octonion& rhs) const noexcept {
        for (std::size_t i = 0; i < 8; ++i) if (fizmo::abs_constexpr(m_data[i] - rhs.m_data[i]) > constants::MIDDLE_EPSILON<T>) return false;
        return true;
    }

    constexpr bool operator!=(const Octonion& rhs) const noexcept { return !(*this == rhs); }

    constexpr bool approx_equal(const Octonion& rhs, T eps = constants::MIDDLE_EPSILON<T>) const noexcept {
        for (std::size_t i = 0; i < 8; ++i) if (fizmo::abs_constexpr(m_data[i] - rhs.m_data[i]) > eps) return false;
        return true;
    }

    constexpr Quaternion<T> lo() const noexcept { return Quaternion<T>(m_data[0], m_data[1], m_data[2], m_data[3]); }
    constexpr Quaternion<T> hi() const noexcept { return Quaternion<T>(m_data[4], m_data[5], m_data[6], m_data[7]); }

    static constexpr Octonion from_quaternion_pair(const Quaternion<T>& q0, const Quaternion<T>& q1) noexcept {
        return Octonion(q0.w(), q0.x(), q0.y(), q0.z(), q1.w(), q1.x(), q1.y(), q1.z());
    }

    constexpr Octonion natural_log() const noexcept {
        const T mag = magnitude();
        if (mag <= constants::MIDDLE_EPSILON<T>) return Octonion(constants::quiet_nan<T>());
        const T log_mag = math::log_constexpr(mag);
        T vec_mag_sq = T(0);
        for (std::size_t i = 1; i < 8; ++i) vec_mag_sq += m_data[i] * m_data[i];
        const T vec_mag = math::sqrt_constexpr(vec_mag_sq);
        if (vec_mag <= constants::MIDDLE_EPSILON<T>) return Octonion(log_mag, T(0), T(0), T(0), T(0), T(0), T(0), T(0));
        const T scale = math::acos_constexpr(m_data[0] / mag) / vec_mag;
        return Octonion(log_mag, m_data[1]*scale, m_data[2]*scale, m_data[3]*scale, m_data[4]*scale, m_data[5]*scale, m_data[6]*scale, m_data[7]*scale);
    }

    Octonion natural_exponential() const noexcept {
        T vec_mag_sq = T(0);
        for (std::size_t i = 1; i < 8; ++i) vec_mag_sq += m_data[i] * m_data[i];
        const T vec_mag = math::sqrt_constexpr(vec_mag_sq);
        const T exp_w = std::exp(m_data[0]);
        if (vec_mag <= constants::MIDDLE_EPSILON<T>) return Octonion(exp_w, T(0), T(0), T(0), T(0), T(0), T(0), T(0));
        const T scale = exp_w * math::sin_constexpr(vec_mag) / vec_mag;
        return Octonion(exp_w * math::cos_constexpr(vec_mag),
            m_data[1]*scale, m_data[2]*scale, m_data[3]*scale,
            m_data[4]*scale, m_data[5]*scale, m_data[6]*scale, m_data[7]*scale);
    }

    Octonion power(T t) const noexcept { return (natural_log() * t).natural_exponential(); }

    static constexpr Octonion slerp(const Octonion& a, const Octonion& b, T t) noexcept {
        const T d = a.dot(b);
        const Octonion b2 = d < T(0) ? -b : b;
        const T ad = fizmo::abs_constexpr(d);
        if (ad >= T(1) - constants::MIDDLE_EPSILON<T>) return (a * (T(1)-t) + b2 * t).unit();
        const T theta = math::acos_constexpr(ad);
        const T inv_sin = T(1) / math::sin_constexpr(theta);
        return a * (math::sin_constexpr((T(1)-t)*theta) * inv_sin) + b2 * (math::sin_constexpr(t*theta) * inv_sin);
    }

    friend std::ostream& operator<<(std::ostream& os, const Octonion& o) {
        static constexpr const char* units[] = {"", "e1", "e2", "e3", "e4", "e5", "e6", "e7"};
        const T eps = constants::MIDDLE_EPSILON<T>;
        auto fmt = [&](T v) { std::ostringstream ss; ss.precision(os.precision()); ss.flags(os.flags()); ss << v; return ss.str(); };
        bool any = false;
        for (std::size_t i = 0; i < 8; ++i) {
            if (fizmo::abs_constexpr(o.m_data[i]) <= eps) continue;
            if (any) os << (o.m_data[i] > T(0) ? " + " : " - ");
            else if (o.m_data[i] < T(0)) os << "-";
            const T av = fizmo::abs_constexpr(o.m_data[i]);
            if (i == 0 || fizmo::abs_constexpr(av - T(1)) > eps) os << fmt(av);
            os << units[i];
            any = true;
        }
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
    std::array<T, 8> m_data;
};

template <typename T>
constexpr Octonion<T> operator*(T s, const Octonion<T>& o) noexcept { return o * s; }

namespace constants {
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_IDENTITY(T(1), T(0), T(0), T(0), T(0), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E1(T(0), T(1), T(0), T(0), T(0), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E2(T(0), T(0), T(1), T(0), T(0), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E3(T(0), T(0), T(0), T(1), T(0), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E4(T(0), T(0), T(0), T(0), T(1), T(0), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E5(T(0), T(0), T(0), T(0), T(0), T(1), T(0), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E6(T(0), T(0), T(0), T(0), T(0), T(0), T(1), T(0));
    template <typename T = long double> OPTIONAL_CPP17_INLINE constexpr Octonion<T> OCTONION_E7(T(0), T(0), T(0), T(0), T(0), T(0), T(0), T(1));
}

template <typename T> struct is_octonion : std::false_type {};
template <typename T> struct is_octonion<Octonion<T>> : std::true_type {};
template <typename T> constexpr bool is_octonion_v = is_octonion<T>::value;

template <typename T> struct is_fizmo_hypercomplex : std::integral_constant<bool, is_quaternion_v<T> || is_octonion_v<T>> {};
template <typename T> constexpr bool is_fizmo_hypercomplex_v = is_fizmo_hypercomplex<T>::value;

} // namespace fizmo

constexpr fizmo::Quaternion<long double> operator"" _qi(long double v) noexcept { return fizmo::Quaternion<long double>(0, v, 0, 0); }
constexpr fizmo::Quaternion<long double> operator"" _qj(long double v) noexcept { return fizmo::Quaternion<long double>(0, 0, v, 0); }
constexpr fizmo::Quaternion<long double> operator"" _qk(long double v) noexcept { return fizmo::Quaternion<long double>(0, 0, 0, v); }
constexpr fizmo::Quaternion<long double> operator"" _qi(unsigned long long v) noexcept { return fizmo::Quaternion<long double>(0, static_cast<long double>(v), 0, 0); }
constexpr fizmo::Quaternion<long double> operator"" _qj(unsigned long long v) noexcept { return fizmo::Quaternion<long double>(0, 0, static_cast<long double>(v), 0); }
constexpr fizmo::Quaternion<long double> operator"" _qk(unsigned long long v) noexcept { return fizmo::Quaternion<long double>(0, 0, 0, static_cast<long double>(v)); }

namespace std {
    template <typename T> string to_string(const fizmo::Quaternion<T>& q) { return q.stringify(); }
    template <typename T> string to_string(const fizmo::Octonion<T>& o) { return o.stringify(); }
}

#endif // FIZMO_QUATERNION_OCTONION_HPP