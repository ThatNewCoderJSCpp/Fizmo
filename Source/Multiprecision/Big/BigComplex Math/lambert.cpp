#include "fizmo_library.hpp"
#include "special_functions.hpp"

#include <algorithm>
#include <cmath>
#include <complex>

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

using cd = std::complex<double>;

constexpr double pi_d = 3.141592653589793;

double to_d(const BF& x) {
    const double l = l2(x);
    if (l == ninf) return 0.0;
    return std::exp2(std::max(std::min(l, 1000.0), -1000.0)) * (x.signbit() ? -1.0 : 1.0);
}

cd to_cd(const CX& z) { return cd(to_d(z.re), to_d(z.im)); }
CX from_cd(const cd& z) { return CX(BF(z.real()), BF(z.imag())); }

std::size_t bits_of(double b) { return b <= 0 ? 0 : static_cast<std::size_t>(std::min(b, 1.0e7)); }

cd hybrid(const cd& zin, bool has_im, int imag_sign, std::int64_t k) {
    cd z = zin;
    const double x = z.real(), y = z.imag();
    cd L1, L2;
    if (k == 0) {
        if (-4.0 < y && y < 4.0 && -1.0 < x && x < 2.5) {
            if (has_im) {
                if (y > 1.00) return cd(0.876, 0.645) + cd(0.118, -0.174) * (z - cd(0.75, 2.5));
                if (y > 0.25) return cd(0.505, 0.204) + cd(0.375, -0.132) * (z - cd(0.75, 0.5));
                if (y < -1.00) return cd(0.876, -0.645) + cd(0.118, 0.174) * (z - cd(0.75, -2.5));
                if (y < -0.25) return cd(0.505, -0.204) + cd(0.375, 0.132) * (z - cd(0.75, -0.5));
            }
            if (x < -0.5) {
                if (imag_sign >= 0) return cd(-0.318, 1.34) + cd(-0.697, -0.593) * (z + 1.0);
                return cd(-0.318, -1.34) + cd(-0.697, 0.593) * (z + 1.0);
            }
            const double r = -0.367879441171442;
            if (x < -0.2) return -1.0 + 2.33164398159712 * std::sqrt(z - r) - 1.81218788563936 * (z - r);
            if (x < 0.5) return z;
            return 0.2 + 0.3 * z;
        }
        L1 = std::log(z);
        L2 = std::log(L1);
    } else if (k == -1) {
        const double r = -0.367879441171442;
        if (imag_sign >= 0 && y < 0.1 && -0.6 < x && x < -0.2) return -1.0 - 2.33164398159712 * std::sqrt(z - r) - 1.81218788563936 * (z - r);
        if (!has_im && -0.2 <= x && x < 0.0) {
            const double l1 = std::log(-x);
            return cd(l1 - std::log(-l1), 0.0);
        }
        if (imag_sign == -1 && !has_im && x < 0.0) {
            L1 = std::log(z) - cd(0.0, pi_d);
        } else {
            L1 = std::log(z) - cd(0.0, 2.0 * pi_d);
        }
        L2 = std::log(L1);
    }
    return L1 - L2 + L2 / L1 + L2 * (L2 - 2.0) / (2.0 * L1 * L1);
}

CX asym_seed(const CX& z, std::int64_t k) {
    const std::size_t p = 96;
    CX L1 = ln(z, p);
    if (k != 0) {
        const BF tk = BF::mul(BF(static_cast<std::int64_t>(2) * k), pi(p + 16), wctx(p));
        L1.im = BF::add(L1.im, tk, wctx(p));
    }
    const CX L2  = ln(L1, p);
    const CX q   = div(L2, L1, p);
    const CX t   = div(mul(L2, add_real(L2, BF(static_cast<std::int64_t>(-2)), p), p), scale2(sqr(L1, p), 1), p);
    return add(add(sub(L1, L2, p), q, p), t, p);
}

bool branch_seed(const CX& z, std::int64_t k, CX& w) {
    if (!(k == 0 || (k == -1 && !z.im.signbit()) || (k == 1 && z.im.signbit()))) return false;
    if (l2(z) > 0.0) return false;
    const BF re = BF::add(z.re, multiprecision::constants::e(wctx(64)).reciprocal(wctx(64)), wctx(64));
    const CX d0(re, z.im);
    if (l2(d0) > std::log2(0.05)) return false;
    const double canc = std::max(0.0, -l2(d0));
    const std::size_t p = 128 + bits_of(canc);
    const BFC pc = wctx(p);
    const BF  e  = multiprecision::constants::e(pc);
    const CX  d(BF::add(z.re, e.reciprocal(pc), pc), z.im);
    CX P = sqrt(scale2(mul(d, e, p), 1), p);
    if (k != 0) P = neg(P);
    const CX P2 = sqr(P, p);
    const CX P3 = mul(P2, P, p);
    w = add_real(P, BF::one(true), p);
    w = sub(w, div(P2, BF(static_cast<std::int64_t>(3)), p), p);
    w = add(w, mul(P3, BF::div(BF(static_cast<std::int64_t>(11)), BF(static_cast<std::int64_t>(72)), pc), p), p);
    return true;
}

CX seed(const CX& z, std::int64_t k) {
    CX w;
    if (branch_seed(z, k, w)) return w;
    const double mz = l2(z);
    if (mz > -10.0 && mz < 900.0 && k > -1000 && k < 1000 && (k == 0 || k == -1)) {
        const bool has_im = !z.im.is_zero();
        const int  sgn    = has_im ? (z.im.signbit() ? -1 : 1) : 0;
        const cd   s      = hybrid(to_cd(z), has_im, sgn, k);
        if (std::isfinite(s.real()) && std::isfinite(s.imag())) return from_cd(s);
    }
    if (k == 0 && mz < -1.0) return mul(z, CX(BF::sub(BF::one(), z.re, wctx(96)), z.im.with_sign(!z.im.signbit())), 96);
    if (k == -1 && z.im.is_zero() && z.re.signbit()) {
        const CX L1 = ln(neg(z), 96);
        return sub(L1, ln(neg(L1), 96), 96);
    }
    return asym_seed(z, k);
}

Approx halley(const CX& z, const CX& w0, std::size_t w) {
    CX x = w0;
    const double l1w = l2(add_real(x, BF::one(), 64));
    const std::size_t extra = bits_of(std::max(l2(x), 0.0) - l1w);
    const std::size_t wb = w + 16 + extra;
    const BF two(static_cast<std::int64_t>(2));
    for (int i = 0; i < 300; ++i) {
        const std::size_t we = wb + bits_of(l2(x)) + 8;
        const CX ew   = exp(x, we);
        const CX wew  = mul(x, ew, wb + 8);
        const CX f    = sub(wew, z, wb + 8);
        const CX xp2  = add_real(x, two, wb);
        const CX tx2  = add_real(scale2(x, 1), two, wb);
        const CX den  = sub(add(wew, ew, wb + 8), div(mul(xp2, f, wb + 8), tx2, wb + 8), wb + 8);
        const CX step = div(f, den, wb);
        const CX xn   = sub(x, step, wb);
        const double ls = l2(step);
        x = xn;
        if (ls == ninf || ls <= l2(xn) - static_cast<double>(wb) + 4.0) break;
    }
    const std::size_t we = wb + bits_of(l2(x)) + 8;
    const CX ew  = exp(x, we);
    const CX r   = sub(mul(x, ew, wb + 8), z, wb + 8);
    const CX fp  = mul(ew, add_real(x, BF::one(), wb + 8), wb + 8);
    const double lfp = l2(fp);
    if (lfp == ninf) return approx_undefined();
    const double er = lsum(l2(r), l2(z) - static_cast<double>(wb) + 4.0);
    return approx(x, lsum(er - lfp + 2.0, l2(x) - static_cast<double>(wb) + 3.0));
}

} // namespace

BC lambert_w(const BC& z, std::int64_t k, const BCC& ctx) {
    if (z.is_indeterminate()) return BC::indeterminate();
    if (z.is_undefined() || !z.is_finite()) return BC::undefined();
    if (z.imaginary().is_zero() && z.imaginary().signbit()) {
        if (k == 0 && !z.real().signbit()) return BC(math::lambert_w(z.real(), 0, ctx.real()), BF::zero());
        return lambert_w(z.conjugate(), -k, ctx).conjugate();
    }
    if (z.imaginary().is_zero() && (k == 0 || k == -1)) {
        const BF r = math::lambert_w(z.real(), k, wctx(32));
        if (!r.is_unordered()) return BC(math::lambert_w(z.real(), k, ctx.real()), BF::zero());
    }
    if (z.is_zero()) return k == 0 ? BC::zero() : BC::undefined();
    const CX Z  = cx(z);
    const CX w0 = seed(Z, k);
    return drive(ctx, [&](std::size_t w) { return halley(Z, w0, w); });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
