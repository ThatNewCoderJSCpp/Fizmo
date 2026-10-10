#ifndef FIZMO_VECTORS_KVECTOR_HPP
#define FIZMO_VECTORS_KVECTOR_HPP

#include <array>
#include <cstddef>
#include <type_traits>
#include <ostream>
#include <sstream>
#include <string>

#include "../Basic/constants.hpp"
#include "../Standard Overloads/abs.hpp"
#include "../Standard Overloads/sqrt.hpp"
#include "vectors.hpp"

namespace fizmo {

namespace detail {

constexpr std::size_t binomial(std::size_t n, std::size_t k) noexcept {
    if (k > n) return 0;
    if (k == 0 || k == n) return 1;
    if (k > n - k) k = n - k;
    std::size_t r = 1;
    for (std::size_t i = 0; i < k; ++i) r = r * (n - i) / (i + 1);
    return r;
}

constexpr std::size_t bivector_index(std::size_t i, std::size_t j, std::size_t N) noexcept { return i * (2 * N - i - 1) / 2 + (j - i - 1); }

constexpr std::size_t trivector_index(std::size_t i, std::size_t j, std::size_t k, std::size_t N) noexcept {
    std::size_t idx = 0;
    for (std::size_t a = 0; a < i; ++a) idx += binomial(N - a - 1, 2);
    for (std::size_t b = i + 1; b < j; ++b) idx += (N - b - 1);
    idx += (k - j - 1);
    return idx;
}

template <std::size_t K>
constexpr std::size_t kvector_index(const std::array<std::size_t, K>& idxs, std::size_t N) noexcept {
    std::size_t index = 0;
    for (std::size_t a = 0; a < K; ++a) { index += binomial(N - idxs[a] - 1, K - a - 1); }
    return index;
}

} // namespace detail

template <std::size_t K, std::size_t N, typename T, typename>
struct KVector {
    static constexpr std::size_t grade          = K;
    static constexpr std::size_t dimension      = N;
    static constexpr std::size_t num_components = detail::binomial(N, K);
    std::array<T, num_components> data{};

public:
    constexpr KVector() noexcept = default;
    constexpr explicit KVector(const std::array<T, num_components>& arr) noexcept : data(arr) {}

    template <std::size_t C = num_components, typename = typename std::enable_if<C == 1>::type>
    constexpr explicit KVector(T val) noexcept : data{val} {}

    constexpr KVector(const KVector&) noexcept = default;
    constexpr KVector(KVector&&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR KVector& operator=(const KVector&) noexcept = default;
    OPTIONAL_CPP14_CONSTEXPR KVector& operator=(KVector&&) noexcept = default;

public:
    constexpr T&       operator[](std::size_t i)       noexcept { return data[i]; }
    constexpr const T& operator[](std::size_t i) const noexcept { return data[i]; }

    template <std::size_t G = K>
    constexpr typename std::enable_if<G == 2, T&>::type
    at(std::size_t i, std::size_t j) noexcept {
        return data[detail::bivector_index(i, j, N)];
    }

    template <std::size_t G = K>
    constexpr typename std::enable_if<G == 2, const T&>::type
    at(std::size_t i, std::size_t j) const noexcept {
        return data[detail::bivector_index(i, j, N)];
    }

    template <std::size_t G = K>
    constexpr typename std::enable_if<G == 3, T&>::type
    at(std::size_t i, std::size_t j, std::size_t k) noexcept {
        return data[detail::trivector_index(i, j, k, N)];
    }

    template <std::size_t G = K>
    constexpr typename std::enable_if<G == 3, const T&>::type
    at(std::size_t i, std::size_t j, std::size_t k) const noexcept {
        return data[detail::trivector_index(i, j, k, N)];
    }

    template <typename... Indices>
    constexpr const T& at(Indices... is) const noexcept {
        static_assert(sizeof...(Indices) == K, "KVector::at() requires exactly K indices");
        std::array<std::size_t, K> idxs{ static_cast<std::size_t>(is)... };
        for (std::size_t a = 1; a < K; ++a) if (idxs[a] <= idxs[a - 1]) throw "KVector::at(): indices must be strictly increasing";
        const std::size_t flat = detail::kvector_index<K>(idxs, N);
        return data[flat];
    }

    template <typename... Indices>
    constexpr T& at(Indices... is) noexcept {
        static_assert(sizeof...(Indices) == K, "KVector::at() requires exactly K indices");
        std::array<std::size_t, K> idxs{ static_cast<std::size_t>(is)... };
        for (std::size_t a = 1; a < K; ++a) if (idxs[a] <= idxs[a - 1]) throw "KVector::at(): indices must be strictly increasing";
        const std::size_t flat = detail::kvector_index<K>(idxs, N);
        return data[flat];
    }

    template <std::size_t C = num_components>
    constexpr typename std::enable_if<C == 1, T>::type
    scalar() const noexcept { return data[0]; }

    constexpr T dot(const KVector& o) const noexcept {
        T s = T(0);
        for (std::size_t i = 0; i < num_components; ++i) s += data[i] * o.data[i];
        return s;
    }

    constexpr T magnitude_squared() const noexcept { return dot(*this); }
    constexpr T magnitude()         const noexcept { return math::sqrt_constexpr(magnitude_squared()); }
    constexpr T norm()              const noexcept { return magnitude(); }

    constexpr KVector unit() const noexcept {
        const T mag = magnitude();
        if (mag == T(0)) return *this;
        return *this / mag;
    }

    constexpr bool is_zero(T eps = constants::middle_epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < num_components; ++i) if (abs_constexpr(data[i]) > eps) return false;
        return true;
    }

    constexpr bool approx_equal(const KVector& o, T eps = constants::epsilon<T>()) const noexcept {
        for (std::size_t i = 0; i < num_components; ++i) if (abs_constexpr(data[i] - o.data[i]) > eps) return false;
        return true;
    }

    constexpr KVector operator+(const KVector& o) const noexcept {
        KVector r;
        for (std::size_t i = 0; i < num_components; ++i) r.data[i] = data[i] + o.data[i];
        return r;
    }

    constexpr KVector operator-(const KVector& o) const noexcept {
        KVector r;
        for (std::size_t i = 0; i < num_components; ++i) r.data[i] = data[i] - o.data[i];
        return r;
    }

    constexpr KVector operator-() const noexcept {
        KVector r;
        for (std::size_t i = 0; i < num_components; ++i) r.data[i] = -data[i];
        return r;
    }

    constexpr KVector operator*(T s) const noexcept {
        KVector r;
        for (std::size_t i = 0; i < num_components; ++i) r.data[i] = data[i] * s;
        return r;
    }

    constexpr KVector operator/(T s) const noexcept {
        KVector r;
        for (std::size_t i = 0; i < num_components; ++i) r.data[i] = data[i] / s;
        return r;
    }

    OPTIONAL_CPP14_CONSTEXPR KVector& operator+=(const KVector& o) noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR KVector& operator-=(const KVector& o) noexcept { return *this = *this - o; }
    OPTIONAL_CPP14_CONSTEXPR KVector& operator*=(T s)              noexcept { return *this = *this * s; }
    OPTIONAL_CPP14_CONSTEXPR KVector& operator/=(T s)              noexcept { return *this = *this / s; }
    constexpr bool operator==(const KVector& o) const noexcept { return data == o.data; }
    constexpr bool operator!=(const KVector& o) const noexcept { return !(*this == o); }
    constexpr auto begin()       noexcept { return data.begin(); }
    constexpr auto end()         noexcept { return data.end(); }
    constexpr auto begin() const noexcept { return data.begin(); }
    constexpr auto end()   const noexcept { return data.end(); }

    friend std::ostream& operator<<(std::ostream& os, const KVector& kv) {
        os << "KVector<" << K << "," << N << ">{";
        for (std::size_t i = 0; i < num_components; ++i) {
            if (i > 0) os << ", ";
            os << kv.data[i];
        }
        os << "}";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }

    static constexpr std::size_t size = num_components;
    static constexpr std::size_t dims() noexcept { return N; }
};

template <std::size_t K, std::size_t N, typename T>
constexpr KVector<K, N, T> operator*(T s, const KVector<K, N, T>& kv) noexcept { return kv * s; }

template <typename TX, typename TY, typename E>
constexpr Bivector<2, typename std::common_type<TX, TY>::type>
Vector2D<TX, TY, E>::wedge(const Vector2D& o) const noexcept {
    using R = typename std::common_type<TX, TY>::type;
    return Bivector<2, R>({ R(x * o.y - y * o.x) });
}

template <typename TX, typename TY, typename TZ, typename E>
constexpr Bivector<3, typename std::common_type<TX, TY, TZ>::type>
Vector3D<TX, TY, TZ, E>::wedge(const Vector3D& o) const noexcept {
    using R = typename std::common_type<TX, TY, double>::type;
    return Bivector<3, R>({
        R(x * o.y - y * o.x),   // 0,1
        R(x * o.z - z * o.x),   // 0,2
        R(y * o.z - z * o.y)    // 1,2
    });
}

template <typename TX, typename TY, typename TZ, typename TW, typename E>
constexpr Bivector<4, typename std::common_type<TX, TY, TZ, TW>::type>
Vector4D<TX, TY, TZ, TW, E>::wedge(const Vector4D& o) const noexcept {
    using R = typename std::common_type<TX, TY, TZ, TW>::type;
    return Bivector<4, R>({
        R(x * o.y - y * o.x),   // 0,1
        R(x * o.z - z * o.x),   // 0,2
        R(x * o.w - w * o.x),   // 0,3
        R(y * o.z - z * o.y),   // 1,2
        R(y * o.w - w * o.y),   // 1,3
        R(z * o.w - w * o.z)    // 2,3
    });
}

template <std::size_t N, typename T, typename E>
constexpr Bivector<N, T>
VectorN<N, T, E>::wedge(const VectorN& o) const noexcept {
    Bivector<N, T> result;
    std::size_t idx = 0;
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t j = i + 1; j < N; ++j)
            result[idx++] = data[i] * o.data[j] - data[j] * o.data[i];
    return result;
}

template <typename T>
struct is_kvector : std::false_type {};

template <std::size_t K, std::size_t N, typename T, typename E>
struct is_kvector<KVector<K, N, T, E>> : std::true_type {};

template <typename X, typename Y>
struct is_kvector<Vector2D<X, Y>> : std::true_type {};

template <typename X, typename Y>
struct is_kvector<Vector3D<X, Y>> : std::true_type {};

template <typename T>
inline constexpr bool is_kvector_v = is_kvector<T>::value;

} // namespace fizmo

#endif // FIZMO_VECTORS_KVECTOR_HPP