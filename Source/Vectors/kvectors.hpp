#ifndef FIZMO_VECTORS_KVECTOR_HPP
#define FIZMO_VECTORS_KVECTOR_HPP

#include <array>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>

#include "vectors.hpp"

namespace fizmo {
namespace linalg {

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
    std::size_t prev = 0;

    for (std::size_t a = 0; a < K; ++a) {
        const std::size_t start = a == 0 ? 0 : prev + 1;
        for (std::size_t v = start; v < idxs[a]; ++v) index += binomial(N - v - 1, K - a - 1);
        prev = idxs[a];
    }

    return index;
}

} // namespace detail

namespace detail {

template <class Derived, std::size_t K, std::size_t N, class S>
struct KVectorImpl : VectorOps<Derived, detail::binomial(N, K), S> {
    static_assert(K <= N, "KVector grade cannot exceed the dimension");

    static constexpr std::size_t grade          = K;
    static constexpr std::size_t dimension      = N;
    static constexpr std::size_t num_components = detail::binomial(N, K);

private:
    using Base = VectorOps<Derived, num_components, S>;
    using F    = scalar::field<S>;

public:
    using context_type = typename Base::context_type;

    std::array<S, num_components> data;

    KVectorImpl() { data.fill(F::zero()); }
    explicit KVectorImpl(const std::array<S, num_components>& arr) : data(arr) {}

    template <class U, std::size_t C = num_components, typename std::enable_if<C == 1 && is_scalar_operand<S, U>::value, int>::type = 0>
    explicit KVectorImpl(const U& val) : data{ { F::from(val) } } {}

    S&       component(std::size_t i) { return data[i]; }
    const S& component(std::size_t i) const { return data[i]; }

    template <class... Indices>
    S& at(Indices... is) { return data[flat_index(is...)]; }

    template <class... Indices>
    const S& at(Indices... is) const { return data[flat_index(is...)]; }

    template <std::size_t C = num_components, typename std::enable_if<C == 1, int>::type = 0>
    const S& scalar() const { return data[0]; }

    Derived unit(const context_type& c = F::default_context()) const { return this->unit_vector(c); }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    friend std::ostream& operator<<(std::ostream& os, const Derived& kv) {
        os << "KVector<" << K << "," << N << ">{";

        for (std::size_t i = 0; i < num_components; ++i) {
            if (i > 0) os << ", ";
            write_scalar(os, kv.data[i]);
        }

        os << "}";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << static_cast<const Derived&>(*this);
        return os.str();
    }

    static constexpr std::size_t size = num_components;
    static constexpr std::size_t dims() noexcept { return N; }

private:
    template <class... Indices>
    static std::size_t flat_index(Indices... is) {
        static_assert(sizeof...(Indices) == K, "KVector::at() requires exactly K indices");
        const std::array<std::size_t, K> idxs{ { static_cast<std::size_t>(is)... } };

        for (std::size_t a = 0; a < K; ++a) {
            if (idxs[a] >= N) throw std::out_of_range("KVector::at(): index out of range");
            if (a > 0 && idxs[a] <= idxs[a - 1]) throw std::invalid_argument("KVector::at(): indices must be strictly increasing");
        }

        return detail::kvector_index<K>(idxs, N);
    }
};

template <class S, std::size_t N, class Out>
Out wedge_components(const S* a, const S* b, const typename scalar::field<S>::context_type& c) {
    using F = scalar::field<S>;
    Out out;
    std::size_t idx = 0;

    for (std::size_t i = 0; i < N; ++i) {
        for (std::size_t j = i + 1; j < N; ++j) out.data[idx++] = F::diff_of_products(a[i], b[j], a[j], b[i], c);
    }

    return out;
}

template <class Derived, class S>
typename family<S>::template KV<2, 2> Vector2DImpl<Derived, S>::wedge(const Derived& v, const context_type& c) const {
    const std::array<S, 2> a = this->to_array();
    const std::array<S, 2> b = v.to_array();
    return wedge_components<S, 2, typename family<S>::template KV<2, 2>>(a.data(), b.data(), c);
}

template <class Derived, class S>
typename family<S>::template KV<2, 3> Vector3DImpl<Derived, S>::wedge(const Derived& v, const context_type& c) const {
    const std::array<S, 3> a = this->to_array();
    const std::array<S, 3> b = v.to_array();
    return wedge_components<S, 3, typename family<S>::template KV<2, 3>>(a.data(), b.data(), c);
}

template <class Derived, class S>
typename family<S>::template KV<2, 4> Vector4DImpl<Derived, S>::wedge(const Derived& v, const context_type& c) const {
    const std::array<S, 4> a = this->to_array();
    const std::array<S, 4> b = v.to_array();
    return wedge_components<S, 4, typename family<S>::template KV<2, 4>>(a.data(), b.data(), c);
}

template <class Derived, std::size_t N, class S>
typename family<S>::template KV<2, N> VectorNImpl<Derived, N, S>::wedge(const Derived& o, const context_type& c) const {
    return wedge_components<S, N, typename family<S>::template KV<2, N>>(this->data.data(), o.data.data(), c);
}

} // namespace detail

template <std::size_t K, std::size_t N, class T>
struct KVector : detail::KVectorImpl<KVector<K, N, T>, K, N, T> {
    static_assert(scalar::is_real<T>::value, "KVector requires a standard or fixed-width floating-point type");
    using detail::KVectorImpl<KVector<K, N, T>, K, N, T>::KVectorImpl;

    KVector() = default;
};

template <std::size_t K, std::size_t N>
struct BigKVector : detail::KVectorImpl<BigKVector<K, N>, K, N, scalar::BigFloat> {
    using detail::KVectorImpl<BigKVector<K, N>, K, N, scalar::BigFloat>::KVectorImpl;

    BigKVector() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigKVector(const KVector<K, N, T>& v) {
        for (std::size_t i = 0; i < this->num_components; ++i) this->data[i] = scalar::BigFloat(v.data[i]);
    }

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    KVector<K, N, T> to() const {
        KVector<K, N, T> out;
        for (std::size_t i = 0; i < this->num_components; ++i) out.data[i] = scalar::from_big<T>(this->data[i]);
        return out;
    }
};

template <class T>
struct is_kvector : std::false_type {};

template <std::size_t K, std::size_t N, class T>
struct is_kvector<KVector<K, N, T>> : std::true_type {};

template <std::size_t K, std::size_t N>
struct is_kvector<BigKVector<K, N>> : std::true_type {};

template <class T> struct is_kvector<Vector2D<T>> : std::true_type {};
template <class T> struct is_kvector<Vector3D<T>> : std::true_type {};
template <> struct is_kvector<BigVector2D> : std::true_type {};
template <> struct is_kvector<BigVector3D> : std::true_type {};

template <class T>
inline constexpr bool is_kvector_v = is_kvector<T>::value;

} // namespace linalg
} // namespace fizmo

#endif // FIZMO_VECTORS_KVECTOR_HPP
