#ifndef FIZMO_VECTORS_FLOATING_HPP
#define FIZMO_VECTORS_FLOATING_HPP

#include <array>
#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>

#include "vector_ops.hpp"

namespace fizmo {
namespace linalg {

template <std::size_t N, class T = double>
using Bivector = KVector<2, N, T>;

template <std::size_t N, class T = double>
using Trivector = KVector<3, N, T>;

template <std::size_t N>
using BigBivector = BigKVector<2, N>;

template <std::size_t N>
using BigTrivector = BigKVector<3, N>;

namespace detail {

template <class Derived, class S>
struct Vector2DImpl : VectorOps<Derived, 2, S> {
private:
    using Base = VectorOps<Derived, 2, S>;
    using F    = scalar::field<S>;
    using Fam  = family<S>;

public:
    using context_type = typename Base::context_type;

    S x;
    S y;

    Vector2DImpl() : x(F::zero()), y(F::zero()) {}
    Vector2DImpl(const S& x_, const S& y_) : x(x_), y(y_) {}

    template <class U, class V, typename std::enable_if<is_scalar_operand<S, U>::value && is_scalar_operand<S, V>::value && !(std::is_same<U, S>::value && std::is_same<V, S>::value), int>::type = 0>
    Vector2DImpl(const U& x_, const V& y_) : x(F::from(x_)), y(F::from(y_)) {}

    S&       component(std::size_t i) { return i == 0 ? x : y; }
    const S& component(std::size_t i) const { return i == 0 ? x : y; }

    static Derived zero() { return Derived(); }

    static Derived from_magnitude_angle(const S& magnitude, const S& angle, bool angle_in_degrees = true, bool angle_counterclockwise = true, const context_type& c = F::default_context()) {
        if constexpr (scalar::dd::eligible<S>::value) {
            S fx, fy;
            if (scalar::dd::fast_scaled_cos_sin(magnitude, angle_counterclockwise ? angle : -angle, angle_in_degrees, fx, fy)) return Derived(fx, fy);
        }

        const BigFloat m = to_bigfloat(magnitude);
        BigFloat a = to_bigfloat(angle);
        if (!angle_counterclockwise) a = -a;

        const std::array<S, 2> r = F::template eval_n<2>(c, [&](const BigFloatContext& w) {
            const scalar::kernel::CosSin cs = scalar::kernel::cos_sin(a, angle_in_degrees, w);
            return std::array<BigFloat, 2>{ BigFloat::mul(m, cs.cos, w), BigFloat::mul(m, cs.sin, w) };
        });

        return Derived(r[0], r[1]);
    }

    S perp_dot(const Derived& v, const context_type& c = F::default_context()) const { return F::diff_of_products(x, v.y, y, v.x, c); }

    typename Fam::V3 cross(const Derived& v, const context_type& c = F::default_context()) const { return typename Fam::V3(F::zero(), F::zero(), perp_dot(v, c)); }

    typename Fam::template KV<2, 2> wedge(const Derived& v, const context_type& c = F::default_context()) const;

    Derived perpendicular() const { return Derived(F::neg(y), x); }

    explicit operator typename Fam::V3() const { return typename Fam::V3(x, y, F::zero()); }
    explicit operator typename Fam::V4() const { return typename Fam::V4(x, y, F::zero(), F::zero()); }
};

template <class Derived, class S>
struct Vector3DImpl : VectorOps<Derived, 3, S> {
private:
    using Base = VectorOps<Derived, 3, S>;
    using F    = scalar::field<S>;
    using Fam  = family<S>;

public:
    using context_type = typename Base::context_type;

    S x;
    S y;
    S z;

    Vector3DImpl() : x(F::zero()), y(F::zero()), z(F::zero()) {}
    Vector3DImpl(const S& x_, const S& y_, const S& z_) : x(x_), y(y_), z(z_) {}
    Vector3DImpl(const typename Fam::V2& v) : x(v.x), y(v.y), z(F::zero()) {}

    template <class U, class V, class W, typename std::enable_if<is_scalar_operand<S, U>::value && is_scalar_operand<S, V>::value && is_scalar_operand<S, W>::value && !(std::is_same<U, S>::value && std::is_same<V, S>::value && std::is_same<W, S>::value), int>::type = 0>
    Vector3DImpl(const U& x_, const V& y_, const W& z_) : x(F::from(x_)), y(F::from(y_)), z(F::from(z_)) {}

    S&       component(std::size_t i) { return i == 0 ? x : (i == 1 ? y : z); }
    const S& component(std::size_t i) const { return i == 0 ? x : (i == 1 ? y : z); }

    static Derived zero() { return Derived(); }

    Derived cross(const Derived& v, const context_type& c = F::default_context()) const {
        return Derived(F::diff_of_products(y, v.z, z, v.y, c), F::diff_of_products(z, v.x, x, v.z, c), F::diff_of_products(x, v.y, y, v.x, c));
    }

    S triple_product(const Derived& b, const Derived& cc, const context_type& c = F::default_context()) const {
        const scalar::kernel::Vec a = this->big();
        const scalar::kernel::Vec p = b.big();
        const scalar::kernel::Vec q = cc.big();
        if (!this->all_finite() || !b.all_finite() || !cc.all_finite()) return this->dot(b.cross(cc, c), c);
        const BigFloat v = scalar::kernel::det({ a[0], a[1], a[2], p[0], p[1], p[2], q[0], q[1], q[2] }, 3);
        return F::round(c, v);
    }

    typename Fam::template KV<2, 3> wedge(const Derived& v, const context_type& c = F::default_context()) const;

    explicit operator typename Fam::V2() const { return typename Fam::V2(x, y); }
    explicit operator typename Fam::V4() const { return typename Fam::V4(x, y, z, F::zero()); }
};

template <class Derived, class S>
struct Vector4DImpl : VectorOps<Derived, 4, S> {
private:
    using Base = VectorOps<Derived, 4, S>;
    using F    = scalar::field<S>;
    using Fam  = family<S>;

public:
    using context_type = typename Base::context_type;

    S x;
    S y;
    S z;
    S w;

    Vector4DImpl() : x(F::zero()), y(F::zero()), z(F::zero()), w(F::zero()) {}
    Vector4DImpl(const S& x_, const S& y_, const S& z_, const S& w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vector4DImpl(const typename Fam::V3& v) : x(v.x), y(v.y), z(v.z), w(F::zero()) {}
    Vector4DImpl(const typename Fam::V2& v) : x(v.x), y(v.y), z(F::zero()), w(F::zero()) {}

    template <class A, class B, class C, class D, typename std::enable_if<is_scalar_operand<S, A>::value && is_scalar_operand<S, B>::value && is_scalar_operand<S, C>::value && is_scalar_operand<S, D>::value && !(std::is_same<A, S>::value && std::is_same<B, S>::value && std::is_same<C, S>::value && std::is_same<D, S>::value), int>::type = 0>
    Vector4DImpl(const A& x_, const B& y_, const C& z_, const D& w_) : x(F::from(x_)), y(F::from(y_)), z(F::from(z_)), w(F::from(w_)) {}

    S&       component(std::size_t i) { return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w)); }
    const S& component(std::size_t i) const { return i == 0 ? x : (i == 1 ? y : (i == 2 ? z : w)); }

    static Derived zero() { return Derived(); }

    typename Fam::template KV<2, 4> wedge(const Derived& v, const context_type& c = F::default_context()) const;

    explicit operator typename Fam::V2() const { return typename Fam::V2(x, y); }
    explicit operator typename Fam::V3() const { return typename Fam::V3(x, y, z); }
};

inline const std::array<std::array<std::array<int, 3>, 6>, 7>& cross7_terms() {
    static const std::array<std::array<std::array<int, 3>, 6>, 7> table = [] {
        const int triples[7][3] = { {0, 1, 3}, {1, 2, 4}, {2, 3, 5}, {3, 4, 6}, {4, 5, 0}, {5, 6, 1}, {6, 0, 2} };
        std::array<std::array<std::array<int, 3>, 6>, 7> t{};
        std::array<std::size_t, 7> fill{};

        for (const auto& tr : triples) {
            for (int rot = 0; rot < 3; ++rot) {
                const int i = tr[rot % 3];
                const int j = tr[(rot + 1) % 3];
                const int k = tr[(rot + 2) % 3];
                t[k][fill[k]++] = { i, j, 1 };
                t[k][fill[k]++] = { j, i, -1 };
            }
        }

        return t;
    }();

    return table;
}

template <class Derived, std::size_t N, class S>
struct VectorNImpl : VectorOps<Derived, N, S> {
private:
    using Base = VectorOps<Derived, N, S>;
    using F    = scalar::field<S>;
    using Fam  = family<S>;

public:
    using context_type = typename Base::context_type;

    std::array<S, N> data;

    VectorNImpl() { data.fill(F::zero()); }
    explicit VectorNImpl(const std::array<S, N>& arr) : data(arr) {}

    template <class... Args, typename std::enable_if<sizeof...(Args) == N && std::conjunction<is_scalar_operand<S, typename std::decay<Args>::type>...>::value, int>::type = 0>
    VectorNImpl(const Args&... args) : data{ { F::from(args)... } } {}

    S&       component(std::size_t i) { return data[i]; }
    const S& component(std::size_t i) const { return data[i]; }

    S&       at(std::size_t i) { return data.at(i); }
    const S& at(std::size_t i) const { return data.at(i); }

    S&       x() { static_assert(N >= 1, "No x component"); return data[0]; }
    const S& x() const { static_assert(N >= 1, "No x component"); return data[0]; }
    S&       y() { static_assert(N >= 2, "No y component"); return data[1]; }
    const S& y() const { static_assert(N >= 2, "No y component"); return data[1]; }
    S&       z() { static_assert(N >= 3, "No z component"); return data[2]; }
    const S& z() const { static_assert(N >= 3, "No z component"); return data[2]; }
    S&       w() { static_assert(N >= 4, "No w component"); return data[3]; }
    const S& w() const { static_assert(N >= 4, "No w component"); return data[3]; }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }

    static Derived zero() { return Derived(); }

    static Derived filled(const S& v) {
        Derived d;
        d.data.fill(v);
        return d;
    }

    static Derived basis(std::size_t axis) {
        Derived d;
        if (axis < N) d.data[axis] = F::one();
        return d;
    }

    template <std::size_t M = N, typename std::enable_if<M == 3, int>::type = 0>
    Derived cross(const Derived& o, const context_type& c = F::default_context()) const {
        return Derived(std::array<S, 3>{ {
            F::diff_of_products(data[1], o.data[2], data[2], o.data[1], c),
            F::diff_of_products(data[2], o.data[0], data[0], o.data[2], c),
            F::diff_of_products(data[0], o.data[1], data[1], o.data[0], c)
        } });
    }

    template <std::size_t M = N, typename std::enable_if<M == 7, int>::type = 0>
    Derived cross(const Derived& o, const context_type& c = F::default_context()) const {
        std::array<S, 7> out;

        for (std::size_t k = 0; k < 7; ++k) {
            S a[6];
            S b[6];

            for (std::size_t t = 0; t < 6; ++t) {
                const auto& term = cross7_terms()[k][t];
                a[t] = term[2] > 0 ? data[static_cast<std::size_t>(term[0])] : F::neg(data[static_cast<std::size_t>(term[0])]);
                b[t] = o.data[static_cast<std::size_t>(term[1])];
            }

            out[k] = F::dot(a, b, 6, c);
        }

        return Derived(out);
    }

    typename Fam::template KV<2, N> wedge(const Derived& o, const context_type& c = F::default_context()) const;

    static constexpr std::size_t size = N;
    static constexpr std::size_t dims() noexcept { return N; }
};

} // namespace detail

template <class T>
struct Vector2D : detail::Vector2DImpl<Vector2D<T>, T> {
    static_assert(scalar::is_real<T>::value, "Vector2D requires a standard or fixed-width floating-point type");
    using detail::Vector2DImpl<Vector2D<T>, T>::Vector2DImpl;

    Vector2D() = default;

    template <class U, typename std::enable_if<!std::is_same<U, T>::value, int>::type = 0>
    explicit Vector2D(const Vector2D<U>& o) : Vector2D(scalar::field<T>::from(o.x), scalar::field<T>::from(o.y)) {}
};

template <class T>
struct Vector3D : detail::Vector3DImpl<Vector3D<T>, T> {
    static_assert(scalar::is_real<T>::value, "Vector3D requires a standard or fixed-width floating-point type");
    using detail::Vector3DImpl<Vector3D<T>, T>::Vector3DImpl;

    Vector3D() = default;

    template <class U, typename std::enable_if<!std::is_same<U, T>::value, int>::type = 0>
    explicit Vector3D(const Vector3D<U>& o) : Vector3D(scalar::field<T>::from(o.x), scalar::field<T>::from(o.y), scalar::field<T>::from(o.z)) {}
};

template <class T>
struct Vector4D : detail::Vector4DImpl<Vector4D<T>, T> {
    static_assert(scalar::is_real<T>::value, "Vector4D requires a standard or fixed-width floating-point type");
    using detail::Vector4DImpl<Vector4D<T>, T>::Vector4DImpl;

    Vector4D() = default;

    template <class U, typename std::enable_if<!std::is_same<U, T>::value, int>::type = 0>
    explicit Vector4D(const Vector4D<U>& o) : Vector4D(scalar::field<T>::from(o.x), scalar::field<T>::from(o.y), scalar::field<T>::from(o.z), scalar::field<T>::from(o.w)) {}
};

template <std::size_t N, class T>
struct VectorN : detail::VectorNImpl<VectorN<N, T>, N, T> {
    static_assert(scalar::is_real<T>::value, "VectorN requires a standard or fixed-width floating-point type");
    using detail::VectorNImpl<VectorN<N, T>, N, T>::VectorNImpl;

    VectorN() = default;
};

struct BigVector2D : detail::Vector2DImpl<BigVector2D, scalar::BigFloat> {
    using detail::Vector2DImpl<BigVector2D, scalar::BigFloat>::Vector2DImpl;

    BigVector2D() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigVector2D(const Vector2D<T>& v) : BigVector2D(scalar::BigFloat(v.x), scalar::BigFloat(v.y)) {}

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Vector2D<T> to() const { return Vector2D<T>(scalar::from_big<T>(x), scalar::from_big<T>(y)); }

    BigVector2D rounded(const scalar::BigFloatContext& c) const { return BigVector2D(x.rounded(c), y.rounded(c)); }
};

struct BigVector3D : detail::Vector3DImpl<BigVector3D, scalar::BigFloat> {
    using detail::Vector3DImpl<BigVector3D, scalar::BigFloat>::Vector3DImpl;

    BigVector3D() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigVector3D(const Vector3D<T>& v) : BigVector3D(scalar::BigFloat(v.x), scalar::BigFloat(v.y), scalar::BigFloat(v.z)) {}

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Vector3D<T> to() const { return Vector3D<T>(scalar::from_big<T>(x), scalar::from_big<T>(y), scalar::from_big<T>(z)); }

    BigVector3D rounded(const scalar::BigFloatContext& c) const { return BigVector3D(x.rounded(c), y.rounded(c), z.rounded(c)); }
};

struct BigVector4D : detail::Vector4DImpl<BigVector4D, scalar::BigFloat> {
    using detail::Vector4DImpl<BigVector4D, scalar::BigFloat>::Vector4DImpl;

    BigVector4D() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigVector4D(const Vector4D<T>& v) : BigVector4D(scalar::BigFloat(v.x), scalar::BigFloat(v.y), scalar::BigFloat(v.z), scalar::BigFloat(v.w)) {}

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Vector4D<T> to() const { return Vector4D<T>(scalar::from_big<T>(x), scalar::from_big<T>(y), scalar::from_big<T>(z), scalar::from_big<T>(w)); }

    BigVector4D rounded(const scalar::BigFloatContext& c) const { return BigVector4D(x.rounded(c), y.rounded(c), z.rounded(c), w.rounded(c)); }
};

template <std::size_t N>
struct BigVectorN : detail::VectorNImpl<BigVectorN<N>, N, scalar::BigFloat> {
    using detail::VectorNImpl<BigVectorN<N>, N, scalar::BigFloat>::VectorNImpl;

    BigVectorN() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigVectorN(const VectorN<N, T>& v) {
        for (std::size_t i = 0; i < N; ++i) this->data[i] = scalar::BigFloat(v.data[i]);
    }

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    VectorN<N, T> to() const {
        VectorN<N, T> out;
        for (std::size_t i = 0; i < N; ++i) out.data[i] = scalar::from_big<T>(this->data[i]);
        return out;
    }

    BigVectorN rounded(const scalar::BigFloatContext& c) const {
        BigVectorN out;
        for (std::size_t i = 0; i < N; ++i) out.data[i] = this->data[i].rounded(c);
        return out;
    }
};

template <class T>
struct is_math_vector : std::false_type {};

template <class T>
struct is_real_math_vector : std::false_type {};

template <class T> struct is_math_vector<Vector2D<T>> : std::true_type {};
template <class T> struct is_math_vector<Vector3D<T>> : std::true_type {};
template <class T> struct is_math_vector<Vector4D<T>> : std::true_type {};
template <std::size_t N, class T> struct is_math_vector<VectorN<N, T>> : std::true_type {};
template <> struct is_math_vector<BigVector2D> : std::true_type {};
template <> struct is_math_vector<BigVector3D> : std::true_type {};
template <> struct is_math_vector<BigVector4D> : std::true_type {};
template <std::size_t N> struct is_math_vector<BigVectorN<N>> : std::true_type {};

template <class T> struct is_real_math_vector<Vector2D<T>> : std::true_type {};
template <class T> struct is_real_math_vector<Vector3D<T>> : std::true_type {};
template <class T> struct is_real_math_vector<Vector4D<T>> : std::true_type {};
template <std::size_t N, class T> struct is_real_math_vector<VectorN<N, T>> : std::true_type {};
template <> struct is_real_math_vector<BigVector2D> : std::true_type {};
template <> struct is_real_math_vector<BigVector3D> : std::true_type {};
template <> struct is_real_math_vector<BigVector4D> : std::true_type {};
template <std::size_t N> struct is_real_math_vector<BigVectorN<N>> : std::true_type {};

template <class T>
inline constexpr bool is_real_math_vector_v = is_real_math_vector<T>::value;

template <class T>
inline constexpr bool is_math_vector_v = is_math_vector<T>::value;

using vector2f = Vector2D<float>;
using vector3f = Vector3D<float>;
using vector4f = Vector4D<float>;
using vector2d = Vector2D<double>;
using vector3d = Vector3D<double>;
using vector4d = Vector4D<double>;
using vector2l = Vector2D<long double>;
using vector3l = Vector3D<long double>;
using vector4l = Vector4D<long double>;

} // namespace linalg
} // namespace fizmo

#endif // FIZMO_VECTORS_FLOATING_HPP
