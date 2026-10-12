#ifndef FIZMO_MATRIX_HPP
#define FIZMO_MATRIX_HPP

#include <array>
#include <cstddef>
#include <type_traits>

#include "matrix_ops.hpp"

namespace fizmo {
namespace linalg {

template <std::size_t N, class T = double> struct Matrix;
template <std::size_t N> struct BigMatrix;

namespace matrix_detail {

template <class S, class = void>
struct matrix_family;

template <class T>
struct matrix_family<T, typename std::enable_if<scalar::is_real<T>::value>::type> {
    template <std::size_t N> using M = Matrix<N, T>;
};

template <>
struct matrix_family<scalar::BigFloat, void> {
    template <std::size_t N> using M = BigMatrix<N>;
};

template <class Derived, std::size_t N, class S>
struct MatrixNImpl : SquareMatrixOps<Derived, N, S> {
private:
    using Base = SquareMatrixOps<Derived, N, S>;
    using F    = scalar::field<S>;

public:
    using context_type = typename Base::context_type;

    std::array<std::array<S, N>, N> data;

    MatrixNImpl() { for (auto& r : data) r.fill(F::zero()); }
    explicit MatrixNImpl(const std::array<std::array<S, N>, N>& d) : data(d) {}

    S&       entry(std::size_t r, std::size_t c) { return data[r][c]; }
    const S& entry(std::size_t r, std::size_t c) const { return data[r][c]; }

    std::array<S, N>&       operator[](std::size_t r) { return data[r]; }
    const std::array<S, N>& operator[](std::size_t r) const { return data[r]; }

    using Base::operator*;

    typename matrix_family<S>::template M<(N > 1 ? N - 1 : 1)> submatrix(std::size_t dr, std::size_t dc) const {
        typename matrix_family<S>::template M<(N > 1 ? N - 1 : 1)> m;
        std::size_t mr = 0;

        for (std::size_t r = 0; r < N; ++r) {
            if (r == dr) continue;
            std::size_t mc = 0;

            for (std::size_t c = 0; c < N; ++c) {
                if (c == dc) continue;
                if (mr < N - 1 && mc < N - 1) m.data[mr][mc] = data[r][c];
                ++mc;
            }

            ++mr;
        }

        return m;
    }

    static Derived translation_2d(const S& tx, const S& ty) {
        static_assert(N >= 3, "translation_2d requires N >= 3 (homogeneous)");
        Derived m = Base::identity();
        m.data[0][N - 1] = tx;
        m.data[1][N - 1] = ty;
        return m;
    }

    static Derived translation_3d(const S& tx, const S& ty, const S& tz) {
        static_assert(N >= 4, "translation_3d requires N >= 4 (homogeneous)");
        Derived m = Base::identity();
        m.data[0][N - 1] = tx;
        m.data[1][N - 1] = ty;
        m.data[2][N - 1] = tz;
        return m;
    }

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
        m.data[a][a] = cs[0];
        m.data[a][b] = F::neg(cs[1]);
        m.data[b][a] = cs[1];
        m.data[b][b] = cs[0];
        return m;
    }

    static Derived rotation_2d(const S& angle, bool degrees = false, const context_type& ctx = F::default_context()) {
        static_assert(N >= 2, "rotation_2d requires N >= 2");
        return plane_rotation(0, 1, angle, degrees, ctx);
    }

    static Derived rotation_x(const S& angle, bool degrees = false, const context_type& ctx = F::default_context()) {
        static_assert(N >= 3, "rotation_x requires N >= 3");
        return plane_rotation(1, 2, angle, degrees, ctx);
    }

    static Derived rotation_y(const S& angle, bool degrees = false, const context_type& ctx = F::default_context()) {
        static_assert(N >= 3, "rotation_y requires N >= 3");
        return plane_rotation(2, 0, angle, degrees, ctx);
    }

    static Derived rotation_z(const S& angle, bool degrees = false, const context_type& ctx = F::default_context()) {
        static_assert(N >= 3, "rotation_z requires N >= 3");
        return plane_rotation(0, 1, angle, degrees, ctx);
    }

    static Derived rotation_axis(const S& ax, const S& ay, const S& az, const S& angle, bool degrees = false, const context_type& ctx = F::default_context()) {
        static_assert(N >= 3, "rotation_axis requires N >= 3");
        const BigFloat x = to_bigfloat(ax);
        const BigFloat y = to_bigfloat(ay);
        const BigFloat z = to_bigfloat(az);
        const BigFloat t = to_bigfloat(angle);
        if (!x.is_finite() || !y.is_finite() || !z.is_finite() || !t.is_finite()) return Base::nan_matrix();
        if (x.is_zero() && y.is_zero() && z.is_zero()) return Base::identity();
        Derived m = Base::identity();

        if constexpr (scalar::dd::eligible<S>::value) {
            std::array<S, 9> f;

            if (scalar::dd::fast_axis_rotation(ax, ay, az, angle, degrees, f)) {
                for (std::size_t i = 0; i < 9; ++i) m.data[i / 3][i % 3] = f[i];
                return m;
            }
        }

        const std::vector<S> v = F::eval_vec(ctx, 9, [&](const BigFloatContext& w) { return scalar::kernel::axis_rotation(x, y, z, t, degrees, w); });
        for (std::size_t i = 0; i < 9; ++i) m.data[i / 3][i % 3] = v[i];
        return m;
    }

    static Derived perspective(const S& fov_y, const S& aspect, const S& near_plane, const S& far_plane, const context_type& ctx = F::default_context()) {
        static_assert(N == 4, "perspective requires N == 4");
        const BigFloat fov = to_bigfloat(fov_y);
        const BigFloat asp = to_bigfloat(aspect);
        const BigFloat n = to_bigfloat(near_plane);
        const BigFloat f = to_bigfloat(far_plane);
        const std::array<S, 2> cot = F::template eval_n<2>(ctx, [&](const BigFloatContext& w) {
            const BigFloatContext ww = scalar::kernel::widen(w, 8);
            const scalar::kernel::CosSin v = scalar::kernel::cos_sin(BigFloat::ldexp(fov, -1), false, ww);
            const BigFloat ct = BigFloat::div(v.cos, v.sin, ww);
            return std::array<BigFloat, 2>{ BigFloat::div(ct, asp, w), ct.rounded(w) };
        });

        Derived m;
        const BigFloat d = scalar::exact_sub(n, f);
        m.data[0][0] = cot[0];
        m.data[1][1] = cot[1];
        m.data[2][2] = F::ratio(scalar::exact_add(f, n), d, ctx);
        m.data[2][3] = F::ratio(BigFloat::ldexp(scalar::exact_mul(f, n), 1), d, ctx);
        m.data[3][2] = F::neg(F::one());
        return m;
    }

    static Derived ortho(const S& l, const S& r, const S& b, const S& t, const S& near_plane, const S& far_plane, const context_type& ctx = F::default_context()) {
        static_assert(N == 4, "ortho requires N == 4");
        const BigFloat L = to_bigfloat(l), R = to_bigfloat(r), B = to_bigfloat(b), T = to_bigfloat(t), Nn = to_bigfloat(near_plane), Ff = to_bigfloat(far_plane);
        const BigFloat two(2);
        Derived m = Base::identity();
        m.data[0][0] = F::ratio(two, scalar::exact_sub(R, L), ctx);
        m.data[1][1] = F::ratio(two, scalar::exact_sub(T, B), ctx);
        m.data[2][2] = F::ratio(-two, scalar::exact_sub(Ff, Nn), ctx);
        m.data[0][3] = F::ratio(-scalar::exact_add(R, L), scalar::exact_sub(R, L), ctx);
        m.data[1][3] = F::ratio(-scalar::exact_add(T, B), scalar::exact_sub(T, B), ctx);
        m.data[2][3] = F::ratio(-scalar::exact_add(Ff, Nn), scalar::exact_sub(Ff, Nn), ctx);
        return m;
    }

    static Derived look_at(const S& ex, const S& ey, const S& ez, const S& cx, const S& cy, const S& cz, const S& ux, const S& uy, const S& uz, const context_type& ctx = F::default_context()) {
        static_assert(N == 4, "look_at requires N == 4");
        const scalar::kernel::Vec eye{ to_bigfloat(ex), to_bigfloat(ey), to_bigfloat(ez) };
        const scalar::kernel::Vec center{ to_bigfloat(cx), to_bigfloat(cy), to_bigfloat(cz) };
        const scalar::kernel::Vec up{ to_bigfloat(ux), to_bigfloat(uy), to_bigfloat(uz) };
        if (!scalar::kernel::all_finite(eye) || !scalar::kernel::all_finite(center) || !scalar::kernel::all_finite(up)) return Base::nan_matrix();
        const std::vector<S> v = F::eval_vec(ctx, 16, [&](const BigFloatContext& w) { return scalar::kernel::look_at(eye, center, up, w); });
        return Base::from_flat(v);
    }
};

} // namespace matrix_detail

template <std::size_t N, class T>
struct Matrix : matrix_detail::MatrixNImpl<Matrix<N, T>, N, T> {
    static_assert(scalar::is_real<T>::value, "Matrix requires a standard or fixed-width floating-point type");
    using matrix_detail::MatrixNImpl<Matrix<N, T>, N, T>::MatrixNImpl;

    Matrix() = default;
};

template <std::size_t N>
struct BigMatrix : matrix_detail::MatrixNImpl<BigMatrix<N>, N, scalar::BigFloat> {
    using matrix_detail::MatrixNImpl<BigMatrix<N>, N, scalar::BigFloat>::MatrixNImpl;

    BigMatrix() = default;

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    explicit BigMatrix(const Matrix<N, T>& m) {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) this->data[r][c] = scalar::BigFloat(m.data[r][c]);
    }

    template <class T, typename std::enable_if<scalar::is_real<T>::value, int>::type = 0>
    Matrix<N, T> to() const {
        Matrix<N, T> out;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) out.data[r][c] = scalar::from_big<T>(this->data[r][c]);
        return out;
    }

    BigMatrix rounded(const scalar::BigFloatContext& ctx) const {
        BigMatrix out;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) out.data[r][c] = this->data[r][c].rounded(ctx);
        return out;
    }
};

using matrix_detail::commutator;
using matrix_detail::inner_product;

template <class T, std::size_t N, std::size_t M>
Matrix<N * M, T> kronecker(const Matrix<N, T>& A, const Matrix<M, T>& B) {
    Matrix<N * M, T> K;

    for (std::size_t ar = 0; ar < N; ++ar)
        for (std::size_t ac = 0; ac < N; ++ac)
            for (std::size_t br = 0; br < M; ++br)
                for (std::size_t bc = 0; bc < M; ++bc)
                    K.data[ar * M + br][ac * M + bc] = A.data[ar][ac] * B.data[br][bc];

    return K;
}

template <std::size_t N, std::size_t M>
BigMatrix<N * M> kronecker(const BigMatrix<N>& A, const BigMatrix<M>& B, const scalar::BigFloatContext& ctx = scalar::BigFloatContext::current()) {
    BigMatrix<N * M> K;

    for (std::size_t ar = 0; ar < N; ++ar)
        for (std::size_t ac = 0; ac < N; ++ac)
            for (std::size_t br = 0; br < M; ++br)
                for (std::size_t bc = 0; bc < M; ++bc)
                    K.data[ar * M + br][ac * M + bc] = scalar::BigFloat::mul(A.data[ar][ac], B.data[br][bc], ctx);

    return K;
}

} // namespace linalg
} // namespace fizmo

#endif // FIZMO_MATRIX_HPP
