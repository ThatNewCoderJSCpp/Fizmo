#ifndef FIZMO_MULTIPRECISION_FIXED_POINT_MATH_HPP
#define FIZMO_MULTIPRECISION_FIXED_POINT_MATH_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "wide_float_ops.hpp"

#ifdef hyper
#undef hyper
#endif

namespace fizmo {
namespace multiprecision {
namespace fpmath {

struct noinit_t {};

template <std::size_t N>
struct fixed {
    std::uint64_t v[N];

    fixed() noexcept : v{} {}
    explicit fixed(noinit_t) noexcept {}

    static constexpr std::size_t frac_bits = 64 * (N - 1);

    bool is_zero() const noexcept { return fdetail::wmath::is_zero(v, N); }

    long long top() const noexcept { return fdetail::wmath::top_bit(v, N); }

    static fixed one() noexcept { fixed r; r.v[N - 1] = 1; return r; }

    static fixed from_int(std::uint64_t k) noexcept { fixed r; r.v[N - 1] = k; return r; }

    friend fixed operator+(const fixed& a, const fixed& b) noexcept { fixed r{ noinit_t{} }; fdetail::wmath::add(r.v, a.v, b.v, N); return r; }
    friend fixed operator-(const fixed& a, const fixed& b) noexcept { fixed r{ noinit_t{} }; fdetail::wmath::sub(r.v, a.v, b.v, N); return r; }
    friend bool operator<(const fixed& a, const fixed& b) noexcept { return fdetail::wmath::compare(a.v, b.v, N) < 0; }
    friend bool operator==(const fixed& a, const fixed& b) noexcept { return fdetail::wmath::compare(a.v, b.v, N) == 0; }

    friend fixed operator*(const fixed& a, const fixed& b) noexcept {
        std::uint64_t p[2 * N + 1] = {};
        const std::size_t low = N >= 2 ? N - 2 : 0;
        for (std::size_t i = 0; i < N; ++i) {
            if (!a.v[i]) continue;
            const std::size_t j0 = i >= low ? 0 : low - i;
            std::uint64_t carry = 0;
            for (std::size_t j = j0; j < N; ++j) {
                std::uint64_t hi, lo;
                fdetail::wmath::mul64(a.v[i], b.v[j], hi, lo);
                lo += carry;
                hi += lo < carry ? 1u : 0u;
                const std::uint64_t t = p[i + j] + lo;
                hi += t < lo ? 1u : 0u;
                p[i + j] = t;
                carry = hi;
            }
            for (std::size_t k = i + N; carry && k < 2 * N + 1; ++k) {
                const std::uint64_t t = p[k] + carry;
                carry = t < carry ? 1u : 0u;
                p[k] = t;
            }
        }
        fixed r{ noinit_t{} };
        for (std::size_t i = 0; i < N; ++i) r.v[i] = p[i + N - 1];
        return r;
    }

    fixed mul_small(std::uint64_t k) const noexcept {
        fixed r{ noinit_t{} };
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < N; ++i) {
            std::uint64_t hi, lo;
            fdetail::wmath::mul64(v[i], k, hi, lo);
            lo += carry;
            hi += lo < carry ? 1u : 0u;
            r.v[i] = lo;
            carry = hi;
        }
        return r;
    }

    fixed div_small(std::uint64_t d) const noexcept {
        fixed r{ noinit_t{} };
        fdetail::wmath::div_small(r.v, v, N, d);
        return r;
    }

    fixed shr(std::size_t s) const noexcept { fixed r = *this; fdetail::wmath::shr(r.v, N, s); return r; }
    fixed shl(std::size_t s) const noexcept { fixed r = *this; fdetail::wmath::shl(r.v, N, s); return r; }

    template <std::size_t M>
    fixed<M> resize() const noexcept {
        fixed<M> r;
        for (std::size_t i = 0; i < M; ++i) {
            const long long src = static_cast<long long>(i) + static_cast<long long>(N) - static_cast<long long>(M);
            if (src >= 0 && src < static_cast<long long>(N)) r.v[i] = v[static_cast<std::size_t>(src)];
        }
        return r;
    }

    double to_double() const noexcept {
        const long long t = top();
        if (t < 0) return 0.0;
        const std::size_t s = t > 60 ? static_cast<std::size_t>(t - 60) : 0;
        const fixed h = shr(s);
        return std::ldexp(static_cast<double>(h.v[0]), static_cast<int>(s) - static_cast<int>(frac_bits));
    }

    static fixed from_double(double d) noexcept {
        fixed r;
        if (!(d > 0.0)) return r;
        int e = 0;
        const double fr = std::frexp(d, &e);
        r.v[0] = static_cast<std::uint64_t>(std::ldexp(fr, 53));
        const long long shift = static_cast<long long>(e) - 53 + static_cast<long long>(frac_bits);
        if (shift >= static_cast<long long>(64 * N)) return fixed();
        if (shift >= 0) return r.shl(static_cast<std::size_t>(shift));
        if (-shift >= 64) return fixed();
        return r.shr(static_cast<std::size_t>(-shift));
    }
};

template <std::size_t N>
struct sfixed {
    fixed<N> m;
    bool neg = false;

    friend sfixed operator+(const sfixed& a, const sfixed& b) noexcept {
        sfixed r;
        if (a.neg == b.neg) { r.m = a.m + b.m; r.neg = a.neg; return r; }
        if (b.m < a.m) { r.m = a.m - b.m; r.neg = a.neg; } else { r.m = b.m - a.m; r.neg = b.neg; }
        if (r.m.is_zero()) r.neg = false;
        return r;
    }
    friend sfixed operator-(const sfixed& a, const sfixed& b) noexcept { sfixed nb = b; nb.neg = !nb.neg; return a + nb; }
    friend sfixed operator*(const sfixed& a, const sfixed& b) noexcept { sfixed r; r.m = a.m * b.m; r.neg = a.neg != b.neg && !r.m.is_zero(); return r; }
};

inline void knuth64(std::uint64_t* q, std::uint64_t* un, std::size_t m, const std::uint64_t* vn, std::size_t n) noexcept {
    const std::uint64_t vh = vn[n - 1];
    const std::uint64_t vl = n >= 2 ? vn[n - 2] : 0;
    for (std::size_t jj = m - n + 1; jj-- > 0;) {
        const std::uint64_t u2 = un[jj + n], u1 = un[jj + n - 1], u0 = (n >= 2) ? un[jj + n - 2] : 0;
        std::uint64_t qhat, rhat;
        bool big = false;
        if (u2 >= vh) {
            qhat = ~std::uint64_t(0);
            rhat = u1 + vh;
            big = rhat < u1;
        } else {
            qhat = fdetail::wmath::div_2by1(u2, u1, vh, rhat);
        }
        while (!big && n >= 2) {
            std::uint64_t ph, pl;
            fdetail::wmath::mul64(qhat, vl, ph, pl);
            if (ph > rhat || (ph == rhat && pl > u0)) {
                --qhat;
                const std::uint64_t r2 = rhat + vh;
                big = r2 < rhat;
                rhat = r2;
            } else {
                break;
            }
        }
        std::uint64_t borrow = 0, carry = 0;
        for (std::size_t i = 0; i < n; ++i) {
            std::uint64_t ph, pl;
            fdetail::wmath::mul64(qhat, vn[i], ph, pl);
            pl += carry;
            ph += pl < carry ? 1u : 0u;
            carry = ph;
            const std::uint64_t t = un[jj + i];
            const std::uint64_t d = t - pl - borrow;
            borrow = (t < pl || (t - pl) < borrow) ? 1u : 0u;
            un[jj + i] = d;
        }
        const std::uint64_t t = un[jj + n];
        const std::uint64_t d = t - carry - borrow;
        const bool neg = t < carry || (t - carry) < borrow;
        un[jj + n] = d;
        if (neg) {
            --qhat;
            std::uint64_t c = 0;
            for (std::size_t i = 0; i < n; ++i) {
                const std::uint64_t s1 = un[jj + i] + c;
                const std::uint64_t c1 = s1 < c ? 1u : 0u;
                const std::uint64_t s2 = s1 + vn[i];
                c = c1 + (s2 < s1 ? 1u : 0u);
                un[jj + i] = s2;
            }
            un[jj + n] += c;
        }
        q[jj] = qhat;
    }
}

template <std::size_t N>
fixed<N> fdiv(const fixed<N>& a, const fixed<N>& b) noexcept {
    std::size_t n = N;
    while (n > 0 && b.v[n - 1] == 0) --n;
    if (n == 0) return fixed<N>();
    const unsigned s = static_cast<unsigned>(clz64(b.v[n - 1]));
    std::uint64_t vn[N];
    for (std::size_t i = 0; i < n; ++i) vn[i] = b.v[i];
    if (s) fdetail::wmath::shl(vn, n, s);
    constexpr std::size_t M = 2 * N - 1;
    std::uint64_t un[M + 2] = {};
    for (std::size_t i = 0; i < N; ++i) un[i + N - 1] = a.v[i];
    if (s) fdetail::wmath::shl(un, M + 2, s);
    std::size_t m = M + 1;
    while (m > n && un[m - 1] == 0) --m;
    fixed<N> r;
    if (m < n) return r;
    std::uint64_t q[M + 2] = {};
    if (m == n) {
        if (fdetail::wmath::compare(un, vn, n) >= 0) q[0] = 1;
    } else {
        knuth64(q, un, m, vn, n);
    }
    for (std::size_t i = 0; i < N; ++i) r.v[i] = q[i];
    return r;
}

template <std::size_t N>
fixed<N> fsqrt(const fixed<N>& in) noexcept {
    if (in.is_zero()) return in;
    const long long k = (static_cast<long long>(fixed<N>::frac_bits) - in.top()) / 2;
    const fixed<N> a = k >= 0 ? in.shl(static_cast<std::size_t>(2 * k)) : in.shr(static_cast<std::size_t>(-2 * k));
    const double ad = a.to_double();
    fixed<N> y = fixed<N>::from_double(1.0 / std::sqrt(ad));
    const fixed<N> three = fixed<N>::from_int(3);
    double bits = 50.0;
    while (bits < static_cast<double>(fixed<N>::frac_bits) + 8.0) {
        const fixed<N> ay2 = a * (y * y);
        y = (y * (three - ay2)).shr(1);
        bits *= 2.0;
    }
    fixed<N> x = a * y;
    const fixed<N> r = a - x * x;
    const bool rneg = a < x * x;
    const fixed<N> corr = (y * (rneg ? (x * x - a) : r)).shr(1);
    x = rneg ? x - corr : x + corr;
    return k >= 0 ? x.shr(static_cast<std::size_t>(k)) : x.shl(static_cast<std::size_t>(-k));
}

template <std::size_t N>
fixed<N> atanh_inverse(std::uint64_t m) noexcept {
    fixed<N> sum, power = fixed<N>::one().div_small(m);
    const std::uint64_t m2 = m * m;
    for (std::uint64_t j = 0; !power.is_zero(); ++j) {
        sum = sum + power.div_small(2 * j + 1);
        power = power.div_small(m2);
    }
    return sum;
}

template <std::size_t N>
fixed<N> atan_inverse(std::uint64_t m) noexcept {
    fixed<N> pos, neg;
    fixed<N> power = fixed<N>::one().div_small(m);
    const std::uint64_t m2 = m * m;
    for (std::uint64_t j = 0; !power.is_zero(); ++j) {
        const fixed<N> t = power.div_small(2 * j + 1);
        if (j % 2) neg = neg + t; else pos = pos + t;
        power = power.div_small(m2);
    }
    return pos - neg;
}

template <std::size_t N>
const fixed<N>& ln2_constant() {
    static const fixed<N> c = [] {
        const fixed<N + 1> a = atanh_inverse<N + 1>(31).mul_small(14);
        const fixed<N + 1> b = atanh_inverse<N + 1>(49).mul_small(10);
        const fixed<N + 1> d = atanh_inverse<N + 1>(161).mul_small(6);
        return (a + b + d).template resize<N>();
    }();
    return c;
}

template <std::size_t N>
const fixed<N>& ln10_constant() {
    static const fixed<N> c = [] {
        const fixed<N + 1> a = atanh_inverse<N + 1>(31).mul_small(46);
        const fixed<N + 1> b = atanh_inverse<N + 1>(49).mul_small(34);
        const fixed<N + 1> d = atanh_inverse<N + 1>(161).mul_small(20);
        return (a + b + d).template resize<N>();
    }();
    return c;
}

template <std::size_t N>
const fixed<N>& half_pi_constant() {
    static const fixed<N> c = [] {
        const fixed<N + 1> q = atan_inverse<N + 1>(5).mul_small(4) - atan_inverse<N + 1>(239);
        return q.mul_small(2).template resize<N>();
    }();
    return c;
}

template <std::size_t N>
const fixed<N>& pi_constant() {
    static const fixed<N> c = half_pi_constant<N + 1>().mul_small(2).template resize<N>();
    return c;
}

template <std::size_t N>
const fixed<N>& inv_ln2_constant() {
    static const fixed<N> c = fdiv(fixed<N + 1>::one(), ln2_constant<N + 1>()).template resize<N>();
    return c;
}

template <std::size_t N>
const fixed<N>& inv_ln10_constant() {
    static const fixed<N> c = fdiv(fixed<N + 1>::one(), ln10_constant<N + 1>()).template resize<N>();
    return c;
}

template <std::size_t N>
const fixed<N>& two_over_sqrt_pi_constant() {
    static const fixed<N> c = fdiv(fixed<N + 1>::from_int(2), fsqrt(pi_constant<N + 1>())).template resize<N>();
    return c;
}

template <std::size_t N>
struct core {
    using fx  = fixed<N>;
    using fw  = fixed<N + 1>;
    using sfx = sfixed<N>;

    static constexpr std::size_t frac = fx::frac_bits;
    static constexpr std::size_t halvings = frac < 128 ? 6 : frac < 192 ? 8 : frac < 320 ? 10 : frac < 640 ? 14 : frac < 1280 ? 18 : 24;

    static fx div_prod(fx v, std::uint64_t lo, std::uint64_t hi, std::uint64_t (*d)(std::uint64_t)) noexcept {
        std::uint64_t acc = 1;
        for (std::uint64_t k = lo; k <= hi; ++k) {
            const std::uint64_t f = d(k);
            if (acc > (~std::uint64_t(0)) / f) { v = v.div_small(acc); acc = 1; }
            acc *= f;
        }
        return acc == 1 ? v : v.div_small(acc);
    }

    static std::size_t ps_terms(const fx& z, bool fact, std::uint64_t (*d)(std::uint64_t)) noexcept {
        const long long t = z.top();
        if (t < 0) return 0;
        const double lz = static_cast<double>(t - static_cast<long long>(frac)) + 1.0;
        const double goal = -static_cast<double>(frac) - 8.0;
        double acc = 0.0;
        for (std::size_t k = 1; k < 100000; ++k) {
            const double ld = std::log2(static_cast<double>(d(k)));
            const double lt = fact ? (acc += lz - ld) : static_cast<double>(k) * lz - ld;
            if (lt < goal) return k;
        }
        return 100000;
    }

    static fx ps_sum(const fx& z, bool alt, bool fact, std::uint64_t (*d)(std::uint64_t)) noexcept {
        const std::size_t T = ps_terms(z, fact, d);
        if (T == 0) return fx::one();
        std::size_t m = 1;
        while (m * m < T) ++m;
        if (m > 24) m = 24;
        sfx pw[25];
        pw[0] = sfx{ fx::one(), false };
        pw[1] = sfx{ z, alt };
        for (std::size_t i = 2; i <= m; ++i) pw[i] = pw[i - 1] * pw[1];
        const std::size_t B = (T + m) / m;
        sfx acc{ fx(), false };
        for (std::size_t jb = B; jb-- > 0;) {
            const std::uint64_t base = static_cast<std::uint64_t>(jb * m);
            sfx inner = (fact || base == 0) ? sfx{ fx::one(), false } : sfx{ fx::one().div_small(d(base)), false };
            for (std::size_t i = 1; i < m; ++i) {
                const std::uint64_t k = base + i;
                sfx t = pw[i];
                t.m = fact ? div_prod(t.m, base + 1, k, d) : t.m.div_small(d(k));
                inner = inner + t;
            }
            if (jb + 1 < B) {
                sfx up = acc * pw[m];
                if (fact) up.m = div_prod(up.m, base + 1, base + m, d);
                inner = inner + up;
            }
            acc = inner;
        }
        return acc.m;
    }

    static std::uint64_t d_k(std::uint64_t k) noexcept { return k; }
    static std::uint64_t d_k1(std::uint64_t k) noexcept { return k + 1; }
    static std::uint64_t d_odd(std::uint64_t k) noexcept { return 2 * k + 1; }
    static std::uint64_t d_sin(std::uint64_t k) noexcept { return (2 * k) * (2 * k + 1); }
    static std::uint64_t d_vers(std::uint64_t k) noexcept { return (2 * k + 1) * (2 * k + 2); }

    static fx exp_series(const fx& r) noexcept {
        fx sum = ps_sum(r.shr(halvings), false, true, &d_k);
        for (std::size_t i = 0; i < halvings; ++i) sum = sum * sum;
        return sum;
    }

    static bool exp_reduce(const fw& X, bool neg, fx& sum, long long& k) noexcept {
        if (X.top() >= static_cast<long long>(fw::frac_bits) + 44) return false;
        const fw& L2 = ln2_constant<N + 1>();
        const double xd = X.to_double();
        k = static_cast<long long>(std::floor((neg ? -xd : xd) / 0.69314718055994530942));
        fw r1;
        bool ok = false;
        for (int tries = 0; tries < 6; ++tries) {
            const fw kl = L2.mul_small(static_cast<std::uint64_t>(k < 0 ? -k : k));
            bool rneg;
            if (!neg && k >= 0) { rneg = X < kl; r1 = rneg ? kl - X : X - kl; }
            else if (neg && k < 0) { rneg = kl < X; r1 = rneg ? X - kl : kl - X; }
            else if (!neg) { rneg = false; r1 = X + kl; }
            else { rneg = true; r1 = X + kl; }
            if (rneg) { --k; continue; }
            if (!(r1 < L2)) { ++k; continue; }
            ok = true;
            break;
        }
        if (!ok) return false;
        sum = exp_series(r1.template resize<N>());
        return true;
    }

    static sfx expm1_small(const sfx& x) noexcept {
        const std::size_t h = halvings;
        const fx xs = x.m.shr(h);
        sfx acc{ xs * ps_sum(xs, x.neg, true, &d_k1), x.neg };
        const sfx two{ fx::from_int(2), false };
        for (std::size_t i = 0; i < h; ++i) acc = acc * (acc + two);
        return acc;
    }

    static fx atanh_series(const fx& t) noexcept {
        return t * ps_sum(t * t, false, false, &d_odd);
    }

    static fx log_plain(const fx& m, bool& neg) noexcept {
        const fx one = fx::one();
        neg = m < one;
        const fx num = neg ? one - m : m - one;
        return atanh_series(fdiv(num, m + one)).shl(1);
    }

    struct log_table_t { std::array<fx, 129> inv; std::array<fx, 129> lg; std::array<fx, 129> inv2; std::array<fx, 129> lg2; };

    static const log_table_t& log_table() {
        static const log_table_t t = [] {
            log_table_t r;
            fw l1, l2;
            for (std::size_t j = 0; j <= 128; ++j) {
                if (j > 0) {
                    l1 = l1 + atanh_inverse<N + 1>(255 + 2 * j).shl(1);
                    l2 = l2 + atanh_inverse<N + 1>((std::uint64_t(1) << 15) + 2 * j - 1).shl(1);
                }
                r.inv[j] = fdiv(fw::one(), fw::one() + fw::one().mul_small(j).shr(7)).template resize<N>();
                r.inv2[j] = fdiv(fw::one(), fw::one() + fw::one().mul_small(j).shr(14)).template resize<N>();
                r.lg[j] = l1.template resize<N>();
                r.lg2[j] = l2.template resize<N>();
            }
            return r;
        }();
        return t;
    }

    static fx log_mantissa(const fx& m) noexcept {
        const fx one = fx::one();
        const log_table_t& t = log_table();
        const std::size_t j = static_cast<std::size_t>((m - one).v[N - 2] >> 57);
        fx m2 = m * t.inv[j];
        fx base = t.lg[j];
        if (!(m2 < one)) {
            const std::size_t j2 = static_cast<std::size_t>((m2 - one).v[N - 2] >> 50);
            if (j2 <= 128) {
                m2 = m2 * t.inv2[j2];
                base = base + t.lg2[j2];
            }
        }
        bool n2 = false;
        const fx rest = log_plain(m2, n2);
        return n2 ? base - rest : base + rest;
    }

    static sfx log_pos(const fx& y) noexcept {
        const long long e = y.top() - static_cast<long long>(frac);
        const fx m = e >= 0 ? y.shr(static_cast<std::size_t>(e)) : y.shl(static_cast<std::size_t>(-e));
        sfx r{ log_mantissa(m), false };
        if (e != 0) {
            const fw el = ln2_constant<N + 1>().mul_small(static_cast<std::uint64_t>(e < 0 ? -e : e));
            r = sfx{ el.template resize<N>(), e < 0 } + r;
        }
        return r;
    }

    static sfx log1p(const sfx& u) noexcept {
        if (u.m.top() < static_cast<long long>(frac) - 10) {
            const fx two = fx::from_int(2);
            const fx den = u.neg ? two - u.m : two + u.m;
            return sfx{ atanh_series(fdiv(u.m, den)).shl(1), u.neg };
        }
        const fx y = u.neg ? fx::one() - u.m : fx::one() + u.m;
        return log_pos(y);
    }

    static void sin_vers(const fx& r, fx& s, fx& v) noexcept {
        const long long t = r.top() - static_cast<long long>(frac);
        const long long want = -static_cast<long long>(halvings) / 2 - 2;
        const std::size_t h = t > want ? static_cast<std::size_t>(t - want) : 0;
        const fx rs = r.shr(h);
        const fx r2 = rs * rs;
        s = rs * ps_sum(r2, true, true, &d_sin);
        v = r2.shr(1) * ps_sum(r2, true, true, &d_vers);
        const fx two = fx::from_int(2);
        for (std::size_t i = 0; i < h; ++i) {
            s = (s * (fx::one() - v)).shl(1);
            v = (v * (two - v)).shl(1);
        }
    }

    enum class gk { sin, cos, sinh, cosh, atan, atanh, asin, asinh, erf, erfi, expm1, log1p };

    static fx series_g(gk kind, const fx& z, bool zneg = false) noexcept {
        fx pos = fx::one(), ng, t = fx::one(), p = fx::one();
        for (std::uint64_t k = 1; k < 100000; ++k) {
            bool minus = false;
            fx term;
            switch (kind) {
            case gk::sin:   t = (t * z).div_small((2 * k) * (2 * k + 1)); term = t; minus = k & 1u; break;
            case gk::sinh:  t = (t * z).div_small((2 * k) * (2 * k + 1)); term = t; break;
            case gk::cos:   t = (t * z).div_small((2 * k - 1) * (2 * k)); term = t; minus = k & 1u; break;
            case gk::cosh:  t = (t * z).div_small((2 * k - 1) * (2 * k)); term = t; break;
            case gk::atan:  p = p * z; term = p.div_small(2 * k + 1); minus = k & 1u; break;
            case gk::atanh: p = p * z; term = p.div_small(2 * k + 1); break;
            case gk::asin:  t = (t * z).mul_small((2 * k - 1) * (2 * k - 1)).div_small((2 * k) * (2 * k + 1)); term = t; break;
            case gk::asinh: t = (t * z).mul_small((2 * k - 1) * (2 * k - 1)).div_small((2 * k) * (2 * k + 1)); term = t; minus = k & 1u; break;
            case gk::erf:   p = (p * z).div_small(k); term = p.div_small(2 * k + 1); minus = k & 1u; break;
            case gk::erfi:  p = (p * z).div_small(k); term = p.div_small(2 * k + 1); break;
            case gk::expm1: t = (t * z).div_small(k + 1); term = t; minus = zneg && (k & 1u); break;
            case gk::log1p: p = p * z; term = p.div_small(k + 1); minus = !zneg && (k & 1u); break;
            }
            if (term.is_zero()) break;
            if (minus) ng = ng + term; else pos = pos + term;
        }
        return pos - ng;
    }

    static const fx& atan_table(std::size_t j) {
        static const auto table = [] {
            std::array<fx, 17> t{};
            for (std::size_t i = 0; i <= 16; ++i) t[i] = core<N + 1>::atan_slow(fw::one().mul_small(i).div_small(16)).template resize<N>();
            return t;
        }();
        return table[j];
    }

    static fx atan_series(const fx& t) noexcept {
        return t * ps_sum(t * t, true, false, &d_odd);
    }

    static fx atan_slow(const fx& t) noexcept {
        fx a = t;
        for (int h = 0; h < 4; ++h) a = fdiv(a, fx::one() + fsqrt(fx::one() + a * a));
        return atan_series(a).shl(4);
    }

    static fx atan_reduced(const fx& a) noexcept {
        const fx sixteen = a.mul_small(16);
        std::size_t j = static_cast<std::size_t>(sixteen.v[N - 1] + ((sixteen.v[N - 2] >> 63) & 1u));
        if (j > 16) j = 16;
        const fx c = fx::one().mul_small(j).div_small(16);
        const bool dneg = a < c;
        const fx diff = dneg ? c - a : a - c;
        const fx t = fdiv(diff, fx::one() + a * c);
        const fx s = atan_series(t);
        return dneg ? atan_table(j) - s : atan_table(j) + s;
    }

    static fx atan_pos(const fx& num, const fx& den) noexcept {
        if (!(den < num)) return atan_reduced(fdiv(num, den));
        return half_pi_constant<N>() - atan_reduced(fdiv(den, num));
    }
};

template <class F>
struct fixed_for {
    using C = floatmp<F::math_bits, F::mantissa_bits, sign::is_signed>;
    static constexpr std::size_t MB = F::mantissa_bits;
    static constexpr std::size_t TB = F::math_bits;
    static constexpr std::size_t N  = (MB + 72 + 63) / 64 + 1;
    static constexpr bool usable = N <= 60;
    using fx  = fixed<N>;
    using fw  = fixed<N + 1>;
    using sfx = sfixed<N>;
    using K   = core<N>;
    using gk  = typename K::gk;
    static constexpr long long frac      = static_cast<long long>(fx::frac_bits);
    static constexpr long long exp_err   = static_cast<long long>(K::halvings) + 10;
    static constexpr long long small_top = -24;

    template <class S>
    static long long sval(const S& s) noexcept {
        return s.is_negative() ? -static_cast<long long>(s.get_lowest_bits()) : static_cast<long long>(s.get_lowest_bits());
    }

    static long long exp_of(const C& x) noexcept { return sval(x.frexp().second); }

    static long long bias() noexcept { return sval(C::exponent_bias); }

    template <std::size_t M>
    static bool to_fixed(const C& x, fixed<M>& out, long long& top) noexcept {
        const auto parts = x.frexp();
        const long long e = sval(parts.second);
        top = e;
        if (e >= 62) return false;
        constexpr std::size_t L = TB / 64 > 0 ? TB / 64 : 1;
        std::uint64_t m[L];
        umag_limbs<TB>::store(parts.first.magnitude(), m);
        out = fixed<M>();
        for (std::size_t i = 0; i < L && i < M; ++i) out.v[i] = m[i];
        const long long shift = static_cast<long long>(fixed<M>::frac_bits) - static_cast<long long>(MB) + e;
        if (shift >= 0) fdetail::wmath::shl(out.v, M, static_cast<std::size_t>(shift));
        else if (-shift < static_cast<long long>(64 * M)) fdetail::wmath::shr(out.v, M, static_cast<std::size_t>(-shift));
        else out = fixed<M>();
        return true;
    }

    static bool get_mant(const C& x, fx& m, long long& e) noexcept {
        const auto parts = x.frexp();
        e = sval(parts.second);
        constexpr std::size_t L = TB / 64 > 0 ? TB / 64 : 1;
        std::uint64_t buf[L];
        umag_limbs<TB>::store(parts.first.magnitude(), buf);
        m = fx();
        for (std::size_t i = 0; i < L && i < N; ++i) m.v[i] = buf[i];
        fdetail::wmath::shl(m.v, N, static_cast<std::size_t>(frac - static_cast<long long>(MB)));
        const long long t = m.top();
        if (t < 0) return false;
        if (t != frac) {
            m = t > frac ? m.shr(static_cast<std::size_t>(t - frac)) : m.shl(static_cast<std::size_t>(frac - t));
            e += t - frac;
        }
        return true;
    }

    static bool fits_exact(long long top, std::size_t limbs) noexcept {
        return top - static_cast<long long>(MB) + 64 * static_cast<long long>(limbs - 1) >= 0;
    }

    static C from_fixed(bool neg, const fx& y, long long exp2) noexcept {
        using W = typename C::wide_t;
        using SS = typename C::sstore_t;
        constexpr std::size_t WB = 2 * TB;
        constexpr std::size_t WL = WB / 64 > 0 ? WB / 64 : 1;
        const long long hb = y.top();
        if (hb < 0) return C::zero(neg);
        std::uint64_t buf[N];
        for (std::size_t i = 0; i < N; ++i) buf[i] = y.v[i];
        long long e = exp2 - frac;
        bool sticky = false;
        const long long keep = static_cast<long long>(WB) - 2;
        if (hb >= keep) {
            const long long d = hb - keep + 1;
            sticky = fdetail::wmath::any_below(buf, N, d);
            fdetail::wmath::shr(buf, N, static_cast<std::size_t>(d));
            e += d;
        }
        std::uint64_t w[WL] = {};
        for (std::size_t i = 0; i < WL && i < N; ++i) w[i] = buf[i];
        return C::from_wide(neg, W(umag_limbs<WB>::load(w)), SS(e), sticky);
    }

    static bool finish(bool neg, const fx& y, long long exp2, long long err_bits, C& out) noexcept {
        const long long hb = y.top();
        if (hb < 0) return false;
        const long long rp = hb - static_cast<long long>(MB);
        if (rp < err_bits + 4) return false;
        const long long E = hb - frac + exp2;
        if (E < 2 - bias() || E > bias() - 1) return false;
        const std::size_t bits = static_cast<std::size_t>(rp - 1);
        const std::size_t w = bits / 64, b = bits % 64;
        const std::uint64_t mask = (b == 0) ? 0 : ((std::uint64_t(1) << b) - 1u);
        const bool half_bit = ((y.v[w] >> b) & 1u) != 0;
        fx low;
        for (std::size_t i = 0; i < w; ++i) low.v[i] = half_bit ? y.v[i] : ~y.v[i];
        low.v[w] = (half_bit ? y.v[w] : ~y.v[w]) & mask;
        if (low.top() < err_bits + 1) return false;
        out = from_fixed(neg, y, exp2);
        return true;
    }

    static bool load(const C& x, fx& X, long long& top) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        if (!to_fixed(x, X, top)) return false;
        return fits_exact(top, N) && top < 60;
    }

    static bool loadw(const C& x, fw& X, long long& top) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        if (!to_fixed(x, X, top)) return false;
        return fits_exact(top, N + 1) && top < 60;
    }

    static long long amp(const fx& d) noexcept {
        const long long t = d.top();
        if (t < 0) return 100000;
        return t < frac ? frac - t : 0;
    }

    static bool tiny(const C& x) noexcept { return x.is_finite() && !x.is_zero() && exp_of(x) < small_top; }

    static bool small_parts(const C& x, fx& m, long long& e, fx& z) noexcept {
        if (!get_mant(x, m, e)) return false;
        const long long sh = -2 * e;
        z = sh >= frac + 64 ? fx() : (m * m).shr(static_cast<std::size_t>(sh));
        return e > -(bias() - 8);
    }

    static bool small_odd(gk kind, const C& x, C& out) noexcept {
        fx m, z;
        long long e = 0;
        if (!small_parts(x, m, e, z)) return false;
        fx P = m * K::series_g(kind, z);
        if (kind == gk::erf || kind == gk::erfi) P = P * two_over_sqrt_pi_constant<N>();
        return finish(x.is_negative(), P, e, 8, out);
    }

    static bool small_abs(gk kind, const C& x, fx& A) noexcept {
        fx m, z;
        long long e = 0;
        if (!small_parts(x, m, e, z)) return false;
        fx P = m * K::series_g(kind, z);
        if (kind == gk::erf) P = P * two_over_sqrt_pi_constant<N>();
        A = -e >= frac + 64 ? fx() : P.shr(static_cast<std::size_t>(-e));
        return true;
    }

    static bool small_linear(gk kind, const C& x, const fx* scale, C& out) noexcept {
        fx m;
        long long e = 0;
        if (!get_mant(x, m, e) || e <= -(bias() - 8)) return false;
        const fx xf = -e >= frac + 64 ? fx() : m.shr(static_cast<std::size_t>(-e));
        fx P = m * K::series_g(kind, xf, x.is_negative());
        if (scale) P = P * *scale;
        return finish(x.is_negative(), P, e, 8, out);
    }

    static bool small_trig(int which, bool hyp, const C& x, C& out) noexcept {
        fx m, z;
        long long e = 0;
        if (!small_parts(x, m, e, z)) return false;
        const fx gs = K::series_g(hyp ? gk::sinh : gk::sin, z);
        const fx gc = K::series_g(hyp ? gk::cosh : gk::cos, z);
        const bool neg = x.is_negative();
        switch (which) {
        case 0: return finish(neg, m * gs, e, 8, out);
        case 1: return finish(false, gc, 0, 8, out);
        case 2: return finish(neg, m * fdiv(gs, gc), e, 8, out);
        case 3: return finish(neg, fdiv(fx::one(), m * gs), -e, 8, out);
        case 4: return finish(false, fdiv(fx::one(), gc), 0, 8, out);
        default: return finish(neg, fdiv(gc, m * gs), -e, 8, out);
        }
    }

    static bool exp(const C& x, C& out) noexcept {
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 40) return false;
        fx sum;
        long long k = 0;
        if (!K::exp_reduce(X, x.is_negative(), sum, k)) return false;
        return finish(false, sum, k, exp_err, out);
    }

    static bool pow2(const C& x, C& out) noexcept {
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 40) return false;
        const bool neg = x.is_negative();
        long long n = static_cast<long long>(X.v[N]);
        fw f = X;
        f.v[N] = 0;
        if (neg && !f.is_zero()) { f = fw::one() - f; ++n; }
        if (neg) n = -n;
        const fx r = (f * ln2_constant<N + 1>()).template resize<N>();
        return finish(false, K::exp_series(r), n, exp_err, out);
    }

    static bool pow10(const C& x, C& out) noexcept {
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 38) return false;
        const fw Y = X * ln10_constant<N + 1>();
        fx sum;
        long long k = 0;
        if (!K::exp_reduce(Y, x.is_negative(), sum, k)) return false;
        return finish(false, sum, k, exp_err, out);
    }

    static bool expm1(const C& x, C& out) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        long long top = exp_of(x);
        if (top < small_top) return small_linear(gk::expm1, x, nullptr, out);
        if (top < 0) {
            fx X;
            if (!load(x, X, top)) return false;
            return finish(x.is_negative(), K::expm1_small(sfx{ X, x.is_negative() }).m, 0, exp_err, out);
        }
        fw X;
        if (!loadw(x, X, top) || top >= 40) return false;
        fx sum;
        long long k = 0;
        if (!K::exp_reduce(X, x.is_negative(), sum, k)) return false;
        if (k >= 0) {
            if (k > 56) return false;
            return finish(false, sum.shl(static_cast<std::size_t>(k)) - fx::one(), 0, exp_err + k, out);
        }
        if (-k > frac - 8) return false;
        return finish(true, fx::one() - sum.shr(static_cast<std::size_t>(-k)), 0, exp_err, out);
    }

    static bool near_one(const C& x, C& d) noexcept {
        d = x - C(1);
        return d.is_finite() && !d.is_zero() && exp_of(d) < small_top;
    }

    static bool log_any(const C& x, int base, C& out) noexcept {
        if (!x.is_finite() || x.is_zero() || x.is_negative()) return false;
        const fx* sc = base == 2 ? &inv_ln2_constant<N>() : base == 10 ? &inv_ln10_constant<N>() : nullptr;
        C d;
        if (near_one(x, d)) return small_linear(gk::log1p, d, sc, out);
        fx m;
        long long e = 0;
        if (!get_mant(x, m, e)) return false;
        if (m == fx::one() && (base == 2 || e == 0)) return false;
        sfx r{ K::log_mantissa(m), false };
        if (base == 2) {
            r.m = r.m * *sc;
            if (e != 0) r = sfx{ fx::from_int(static_cast<std::uint64_t>(e < 0 ? -e : e)), e < 0 } + r;
            return finish(r.neg, r.m, 0, 10, out);
        }
        if (e != 0) {
            const fw el = ln2_constant<N + 1>().mul_small(static_cast<std::uint64_t>(e < 0 ? -e : e));
            r = sfx{ el.template resize<N>(), e < 0 } + r;
        }
        if (sc) r.m = r.m * *sc;
        return finish(r.neg, r.m, 0, 10, out);
    }

    static bool log(const C& x, C& out) noexcept { return log_any(x, 0, out); }
    static bool log2(const C& x, C& out) noexcept { return log_any(x, 2, out); }
    static bool log10(const C& x, C& out) noexcept { return log_any(x, 10, out); }

    static bool log1p(const C& x, C& out) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        long long top = exp_of(x);
        if (top < small_top) return small_linear(gk::log1p, x, nullptr, out);
        if (x.is_negative() && top >= 0) return false;
        if (top >= 60) return false;
        fx X;
        if (!load(x, X, top)) return false;
        const sfx r = K::log1p(sfx{ X, x.is_negative() });
        return finish(r.neg, r.m, 0, 12, out);
    }

    static bool reduce_half_pi(const C& x, fx& r, bool& rneg, std::uint64_t& n) noexcept {
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 48) return false;
        const fw& H = half_pi_constant<N + 1>();
        n = static_cast<std::uint64_t>(std::floor(X.to_double() / 1.57079632679489661923 + 0.5));
        const fw nh = H.mul_small(n);
        rneg = X < nh;
        r = (rneg ? nh - X : X - nh).template resize<N>();
        return r.top() >= frac - 60;
    }

    static bool sin_cos_parts(const C& x, fx& S, fx& Cc, bool& sneg, bool& cneg) noexcept {
        fx r;
        bool rneg = false;
        std::uint64_t n = 0;
        if (!reduce_half_pi(x, r, rneg, n)) return false;
        fx s, v;
        K::sin_vers(r, s, v);
        const fx c = fx::one() - v;
        const unsigned q = static_cast<unsigned>(n & 3u);
        S = s; Cc = c; sneg = rneg; cneg = false;
        if (q == 1) { S = c; Cc = s; sneg = false; cneg = !rneg; }
        else if (q == 2) { sneg = !rneg; cneg = true; }
        else if (q == 3) { S = c; Cc = s; sneg = true; cneg = rneg; }
        if (x.is_negative()) sneg = !sneg;
        return true;
    }

    static bool trig(int which, const C& x, C& out) noexcept {
        if (tiny(x)) return small_trig(which, false, x, out);
        fx S, Cc;
        bool sneg = false, cneg = false;
        if (!sin_cos_parts(x, S, Cc, sneg, cneg)) return false;
        const long long e = 14 + static_cast<long long>(K::halvings);
        switch (which) {
        case 0: return finish(sneg, S, 0, e, out);
        case 1: return finish(cneg, Cc, 0, e, out);
        case 2: return finish(sneg != cneg, fdiv(S, Cc), 0, e + 2 + 2 * amp(Cc), out);
        case 3: return finish(sneg, fdiv(fx::one(), S), 0, e + 2 + 2 * amp(S), out);
        case 4: return finish(cneg, fdiv(fx::one(), Cc), 0, e + 2 + 2 * amp(Cc), out);
        default: return finish(sneg != cneg, fdiv(Cc, S), 0, e + 2 + 2 * amp(S), out);
        }
    }

    static bool atan(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::atan, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top)) return false;
        return finish(x.is_negative(), K::atan_pos(X, fx::one()), 0, 12, out);
    }

    static bool acot(const C& x, C& out) noexcept {
        fx a;
        if (tiny(x)) {
            if (!small_abs(gk::atan, x, a)) return false;
        } else {
            fx X;
            long long top = 0;
            if (!load(x, X, top)) return false;
            a = K::atan_pos(X, fx::one());
        }
        const fx& h = half_pi_constant<N>();
        return finish(false, x.is_negative() ? h + a : h - a, 0, 12, out);
    }

    static bool asin(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::asin, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top) || !(X < fx::one())) return false;
        const fx c = fsqrt((fx::one() - X) * (fx::one() + X));
        return finish(x.is_negative(), K::atan_pos(X, c), 0, 14 + amp(c), out);
    }

    static bool acos(const C& x, C& out) noexcept {
        if (tiny(x)) {
            fx a;
            if (!small_abs(gk::asin, x, a)) return false;
            const fx& h = half_pi_constant<N>();
            return finish(false, x.is_negative() ? h + a : h - a, 0, 10, out);
        }
        fx X;
        long long top = 0;
        if (!load(x, X, top) || !(X < fx::one())) return false;
        const fx c = fsqrt((fx::one() - X) * (fx::one() + X));
        const fx a = K::atan_pos(c, X);
        return finish(false, x.is_negative() ? pi_constant<N>() - a : a, 0, 14 + amp(c), out);
    }

    static bool asec(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 28 || !(fx::one() < X)) return false;
        const fx t = fsqrt((X - fx::one()) * (X + fx::one()));
        const fx a = K::atan_pos(t, fx::one());
        return finish(false, x.is_negative() ? pi_constant<N>() - a : a, 0, 14 + amp(t), out);
    }

    static bool acsc(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 28 || !(fx::one() < X)) return false;
        const fx t = fsqrt((X - fx::one()) * (X + fx::one()));
        return finish(x.is_negative(), K::atan_pos(fx::one(), t), 0, 14 + amp(t), out);
    }

    static bool expm1_fixed(const C& ax, fx& E1) noexcept {
        long long top = exp_of(ax);
        if (top < 0) {
            fx X;
            if (!load(ax, X, top)) return false;
            E1 = K::expm1_small(sfx{ X, false }).m;
            return true;
        }
        return false;
    }

    static bool hyper(int which, const C& x, C& out) noexcept {
        if (tiny(x)) return small_trig(which, true, x, out);
        if (!x.is_finite() || x.is_zero()) return false;
        const bool xneg = x.is_negative();
        const C ax = xneg ? -x : x;
        const long long top = exp_of(ax);
        const long long e0 = exp_err + 8;
        const fx one = fx::one(), two = fx::from_int(2);
        if (top < 0) {
            fx E1;
            if (!expm1_fixed(ax, E1)) return false;
            const fx num = E1 * (E1 + two);
            const fx d2 = (E1 + one).shl(1);
            const fx dt = E1 * E1 + E1.shl(1) + two;
            switch (which) {
            case 0: return finish(xneg, fdiv(num, d2), 0, e0, out);
            case 1: return finish(false, one + fdiv(E1 * E1, d2), 0, e0, out);
            case 2: return finish(xneg, fdiv(num, dt), 0, e0, out);
            case 3: return finish(xneg, fdiv(d2, num), 0, e0 + 2 + 2 * amp(num), out);
            case 4: return finish(false, fdiv(d2, dt), 0, e0, out);
            default: return finish(xneg, fdiv(dt, num), 0, e0 + 2 + 2 * amp(num), out);
            }
        }
        fw X;
        long long t2 = 0;
        if (!loadw(ax, X, t2) || t2 >= 40) return false;
        fx sum;
        long long k = 0;
        if (!K::exp_reduce(X, false, sum, k) || k < 1) return false;
        const long long sh = 2 * k;
        fx q;
        if (sh < frac + 4) q = fdiv(one, sum).shr(static_cast<std::size_t>(sh));
        switch (which) {
        case 0: return finish(xneg, sum - q, k - 1, e0, out);
        case 1: return finish(false, sum + q, k - 1, e0, out);
        case 3: return finish(xneg, fdiv(one, sum - q), 1 - k, e0, out);
        case 4: return finish(false, fdiv(one, sum + q), 1 - k, e0, out);
        default: break;
        }
        fx qq;
        if (sh < frac + 4) qq = fdiv(one, sum * sum).shr(static_cast<std::size_t>(sh));
        if (which == 2) return finish(xneg, fdiv(one - qq, one + qq), 0, e0, out);
        return finish(xneg, fdiv(one + qq, one - qq), 0, e0, out);
    }

    static sfx asinh_fixed(const fx& X, long long& err) noexcept {
        const fx one = fx::one();
        if (X < one) {
            const fx x2 = X * X;
            const fx u = X + fdiv(x2, one + fsqrt(one + x2));
            err = 14;
            return K::log1p(sfx{ u, false });
        }
        err = 14;
        return K::log_pos(X + fsqrt(X * X + one));
    }

    static bool asinh(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::asinh, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 28) return false;
        long long err = 0;
        const sfx r = asinh_fixed(X, err);
        return finish(x.is_negative(), r.m, 0, err, out);
    }

    static bool acsch(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top < -28 || top >= 56) return false;
        const fx Y = fdiv(fx::one(), X);
        long long err = 0;
        const sfx r = asinh_fixed(Y, err);
        return finish(x.is_negative(), r.m, 0, err + 2 + (top < 0 ? 2 * (-top) : 0), out);
    }

    static bool acosh(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || x.is_negative() || top >= 28 || !(fx::one() < X)) return false;
        const fx one = fx::one();
        const fx t = X - one;
        if (t < one) {
            const fx sq = fsqrt(t * (t + fx::from_int(2)));
            const sfx r = K::log1p(sfx{ t + sq, false });
            return finish(false, r.m, 0, 14 + amp(sq), out);
        }
        const sfx r = K::log_pos(X + fsqrt(t * (X + one)));
        return finish(false, r.m, 0, 14, out);
    }

    static bool asech(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || x.is_negative() || top < -56 || !(X < fx::one())) return false;
        const fx one = fx::one();
        const fx om = one - X;
        const fx sq = fsqrt(om * (one + X));
        const fx u = fdiv(om + sq, X);
        const sfx r = K::log1p(sfx{ u, false });
        return finish(false, r.m, 0, 14 + amp(sq) + amp(X), out);
    }

    static bool atanh(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::atanh, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top) || !(X < fx::one())) return false;
        if (top < -10) return finish(x.is_negative(), K::atanh_series(X), 0, 10, out);
        const sfx r = K::log1p(sfx{ fdiv(X.shl(1), fx::one() - X), false });
        return finish(x.is_negative(), r.m.shr(1), 0, 14, out);
    }

    static bool acoth(const C& x, C& out) noexcept {
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 56 || !(fx::one() < X)) return false;
        if (top >= 10) return finish(x.is_negative(), K::atanh_series(fdiv(fx::one(), X)), 0, 10, out);
        const sfx r = K::log1p(sfx{ fdiv(fx::from_int(2), X - fx::one()), false });
        return finish(x.is_negative(), r.m.shr(1), 0, 14, out);
    }

    static bool sinc(const C& x, C& out) noexcept {
        if (tiny(x)) {
            fx m, z;
            long long e = 0;
            if (!small_parts(x, m, e, z)) return false;
            return finish(false, K::series_g(gk::sin, z), 0, 8, out);
        }
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 46) return false;
        fx S, Cc;
        bool sneg = false, cneg = false;
        if (!sin_cos_parts(x, S, Cc, sneg, cneg) || S.is_zero()) return false;
        if (x.is_negative()) sneg = !sneg;
        return finish(sneg, fdiv(S, X), 0, 16 + static_cast<long long>(K::halvings) + amp(X), out);
    }

    static bool nsinc(const C& x, C& out) noexcept {
        if (tiny(x)) {
            fx m;
            long long e = 0;
            if (!get_mant(x, m, e)) return false;
            const fx pm = m * pi_constant<N>();
            const long long sh = -2 * e;
            const fx z = sh >= frac + 64 ? fx() : (pm * pm).shr(static_cast<std::size_t>(sh));
            return finish(false, K::series_g(gk::sin, z), 0, 8, out);
        }
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 44) return false;
        const fw Xh = X.shl(1);
        std::uint64_t n = Xh.v[N];
        fw rem = Xh;
        rem.v[N] = 0;
        bool rneg = false;
        if (!(rem < fw::one().shr(1))) { rem = fw::one() - rem; ++n; rneg = true; }
        if (rem.is_zero() && (n & 1u) == 0) return false;
        const fx r = (rem.shr(1) * pi_constant<N + 1>()).template resize<N>();
        fx s, v;
        if (!r.is_zero()) K::sin_vers(r, s, v);
        const fx c = fx::one() - v;
        const unsigned q = static_cast<unsigned>(n & 3u);
        fx S = s;
        bool sneg = rneg;
        if (q == 1) { S = c; sneg = false; }
        else if (q == 2) { sneg = !rneg; }
        else if (q == 3) { S = c; sneg = true; }
        if (S.is_zero()) return false;
        const fx den = (X * pi_constant<N + 1>()).template resize<N>();
        return finish(sneg, fdiv(S, den), 0, 16 + static_cast<long long>(K::halvings) + amp(den), out);
    }

    static bool gd(const C& x, C& out) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        const bool xneg = x.is_negative();
        if (tiny(x)) {
            fx m, z;
            long long e = 0;
            if (!small_parts(x, m, e, z)) return false;
            const fx Sm = m * K::series_g(gk::sinh, z);
            const fx z2 = (Sm * Sm).shr(static_cast<std::size_t>(-2 * e));
            return finish(xneg, Sm * K::series_g(gk::atan, z2), e, 10, out);
        }
        const C ax = xneg ? -x : x;
        const long long top = exp_of(ax);
        if (top >= 5) return false;
        fx S;
        if (top < 0) {
            fx E1;
            if (!expm1_fixed(ax, E1)) return false;
            S = fdiv(E1 * (E1 + fx::from_int(2)), (E1 + fx::one()).shl(1));
        } else {
            fw X;
            long long t2 = 0;
            if (!loadw(ax, X, t2)) return false;
            fx sum;
            long long k = 0;
            if (!K::exp_reduce(X, false, sum, k) || k < 1 || k > 54) return false;
            S = (sum - fdiv(fx::one(), sum).shr(static_cast<std::size_t>(2 * k))).shl(static_cast<std::size_t>(k - 1));
        }
        return finish(xneg, K::atan_pos(S, fx::one()), 0, exp_err + 12, out);
    }

    static bool inv_gd(const C& x, C& out) noexcept {
        if (tiny(x)) {
            fx m, z;
            long long e = 0;
            if (!small_parts(x, m, e, z)) return false;
            const fx Sm = m * K::series_g(gk::sin, z);
            const fx z2 = (Sm * Sm).shr(static_cast<std::size_t>(-2 * e));
            return finish(x.is_negative(), Sm * K::series_g(gk::atanh, z2), e, 10, out);
        }
        fx X;
        long long top = 0;
        if (!load(x, X, top) || !(X < half_pi_constant<N>())) return false;
        fx s, v;
        K::sin_vers(X, s, v);
        const fx c = fx::one() - v;
        if (c.top() < frac - 30) return false;
        const sfx r = K::log1p(sfx{ fdiv(s + v, c), false });
        return finish(x.is_negative(), r.m, 0, 18 + static_cast<long long>(K::halvings) + 2 * amp(c), out);
    }

    template <std::size_t M>
    static fixed<M> scaled_series(const fixed<M>& X, const fixed<M>& z, long long Ks, bool erfi_kind) noexcept {
        using fm = fixed<M>;
        constexpr long long fb = static_cast<long long>(fm::frac_bits);
        fm tm = X;
        long long te = 0;
        const long long t0 = tm.top();
        if (t0 < 0) return fm();
        if (t0 > fb) { tm = tm.shr(static_cast<std::size_t>(t0 - fb)); te += t0 - fb; }
        else if (t0 < fb) { tm = tm.shl(static_cast<std::size_t>(fb - t0)); te -= fb - t0; }
        fm sum;
        bool peaked = false;
        for (std::uint64_t n = 0; n < 400000; ++n) {
            if (n > 0) {
                tm = erfi_kind ? (tm * z).div_small(n) : (tm * z).div_small(2 * n + 1);
                const long long tt = tm.top();
                if (tt < 0) break;
                if (tt > fb) { tm = tm.shr(static_cast<std::size_t>(tt - fb)); te += tt - fb; }
                else if (tt < fb) { tm = tm.shl(static_cast<std::size_t>(fb - tt)); te -= fb - tt; }
            }
            fm c = erfi_kind ? tm.div_small(2 * n + 1) : tm;
            const long long sh = te - Ks;
            if (sh >= 0) c = c.shl(static_cast<std::size_t>(sh));
            else if (-sh >= 64 * static_cast<long long>(M)) c = fm();
            else c = c.shr(static_cast<std::size_t>(-sh));
            if (c.is_zero()) { if (peaked) break; }
            else sum = sum + c;
            const double ratio = erfi_kind ? z.to_double() / static_cast<double>(n + 1) : z.to_double() / static_cast<double>(2 * n + 3);
            if (ratio < 1.0) peaked = true;
        }
        return sum;
    }

    template <std::size_t M>
    static bool erf_parts(const fixed<M>& X, fixed<M>& P, long long& exp2) noexcept {
        using fm = fixed<M>;
        const fm x2 = X * X;
        const double xd = X.to_double();
        const double l2 = xd * xd * 1.4426950408889634;
        const long long Ks = l2 > 3.0 ? static_cast<long long>(l2) - 2 : 0;
        const fm sum = scaled_series<M>(X, x2.shl(1), Ks, false);
        const fixed<M + 1> A = x2.template resize<M + 1>();
        fm es;
        long long k = 0;
        if (!core<M>::exp_reduce(A, true, es, k)) return false;
        P = (sum * es) * two_over_sqrt_pi_constant<M>();
        exp2 = k + Ks;
        return true;
    }

    static bool erf(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::erf, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 5) return false;
        const double xd = X.to_double();
        if (xd * xd * 1.4426950408889634 > static_cast<double>(MB) + 12.0) {
            out = x.is_negative() ? -C(1) : C(1);
            return true;
        }
        fx P;
        long long e2 = 0;
        if (!erf_sum(X, P, e2)) return false;
        return finish(x.is_negative(), P, e2, exp_err + 14, out);
    }

    template <std::size_t M>
    static bool erfc_at(const C& x, C& out) noexcept {
        using fm = fixed<M>;
        fm X;
        long long top = 0;
        if (!to_fixed(x, X, top) || !fits_exact(top, M)) return false;
        fm P;
        long long e2 = 0;
        if (!erf_parts<M>(X, P, e2)) return false;
        if (e2 > 4 || e2 < -64) return false;
        const fm one = fm::one();
        const fm Pv = e2 >= 0 ? P.shl(static_cast<std::size_t>(e2)) : P.shr(static_cast<std::size_t>(-e2));
        fm r;
        if (x.is_negative()) r = one + Pv;
        else if (Pv < one) r = one - Pv;
        else return false;
        const long long rt = r.top();
        if (rt < 0) return false;
        const long long sh = static_cast<long long>(fm::frac_bits) - rt;
        const long long ex = 64 * static_cast<long long>(M - N);
        if (sh <= 0) return finish(false, r.template resize<N>(), 0, 8, out);
        const long long use = sh < ex ? sh : ex;
        return finish(false, r.shl(static_cast<std::size_t>(use)).template resize<N>(), -use, 8 + (sh > ex ? 0 : 0), out);
    }

    static bool erfc(const C& x, C& out) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        if (tiny(x)) {
            fx a;
            if (!small_abs(gk::erf, x, a)) return false;
            return finish(false, x.is_negative() ? fx::one() + a : fx::one() - a, 0, 8, out);
        }
        const long long top = exp_of(x);
        if (x.is_negative()) {
            if (top >= 5) return false;
            return erfc_at<N>(x, out);
        }
        if (top < -1) return erfc_at<N>(x, out);
        if (top >= 4) return false;
        fx X;
        long long t2 = 0;
        if (!load(x, X, t2)) return false;
        const double xd = X.to_double();
        const double lost = xd * xd * 1.4426950408889634 + 12.0;
        if (lost < 56.0) return erfc_at<N + 1>(x, out);
        if (lost < 120.0) return erfc_at<N + 2>(x, out);
        if (lost < 248.0) return erfc_at<N + 4>(x, out);
        return false;
    }

    static bool erfi(const C& x, C& out) noexcept {
        if (tiny(x)) return small_odd(gk::erfi, x, out);
        fx X;
        long long top = 0;
        if (!load(x, X, top) || top >= 7) return false;
        const fx x2 = X * X;
        const double xd = X.to_double();
        if (xd < 1.4) {
            fx V;
            if (!erfi_value(X, V)) return false;
            return finish(x.is_negative(), V, 0, exp_err + 12, out);
        }
        const double l2 = xd * xd * 1.4426950408889634;
        const long long Ks = l2 > 3.0 ? static_cast<long long>(l2) - 2 : 0;
        const fx sum = scaled_series<N>(X, x2, Ks, true);
        return finish(x.is_negative(), sum * two_over_sqrt_pi_constant<N>(), Ks, 16, out);
    }

    static bool atan2(const C& y, const C& x, C& out) noexcept {
        if (!y.is_finite() || !x.is_finite() || y.is_zero() || x.is_zero()) return false;
        fx my, mx;
        long long ey = 0, ex = 0;
        if (!get_mant(y, my, ey) || !get_mant(x, mx, ex)) return false;
        const long long d = ey - ex;
        if (d > 56 || d < -56) return false;
        const fx a = d >= 0 ? K::atan_pos(my.shl(static_cast<std::size_t>(d)), mx) : K::atan_pos(my, mx.shl(static_cast<std::size_t>(-d)));
        const fx r = x.is_negative() ? pi_constant<N>() - a : a;
        return finish(y.is_negative(), r, 0, 14, out);
    }

    static bool pow(const C& x, const C& y, C& out) noexcept {
        if (!x.is_finite() || !y.is_finite() || x.is_zero() || y.is_zero() || x.is_negative()) return false;
        using fv = fixed<N + 1>;
        using KW = core<N + 1>;
        fx mm;
        long long e = 0;
        if (!get_mant(x, mm, e)) return false;
        const fv m = mm.template resize<N + 1>();
        if (m == fv::one() && e == 0) return false;
        sfixed<N + 1> L{ KW::log_mantissa(m), false };
        if (e != 0) {
            const fixed<N + 2> el = ln2_constant<N + 2>().mul_small(static_cast<std::uint64_t>(e < 0 ? -e : e));
            L = sfixed<N + 1>{ el.template resize<N + 1>(), e < 0 } + L;
        }
        if (L.m.is_zero()) return false;
        fv Y;
        long long ty = 0;
        if (!to_fixed(y, Y, ty) || ty >= 40 || !fits_exact(ty, N + 1)) return false;
        const double prod = L.m.to_double() * Y.to_double();
        if (!(prod < 1.0e12)) return false;
        const fv Z = L.m * Y;
        if (Z.top() < static_cast<long long>(fv::frac_bits) - 60) return false;
        fx sum;
        long long k = 0;
        if (!K::exp_reduce(Z, L.neg != y.is_negative(), sum, k)) return false;
        return finish(false, sum, k, exp_err + 4, out);
    }

    static bool erf_sum(const fx& X, fx& P, long long& e2) noexcept {
        const fx z = (X * X).shl(1);
        if (z.top() < frac + 2) {
            fixed<N + 1> A = (X * X).template resize<N + 1>();
            fx es;
            long long k = 0;
            if (!K::exp_reduce(A, true, es, k)) return false;
            P = ((X * K::ps_sum(z, false, true, &K::d_odd)) * es) * two_over_sqrt_pi_constant<N>();
            e2 = k;
            return true;
        }
        return erf_parts<N>(X, P, e2);
    }

    static bool erf_value(const fx& X, fx& V) noexcept {
        fx P;
        long long e2 = 0;
        if (!erf_sum(X, P, e2)) return false;
        if (e2 > 4 || e2 < -frac) return false;
        V = e2 >= 0 ? P.shl(static_cast<std::size_t>(e2)) : P.shr(static_cast<std::size_t>(-e2));
        return true;
    }

    static bool exp_value(const sfx& w, fx& E, long long& k) noexcept {
        fx sum;
        if (!K::exp_reduce(w.m.template resize<N + 1>(), w.neg, sum, k)) return false;
        E = sum;
        return true;
    }

    static double inv_erf_double(double y) noexcept {
        const double a = 0.147;
        const double l = std::log(1.0 - y * y);
        const double t = 2.0 / (3.14159265358979323846 * a) + l / 2.0;
        double x = std::copysign(std::sqrt(std::sqrt(t * t - l / a) - t), y);
        for (int i = 0; i < 4; ++i) {
            const double f = std::erf(x) - y;
            const double d = 1.1283791670955126 * std::exp(-x * x);
            if (d == 0.0) break;
            x -= f / (d + x * f);
        }
        return x;
    }

    static bool inv_erf_core(const fx& Y, bool yneg, C& out) noexcept {
        const double yd = Y.to_double();
        if (!(yd < 0.97)) return false;
        double xd = inv_erf_double(yd);
        if (!(xd > 0.0) || !std::isfinite(xd)) return false;
        fx x = fx::from_double(xd);
        const fx hs = fdiv(fx::one(), two_over_sqrt_pi_constant<N>());
        double bits = 40.0;
        int iters = 0;
        while (iters < 8) {
            fx V;
            if (!erf_value(x, V)) return false;
            fx E;
            long long k = 0;
            if (!exp_value(sfx{ x * x, false }, E, k) || k > 8) return false;
            E = k >= 0 ? E.shl(static_cast<std::size_t>(k)) : E.shr(static_cast<std::size_t>(-k));
            const bool fneg = V < Y;
            const fx f = fneg ? Y - V : V - Y;
            const fx q = (f * E) * hs;
            const fx xq = x * q;
            const fx den = fneg ? fx::one() - xq : fx::one() + xq;
            const fx step = fdiv(q, den);
            x = fneg ? x + step : x - step;
            ++iters;
            bits *= 3.0;
            if (bits > static_cast<double>(frac) + 40.0) break;
        }
        return finish(yneg, x, 0, exp_err + 20, out);
    }

    static bool inv_erf(const C& y, C& out) noexcept {
        fx Y;
        long long top = 0;
        if (!load(y, Y, top) || top < small_top || !(Y < fx::one())) return false;
        return inv_erf_core(Y, y.is_negative(), out);
    }

    static bool inv_erfc(const C& z, C& out) noexcept {
        fx Z;
        long long top = 0;
        if (!load(z, Z, top) || z.is_negative() || !(Z < fx::from_int(2))) return false;
        const fx one = fx::one();
        const bool neg = one < Z;
        const fx Y = neg ? Z - one : one - Z;
        if (Y.top() < frac + small_top) return false;
        return inv_erf_core(Y, neg, out);
    }

    static bool erfi_value(const fx& X, fx& V) noexcept {
        const fx x2 = X * X;
        const fx s = K::ps_sum(x2.shl(1), true, true, &K::d_odd);
        fx E;
        long long k = 0;
        if (!exp_value(sfx{ x2, false }, E, k) || k > 8) return false;
        V = ((X * s) * (k >= 0 ? E.shl(static_cast<std::size_t>(k)) : E.shr(static_cast<std::size_t>(-k)))) * two_over_sqrt_pi_constant<N>();
        return true;
    }

    static bool inv_erfi(const C& y, C& out) noexcept {
        fx Y;
        long long top = 0;
        if (!load(y, Y, top) || top < small_top) return false;
        const double yd = Y.to_double();
        if (!(yd < 2.0)) return false;
        double xd = yd * 0.886226925452758;
        for (int i = 0; i < 40; ++i) {
            double s = 0.0, t = xd;
            for (int n = 0; n < 60; ++n) { s += t / (2 * n + 1); t *= xd * xd / (n + 1); }
            const double f = 1.1283791670955126 * s - yd;
            const double d = 1.1283791670955126 * std::exp(xd * xd);
            const double st = f / (d - xd * f);
            xd -= st;
            if (std::fabs(st) < 1e-15 * xd) break;
        }
        if (!(xd > 0.0) || !(xd < 1.35)) return false;
        fx x = fx::from_double(xd);
        const fx hs = fdiv(fx::one(), two_over_sqrt_pi_constant<N>());
        double bits = 40.0;
        for (int it = 0; it < 8; ++it) {
            fx V;
            if (!erfi_value(x, V)) return false;
            fx E;
            long long k = 0;
            if (!exp_value(sfx{ x * x, true }, E, k) || k < -8) return false;
            E = k >= 0 ? E.shl(static_cast<std::size_t>(k)) : E.shr(static_cast<std::size_t>(-k));
            const bool fneg = V < Y;
            const fx f = fneg ? Y - V : V - Y;
            const fx q = (f * E) * hs;
            const fx xq = x * q;
            const fx den = fneg ? fx::one() + xq : fx::one() - xq;
            const fx step = fdiv(q, den);
            x = fneg ? x + step : x - step;
            bits *= 3.0;
            if (bits > static_cast<double>(frac) + 40.0) break;
        }
        return finish(y.is_negative(), x, 0, exp_err + 20, out);
    }

    static bool agm_k(const C& m, fx& a, fx& S, bool& Sneg, int& iters) noexcept {
        fx M;
        long long top = 0;
        if (!load(m, M, top) || top >= 40) return false;
        const bool mneg = m.is_negative();
        const fx one = fx::one();
        if (!mneg && !(M < one)) return false;
        a = one;
        fx b = fsqrt(mneg ? one + M : one - M);
        sfx s{ M.shr(1), mneg };
        iters = 0;
        for (int n = 1; n < 64; ++n) {
            const fx c = a < b ? (b - a).shr(1) : (a - b).shr(1);
            const fx an = (a + b).shr(1);
            if (c.top() < 4) { a = an; break; }
            b = fsqrt(a * b);
            a = an;
            s = s + sfx{ (c * c).shl(static_cast<std::size_t>(n - 1)), false };
            ++iters;
        }
        S = s.m;
        Sneg = s.neg;
        return true;
    }

    static bool elliptic_k(const C& m, C& out) noexcept {
        fx a, S;
        bool sn = false;
        int it = 0;
        if (!agm_k(m, a, S, sn, it)) return false;
        return finish(false, fdiv(half_pi_constant<N>(), a), 0, 16 + it, out);
    }

    static bool elliptic_e(const C& m, C& out) noexcept {
        fx a, S;
        bool sn = false;
        int it = 0;
        if (!agm_k(m, a, S, sn, it)) return false;
        if (!sn && !(S < fx::one())) return false;
        const fx Kv = fdiv(half_pi_constant<N>(), a);
        const fx om = sn ? fx::one() + S : fx::one() - S;
        return finish(false, Kv * om, 0, 18 + it + amp(om) + (Kv.top() > frac ? Kv.top() - frac : 0), out);
    }

    static bool lambert(const C& x, bool lower, C& out) noexcept {
        if (!x.is_finite() || x.is_zero()) return false;
        const bool xneg = x.is_negative();
        if (lower && !xneg) return false;
        fx m;
        long long e = 0;
        if (!get_mant(x, m, e) || e < small_top) return false;
        const sfx L = K::log_pos(m) + sfx{ ln2_constant<N + 1>().mul_small(static_cast<std::uint64_t>(e < 0 ? -e : e)).template resize<N>(), e < 0 };
        const double Ld = (L.neg ? -1.0 : 1.0) * L.m.to_double();
        double wd;
        if (!xneg) {
            wd = Ld > 1.0 ? Ld - std::log(Ld) : std::exp(Ld) / (1.0 + std::exp(Ld));
        } else {
            const double xv = -std::exp(Ld);
            const double p2 = 2.0 * (2.718281828459045 * xv + 1.0);
            if (!(p2 > 1e-6)) return false;
            const double p = std::sqrt(p2);
            if (lower) wd = Ld < -3.0 ? Ld - std::log(-Ld) : -1.0 - p - p2 / 3.0;
            else wd = xv > -0.25 ? xv * (1.0 - xv * (1.0 - 1.5 * xv)) : -1.0 + p - p2 / 3.0;
        }
        if (std::fabs(Ld) < 700.0) {
            const double xv = (xneg ? -1.0 : 1.0) * std::exp(Ld);
            for (int i = 0; i < 60; ++i) {
                const double ew = std::exp(wd);
                const double f = wd * ew - xv;
                const double d = ew * (wd + 1.0) - (wd + 2.0) * f / (2.0 * wd + 2.0);
                if (d == 0.0) break;
                const double st = f / d;
                wd -= st;
                if (std::fabs(st) <= 1e-16 * std::fabs(wd)) break;
            }
        } else {
            for (int i = 0; i < 60; ++i) {
                const double g = wd + std::log(std::fabs(wd)) - Ld;
                const double st = g / (1.0 + 1.0 / wd);
                wd -= st;
                if (std::fabs(st) < 1e-15 * std::fabs(wd)) break;
            }
        }
        if (xneg != (wd < 0.0)) return false;
        if (!std::isfinite(wd) || wd == 0.0) return false;
        if (lower ? !(wd < -1.0) : !(wd > -1.0)) return false;
        if (std::fabs(1.0 + wd) < 1e-6) return false;
        sfx w{ fx::from_double(std::fabs(wd)), wd < 0.0 };
        double bits = 40.0;
        for (int it = 0; it < 10 && bits < static_cast<double>(frac) + 40.0; ++it) {
            const sfx g = w + K::log_pos(w.m) - L;
            const fx den = w.neg ? (w.m < fx::one() ? fdiv(fx::one(), w.m) - fx::one() : fx::one() - fdiv(fx::one(), w.m)) : fx::one() + fdiv(fx::one(), w.m);
            const bool dneg = w.neg && w.m < fx::one();
            sfx st{ fdiv(g.m, den), g.neg != dneg };
            w = w - st;
            bits *= 2.0;
        }
        const fx wp1 = w.neg ? (w.m < fx::one() ? fx::one() - w.m : w.m - fx::one()) : w.m + fx::one();
        return finish(w.neg, w.m, 0, 16 + 2 * amp(wp1), out);
    }

    static bool sin_of(const fx& phi, sfx& s, sfx& c) noexcept {
        if (phi.top() >= frac + 58) return false;
        const fw X = phi.template resize<N + 1>();
        const fw& H = half_pi_constant<N + 1>();
        const std::uint64_t n = static_cast<std::uint64_t>(std::floor(X.to_double() / 1.57079632679489661923 + 0.5));
        const fw nh = H.mul_small(n);
        const bool rneg = X < nh;
        const fx r = (rneg ? nh - X : X - nh).template resize<N>();
        fx sv, v;
        if (r.is_zero()) { sv = fx(); v = fx(); } else K::sin_vers(r, sv, v);
        const fx cv = fx::one() - v;
        const unsigned q = static_cast<unsigned>(n & 3u);
        if (q == 0) { s = sfx{ sv, rneg }; c = sfx{ cv, false }; }
        else if (q == 1) { s = sfx{ cv, false }; c = sfx{ sv, !rneg }; }
        else if (q == 2) { s = sfx{ sv, !rneg }; c = sfx{ cv, true }; }
        else { s = sfx{ cv, true }; c = sfx{ sv, rneg }; }
        if (s.m.is_zero()) s.neg = false;
        if (c.m.is_zero()) c.neg = false;
        return true;
    }

    static void quadrant(const fx& r, bool rneg, std::uint64_t n, sfx& s, sfx& c) noexcept {
        fx sv, v;
        if (r.is_zero()) { sv = fx(); v = fx(); } else K::sin_vers(r, sv, v);
        const fx cv = fx::one() - v;
        const unsigned q = static_cast<unsigned>(n & 3u);
        if (q == 0) { s = sfx{ sv, rneg }; c = sfx{ cv, false }; }
        else if (q == 1) { s = sfx{ cv, false }; c = sfx{ sv, !rneg }; }
        else if (q == 2) { s = sfx{ sv, !rneg }; c = sfx{ cv, true }; }
        else { s = sfx{ cv, true }; c = sfx{ sv, rneg }; }
        if (s.m.is_zero()) s.neg = false;
        if (c.m.is_zero()) c.neg = false;
    }

    static bool sincos_pi(const C& x, sfx& s, sfx& c) noexcept {
        fw X;
        long long top = 0;
        if (!loadw(x, X, top) || top >= 44) return false;
        const fw Xh = X.shl(1);
        std::uint64_t n = Xh.v[N];
        fw rem = Xh;
        rem.v[N] = 0;
        bool rneg = false;
        if (!(rem < fw::one().shr(1))) { rem = fw::one() - rem; ++n; rneg = true; }
        const fx r = (rem.shr(1) * pi_constant<N + 1>()).template resize<N>();
        quadrant(r, rneg, n, s, c);
        if (x.is_negative() && !s.m.is_zero()) s.neg = !s.neg;
        return true;
    }

    static sfx asin_of(const sfx& r) noexcept {
        const fx c = fsqrt((fx::one() - r.m) * (fx::one() + r.m));
        return sfx{ K::atan_pos(r.m, c), r.neg };
    }

    static bool jacobi(int which, const C& u, const C& m, C& out) noexcept {
        if (!u.is_finite() || !m.is_finite() || u.is_zero() || m.is_zero() || m.is_negative()) return false;
        fx U, M;
        long long tu = 0, tm = 0;
        if (!load(u, U, tu) || tu >= 24 || tu < small_top) return false;
        if (!load(m, M, tm) || !(M < fx::one()) || tm < small_top) return false;
        const fx one = fx::one();
        fx a[40], c[40];
        a[0] = one;
        fx b = fsqrt(one - M);
        c[0] = fsqrt(M);
        int Nn = 0;
        while (Nn < 38) {
            const fx an = (a[Nn] + b).shr(1);
            const fx cn = (a[Nn] < b ? b - a[Nn] : a[Nn] - b).shr(1);
            b = fsqrt(a[Nn] * b);
            ++Nn;
            a[Nn] = an;
            c[Nn] = cn;
            if (cn.top() < 4) break;
        }
        if (Nn >= 38) return false;
        fx phi = (U * a[Nn]).shl(static_cast<std::size_t>(Nn));
        if (phi.top() >= frac + 56) return false;
        fx phi1 = phi;
        for (int n = Nn; n >= 1; --n) {
            sfx s, cc;
            if (!sin_of(phi, s, cc)) return false;
            const sfx r{ fdiv(c[n], a[n]) * s.m, s.neg };
            if (!(r.m < one)) return false;
            const sfx as = asin_of(r);
            phi1 = phi;
            const sfx t = sfx{ phi, false } + as;
            if (t.neg) return false;
            phi = t.m.shr(1);
        }
        const bool uneg = u.is_negative();
        const long long eb = 26 + 3 * static_cast<long long>(Nn);
        if (which == 0) return finish(uneg, phi, 0, eb, out);
        sfx s, cc;
        if (!sin_of(phi, s, cc)) return false;
        const fx d = phi1 < phi ? phi - phi1 : phi1 - phi;
        sfx s2, c2;
        if (!sin_of(d, s2, c2) || c2.m.is_zero()) return false;
        const sfx dn{ fdiv(cc.m, c2.m), cc.neg != c2.neg };
        const long long ed = eb + 2 + 2 * amp(c2.m);
        const bool sn_neg = s.neg != uneg;
        switch (which) {
        case 1: return finish(sn_neg, s.m, 0, eb, out);
        case 2: return finish(cc.neg, cc.m, 0, eb, out);
        case 3: return finish(dn.neg, dn.m, 0, ed, out);
        case 4: return finish(sn_neg, fdiv(one, s.m), 0, eb + 2 + 2 * amp(s.m), out);
        case 5: return finish(cc.neg, fdiv(one, cc.m), 0, eb + 2 + 2 * amp(cc.m), out);
        case 6: return finish(dn.neg, fdiv(one, dn.m), 0, ed + 2 + 2 * amp(dn.m), out);
        case 7: return finish(sn_neg != cc.neg, fdiv(s.m, cc.m), 0, eb + 2 + 2 * amp(cc.m), out);
        case 8: return finish(sn_neg != dn.neg, fdiv(s.m, dn.m), 0, ed + 2 + 2 * amp(dn.m), out);
        case 9: return finish(sn_neg != cc.neg, fdiv(cc.m, s.m), 0, eb + 2 + 2 * amp(s.m), out);
        case 10: return finish(cc.neg != dn.neg, fdiv(cc.m, dn.m), 0, ed + 2 + 2 * amp(dn.m), out);
        case 11: return finish(sn_neg != dn.neg, fdiv(dn.m, s.m), 0, ed + 2 + 2 * amp(s.m), out);
        default: return finish(cc.neg != dn.neg, fdiv(dn.m, cc.m), 0, ed + 2 + 2 * amp(cc.m), out);
        }
    }
};

} // namespace fpmath
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_POINT_MATH_HPP
