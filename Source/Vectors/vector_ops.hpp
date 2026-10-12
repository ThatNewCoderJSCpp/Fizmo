#ifndef FIZMO_VECTORS_VECTOR_OPS_HPP
#define FIZMO_VECTORS_VECTOR_OPS_HPP

#include <array>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "../Scalar/linear_kernels.hpp"

namespace fizmo {
namespace linalg {

template <class T = double> struct Vector2D;
template <class T = double> struct Vector3D;
template <class T = double> struct Vector4D;
template <std::size_t N, class T = double> struct VectorN;
template <std::size_t K, std::size_t N, class T = double> struct KVector;

struct BigVector2D;
struct BigVector3D;
struct BigVector4D;
template <std::size_t N> struct BigVectorN;
template <std::size_t K, std::size_t N> struct BigKVector;

namespace detail {

using scalar::BigFloat;
using scalar::BigFloatContext;

template <class S, class = void>
struct family;

template <class T>
struct family<T, typename std::enable_if<scalar::is_real<T>::value>::type> {
    using V2 = Vector2D<T>;
    using V3 = Vector3D<T>;
    using V4 = Vector4D<T>;
    template <std::size_t N> using VN = VectorN<N, T>;
    template <std::size_t K, std::size_t N> using KV = KVector<K, N, T>;
};

template <>
struct family<scalar::BigFloat, void> {
    using V2 = BigVector2D;
    using V3 = BigVector3D;
    using V4 = BigVector4D;
    template <std::size_t N> using VN = BigVectorN<N>;
    template <std::size_t K, std::size_t N> using KV = BigKVector<K, N>;
};

template <class S, class U>
struct is_scalar_operand : std::integral_constant<bool, std::is_same<U, S>::value || scalar::is_mixed_scalar<U>::value> {};

template <class S, class U>
using if_scalar_operand = typename std::enable_if<is_scalar_operand<S, U>::value, int>::type;

template <class S>
void write_scalar(std::ostream& os, const S& v) {
    if constexpr (std::is_floating_point<S>::value) os << v;
    else os << scalar::field<S>::to_string(v);
}

template <class S>
BigFloat to_bigfloat(const S& v) { return BigFloat(scalar::field<S>::big(v)); }

template <class S, std::size_t M>
std::array<BigFloat, M> to_bigfloats(const std::array<S, M>& a) {
    std::array<BigFloat, M> out;
    for (std::size_t i = 0; i < M; ++i) out[i] = to_bigfloat(a[i]);
    return out;
}

template <std::size_t M>
std::array<BigFloat, M> to_fixed(const scalar::kernel::Vec& v) {
    std::array<BigFloat, M> out;
    for (std::size_t i = 0; i < M; ++i) out[i] = v[i];
    return out;
}

template <class Derived, std::size_t N, class S>
class VectorOps {
public:
    using value_type   = S;
    using field_type   = scalar::field<S>;
    using context_type = typename field_type::context_type;

    static constexpr std::size_t components = N;

protected:
    using F   = field_type;
    using Vec = scalar::kernel::Vec;

    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }
    Derived&       self() noexcept { return static_cast<Derived&>(*this); }

    const S& get(std::size_t i) const { return self().component(i); }
    S&       get(std::size_t i) { return self().component(i); }

    template <class Fn>
    static Derived generate(Fn&& f) {
        Derived d;
        for (std::size_t i = 0; i < N; ++i) d.component(i) = f(i);
        return d;
    }

    static Derived from_values(const std::array<S, N>& a) {
        Derived d;
        for (std::size_t i = 0; i < N; ++i) d.component(i) = a[i];
        return d;
    }

    Vec big() const {
        Vec v(N);
        for (std::size_t i = 0; i < N; ++i) v[i] = to_bigfloat(get(i));
        return v;
    }

    bool all_finite() const {
        for (std::size_t i = 0; i < N; ++i) if (!F::is_finite(get(i))) return false;
        return true;
    }

public:
    std::array<S, N> to_array() const {
        std::array<S, N> a;
        for (std::size_t i = 0; i < N; ++i) a[i] = get(i);
        return a;
    }

    S&       operator[](std::size_t i) { return get(i); }
    const S& operator[](std::size_t i) const { return get(i); }

    Derived add(const Derived& o, const context_type& c = F::default_context()) const { return generate([&](std::size_t i) { return F::add(get(i), o.get(i), c); }); }
    Derived sub(const Derived& o, const context_type& c = F::default_context()) const { return generate([&](std::size_t i) { return F::sub(get(i), o.get(i), c); }); }
    Derived negated() const { return generate([&](std::size_t i) { return F::neg(get(i)); }); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived scaled(const U& s, const context_type& c = F::default_context()) const { return generate([&](std::size_t i) { return F::mul_mixed(get(i), s, c); }); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived divided(const U& s, const context_type& c = F::default_context()) const { return generate([&](std::size_t i) { return F::div_mixed(get(i), s, c); }); }

    Derived hadamard(const Derived& o, const context_type& c = F::default_context()) const { return generate([&](std::size_t i) { return F::mul(get(i), o.get(i), c); }); }

    Derived operator+(const Derived& o) const { return add(o); }
    Derived operator-(const Derived& o) const { return sub(o); }
    Derived operator-() const { return negated(); }
    Derived operator+() const { return self(); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived operator*(const U& s) const { return scaled(s); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived operator/(const U& s) const { return divided(s); }

    Derived& operator+=(const Derived& o) { return self() = add(o); }
    Derived& operator-=(const Derived& o) { return self() = sub(o); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived& operator*=(const U& s) { return self() = scaled(s); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived& operator/=(const U& s) { return self() = divided(s); }

    template <class U, if_scalar_operand<S, U> = 0>
    friend Derived operator*(const U& s, const Derived& v) { return v.scaled(s); }

    bool operator==(const Derived& o) const {
        for (std::size_t i = 0; i < N; ++i) if (!(get(i) == o.get(i))) return false;
        return true;
    }

    bool operator!=(const Derived& o) const { return !(*this == o); }

    S dot(const Derived& o, const context_type& c = F::default_context()) const {
        const std::array<S, N> a = to_array();
        const std::array<S, N> b = o.to_array();
        return F::dot(a.data(), b.data(), N, c);
    }

    S magnitude_squared(const context_type& c = F::default_context()) const { return dot(self(), c); }

    S magnitude(const context_type& c = F::default_context()) const {
        const std::array<S, N> a = to_array();
        return F::norm(a.data(), N, c);
    }

    S length(const context_type& c = F::default_context()) const { return magnitude(c); }
    S norm(const context_type& c = F::default_context()) const { return magnitude(c); }

    S sum(const context_type& c = F::default_context()) const {
        const std::array<S, N> a = to_array();
        return F::sum(a.data(), N, c);
    }

    S product(const context_type& c = F::default_context()) const {
        if (!all_finite()) {
            S p = F::one();
            for (std::size_t i = 0; i < N; ++i) p = F::mul(p, get(i), c);
            return p;
        }

        const Vec a = big();
        return F::round(c, scalar::kernel::exact_product(a.data(), N));
    }

    Derived unit_vector(const context_type& c = F::default_context()) const {
        if (is_zero()) return self();
        if (!all_finite()) return divided(magnitude(c), c);
        const std::array<S, N> in = to_array();
        std::array<S, N> fast;
        if (F::fast_unit(in.data(), N, fast.data(), c)) return from_values(fast);
        const Vec a = big();
        const std::array<S, N> r = F::template eval_n<N>(c, [&](const BigFloatContext& w) { return to_fixed<N>(scalar::kernel::unit(a, w)); });
        return from_values(r);
    }

    Derived direction(const context_type& c = F::default_context()) const { return unit_vector(c); }
    Derived normalized(const context_type& c = F::default_context()) const { return unit_vector(c); }
    Derived& normalize(const context_type& c = F::default_context()) { return self() = unit_vector(c); }

    S scalar_project(const Derived& onto, const context_type& c = F::default_context()) const {
        if (!all_finite() || !onto.all_finite()) return F::nan();
        const Vec a = big();
        const Vec b = onto.big();
        return F::eval(c, [&](const BigFloatContext& w) { return scalar::kernel::scalar_project(a, b, w); });
    }

    Derived project_onto(const Derived& onto, const context_type& c = F::default_context()) const {
        if (!all_finite() || !onto.all_finite()) return generate([](std::size_t) { return F::nan(); });
        const Vec a = big();
        const Vec b = onto.big();
        const BigFloat bb = scalar::kernel::exact_dot(b, b);
        if (bb.is_zero()) return self();
        const BigFloat d = scalar::kernel::exact_dot(a, b);
        return generate([&](std::size_t i) { return F::ratio(scalar::exact_mul(b[i], d), bb, c); });
    }

    Derived project_from(const Derived& onto, const context_type& c = F::default_context()) const {
        if (!all_finite() || !onto.all_finite()) return generate([](std::size_t) { return F::nan(); });
        const Vec a = big();
        const Vec b = onto.big();
        const BigFloat bb = scalar::kernel::exact_dot(b, b);
        if (bb.is_zero()) return generate([](std::size_t) { return F::zero(); });
        const BigFloat d = scalar::kernel::exact_dot(a, b);
        return generate([&](std::size_t i) { return F::ratio(scalar::exact_sub(scalar::exact_mul(a[i], bb), scalar::exact_mul(b[i], d)), bb, c); });
    }

    Derived reject_from(const Derived& onto, const context_type& c = F::default_context()) const { return project_from(onto, c); }

    Derived reflect(const Derived& normal, const context_type& c = F::default_context()) const {
        if (!all_finite() || !normal.all_finite()) return generate([](std::size_t) { return F::nan(); });
        const Vec a = big();
        const Vec n = normal.big();
        const BigFloat d2 = BigFloat::ldexp(scalar::kernel::exact_dot(a, n), 1);
        return generate([&](std::size_t i) { return F::round(c, scalar::exact_sub(a[i], scalar::exact_mul(d2, n[i]))); });
    }

    S angle_between(const Derived& o, bool degrees = true, const context_type& c = F::default_context()) const {
        if (!all_finite() || !o.all_finite()) return F::nan();
        const Vec a = big();
        const Vec b = o.big();
        return F::eval(c, [&](const BigFloatContext& w) { return scalar::kernel::angle_between(a, b, degrees, w); });
    }

    bool is_zero() const {
        for (std::size_t i = 0; i < N; ++i) if (!F::is_zero(get(i))) return false;
        return true;
    }

    bool is_zero(const S& eps) const {
        for (std::size_t i = 0; i < N; ++i) if (F::abs(get(i)) > eps) return false;
        return true;
    }

    bool has_nan() const {
        for (std::size_t i = 0; i < N; ++i) if (F::is_nan(get(i))) return true;
        return false;
    }

    bool is_finite() const { return all_finite(); }

    bool approx_equal(const Derived& o, const S& eps = F::default_epsilon()) const {
        const BigFloat e = to_bigfloat(eps);

        for (std::size_t i = 0; i < N; ++i) {
            const BigFloat a = to_bigfloat(get(i));
            const BigFloat b = to_bigfloat(o.get(i));
            if (!a.is_finite() || !b.is_finite()) { if (!(get(i) == o.get(i))) return false; continue; }
            if (scalar::exact_sub(a, b).abs() > e) return false;
        }

        return true;
    }

    bool is_orthogonal(const Derived& o, const S& eps = F::zero()) const {
        if (!all_finite() || !o.all_finite()) return false;
        return scalar::kernel::exact_dot(big(), o.big()).abs() <= to_bigfloat(eps);
    }

    bool is_parallel(const Derived& o, const S& eps = F::zero()) const {
        if (!all_finite() || !o.all_finite()) return false;
        const BigFloat e = to_bigfloat(eps);
        return scalar::kernel::wedge_norm_squared(big(), o.big()) <= scalar::exact_mul(e, e);
    }

    bool is_antiparallel(const Derived& o, const S& eps = F::zero()) const {
        return is_parallel(o, eps) && scalar::kernel::exact_dot(big(), o.big()).signbit() && !scalar::kernel::exact_dot(big(), o.big()).is_zero();
    }

    friend std::ostream& operator<<(std::ostream& os, const VectorOps& v) {
        os << "<";

        for (std::size_t i = 0; i < N; ++i) {
            os << "\n    ";
            write_scalar(os, v.get(i));
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

} // namespace detail
} // namespace linalg
} // namespace fizmo

#endif // FIZMO_VECTORS_VECTOR_OPS_HPP
