#ifndef FIZMO_MULTIPRECISION_FIXED_POINT_GAMMA_HPP
#define FIZMO_MULTIPRECISION_FIXED_POINT_GAMMA_HPP

#include <vector>
#include "fixed_point_math.hpp"
#include "../Big/BigFloat Math/bernoulli_tangent.hpp"

namespace fizmo {
namespace multiprecision {
namespace fpmath {

template <class F>
struct fixed_gamma : fixed_for<F> {
    using B   = fixed_for<F>;
    using C   = typename B::C;
    using fx  = typename B::fx;
    using fw  = typename B::fw;
    using sfx = typename B::sfx;
    using KC  = typename B::K;
    static constexpr std::size_t N = B::N;
    static constexpr long long frac = B::frac;
    static constexpr std::uint64_t X0 = static_cast<std::uint64_t>(frac) / 4 + 4;
    static constexpr std::size_t KMAX = static_cast<std::size_t>(frac) * 2 / 15 + 4;

    struct tables {
        std::vector<sfx> st;
        std::vector<sfx> dg;
        std::vector<sfx> tg;
        fx half_ln_2pi;
    };

    static fx from_big(const BigFloat& v) noexcept {
        fx r;
        if (!v.is_finite() || v.is_zero()) return r;
        BigUInt m = v.significand();
        const long long sh = static_cast<long long>(frac) + v.exponent();
        if (sh >= 0) m.shift_left_mutable(static_cast<std::size_t>(sh));
        else m.shift_right_mutable(static_cast<std::size_t>(-sh));
        for (std::size_t i = 0; i < N && !m.is_zero(); ++i) {
            r.v[i] = m.get_lowest_bits();
            m.shift_right_mutable(64);
        }
        return r;
    }

    static const tables& tab() {
        static const tables t = [] {
            tables r;
            const BigFloatContext ctx(static_cast<std::size_t>(frac) + 96, RoundingMode::nearest_even);
            const BigFloat x2(static_cast<std::uint64_t>(X0 * X0));
            BigFloat scale = BigFloat::one();
            for (std::size_t k = 1; k <= KMAX; ++k) {
                const BigFloat b = constants::bernoulli_number(2 * k).to_bigfloat(ctx);
                const bool neg = b.signbit();
                const BigFloat ab = BigFloat::div(b.abs(), scale, ctx);
                scale = BigFloat::mul(scale, x2, ctx);
                r.st.push_back(sfx{ from_big(BigFloat::div(ab, BigFloat(static_cast<std::uint64_t>((2 * k) * (2 * k - 1))), ctx)), neg });
                r.dg.push_back(sfx{ from_big(BigFloat::div(ab, BigFloat(static_cast<std::uint64_t>(2 * k)), ctx)), neg });
                r.tg.push_back(sfx{ from_big(ab), neg });
            }
            const sfx lp = KC::log_pos(pi_constant<N>());
            r.half_ln_2pi = (lp.m + ln2_constant<N>()).shr(1);
            return r;
        }();
        return t;
    }

    static sfx horner(const std::vector<sfx>& c, const fx& u) noexcept {
        const fx w0 = u.mul_small(X0);
        const fx z = w0 * w0;
        sfx acc = c.back();
        for (std::size_t k = c.size() - 1; k-- > 0;) acc = c[k] + sfx{ z, false } * acc;
        return acc;
    }

    static std::uint64_t shift_for(const fx& X) noexcept {
        const double xd = X.to_double();
        if (xd >= static_cast<double>(X0)) return 0;
        return static_cast<std::uint64_t>(std::ceil(static_cast<double>(X0) - xd));
    }

    static sfx log_shift_product(const fx& X, std::uint64_t n) noexcept {
        if (n == 0) return sfx{ fx(), false };
        fx pm = fx::one();
        long long pe = 0;
        const fx u = X * X + X.mul_small(n - 1);
        auto mul_in = [&](fx f) {
            const long long ft = f.top();
            if (ft < frac) { f = f.shl(static_cast<std::size_t>(frac - ft)); pe -= frac - ft; }
            pm = pm * f;
            const long long t = pm.top();
            if (t > frac) { pm = pm.shr(static_cast<std::size_t>(t - frac)); pe += t - frac; }
        };
        if (n >= 2) {
            mul_in(X);
            mul_in(X + fx::from_int(n - 1));
        }
        for (std::uint64_t j = 1; j < n / 2; ++j) mul_in(u + fx::from_int(j * (n - 1 - j)));
        if (n & 1u) mul_in(X + fx::from_int((n - 1) / 2));
        sfx r = KC::log_pos(pm);
        if (pe != 0) r = r + sfx{ ln2_constant<N + 1>().mul_small(static_cast<std::uint64_t>(pe < 0 ? -pe : pe)).template resize<N>(), pe < 0 };
        return r;
    }

    static bool lgamma_core(const fx& X, sfx& L, long long& err) noexcept {
        if (X.is_zero()) return false;
        const std::uint64_t n = shift_for(X);
        const fx y = X + fx::from_int(n);
        if (y.top() >= frac + 40) return false;
        const tables& t = tab();
        const sfx ly = KC::log_pos(y);
        const fx u = fdiv(fx::one(), y);
        const sfx S = sfx{ u, false } * horner(t.st, u);
        const fx half = fx::one().shr(1);
        sfx lg = sfx{ y - half, false } * ly;
        lg = lg - sfx{ y, false } + sfx{ t.half_ln_2pi, false } + S;
        L = lg - log_shift_product(X, n);
        err = 14 + (y.top() - frac) + 2 + static_cast<long long>(ztlen(n));
        return true;
    }

    static std::size_t ztlen(std::uint64_t v) noexcept {
        std::size_t r = 0;
        while (v) { ++r; v >>= 1; }
        return r;
    }

    static bool load_pos(const C& x, fx& X) noexcept {
        long long top = 0;
        if (!B::load(x, X, top) || x.is_negative()) return false;
        return top >= -60;
    }

    static bool reflect(const C& x, fx& Y, sfx& sp, sfx& cp) noexcept {
        if (!x.is_finite() || x.is_zero() || !x.is_negative()) return false;
        if (!B::sincos_pi(x, sp, cp) || sp.m.is_zero()) return false;
        long long top = 0;
        fx X;
        if (!B::load(x, X, top) || top >= 40) return false;
        Y = X + fx::one();
        return true;
    }

    static bool gamma(const C& x, C& out) noexcept {
        fx X;
        if (x.is_negative()) {
            fx Y;
            sfx sp, cp;
            if (!reflect(x, Y, sp, cp)) return false;
            sfx L;
            long long err = 0;
            if (!lgamma_core(Y, L, err)) return false;
            const sfx ls = KC::log_pos(sp.m);
            const sfx lp = KC::log_pos(pi_constant<N>());
            const sfx T = lp - ls - L;
            if (T.m.top() >= frac + 40) return false;
            fx sum;
            long long k = 0;
            if (!KC::exp_reduce(T.m.template resize<N + 1>(), T.neg, sum, k)) return false;
            const long long e = (err > B::exp_err ? err : B::exp_err) + 4 + B::amp(sp.m);
            return B::finish(sp.neg, sum, k, e, out);
        }
        if (!load_pos(x, X)) return false;
        return gamma_fixed(X, out);
    }

    static bool log_abs_gamma(const C& x, C& out) noexcept {
        if (!x.is_negative()) return log_gamma(x, out);
        fx Y;
        sfx sp, cp;
        if (!reflect(x, Y, sp, cp)) return false;
        sfx L;
        long long err = 0;
        if (!lgamma_core(Y, L, err)) return false;
        const sfx T = KC::log_pos(pi_constant<N>()) - KC::log_pos(sp.m) - L;
        return B::finish(T.neg, T.m, 0, err + 2 + B::amp(sp.m), out);
    }

    static bool gamma_fixed(const fx& X, C& out) noexcept {
        sfx L;
        long long err = 0;
        if (!lgamma_core(X, L, err)) return false;
        if (L.m.top() >= frac + 40) return false;
        fx sum;
        long long k = 0;
        if (!KC::exp_reduce(L.m.template resize<N + 1>(), L.neg, sum, k)) return false;
        const long long e = (err > B::exp_err ? err : B::exp_err) + 3;
        return B::finish(false, sum, k, e, out);
    }

    static bool factorial(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!B::load(x, X, top) || x.is_negative() || top < -60) return false;
        return gamma_fixed(X + fx::one(), out);
    }

    static bool log_gamma(const C& x, C& out) noexcept {
        fx X;
        if (!load_pos(x, X)) return false;
        sfx L;
        long long err = 0;
        if (!lgamma_core(X, L, err)) return false;
        return B::finish(L.neg, L.m, 0, err, out);
    }

    static void recip_sum(const fx& X, std::uint64_t n, bool squares, fx& acc) noexcept {
        acc = fx();
        if (n == 0) return;
        const fx u = X * X + X.mul_small(n - 1);
        const fx s = X.shl(1) + fx::from_int(n - 1);
        const fx s2 = squares ? s * s : fx();
        for (std::uint64_t j = 0; j < n / 2; ++j) {
            const fx q = u + fx::from_int(j * (n - 1 - j));
            if (squares) acc = acc + fdiv(s2 - q.shl(1), q * q);
            else acc = acc + fdiv(s, q);
        }
        if (n & 1u) {
            const fx m = X + fx::from_int((n - 1) / 2);
            acc = acc + (squares ? fdiv(fx::one(), m * m) : fdiv(fx::one(), m));
        }
    }

    static bool digamma_pos(const fx& X, sfx& r, long long& err) noexcept {
        const std::uint64_t n = shift_for(X);
        const fx y = X + fx::from_int(n);
        const tables& t = tab();
        const sfx ly = KC::log_pos(y);
        const fx u = fdiv(fx::one(), y);
        const fx z = u * u;
        const sfx T = sfx{ z, false } * horner(t.dg, u);
        fx rs;
        recip_sum(X, n, false, rs);
        r = ly - sfx{ u.shr(1), false } - T - sfx{ rs, false };
        err = 16 + static_cast<long long>(ztlen(n)) + (X.top() < frac ? 2 * (frac - X.top()) : 0);
        return true;
    }

    static bool digamma(const C& x, C& out) noexcept {
        fx X;
        if (x.is_negative()) {
            fx Y;
            sfx sp, cp;
            if (!reflect(x, Y, sp, cp)) return false;
            sfx r;
            long long err = 0;
            if (!digamma_pos(Y, r, err)) return false;
            const sfx ct{ fdiv(cp.m, sp.m) * pi_constant<N>(), cp.neg != sp.neg };
            const sfx v = r - ct;
            return B::finish(v.neg, v.m, 0, err + 4 + 2 * B::amp(sp.m), out);
        }
        if (!load_pos(x, X)) return false;
        const std::uint64_t n = shift_for(X);
        const fx y = X + fx::from_int(n);
        const tables& t = tab();
        const sfx ly = KC::log_pos(y);
        const fx u = fdiv(fx::one(), y);
        const fx z = u * u;
        const sfx T = sfx{ z, false } * horner(t.dg, u);
        fx rs;
        recip_sum(X, n, false, rs);
        const sfx r = ly - sfx{ u.shr(1), false } - T - sfx{ rs, false };
        return B::finish(r.neg, r.m, 0, 16 + static_cast<long long>(ztlen(n)) + (X.top() < frac ? 2 * (frac - X.top()) : 0), out);
    }

    static bool trigamma(const C& x, C& out) noexcept {
        fx X;
        if (!load_pos(x, X)) return false;
        const std::uint64_t n = shift_for(X);
        const fx y = X + fx::from_int(n);
        const tables& t = tab();
        const fx u = fdiv(fx::one(), y);
        const fx z = u * u;
        const sfx inner = sfx{ fx::one() + u.shr(1), false } + sfx{ z, false } * horner(t.tg, u);
        const fx head = (u * inner.m);
        fx rs;
        recip_sum(X, n, true, rs);
        const fx r = head + rs;
        return B::finish(false, r, 0, 16 + static_cast<long long>(ztlen(n)) + (X.top() < frac ? 3 * (frac - X.top()) : 0), out);
    }
};

} // namespace fpmath
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_POINT_GAMMA_HPP
