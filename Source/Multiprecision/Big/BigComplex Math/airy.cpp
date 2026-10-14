#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

enum class kind { ai, bi, gi, hi };

constexpr double log2e = 1.4426950408889634;
constexpr double pi_d  = 3.141592653589793;

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

double phase(const CX& z) {
    const double a = l2(z.re), b = l2(z.im);
    const double m = std::max(a, b);
    if (m == ninf) return 0.0;
    const double x = a == ninf ? 0.0 : std::exp2(std::max(a - m, -1000.0)) * (z.re.signbit() ? -1.0 : 1.0);
    const double y = b == ninf ? 0.0 : std::exp2(std::max(b - m, -1000.0)) * (z.im.signbit() ? -1.0 : 1.0);
    return std::atan2(y, x);
}

double zeta_mag(const CX& z) { return (2.0 / 3.0) * std::exp2(std::min(1.5 * l2(z), 900.0)); }

struct Consts {
    BF c1, c2, s3, ip;
};

Consts consts(std::size_t w) {
    const std::size_t p = w + 16;
    const BFC c = wctx(p);
    const BF  three(static_cast<std::int64_t>(3));
    const BF  g13 = math::gamma(BF::div(BF::one(), three, c), c);
    const BF  l3  = math::ln(three, c);
    const BF  p16 = math::exp(-BF::div(l3, BF(static_cast<std::int64_t>(6)), c), c);
    const BF  p13 = math::exp(-BF::div(l3, three, c), c);
    Consts K;
    K.c1 = BF::div(BF::mul(p16, g13, c), multiprecision::constants::two_pi(c), wctx(w));
    K.c2 = BF::div(p13, g13, wctx(w));
    K.s3 = math::sqrt(three, wctx(w));
    K.ip = multiprecision::constants::inv_pi(wctx(w));
    return K;
}

CX unit(int j, std::size_t w) {
    const BF h  = BF::one().scaled_pow2(-1);
    const BF s3 = math::sqrt(BF(static_cast<std::int64_t>(3)), wctx(w)).scaled_pow2(-1);
    switch (((j % 12) + 12) % 12) {
        case 0: return CX(BF::one());
        case 1: return CX(s3, h);
        case 2: return CX(h, s3);
        case 3: return CX(BF::zero(), BF::one());
        case 4: return CX(-h, s3);
        case 5: return CX(-s3, h);
        case 6: return CX(BF::one(true));
        case 7: return CX(-s3, -h);
        case 8: return CX(-h, -s3);
        case 9: return CX(BF::zero(), BF::one(true));
        case 10: return CX(h, -s3);
        default: return CX(s3, -h);
    }
}

Approx series(const CX& z, kind k, std::size_t w) {
    const double lz   = l2(z);
    const double loss = (4.0 / 3.0) * std::exp2(std::min(1.5 * lz, 900.0)) * log2e;
    const std::size_t wb = w + bits_of(loss) + 16;
    const bool need_h = k == kind::gi || k == kind::hi;
    const CX z3 = mul(sqr(z, wb + 8), z, wb + 8);
    const double lz3 = l2(z3);
    CX t(BF::one()), u = z, v = need_h ? scale2(sqr(z, wb), -1) : CX();
    CX f = t, g = u, h = v;
    double E   = lsum(0.0, lsum(l2(u), need_h ? l2(v) : ninf));
    double tail = ninf;
    std::uint64_t M = 0;
    for (std::uint64_t m = 0;; ++m) {
        const std::uint64_t a = 3 * m;
        t = div(mul(t, z3, wb), BF((a + 2) * (a + 3)), wb);
        u = div(mul(u, z3, wb), BF((a + 3) * (a + 4)), wb);
        if (need_h) v = div(mul(v, z3, wb), BF((a + 4) * (a + 5)), wb);
        f = add(f, t, wb);
        g = add(g, u, wb);
        if (need_h) h = add(h, v, wb);
        const double lt = std::max(l2(t), std::max(l2(u), need_h ? l2(v) : ninf));
        E = lsum(E, lt + 1.6);
        const double rho = lz3 - std::log2(static_cast<double>(a + 5) * static_cast<double>(a + 6));
        if (rho < -1.0) {
            const double bound = lt + 2.0;
            if (lt == ninf || bound < E - static_cast<double>(wb)) {
                tail = bound;
                M    = m;
                break;
            }
        }
    }
    const double es = lsum(E + std::log2(3.0 * static_cast<double>(M) + 12.0) - static_cast<double>(wb), tail);
    const Consts K = consts(wb + 8);
    const CX c1f = mul(f, K.c1, wb);
    const CX c2g = mul(g, K.c2, wb);
    CX r;
    double scale = 0.0;
    switch (k) {
        case kind::ai: r = sub(c1f, c2g, wb); break;
        case kind::bi:
            r     = mul(add(c1f, c2g, wb), K.s3, wb);
            scale = 0.8;
            break;
        case kind::hi: r = add(div(scale2(add(c1f, c2g, wb), 1), K.s3, wb), mul(h, K.ip, wb), wb); scale = 0.3; break;
        default: r = sub(div(add(c1f, c2g, wb), K.s3, wb), mul(h, K.ip, wb), wb); break;
    }
    const double e = es + scale + 1.0;
    return approx(r, lsum(e, lsum(l2(c1f), l2(c2g)) + scale - static_cast<double>(wb) + 4.0));
}

bool ai_asym(const CX& z, std::size_t w, Approx& out) {
    const double ph = std::fabs(phase(z));
    if (ph > 2.0 * pi_d / 3.0 + 1e-9) return false;
    const double zm = zeta_mag(z);
    if (zm < 0.35 * (static_cast<double>(w) + 24.0)) return false;
    const std::size_t wb = w + 16;
    const std::size_t wz = wb + bits_of(1.5 * l2(z)) + 8;
    const CX sz  = sqrt(z, wz);
    const CX zt  = scale2(div(mul(z, sz, wz), BF(static_cast<std::int64_t>(3)), wz), 1);
    const CX iz  = recip(zt, wb + 8);
    const double lz = l2(zt);
    const bool region1 = ph <= pi_d / 3.0 + 1e-9;
    const double base = region1 ? 1.0 + 7.0 / (36.0 * zm) * log2e : 1.0 + 7.0 * pi_d / (72.0 * zm) * log2e;
    CX   term(BF::one());
    CX   S = term;
    Term T;
    T.add(0.0);
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t k = 1;; ++k) {
        const double num = static_cast<double>(6 * k - 5) * static_cast<double>(6 * k - 3) * static_cast<double>(6 * k - 1);
        const double den = static_cast<double>(2 * k - 1) * 216.0 * static_cast<double>(k);
        if (std::log2(num / den) - lz >= -0.5) return false;
        term = neg(div(mul(mul(term, iz, wb), BF((6 * k - 5) * (6 * k - 3) * (6 * k - 1)), wb), BF((2 * k - 1) * 216 * k), wb));
        const double lt  = l2(term);
        const double chi = region1 ? 0.0 : 0.5 * std::log2(pi_d * (static_cast<double>(k) + 2.0) / 2.0);
        const double bound = lt + base + chi;
        if (bound < -static_cast<double>(wb) - 4.0) {
            tail = bound;
            K    = k;
            break;
        }
        S = add(S, term, wb);
        T.add(lt);
    }
    const BFC pc = wctx(wb + 8);
    const CX  q  = sqrt(sz, wb + 8);
    const BF  tp = BF::mul(math::sqrt(multiprecision::constants::pi(pc), pc), BF(static_cast<std::int64_t>(2)), pc);
    const CX  P  = div(exp(neg(zt), wb + 8), mul(q, tp, wb + 8), wb);
    const CX  v  = mul(S, P, wb);
    const double es = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail);
    out = approx(v, lsum(l2(P) + es, l2(v) - static_cast<double>(wb) + 5.0));
    return true;
}

bool large(const CX& z, std::size_t w) { return zeta_mag(z) >= 0.35 * (static_cast<double>(w) + 24.0); }

Approx lin2(const CX& a, const Approx& x, const CX& b, const Approx& y, std::size_t w) {
    const std::size_t wb = w + 8;
    Approx r;
    r.v = add(mul(a, x.v, wb), mul(b, y.v, wb), wb);
    const double ex = lsum(x.er, x.ei) + std::max(l2(a), 0.0) + 0.5;
    const double ey = lsum(y.er, y.ei) + std::max(l2(b), 0.0) + 0.5;
    const double ro = lsum(l2(mul(a, x.v, 64)), l2(mul(b, y.v, 64))) - static_cast<double>(wb) + 3.0;
    r.er = lsum(lsum(ex, ey), ro);
    r.ei = r.er;
    return r;
}

Approx with_prop(Approx a, const CX& z, std::size_t wr) {
    const double p = l2(a.v) + 1.5 * std::max(l2(z), 0.0) + 2.0 - static_cast<double>(wr);
    a.er = lsum(a.er, p);
    a.ei = lsum(a.ei, p);
    return a;
}

Approx ai_eval(const CX& z, std::size_t w);

Approx ai_rotated(const CX& z, int j, std::size_t w) {
    const std::size_t wr = w + bits_of(1.5 * l2(z)) + 16;
    const CX zr = mul(z, unit(j, wr + 8), wr);
    return with_prop(ai_eval(zr, w), zr, wr);
}

Approx ai_eval(const CX& z, std::size_t w) {
    if (!large(z, w)) return series(z, kind::ai, w);
    Approx a;
    if (ai_asym(z, w, a)) return a;
    if (std::fabs(phase(z)) > 2.0 * pi_d / 3.0) {
        const Approx a1 = ai_rotated(z, 8, w + 2);
        const Approx a2 = ai_rotated(z, 4, w + 2);
        return lin2(unit(2, w + 16), a1, unit(-2, w + 16), a2, w);
    }
    return series(z, kind::ai, w);
}

Approx bi_eval(const CX& z, std::size_t w) {
    if (!large(z, w)) return series(z, kind::bi, w);
    if (std::fabs(phase(z)) > 2.0 * pi_d / 3.0) {
        const Approx a1 = ai_rotated(z, 8, w + 2);
        const Approx a2 = ai_rotated(z, 4, w + 2);
        return lin2(unit(-1, w + 16), a1, unit(1, w + 16), a2, w);
    }
    const Approx a0 = ai_eval(z, w + 2);
    if (!z.im.signbit()) {
        const Approx ar = ai_rotated(z, -4, w + 2);
        return lin2(neg(scale2(mul_i(unit(2, w + 16)), 1)), ar, CX(BF::zero(), BF::one()), a0, w);
    }
    const Approx ar = ai_rotated(z, 4, w + 2);
    return lin2(scale2(mul_i(unit(-2, w + 16)), 1), ar, CX(BF::zero(), BF::one(true)), a0, w);
}

bool hi_asym(const CX& z, std::size_t w, Approx& out) {
    const CX     mz = neg(z);
    const double ph = std::fabs(phase(mz));
    if (ph >= 2.0 * pi_d / 3.0 - 0.02) return false;
    const double eff = std::max(0.0, ph - pi_d / 6.0);
    const double lR  = l2(z) + std::log2(std::cos(eff));
    const double lzm = l2(z);
    const std::size_t wb = w + 16;
    const CX iz3 = recip(mul(sqr(z, wb + 8), z, wb + 8), wb + 8);
    CX   a(BF::one());
    CX   S = a;
    Term T;
    T.add(0.0);
    double lb = 0.0;
    double tail = ninf;
    std::uint64_t K = 0;
    for (std::uint64_t k = 1;; ++k) {
        const double f = std::log2(static_cast<double>(3 * k - 2) * static_cast<double>(3 * k - 1));
        lb += f - 3.0 * lR;
        if (f - 3.0 * lR >= -0.5) return false;
        const double bound = lb + lzm - lR;
        if (bound < -static_cast<double>(wb) - 4.0) {
            tail = bound;
            K    = k;
            break;
        }
        a = mul(mul(a, iz3, wb), BF((3 * k - 2) * (3 * k - 1)), wb);
        S = add(S, a, wb);
        T.add(l2(a));
        if (k > 10000000) return false;
    }
    const CX P = neg(mul(recip(z, wb + 8), multiprecision::constants::inv_pi(wctx(wb + 8)), wb));
    const CX v = mul(S, P, wb);
    const double es = lsum(T.l2sum + std::log2(3.0 * static_cast<double>(K) + 8.0) - static_cast<double>(wb), tail);
    out = approx(v, lsum(l2(P) + es, l2(v) - static_cast<double>(wb) + 4.0));
    return true;
}

bool hi_rotated(const CX& z, int j, std::size_t w, Approx& out) {
    const std::size_t wr = w + bits_of(1.5 * l2(z)) + 16;
    const CX zr = mul(z, unit(j, wr + 8), wr);
    Approx a;
    if (!hi_asym(zr, w, a)) return false;
    out = with_prop(a, zr, wr);
    return true;
}

Approx hi_eval(const CX& z, std::size_t w) {
    if (!large(z, w)) return series(z, kind::hi, w);
    Approx a;
    if (hi_asym(z, w, a)) return a;
    const bool up = !z.im.signbit();
    Approx h;
    if (hi_rotated(z, up ? 4 : -4, w + 2, h)) {
        const Approx ar = ai_rotated(z, up ? -4 : 4, w + 2);
        return lin2(unit(up ? 4 : -4, w + 16), h, scale2(unit(up ? -1 : 1, w + 16), 1), ar, w);
    }
    return series(z, kind::hi, w);
}

Approx gi_eval(const CX& z, std::size_t w) {
    if (!large(z, w)) return series(z, kind::gi, w);
    Approx h1, h2;
    if (hi_rotated(z, 4, w + 2, h1) && hi_rotated(z, -4, w + 2, h2)) {
        const CX c1 = scale2(div_i(unit(1, w + 16)), -1);
        const CX c2 = neg(scale2(div_i(unit(-1, w + 16)), -1));
        return lin2(c1, h1, c2, h2, w);
    }
    const Approx b = bi_eval(z, w + 4);
    const Approx h = hi_eval(z, w + 4);
    return lin2(CX(BF::one()), b, CX(BF::one(true)), h, w);
}

Approx eval(kind k, const CX& z, std::size_t w) {
    switch (k) {
        case kind::ai: return ai_eval(z, w);
        case kind::bi: return bi_eval(z, w);
        case kind::hi: return hi_eval(z, w);
        default: return gi_eval(z, w);
    }
}

BC airy_public(kind k, const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (z.imaginary().is_zero()) {
        BF r;
        switch (k) {
            case kind::ai: r = math::airy_ai(z.real(), ctx.real()); break;
            case kind::bi: r = math::airy_bi(z.real(), ctx.real()); break;
            case kind::hi: r = math::scorer_hi(z.real(), ctx.real()); break;
            default: r = math::scorer_gi(z.real(), ctx.real()); break;
        }
        return BC(r, BF::zero());
    }
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) { return eval(k, Z, w); });
}

} // namespace

BC airy_ai(const BC& z, const BCC& ctx) { return airy_public(kind::ai, z, ctx); }
BC airy_bi(const BC& z, const BCC& ctx) { return airy_public(kind::bi, z, ctx); }
BC scorer_gi(const BC& z, const BCC& ctx) { return airy_public(kind::gi, z, ctx); }
BC scorer_hi(const BC& z, const BCC& ctx) { return airy_public(kind::hi, z, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
