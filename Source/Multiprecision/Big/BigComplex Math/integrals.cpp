#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

constexpr double log2e = 1.4426950408889634;
constexpr double pi_d  = 3.141592653589793;

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

double mag_d(const CX& z) { return std::exp2(std::min(l2(z), 900.0)); }

double re_d(const CX& z) {
    const double l = l2(z.re);
    if (l == ninf) return 0.0;
    return std::exp2(std::min(l, 900.0)) * (z.re.signbit() ? -1.0 : 1.0);
}

double phase(const CX& z) {
    const double a = l2(z.re), b = l2(z.im);
    const double m = std::max(a, b);
    if (m == ninf) return 0.0;
    const double x = a == ninf ? 0.0 : std::exp2(std::max(a - m, -1000.0)) * (z.re.signbit() ? -1.0 : 1.0);
    const double y = b == ninf ? 0.0 : std::exp2(std::max(b - m, -1000.0)) * (z.im.signbit() ? -1.0 : 1.0);
    return std::atan2(y, x);
}

BF gamma_e(std::size_t w) { return multiprecision::constants::euler_mascheroni(wctx(w)); }

BF psi_int(std::uint64_t n, std::size_t w) {
    const BFC c = wctx(w + 16);
    BF h = BF::zero();
    for (std::uint64_t j = 1; j < n; ++j) h = BF::add(h, BF::div(BF::one(), BF(j), c), c);
    return BF::sub(h, gamma_e(w + 16), wctx(w));
}

Approx en_series(const CX& u, std::uint64_t n, std::size_t w) {
    const double au   = mag_d(u);
    const double loss = (au + re_d(u)) * log2e;
    const std::size_t wb = w + bits_of(loss) + bits_of(std::log2(static_cast<double>(n) + 1.0)) + 16;
    const CX mu = neg(u);
    const CX L  = ln(u, wb + 8);
    const BF ps = psi_int(n, wb + 8);
    const CX D  = sub(CX(ps), L, wb + 8);
    const double lD = l2(D);
    CX   p(BF::one());
    CX   pk;
    CX   sum;
    Term T;
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t k = 0;; ++k) {
        if (k + 1 == n) {
            pk = p;
            T.add(l2(p) + lD);
        } else {
            const std::int64_t den = static_cast<std::int64_t>(k) - static_cast<std::int64_t>(n) + 1;
            const CX term = div(p, BF(den), wb);
            sum = add(sum, term, wb);
            T.add(l2(term));
        }
        p = div(mul(p, mu, wb), BF(k + 1), wb);
        if (k + 1 >= n && std::log2(au) - std::log2(static_cast<double>(k + 2)) < -1.0) {
            const double bound = l2(p) + 1.0;
            if (bound == ninf || bound < T.l2max - static_cast<double>(wb)) {
                tail = bound;
                K    = k;
                break;
            }
        }
    }
    const CX spec = mul(pk, D, wb);
    const CX v    = sub(spec, sum, wb);
    const double e = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K + n) + 8.0) - static_cast<double>(wb), tail) + 1.0;
    return approx(v, lsum(e, l2(v) - static_cast<double>(wb) + 2.0));
}

bool en_asym(const CX& u, std::uint64_t n, std::size_t w, Approx& out) {
    const double ph = std::fabs(phase(u));
    if (ph > 3.0 * pi_d / 4.0) return false;
    if (l2(u) + std::log2(log2e) < std::log2(static_cast<double>(w) + 24.0)) return false;
    const double cf = ph <= pi_d / 2.0 ? 1.0 : 1.5 - std::log2(std::sin(ph));
    const std::size_t wb = w + 16;
    const CX r  = recip(u, wb + 8);
    const double lr = l2(r);
    CX   a(BF::one());
    CX   sum = a;
    Term T;
    T.add(0.0);
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t k = 1;; ++k) {
        const double nk = static_cast<double>(n) + static_cast<double>(k) - 1.0;
        if (std::log2(nk) + lr >= -0.5) return false;
        a = neg(mul(mul(a, r, wb), BF(n + k - 1), wb));
        const double la = l2(a);
        if (la + cf < T.l2max - static_cast<double>(wb) - 4.0) {
            tail = la + cf;
            K    = k;
            break;
        }
        sum = add(sum, a, wb);
        T.add(la);
    }
    const CX P = mul(exp(neg(u), wb + 8), r, wb);
    const CX v = mul(sum, P, wb);
    const double es = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail);
    out = approx(v, lsum(l2(P) + es, l2(v) - static_cast<double>(wb) + 5.0));
    return true;
}

Approx en_raw(const CX& u, std::uint64_t n, std::size_t w) {
    Approx a;
    if (en_asym(u, n, w, a)) return a;
    return en_series(u, n, w);
}

Approx en_neg(const CX& z, std::uint64_t m, std::size_t w) {
    const std::size_t wb = w + bits_of(std::log2(static_cast<double>(m) + 2.0)) + 16;
    CX P(BF::one());
    CX vj(BF::one());
    BF B  = BF::one();
    BF av = BF::one();
    const BF az = math::sqrt(BF::add(BF::mul(z.re, z.re, wctx(64)), BF::mul(z.im, z.im, wctx(64)), wctx(64)), wctx(64));
    for (std::uint64_t j = 1; j <= m; ++j) {
        vj = mul(vj, z, wb);
        P  = add(vj, mul(P, BF(j), wb), wb);
        av = BF::mul(av, az, wctx(64));
        B  = BF::add(av, BF::mul(B, BF(j), wctx(64)), wctx(64));
    }
    const std::size_t wz = wb + bits_of(l2(z)) + 8;
    const CX Q = div(exp(neg(z), wz), pow_int(z, static_cast<std::int64_t>(m + 1), wb + 8), wb);
    const CX v = mul(P, Q, wb);
    const double e = l2(Q) + l2(B) + std::log2(3.0 * static_cast<double>(m) + 8.0) - static_cast<double>(wb);
    return approx(v, lsum(e, l2(v) - static_cast<double>(wb) + 4.0));
}

Approx add_i(Approx a, const BF& im, std::size_t w) {
    a.v.im = BF::add(a.v.im, im, wctx(w + 8));
    a.ei   = lsum(a.ei, l2(a.v.im) - static_cast<double>(w + 8));
    return a;
}

Approx scale_a(Approx a, std::int64_t k) {
    a.v = scale2(a.v, k);
    if (a.er != ninf) a.er += static_cast<double>(k);
    if (a.ei != ninf) a.ei += static_cast<double>(k);
    return a;
}

Approx comb(const Approx& p, const Approx& q, bool minus, std::size_t w) {
    Approx r;
    r.v  = minus ? sub(p.v, q.v, w + 8) : add(p.v, q.v, w + 8);
    r.er = lsum(lsum(p.er, q.er), l2(r.v.re) - static_cast<double>(w + 8));
    r.ei = lsum(lsum(p.ei, q.ei), l2(r.v.im) - static_cast<double>(w + 8));
    return r;
}

Approx neg_a(Approx a) {
    a.v = neg(a.v);
    return a;
}

Approx rot(Approx a, bool times_i) {
    a.v = times_i ? mul_i(a.v) : div_i(a.v);
    std::swap(a.er, a.ei);
    return a;
}

BF half_pi_signed(bool negative, std::size_t w) {
    BF h = pi(w).scaled_pow2(-1);
    return negative ? -h : h;
}

Approx hyp_series(const CX& z, bool odd, bool alt, bool logs, std::size_t w) {
    const std::size_t wb = w + bits_of(mag_d(z) * log2e) + 16;
    const double lz = l2(z);
    CX   p = z;
    CX   sum;
    Term T;
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t j = 1;; ++j) {
        if ((j & 1u) == (odd ? 1u : 0u)) {
            CX term = div(p, BF(j), wb);
            if (alt && ((j / 2) & 1u)) term = neg(term);
            sum = add(sum, term, wb);
            T.add(l2(term));
        }
        p = div(mul(p, z, wb), BF(j + 1), wb);
        if (lz - std::log2(static_cast<double>(j + 2)) < -1.0) {
            const double bound = l2(p) + 1.0;
            if (bound == ninf || bound < T.l2max - static_cast<double>(wb)) {
                tail = bound;
                K    = j;
                break;
            }
        }
    }
    double e = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail) + 1.0;
    if (logs) {
        const CX L = ln(z, wb + 8);
        sum = add(add_real(L, gamma_e(wb + 8), wb + 8), sum, wb);
        e   = lsum(e, l2(L) - static_cast<double>(wb) + 2.0);
    }
    return approx(sum, lsum(e, l2(sum) - static_cast<double>(wb) + 2.0));
}

bool small_regime(const CX& z, std::size_t w) { return l2(z) + std::log2(log2e) < std::log2(static_cast<double>(w) + 24.0); }

Approx shi_raw(const CX& z, std::size_t w) {
    if (small_regime(z, w)) return hyp_series(z, true, false, false, w);
    const Approx a = en_raw(z, 1, w + 4);
    const Approx b = en_raw(neg(z), 1, w + 4);
    return add_i(scale_a(comb(a, b, true, w + 4), -1), half_pi_signed(z.im.signbit(), w + 8), w);
}

Approx chi_raw(const CX& z, std::size_t w) {
    if (small_regime(z, w)) return hyp_series(z, false, false, true, w);
    const Approx a = en_raw(z, 1, w + 4);
    const Approx b = en_raw(neg(z), 1, w + 4);
    return add_i(neg_a(scale_a(comb(a, b, false, w + 4), -1)), half_pi_signed(z.im.signbit(), w + 8), w);
}

Approx ci_raw(const CX& z, std::size_t w) {
    if (small_regime(z, w)) return hyp_series(z, false, true, true, w);
    const CX iz = mul_i(z);
    const Approx a = en_raw(iz, 1, w + 4);
    const Approx b = en_raw(neg(iz), 1, w + 4);
    Approx r = neg_a(scale_a(comb(a, b, false, w + 4), -1));
    if (z.re.signbit()) r = add_i(r, z.im.signbit() ? -pi(w + 8) : pi(w + 8), w);
    return r;
}

Approx si_raw(const CX& z, std::size_t w) {
    if (small_regime(z, w)) return hyp_series(z, true, true, false, w);
    return rot(shi_raw(mul_i(z), w), false);
}

Approx ei_raw(const CX& z, std::size_t w) {
    const Approx a = en_raw(neg(z), 1, w + 4);
    return add_i(neg_a(a), z.im.signbit() ? -pi(w + 8) : pi(w + 8), w);
}

bool real_axis(const BC& z) { return z.imaginary().is_zero(); }
bool imag_axis(const BC& z) { return z.real().is_zero() && !z.imaginary().is_zero(); }

BC real_neg_shift(const BF& r, const BC& z, const BCC& ctx, BF (*f)(const BF&, const BFC&)) {
    const BF p = multiprecision::constants::pi(ctx.imaginary());
    return BC(f(r, ctx.real()), z.imaginary().signbit() ? -p : p);
}

BF ci_real(const BF& x, const BFC& c) { return math::cos_integral(x, c); }
BF chi_real(const BF& x, const BFC& c) { return math::cosh_integral(x, c); }

BC axis_log_shift(const BF& v, bool negative, const BCC& ctx) {
    const BF h = multiprecision::constants::pi(ctx.imaginary()).scaled_pow2(-1);
    return BC(v, negative ? -h : h);
}

} // namespace

BC sin_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z)) return BC(math::sin_integral(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), math::sinh_integral(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return si_raw(Z, w); });
}

BC sinh_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z)) return BC(math::sinh_integral(z.real(), ctx.real()), BF::zero());
    if (imag_axis(z)) return BC(BF::zero(), math::sin_integral(z.imaginary(), ctx.imaginary()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return shi_raw(Z, w); });
}

BC cos_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z)) {
        if (!z.real().signbit() || z.real().is_zero()) return BC(math::cos_integral(z.real().abs(), ctx.real()), BF::zero());
        if (!z.real().is_finite()) return BC::undefined();
        return real_neg_shift(z.real().abs(), z, ctx, &ci_real);
    }
    if (imag_axis(z)) {
        if (!z.imaginary().is_finite()) return BC::undefined();
        return axis_log_shift(math::cosh_integral(z.imaginary().abs(), ctx.real()), z.imaginary().signbit(), ctx);
    }
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return ci_raw(Z, w); });
}

BC cosh_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z)) {
        if (!z.real().signbit() || z.real().is_zero()) return BC(math::cosh_integral(z.real().abs(), ctx.real()), BF::zero());
        if (!z.real().is_finite()) return BC::undefined();
        return real_neg_shift(z.real().abs(), z, ctx, &chi_real);
    }
    if (imag_axis(z)) {
        if (!z.imaginary().is_finite()) return BC::undefined();
        return axis_log_shift(math::cos_integral(z.imaginary().abs(), ctx.real()), z.imaginary().signbit(), ctx);
    }
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return chi_raw(Z, w); });
}

BC exp_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z)) return BC(math::exp_integral(z.real(), ctx.real()), BF::zero());
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return ei_raw(Z, w); });
}

BC exp_integral_generalized(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z) && (n <= 0 || !z.real().signbit() || z.real().is_zero() || !z.real().is_finite())) {
        return BC(math::exp_integral_generalized(z.real(), n, ctx.real()), BF::zero());
    }
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    if (n <= 0) {
        const std::uint64_t m = static_cast<std::uint64_t>(-(n + 1)) + 1u;
        return drive(ctx, [&](std::size_t w) { return en_neg(Z, m, w); });
    }
    return drive(ctx, [&](std::size_t w) { return en_raw(Z, static_cast<std::uint64_t>(n), w); });
}

namespace {

Approx li_n_raw(const CX& z, std::int64_t n, std::size_t w) {
    const std::size_t wy = w + 24 + bits_of(std::fabs(l2(z)));
    const CX y = ln(z, wy);
    const double ly = l2(y);
    if (n == 1) {
        Approx a = ei_raw(y, w);
        const double pe = l2(z) - static_cast<double>(wy) + 3.0;
        a.er = lsum(a.er, pe);
        a.ei = lsum(a.ei, pe);
        return a;
    }
    if (n <= 0) {
        const std::uint64_t m  = n == 0 ? 0 : static_cast<std::uint64_t>(-(n + 1)) + 1u;
        const std::size_t   wb = w + bits_of(std::log2(static_cast<double>(m) + 2.0)) + 16;
        const CX v = neg(y);
        CX P(BF::one());
        CX vj(BF::one());
        BF B  = BF::one();
        BF av = BF::one();
        const BF ay = BF(std::exp2(std::min(ly, 900.0)));
        for (std::uint64_t j = 1; j <= m; ++j) {
            vj = mul(vj, v, wb);
            P  = add(vj, mul(P, BF(j), wb), wb);
            av = BF::mul(av, ay, wctx(64));
            B  = BF::add(av, BF::mul(B, BF(j), wctx(64)), wctx(64));
        }
        CX r = mul(P, z, wb);
        if (m & 1u) r = neg(r);
        double e = l2(z) + l2(B) + std::log2(3.0 * static_cast<double>(m) + 8.0) - static_cast<double>(wb) + 1.0;
        double dB = 0.0;
        if (m > 0) dB = std::log2(static_cast<double>(m)) + l2(B) - std::max(ly, 0.0) + 2.0;
        e = lsum(e, l2(z) + dB + ly - static_cast<double>(wy) + 2.0);
        return approx(r, lsum(e, l2(r) - static_cast<double>(wb) + 2.0));
    }
    const std::uint64_t un = static_cast<std::uint64_t>(n);
    Approx a = en_raw(neg(y), un, w + 8);
    const std::size_t wb = w + 16;
    const CX yp = pow_int(y, n - 1, wb + 8);
    const CX v  = neg(div(a.v, yp, wb));
    const double scale = -l2(yp);
    BF f = BF::one();
    for (std::uint64_t j = 2; j < un; ++j) f = BF::mul(f, BF(j), wctx(wb + 8));
    BF c = BF::div(pi(wb + 8), f, wctx(wb));
    if (z.im.signbit()) c = -c;
    Approx r;
    r.v  = CX(v.re, BF::add(v.im, c, wctx(wb)));
    const double pe = l2(z) - static_cast<double>(n) * ly + ly - static_cast<double>(wy) + 3.0;
    r.er = lsum(lsum(a.er + scale, l2(v) - static_cast<double>(wb) + 3.0), pe);
    r.ei = lsum(lsum(a.ei + scale, l2(r.v.im) - static_cast<double>(wb) + 3.0), pe);
    r.ei = lsum(r.ei, l2(v) - static_cast<double>(wb) + 3.0);
    return r;
}

} // namespace

BC log_integral(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z) && (!z.real().signbit() || z.real().is_zero())) return BC(math::log_integral(z.real(), ctx.real()), BF::zero());
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return li_n_raw(Z, 1, w); });
}

BC log_integral_generalized(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (real_axis(z) && (!z.real().signbit() || z.real().is_zero())) return BC(math::log_integral_generalized(z.real(), n, ctx.real()), BF::zero());
    if (!z.is_finite()) return BC::undefined();
    if (n == 0) return BC(z.real().rounded(ctx.real()), z.imaginary().rounded(ctx.imaginary()));
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return li_n_raw(Z, n, w); });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
