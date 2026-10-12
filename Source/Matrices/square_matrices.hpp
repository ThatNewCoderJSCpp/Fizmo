#ifndef FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP
#define FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP

#include "matrix.hpp"

namespace fizmo {
namespace linalg {

template <class T = double> struct Matrix2x2;
template <class T = double> struct Matrix3x3;
template <class T = double> struct Matrix4x4;
struct BigMatrix2x2;
struct BigMatrix3x3;
struct BigMatrix4x4;

namespace matrix_detail {

template <class S, class = void>
struct square_family;

template <class T>
struct square_family<T, typename std::enable_if<scalar::is_real<T>::value>::type> {
    using M2 = Matrix2x2<T>;
    using M3 = Matrix3x3<T>;
    using M4 = Matrix4x4<T>;
};

template <>
struct square_family<scalar::BigFloat, void> {
    using M2 = BigMatrix2x2;
    using M3 = BigMatrix3x3;
    using M4 = BigMatrix4x4;
};

template <class Derived, std::size_t N, class S>
struct FixedMatrixImpl : SquareMatrixOps<Derived, N, S> {
private:
    using Base = SquareMatrixOps<Derived, N, S>;
    using F    = scalar::field<S>;

public:
    using context_type = typename Base::context_type;

    std::array<S, N * N> data;

    FixedMatrixImpl() { data.fill(F::zero()); }
    explicit FixedMatrixImpl(const std::array<S, N * N>& d) : data(d) {}

    explicit FixedMatrixImpl(const typename matrix_family<S>::template M<N>& m) {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) data[r * N + c] = m.data[r][c];
    }

    explicit operator typename matrix_family<S>::template M<N>() const {
        typename matrix_family<S>::template M<N> m;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) m.data[r][c] = data[r * N + c];
        return m;
    }

    S&       entry(std::size_t r, std::size_t c) { return data[r * N + c]; }
    const S& entry(std::size_t r, std::size_t c) const { return data[r * N + c]; }

    S*       operator[](std::size_t r) { return &data[r * N]; }
    const S* operator[](std::size_t r) const { return &data[r * N]; }

protected:
    static Derived plane_rotation(std::size_t a, std::size_t b, const S& angle, bool degrees, const context_type& ctx) {
        std::array<S, 2> cs;
        bool fast = false;
        if constexpr (scalar::dd::eligible<S>::value) fast = scalar::dd::fast_cos_sin(angle, degrees, cs[0], cs[1]);

        if (!fast) {
            const BigFloat t = to_bigfloat(angle);
            cs = F::template eval_n<2>(ctx, [&](const BigFloatContext& w) {
                const scalar::kernel::CosSin v = scalar::kernel::cos_sin(t, degrees, w);
                return std::array<BigFloat, 2>{ v.cos, v.sin };
            });
        }

        Derived m = Base::identity();
        m.entry(a, a) = cs[0];
        m.entry(a, b) = F::neg(cs[1]);
        m.entry(b, a) = cs[1];
        m.entry(b, b) = cs[0];
        return m;
    }

    template <class V>
    std::array<S, N> times(const V& v, const context_type& ctx) const {
        const std::array<S, N> a = v.to_array();
        return Base::mul(a, ctx);
    }
};

template <class Derived, class S>
struct Matrix2x2Impl : FixedMatrixImpl<Derived, 2, S> {
private:
    using Base = FixedMatrixImpl<Derived, 2, S>;
    using Ops  = SquareMatrixOps<Derived, 2, S>;
    using F    = scalar::field<S>;
    using V2   = typename detail::family<S>::V2;

public:
    using context_type = typename Base::context_type;
    using Base::Base;
    using Ops::operator*;
    using Ops::diagonal;
    using Ops::scaling;
    using Ops::shear;
    using Ops::reflection;
    using Ops::swap_rows;
    using Ops::swap_cols;

    Matrix2x2Impl() = default;
    Matrix2x2Impl(const S& m00, const S& m01, const S& m10, const S& m11) : Base(std::array<S, 4>{ { m00, m01, m10, m11 } }) {}

    static Derived diagonal(const S& d0, const S& d1) { return Derived(d0, F::zero(), F::zero(), d1); }
    static Derived scaling(const S& sx, const S& sy) { return diagonal(sx, sy); }
    static Derived shear(const S& kx, const S& ky) { return Derived(F::one(), kx, ky, F::one()); }

    static Derived rotation(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 1, angle, angle_in_degrees, ctx); }

    static Derived reflection(const S& nx, const S& ny, const context_type& ctx = F::default_context()) { return Ops::reflection(std::array<S, 2>{ { nx, ny } }, ctx); }

    V2 operator*(const V2& v) const { return mul(v); }

    V2 mul(const V2& v, const context_type& ctx = F::default_context()) const {
        const std::array<S, 2> r = Base::times(v, ctx);
        return V2(r[0], r[1]);
    }

    using Ops::mul;

    Derived swap_rows() const { return Ops::swap_rows(0, 1); }
    Derived swap_cols() const { return Ops::swap_cols(0, 1); }

    std::array<S, 2> eigenvalues(const context_type& ctx = F::default_context()) const {
        if (!this->all_finite()) return { { F::nan(), F::nan() } };
        const scalar::kernel::Vec m = this->big();
        const BigFloat tr = scalar::exact_add(m[0], m[3]);
        const BigFloat det = scalar::exact_sub(scalar::exact_mul(m[0], m[3]), scalar::exact_mul(m[1], m[2]));
        const BigFloat amd = scalar::exact_sub(m[0], m[3]);
        const BigFloat disc = scalar::exact_add(scalar::exact_mul(amd, amd), BigFloat::ldexp(scalar::exact_mul(m[1], m[2]), 2));

        if (disc.signbit() && !disc.is_zero()) {
            const S re = F::round(ctx, BigFloat::ldexp(tr, -1));
            return { { re, re } };
        }

        return F::template eval_n<2>(ctx, [&](const BigFloatContext& w) {
            const BigFloatContext ww = scalar::kernel::widen(w, 8);
            const BigFloat s = math::sqrt(disc, ww);
            const BigFloat big = BigFloat::ldexp(tr.signbit() ? BigFloat::sub(tr, s, ww) : BigFloat::add(tr, s, ww), -1);
            const BigFloat small = big.is_zero() ? BigFloat::zero() : BigFloat::div(det, big, ww);
            const BigFloat plus = tr.signbit() ? small : big;
            const BigFloat minus = tr.signbit() ? big : small;
            return std::array<BigFloat, 2>{ plus.rounded(w), minus.rounded(w) };
        });
    }
};

template <class Derived, class S>
struct Matrix3x3Impl : FixedMatrixImpl<Derived, 3, S> {
private:
    using Base = FixedMatrixImpl<Derived, 3, S>;
    using Ops  = SquareMatrixOps<Derived, 3, S>;
    using F    = scalar::field<S>;
    using V3   = typename detail::family<S>::V3;
    using M2   = typename square_family<S>::M2;

public:
    using context_type = typename Base::context_type;
    using Base::Base;
    using Ops::operator*;
    using Ops::diagonal;
    using Ops::scaling;
    using Ops::reflection;
    using Ops::mul;

    Matrix3x3Impl() = default;

    Matrix3x3Impl(const S& m00, const S& m01, const S& m02, const S& m10, const S& m11, const S& m12, const S& m20, const S& m21, const S& m22)
        : Base(std::array<S, 9>{ { m00, m01, m02, m10, m11, m12, m20, m21, m22 } }) {}

    static Derived diagonal(const S& d0, const S& d1, const S& d2) { return Ops::diagonal(std::array<S, 3>{ { d0, d1, d2 } }); }
    static Derived scaling(const S& sx, const S& sy, const S& sz) { return diagonal(sx, sy, sz); }

    static Derived rotation_x(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(1, 2, angle, angle_in_degrees, ctx); }
    static Derived rotation_y(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(2, 0, angle, angle_in_degrees, ctx); }
    static Derived rotation_z(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 1, angle, angle_in_degrees, ctx); }
    static Derived rotation_2d(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 1, angle, angle_in_degrees, ctx); }

    static Derived rotation_axis(const S& ax, const S& ay, const S& az, const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) {
        const BigFloat x = to_bigfloat(ax);
        const BigFloat y = to_bigfloat(ay);
        const BigFloat z = to_bigfloat(az);
        const BigFloat t = to_bigfloat(angle);
        if (!x.is_finite() || !y.is_finite() || !z.is_finite() || !t.is_finite()) return Ops::nan_matrix();
        if (x.is_zero() && y.is_zero() && z.is_zero()) return Ops::identity();

        if constexpr (scalar::dd::eligible<S>::value) {
            std::array<S, 9> f;
            if (scalar::dd::fast_axis_rotation(ax, ay, az, angle, angle_in_degrees, f)) return Derived(f);
        }

        const std::vector<S> v = F::eval_vec(ctx, 9, [&](const BigFloatContext& w) { return scalar::kernel::axis_rotation(x, y, z, t, angle_in_degrees, w); });
        return Ops::from_flat(v);
    }

    static Derived reflection(const S& nx, const S& ny, const S& nz, const context_type& ctx = F::default_context()) { return Ops::reflection(std::array<S, 3>{ { nx, ny, nz } }, ctx); }

    static Derived skew_symmetric(const S& x, const S& y, const S& z) {
        const S o = F::zero();
        return Derived(o, F::neg(z), y, z, o, F::neg(x), F::neg(y), x, o);
    }

    static Derived translation_2d(const S& tx, const S& ty) {
        const S o = F::zero();
        const S l = F::one();
        return Derived(l, o, tx, o, l, ty, o, o, l);
    }

    V3 operator*(const V3& v) const { return mul(v); }

    V3 mul(const V3& v, const context_type& ctx = F::default_context()) const {
        const std::array<S, 3> r = Base::times(v, ctx);
        return V3(r[0], r[1], r[2]);
    }

    M2 submatrix(std::size_t dr, std::size_t dc) const {
        M2 m;
        std::size_t idx = 0;

        for (std::size_t r = 0; r < 3; ++r) {
            if (r == dr) continue;
            for (std::size_t c = 0; c < 3; ++c) if (c != dc) m.data[idx++] = this->data[r * 3 + c];
        }

        return m;
    }

    static std::array<S, 3> cross(const std::array<S, 3>& a, const std::array<S, 3>& b, const context_type& ctx = F::default_context()) {
        return { {
            F::diff_of_products(a[1], b[2], a[2], b[1], ctx),
            F::diff_of_products(a[2], b[0], a[0], b[2], ctx),
            F::diff_of_products(a[0], b[1], a[1], b[0], ctx)
        } };
    }
};

template <class Derived, class S>
struct Matrix4x4Impl : FixedMatrixImpl<Derived, 4, S> {
private:
    using Base = FixedMatrixImpl<Derived, 4, S>;
    using Ops  = SquareMatrixOps<Derived, 4, S>;
    using F    = scalar::field<S>;
    using V4   = typename detail::family<S>::V4;
    using M3   = typename square_family<S>::M3;

public:
    using context_type = typename Base::context_type;
    using Base::Base;
    using Ops::operator*;
    using Ops::diagonal;
    using Ops::scaling;
    using Ops::reflection;
    using Ops::mul;

    Matrix4x4Impl() = default;

    Matrix4x4Impl(const S& m00, const S& m01, const S& m02, const S& m03,
                  const S& m10, const S& m11, const S& m12, const S& m13,
                  const S& m20, const S& m21, const S& m22, const S& m23,
                  const S& m30, const S& m31, const S& m32, const S& m33)
        : Base(std::array<S, 16>{ { m00, m01, m02, m03, m10, m11, m12, m13, m20, m21, m22, m23, m30, m31, m32, m33 } }) {}

    static Derived diagonal(const S& d0, const S& d1, const S& d2, const S& d3) { return Ops::diagonal(std::array<S, 4>{ { d0, d1, d2, d3 } }); }
    static Derived scaling(const S& sx, const S& sy, const S& sz, const S& sw = F::one()) { return diagonal(sx, sy, sz, sw); }

    static Derived rotation_xy(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 1, angle, angle_in_degrees, ctx); }
    static Derived rotation_xz(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 2, angle, angle_in_degrees, ctx); }
    static Derived rotation_xw(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(0, 3, angle, angle_in_degrees, ctx); }
    static Derived rotation_yz(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(1, 2, angle, angle_in_degrees, ctx); }
    static Derived rotation_yw(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(1, 3, angle, angle_in_degrees, ctx); }
    static Derived rotation_zw(const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) { return Base::plane_rotation(2, 3, angle, angle_in_degrees, ctx); }

    static Derived rotation_plane(const V4& u, const V4& v, const S& angle, bool angle_in_degrees = true, const context_type& ctx = F::default_context()) {
        const scalar::kernel::Vec U{ to_bigfloat(u.x), to_bigfloat(u.y), to_bigfloat(u.z), to_bigfloat(u.w) };
        const scalar::kernel::Vec W{ to_bigfloat(v.x), to_bigfloat(v.y), to_bigfloat(v.z), to_bigfloat(v.w) };
        const BigFloat t = to_bigfloat(angle);
        if (!scalar::kernel::all_finite(U) || !scalar::kernel::all_finite(W) || !t.is_finite()) return Ops::nan_matrix();

        const std::vector<S> r = F::eval_vec(ctx, 16, [&](const BigFloatContext& w) {
            const BigFloatContext ww = scalar::kernel::widen(w, 8);
            const scalar::kernel::CosSin cs = scalar::kernel::cos_sin(t, angle_in_degrees, ww);
            const BigFloat cm1 = BigFloat::sub(cs.cos, BigFloat::one(), ww);
            scalar::kernel::Vec out(16);

            for (std::size_t i = 0; i < 4; ++i) {
                for (std::size_t j = 0; j < 4; ++j) {
                    const BigFloat sym = scalar::exact_add(scalar::exact_mul(U[i], U[j]), scalar::exact_mul(W[i], W[j]));
                    const BigFloat anti = scalar::exact_sub(scalar::exact_mul(W[i], U[j]), scalar::exact_mul(U[i], W[j]));
                    BigFloat e = BigFloat::add(BigFloat::mul(cm1, sym, ww), BigFloat::mul(cs.sin, anti, ww), ww);
                    if (i == j) e = BigFloat::add(e, BigFloat::one(), ww);
                    out[i * 4 + j] = e.rounded(w);
                }
            }

            return out;
        });

        return Ops::from_flat(r);
    }

    static Derived translation(const S& tx, const S& ty, const S& tz) {
        const S o = F::zero();
        const S l = F::one();
        return Derived(l, o, o, tx, o, l, o, ty, o, o, l, tz, o, o, o, l);
    }

    static Derived skew_symmetric(const S& a01, const S& a02, const S& a03, const S& a12, const S& a13, const S& a23) {
        const S o = F::zero();
        return Derived(o, F::neg(a01), F::neg(a02), F::neg(a03), a01, o, F::neg(a12), F::neg(a13), a02, a12, o, F::neg(a23), a03, a13, a23, o);
    }

    static Derived reflection(const V4& normal, const context_type& ctx = F::default_context()) {
        const scalar::kernel::Vec n{ to_bigfloat(normal.x), to_bigfloat(normal.y), to_bigfloat(normal.z), to_bigfloat(normal.w) };
        if (!scalar::kernel::all_finite(n)) return Ops::nan_matrix();
        const BigFloat nn = scalar::kernel::exact_dot(n, n);
        if (nn.is_zero()) return Ops::identity();

        return Ops::generate([&](std::size_t r, std::size_t c) {
            const BigFloat t = BigFloat::ldexp(scalar::exact_mul(n[r], n[c]), 1);
            return F::ratio(r == c ? scalar::exact_sub(nn, t) : -t, nn, ctx);
        });
    }

    V4 operator*(const V4& v) const { return mul(v); }

    V4 mul(const V4& v, const context_type& ctx = F::default_context()) const {
        const std::array<S, 4> r = Base::times(v, ctx);
        return V4(r[0], r[1], r[2], r[3]);
    }

    M3 submatrix(std::size_t dr, std::size_t dc) const {
        M3 m;
        std::size_t idx = 0;

        for (std::size_t r = 0; r < 4; ++r) {
            if (r == dr) continue;
            for (std::size_t c = 0; c < 4; ++c) if (c != dc) m.data[idx++] = this->data[r * 4 + c];
        }

        return m;
    }
};

} // namespace matrix_detail

template <class T>
struct Matrix2x2 : matrix_detail::Matrix2x2Impl<Matrix2x2<T>, T> {
    static_assert(scalar::is_real<T>::value, "Matrix2x2 requires a standard or fixed-width floating-point type");
    using matrix_detail::Matrix2x2Impl<Matrix2x2<T>, T>::Matrix2x2Impl;
    Matrix2x2() = default;
};

template <class T>
struct Matrix3x3 : matrix_detail::Matrix3x3Impl<Matrix3x3<T>, T> {
    static_assert(scalar::is_real<T>::value, "Matrix3x3 requires a standard or fixed-width floating-point type");
    using matrix_detail::Matrix3x3Impl<Matrix3x3<T>, T>::Matrix3x3Impl;
    Matrix3x3() = default;
};

template <class T>
struct Matrix4x4 : matrix_detail::Matrix4x4Impl<Matrix4x4<T>, T> {
    static_assert(scalar::is_real<T>::value, "Matrix4x4 requires a standard or fixed-width floating-point type");
    using matrix_detail::Matrix4x4Impl<Matrix4x4<T>, T>::Matrix4x4Impl;
    Matrix4x4() = default;
};

#define FIZMO_BIG_SQUARE_MATRIX(NAME, IMPL, SMALL, N)                                                                                    \
    struct NAME : matrix_detail::IMPL<NAME, scalar::BigFloat> {                                                                          \
        using matrix_detail::IMPL<NAME, scalar::BigFloat>::IMPL;                                                                         \
        NAME() = default;                                                                                                                \
                                                                                                                                         \
        template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>                                            \
        explicit NAME(const SMALL<T>& m) {                                                                                               \
            for (std::size_t i = 0; i < N * N; ++i) this->data[i] = scalar::BigFloat(m.data[i]);                                         \
        }                                                                                                                                \
                                                                                                                                         \
        template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>                                            \
        SMALL<T> to() const {                                                                                                            \
            SMALL<T> out;                                                                                                                \
            for (std::size_t i = 0; i < N * N; ++i) out.data[i] = scalar::from_big<T>(this->data[i]);                                    \
            return out;                                                                                                                  \
        }                                                                                                                                \
                                                                                                                                         \
        NAME rounded(const scalar::BigFloatContext& ctx) const {                                                                         \
            NAME out;                                                                                                                    \
            for (std::size_t i = 0; i < N * N; ++i) out.data[i] = this->data[i].rounded(ctx);                                            \
            return out;                                                                                                                  \
        }                                                                                                                                \
    };

FIZMO_BIG_SQUARE_MATRIX(BigMatrix2x2, Matrix2x2Impl, Matrix2x2, 2)
FIZMO_BIG_SQUARE_MATRIX(BigMatrix3x3, Matrix3x3Impl, Matrix3x3, 3)
FIZMO_BIG_SQUARE_MATRIX(BigMatrix4x4, Matrix4x4Impl, Matrix4x4, 4)

#undef FIZMO_BIG_SQUARE_MATRIX

using Matrix2f = Matrix2x2<float>;
using Matrix3f = Matrix3x3<float>;
using Matrix4f = Matrix4x4<float>;
using Matrix2d = Matrix2x2<double>;
using Matrix3d = Matrix3x3<double>;
using Matrix4d = Matrix4x4<double>;

} // namespace linalg
} // namespace fizmo

#endif // FIZMO_SPECIALIZED_SQUARE_MATRICES_HPP
