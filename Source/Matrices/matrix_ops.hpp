#ifndef FIZMO_MATRICES_MATRIX_OPS_HPP
#define FIZMO_MATRICES_MATRIX_OPS_HPP

#include <array>
#include <cstddef>
#include <ostream>
#include <sstream>
#include <string>
#include <type_traits>
#include <utility>

#include "../Vectors/kvectors.hpp"

namespace fizmo {
namespace linalg {
namespace matrix_detail {

using scalar::BigFloat;
using scalar::BigFloatContext;
using detail::is_scalar_operand;
using detail::if_scalar_operand;
using detail::to_bigfloat;

template <class Derived, std::size_t N, class S>
class SquareMatrixOps {
public:
    using value_type   = S;
    using field_type   = scalar::field<S>;
    using context_type = typename field_type::context_type;
    using row_type     = std::array<S, N>;

    static constexpr std::size_t size = N;

    struct LUResult {
        Derived L;
        Derived U;
        Derived P;
        int     sign;
    };

    struct QRResult {
        Derived Q;
        Derived R;
    };

protected:
    using F   = field_type;
    using Vec = scalar::kernel::Vec;

    const Derived& self() const noexcept { return static_cast<const Derived&>(*this); }
    Derived&       self() noexcept { return static_cast<Derived&>(*this); }

    const S& get(std::size_t r, std::size_t c) const { return self().entry(r, c); }
    S&       get(std::size_t r, std::size_t c) { return self().entry(r, c); }

    template <class Fn>
    static Derived generate(Fn&& f) {
        Derived d;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) d.entry(r, c) = f(r, c);
        return d;
    }

    template <class Container>
    static Derived from_flat(const Container& v) {
        return generate([&](std::size_t r, std::size_t c) { return S(v[r * N + c]); });
    }

    static Derived rounded_from(const Vec& v, const context_type& ctx) {
        return generate([&](std::size_t r, std::size_t c) { return F::round(ctx, v[r * N + c]); });
    }

    static Derived nan_matrix() { return generate([](std::size_t, std::size_t) { return F::nan(); }); }

    Vec big() const {
        Vec v(N * N);
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) v[r * N + c] = to_bigfloat(get(r, c));
        return v;
    }

    bool all_finite() const {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (!F::is_finite(get(r, c))) return false;
        return true;
    }

    static S exact_entry(const BigFloat& v, const context_type& ctx) { return F::round(ctx, v); }

public:
    static Derived zero() { return Derived(); }
    static Derived filled(const S& v) { return generate([&](std::size_t, std::size_t) { return v; }); }
    static Derived identity() { return generate([](std::size_t r, std::size_t c) { return r == c ? F::one() : F::zero(); }); }
    static Derived diagonal(const std::array<S, N>& d) { return generate([&](std::size_t r, std::size_t c) { return r == c ? d[r] : F::zero(); }); }
    static Derived scaling(const S& s) { return generate([&](std::size_t r, std::size_t c) { return r == c ? s : F::zero(); }); }
    static Derived scaling(const std::array<S, N>& s) { return diagonal(s); }

    static Derived shear(std::size_t along, std::size_t by, const S& k) {
        Derived m = identity();
        if (along != by && along < N && by < N) m.entry(along, by) = k;
        return m;
    }

    static Derived outer(const std::array<S, N>& a, const std::array<S, N>& b, const context_type& ctx = F::default_context()) {
        return generate([&](std::size_t r, std::size_t c) { return F::mul(a[r], b[c], ctx); });
    }

    static Derived reflection(const std::array<S, N>& n, const context_type& ctx = F::default_context()) {
        std::array<BigFloat, N> b;
        for (std::size_t i = 0; i < N; ++i) b[i] = to_bigfloat(n[i]);
        return generate([&](std::size_t r, std::size_t c) {
            const BigFloat t = BigFloat::ldexp(scalar::exact_mul(b[r], b[c]), 1);
            return F::round(ctx, r == c ? scalar::exact_sub(BigFloat::one(), t) : -t);
        });
    }

    static Derived projection(const std::array<S, N>& n, const context_type& ctx = F::default_context()) {
        std::array<BigFloat, N> b;
        for (std::size_t i = 0; i < N; ++i) b[i] = to_bigfloat(n[i]);
        return generate([&](std::size_t r, std::size_t c) {
            const BigFloat t = scalar::exact_mul(b[r], b[c]);
            return F::round(ctx, r == c ? scalar::exact_sub(BigFloat::one(), t) : -t);
        });
    }

    const S& at(std::size_t r, std::size_t c) const { return get(r, c); }
    S&       at(std::size_t r, std::size_t c) { return get(r, c); }

    bool operator==(const Derived& o) const {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (!(get(r, c) == o.get(r, c))) return false;
        return true;
    }

    bool operator!=(const Derived& o) const { return !(*this == o); }

    bool approx_equal(const Derived& o, const S& eps = F::default_epsilon()) const {
        const BigFloat e = to_bigfloat(eps);

        for (std::size_t r = 0; r < N; ++r) {
            for (std::size_t c = 0; c < N; ++c) {
                const BigFloat a = to_bigfloat(get(r, c));
                const BigFloat b = to_bigfloat(o.get(r, c));
                if (!a.is_finite() || !b.is_finite()) { if (!(get(r, c) == o.get(r, c))) return false; continue; }
                if (scalar::exact_sub(a, b).abs() > e) return false;
            }
        }

        return true;
    }

    Derived add(const Derived& o, const context_type& ctx = F::default_context()) const { return generate([&](std::size_t r, std::size_t c) { return F::add(get(r, c), o.get(r, c), ctx); }); }
    Derived sub(const Derived& o, const context_type& ctx = F::default_context()) const { return generate([&](std::size_t r, std::size_t c) { return F::sub(get(r, c), o.get(r, c), ctx); }); }
    Derived negated() const { return generate([&](std::size_t r, std::size_t c) { return F::neg(get(r, c)); }); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived add_scalar(const U& s, const context_type& ctx = F::default_context()) const {
        if constexpr (std::is_same<U, S>::value) {
            return generate([&](std::size_t r, std::size_t c) { return F::add(get(r, c), s, ctx); });
        } else {
            const BigFloat b = to_bigfloat(s);
            if (!b.is_finite() || !all_finite()) return add_scalar(F::from(s), ctx);
            return generate([&](std::size_t r, std::size_t c) { return F::round(ctx, scalar::exact_add(to_bigfloat(get(r, c)), b)); });
        }
    }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived sub_scalar(const U& s, const context_type& ctx = F::default_context()) const {
        if constexpr (std::is_same<U, S>::value) {
            return generate([&](std::size_t r, std::size_t c) { return F::sub(get(r, c), s, ctx); });
        } else {
            const BigFloat b = to_bigfloat(s);
            if (!b.is_finite() || !all_finite()) return sub_scalar(F::from(s), ctx);
            return generate([&](std::size_t r, std::size_t c) { return F::round(ctx, scalar::exact_sub(to_bigfloat(get(r, c)), b)); });
        }
    }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived scaled(const U& s, const context_type& ctx = F::default_context()) const { return generate([&](std::size_t r, std::size_t c) { return F::mul_mixed(get(r, c), s, ctx); }); }

    template <class U, if_scalar_operand<S, U> = 0>
    Derived divided(const U& s, const context_type& ctx = F::default_context()) const { return generate([&](std::size_t r, std::size_t c) { return F::div_mixed(get(r, c), s, ctx); }); }

    Derived mul(const Derived& o, const context_type& ctx = F::default_context()) const {
        std::array<S, N> col;
        Derived out;

        for (std::size_t c = 0; c < N; ++c) {
            for (std::size_t k = 0; k < N; ++k) col[k] = o.get(k, c);

            for (std::size_t r = 0; r < N; ++r) {
                std::array<S, N> row;
                for (std::size_t k = 0; k < N; ++k) row[k] = get(r, k);
                out.entry(r, c) = F::dot(row.data(), col.data(), N, ctx);
            }
        }

        return out;
    }

    std::array<S, N> mul(const std::array<S, N>& v, const context_type& ctx = F::default_context()) const {
        std::array<S, N> out;

        for (std::size_t r = 0; r < N; ++r) {
            std::array<S, N> row;
            for (std::size_t k = 0; k < N; ++k) row[k] = get(r, k);
            out[r] = F::dot(row.data(), v.data(), N, ctx);
        }

        return out;
    }

    Derived hadamard(const Derived& o, const context_type& ctx = F::default_context()) const { return generate([&](std::size_t r, std::size_t c) { return F::mul(get(r, c), o.get(r, c), ctx); }); }

    Derived operator+(const Derived& o) const { return add(o); }
    Derived operator-(const Derived& o) const { return sub(o); }
    Derived operator-() const { return negated(); }
    Derived operator+() const { return self(); }
    Derived operator*(const Derived& o) const { return mul(o); }
    std::array<S, N> operator*(const std::array<S, N>& v) const { return mul(v); }

    template <class U, if_scalar_operand<S, U> = 0> Derived operator+(const U& s) const { return add_scalar(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived operator-(const U& s) const { return sub_scalar(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived operator*(const U& s) const { return scaled(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived operator/(const U& s) const { return divided(s); }

    Derived& operator+=(const Derived& o) { return self() = add(o); }
    Derived& operator-=(const Derived& o) { return self() = sub(o); }
    Derived& operator*=(const Derived& o) { return self() = mul(o); }

    template <class U, if_scalar_operand<S, U> = 0> Derived& operator+=(const U& s) { return self() = add_scalar(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived& operator-=(const U& s) { return self() = sub_scalar(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived& operator*=(const U& s) { return self() = scaled(s); }
    template <class U, if_scalar_operand<S, U> = 0> Derived& operator/=(const U& s) { return self() = divided(s); }

    template <class U, if_scalar_operand<S, U> = 0>
    friend Derived operator*(const U& s, const Derived& m) { return m.scaled(s); }

    S trace(const context_type& ctx = F::default_context()) const {
        std::array<S, N> d;
        for (std::size_t i = 0; i < N; ++i) d[i] = get(i, i);
        return F::sum(d.data(), N, ctx);
    }

    S determinant(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return F::round(ctx, scalar::kernel::naive_det(big(), N, scalar::context(64)));
        return F::round(ctx, scalar::kernel::det(big(), N));
    }

    Derived transpose() const { return generate([&](std::size_t r, std::size_t c) { return get(c, r); }); }

    S frobenius_norm(const context_type& ctx = F::default_context()) const {
        std::array<S, N * N> a;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) a[r * N + c] = get(r, c);
        return F::norm(a.data(), N * N, ctx);
    }

    S max_abs() const {
        S m = F::abs(get(0, 0));

        for (std::size_t r = 0; r < N; ++r) {
            for (std::size_t c = 0; c < N; ++c) {
                const S v = F::abs(get(r, c));
                if (F::is_nan(v)) return v;
                if (v > m) m = v;
            }
        }

        return m;
    }

    bool is_symmetric() const {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = r + 1; c < N; ++c) if (!(get(r, c) == get(c, r))) return false;
        return true;
    }

    bool is_diagonal() const {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (r != c && !F::is_zero(get(r, c))) return false;
        return true;
    }

    bool is_identity() const { return *this == identity(); }
    bool is_identity(const S& eps) const { return approx_equal(identity(), eps); }

    bool is_zero() const {
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) if (!F::is_zero(get(r, c))) return false;
        return true;
    }

    bool is_zero(const S& eps) const { return approx_equal(Derived(), eps); }
    bool is_singular() const { return all_finite() && scalar::kernel::det(big(), N).is_zero(); }
    bool is_invertible() const { return all_finite() && !scalar::kernel::det(big(), N).is_zero(); }

    std::array<S, N> row(std::size_t r) const {
        std::array<S, N> v;
        for (std::size_t c = 0; c < N; ++c) v[c] = get(r, c);
        return v;
    }

    std::array<S, N> col(std::size_t c) const {
        std::array<S, N> v;
        for (std::size_t r = 0; r < N; ++r) v[r] = get(r, c);
        return v;
    }

    Derived set_row(std::size_t r, const std::array<S, N>& v) const {
        Derived m = self();
        for (std::size_t c = 0; c < N; ++c) m.entry(r, c) = v[c];
        return m;
    }

    Derived set_col(std::size_t c, const std::array<S, N>& v) const {
        Derived m = self();
        for (std::size_t r = 0; r < N; ++r) m.entry(r, c) = v[r];
        return m;
    }

    Derived swap_rows(std::size_t r1, std::size_t r2) const {
        Derived m = self();
        for (std::size_t c = 0; c < N; ++c) std::swap(m.entry(r1, c), m.entry(r2, c));
        return m;
    }

    Derived swap_cols(std::size_t c1, std::size_t c2) const {
        Derived m = self();
        for (std::size_t r = 0; r < N; ++r) std::swap(m.entry(r, c1), m.entry(r, c2));
        return m;
    }

    Derived scale_row(std::size_t r, const S& s, const context_type& ctx = F::default_context()) const {
        Derived m = self();
        for (std::size_t c = 0; c < N; ++c) m.entry(r, c) = F::mul(m.entry(r, c), s, ctx);
        return m;
    }

    Derived scale_col(std::size_t c, const S& s, const context_type& ctx = F::default_context()) const {
        Derived m = self();
        for (std::size_t r = 0; r < N; ++r) m.entry(r, c) = F::mul(m.entry(r, c), s, ctx);
        return m;
    }

    Derived add_scaled_row(std::size_t dst, std::size_t src, const S& s, const context_type& ctx = F::default_context()) const {
        Derived m = self();
        for (std::size_t c = 0; c < N; ++c) m.entry(dst, c) = F::sum_of_products(F::one(), get(dst, c), s, get(src, c), ctx);
        return m;
    }

    Derived cofactor_matrix(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return nan_matrix();
        return rounded_from(scalar::kernel::cofactors(big(), N), ctx);
    }

    Derived adjugate(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return nan_matrix();
        return rounded_from(scalar::kernel::adjugate(big(), N), ctx);
    }

    Derived inverse(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return nan_matrix();
        const Vec m = big();
        const BigFloat d = scalar::kernel::det(m, N);
        if (d.is_zero()) return nan_matrix();
        const Vec adj = scalar::kernel::adjugate(m, N);
        return generate([&](std::size_t r, std::size_t c) { return F::ratio(adj[r * N + c], d, ctx); });
    }

    Derived pinverse(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return nan_matrix();
        const Vec m = big();
        if (!scalar::kernel::det(m, N).is_zero()) return inverse(ctx);
        const std::vector<std::size_t> pivots = scalar::kernel::pivot_columns(m, N, N);
        const std::vector<S> v = F::eval_vec(ctx, N * N, [&](const BigFloatContext& w) { return scalar::kernel::pinverse(m, N, pivots, w); });
        return from_flat(v);
    }

    std::array<S, N> solve(const std::array<S, N>& b, const context_type& ctx = F::default_context()) const {
        std::array<S, N> x;
        x.fill(F::nan());
        if (!all_finite()) return x;
        for (std::size_t i = 0; i < N; ++i) if (!F::is_finite(b[i])) return x;

        const Vec m = big();
        const BigFloat d = scalar::kernel::det(m, N);
        if (d.is_zero()) return x;

        for (std::size_t i = 0; i < N; ++i) {
            Vec mi = m;
            for (std::size_t r = 0; r < N; ++r) mi[r * N + i] = to_bigfloat(b[r]);
            x[i] = F::ratio(scalar::kernel::det(mi, N), d, ctx);
        }

        return x;
    }

    LUResult lu(const context_type& ctx = F::default_context()) const {
        const Vec m = big();
        const scalar::kernel::ExactLU e = scalar::kernel::exact_lu(m, N);

        if (e.ok) {
            LUResult r{ Derived(), Derived(), Derived(), e.sign };

            for (std::size_t i = 0; i < N; ++i) {
                for (std::size_t j = 0; j < N; ++j) {
                    r.L.entry(i, j) = F::ratio(e.l_num[i * N + j], e.l_den[i * N + j], ctx);
                    r.U.entry(i, j) = F::ratio(e.u_num[i * N + j], e.u_den[i * N + j], ctx);
                    r.P.entry(i, j) = e.perm[i] == j ? F::one() : F::zero();
                }
            }

            return r;
        }

        const std::vector<S> v = F::eval_vec(ctx, 3 * N * N + 1, [&](const BigFloatContext& w) { return scalar::kernel::lu_working(m, N, w); });
        LUResult r{ Derived(), Derived(), Derived(), F::signbit(v[3 * N * N]) ? -1 : 1 };

        for (std::size_t i = 0; i < N * N; ++i) {
            r.L.entry(i / N, i % N) = v[i];
            r.U.entry(i / N, i % N) = v[N * N + i];
            r.P.entry(i / N, i % N) = v[2 * N * N + i];
        }

        return r;
    }

    QRResult qr(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return { nan_matrix(), nan_matrix() };
        const Vec m = big();
        const std::vector<S> v = F::eval_vec(ctx, 2 * N * N, [&](const BigFloatContext& w) { return scalar::kernel::qr(m, N, w); });
        QRResult r{ Derived(), Derived() };

        for (std::size_t i = 0; i < N * N; ++i) {
            r.Q.entry(i / N, i % N) = v[i];
            r.R.entry(i / N, i % N) = v[N * N + i];
        }

        return r;
    }

    Derived pow(long long exp, const context_type& ctx = F::default_context()) const {
        if (exp == 0) return identity();
        if (exp == 1) return self();
        if (!all_finite()) return nan_matrix();
        const Vec m = big();
        if (exp < 0 && scalar::kernel::det(m, N).is_zero()) return nan_matrix();
        const std::vector<S> v = F::eval_vec(ctx, N * N, [&](const BigFloatContext& w) { return scalar::kernel::matrix_pow(m, N, exp, w); });
        return from_flat(v);
    }

    template <class Fn>
    Derived apply(Fn&& fn) const { return generate([&](std::size_t r, std::size_t c) { return S(fn(get(r, c))); }); }

    Derived abs_matrix() const { return generate([&](std::size_t r, std::size_t c) { return F::abs(get(r, c)); }); }

    S sum(const context_type& ctx = F::default_context()) const {
        std::array<S, N * N> a;
        for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) a[r * N + c] = get(r, c);
        return F::sum(a.data(), N * N, ctx);
    }

    S product(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) {
            S p = F::one();
            for (std::size_t r = 0; r < N; ++r) for (std::size_t c = 0; c < N; ++c) p = F::mul(p, get(r, c), ctx);
            return p;
        }

        const Vec a = big();
        return F::round(ctx, scalar::kernel::exact_product(a.data(), N * N));
    }

    std::array<S, N> col_sums(const context_type& ctx = F::default_context()) const {
        std::array<S, N> out;
        for (std::size_t c = 0; c < N; ++c) { const std::array<S, N> v = col(c); out[c] = F::sum(v.data(), N, ctx); }
        return out;
    }

    std::array<S, N> row_sums(const context_type& ctx = F::default_context()) const {
        std::array<S, N> out;
        for (std::size_t r = 0; r < N; ++r) { const std::array<S, N> v = row(r); out[r] = F::sum(v.data(), N, ctx); }
        return out;
    }

    S log_det(const context_type& ctx = F::default_context()) const {
        if (!all_finite()) return F::nan();
        const BigFloat d = scalar::kernel::det(big(), N).abs();
        if (d.is_zero()) return F::infinity(true);
        return F::eval(ctx, [&](const BigFloatContext& w) { return math::ln(d, w); });
    }

    std::size_t rank() const { return scalar::kernel::rank(big(), N, N); }

    static std::array<S, N> back_substitute(const Derived& U, const std::array<S, N>& b, const context_type& ctx = F::default_context()) {
        const Vec u = U.big();
        Vec bb(N);
        for (std::size_t i = 0; i < N; ++i) bb[i] = to_bigfloat(b[i]);
        const std::vector<S> v = F::eval_vec(ctx, N, [&](const BigFloatContext& w) {
            Vec x(N, BigFloat::zero());

            for (std::size_t k = N; k-- > 0;) {
                BigFloat s = bb[k];
                for (std::size_t c = k + 1; c < N; ++c) s = BigFloat::sub(s, BigFloat::mul(u[k * N + c], x[c], w), w);
                x[k] = u[k * N + k].is_zero() ? BigFloat::zero() : BigFloat::div(s, u[k * N + k], w);
            }

            return x;
        });

        std::array<S, N> out;
        for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
        return out;
    }

    static std::array<S, N> forward_substitute(const Derived& L, const std::array<S, N>& b, const context_type& ctx = F::default_context()) {
        const Vec l = L.big();
        Vec bb(N);
        for (std::size_t i = 0; i < N; ++i) bb[i] = to_bigfloat(b[i]);
        const std::vector<S> v = F::eval_vec(ctx, N, [&](const BigFloatContext& w) {
            Vec x(N, BigFloat::zero());

            for (std::size_t k = 0; k < N; ++k) {
                BigFloat s = bb[k];
                for (std::size_t c = 0; c < k; ++c) s = BigFloat::sub(s, BigFloat::mul(l[k * N + c], x[c], w), w);
                x[k] = s;
            }

            return x;
        });

        std::array<S, N> out;
        for (std::size_t i = 0; i < N; ++i) out[i] = v[i];
        return out;
    }

    friend std::ostream& operator<<(std::ostream& os, const SquareMatrixOps& m) {
        os << "[";

        for (std::size_t r = 0; r < N; ++r) {
            os << (r == 0 ? "[" : " [");

            for (std::size_t c = 0; c < N; ++c) {
                if (c > 0) os << ", ";
                detail::write_scalar(os, m.get(r, c));
            }

            os << "]";
            if (r + 1 < N) os << "\n";
        }

        os << "]";
        return os;
    }

    std::string to_string() const {
        std::ostringstream os;
        os << *this;
        return os.str();
    }
};

template <class Derived, std::size_t N, class S>
Derived commutator(const SquareMatrixOps<Derived, N, S>& A, const SquareMatrixOps<Derived, N, S>& B) {
    using F = scalar::field<S>;
    const typename F::context_type ctx = F::default_context();
    Derived out;

    for (std::size_t r = 0; r < N; ++r) {
        for (std::size_t c = 0; c < N; ++c) {
            std::array<S, 2 * N> x;
            std::array<S, 2 * N> y;

            for (std::size_t k = 0; k < N; ++k) {
                x[k] = A.at(r, k);
                y[k] = B.at(k, c);
                x[N + k] = F::neg(B.at(r, k));
                y[N + k] = A.at(k, c);
            }

            out.at(r, c) = F::dot(x.data(), y.data(), 2 * N, ctx);
        }
    }

    return out;
}

template <class Derived, std::size_t N, class S>
S inner_product(const SquareMatrixOps<Derived, N, S>& A, const SquareMatrixOps<Derived, N, S>& B) {
    using F = scalar::field<S>;
    std::array<S, N * N> a;
    std::array<S, N * N> b;

    for (std::size_t r = 0; r < N; ++r) {
        for (std::size_t c = 0; c < N; ++c) {
            a[r * N + c] = A.at(r, c);
            b[r * N + c] = B.at(r, c);
        }
    }

    return F::dot(a.data(), b.data(), N * N, F::default_context());
}

} // namespace matrix_detail
} // namespace linalg
} // namespace fizmo

#endif // FIZMO_MATRICES_MATRIX_OPS_HPP
