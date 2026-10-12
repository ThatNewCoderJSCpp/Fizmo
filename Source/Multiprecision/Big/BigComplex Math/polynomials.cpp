#include "fizmo_library.hpp"
#include "special_functions.hpp"

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

std::uint64_t uabs(std::int64_t n) { return n < 0 ? static_cast<std::uint64_t>(-(n + 1)) + 1 : static_cast<std::uint64_t>(n); }

int msb(std::uint64_t n) {
    int i = -1;
    while (n) {
        ++i;
        n >>= 1;
    }
    return i;
}

CV cint(std::int64_t v) { return cv_exact(CX(BF(v))); }

CV twice(const CV& a) { return CV{scale2(a.v, 1), a.er == ninf ? ninf : a.er + 1, a.ei == ninf ? ninf : a.ei + 1}; }
CV half(const CV& a) { return CV{scale2(a.v, -1), a.er == ninf ? ninf : a.er - 1, a.ei == ninf ? ninf : a.ei - 1}; }

void lucas_u(std::uint64_t n, const CV& P, bool q_neg, std::size_t w, CV& A, CV& B) {
    A = cint(0);
    B = cint(1);
    for (int i = msb(n); i >= 0; --i) {
        const CV V   = cv_sub(twice(B), cv_mul(P, A, w), w);
        const CV U2  = cv_mul(A, V, w);
        const CV BB  = cv_mul(B, B, w);
        const CV AA  = cv_mul(A, A, w);
        const CV U21 = q_neg ? cv_add(BB, AA, w) : cv_sub(BB, AA, w);
        if ((n >> i) & 1u) {
            const CV PU = cv_mul(P, U21, w);
            B = q_neg ? cv_add(PU, U2, w) : cv_sub(PU, U2, w);
            A = U21;
        } else {
            A = U2;
            B = U21;
        }
    }
}

CV lucas_v(std::uint64_t n, const CV& P, bool q_neg, std::size_t w) {
    CV            A = cint(2);
    CV            B = P;
    std::uint64_t m = 0;
    for (int i = msb(n); i >= 0; --i) {
        const bool qm  = q_neg && (m & 1u);
        const CV   V2  = cv_add(cv_mul(A, A, w), cint(qm ? 2 : -2), w);
        const CV   AB  = cv_mul(A, B, w);
        const CV   V21 = qm ? cv_add(AB, P, w) : cv_sub(AB, P, w);
        if ((n >> i) & 1u) {
            const bool qm1 = q_neg && ((m + 1) & 1u);
            B = cv_add(cv_mul(B, B, w), cint(qm1 ? 2 : -2), w);
            A = V21;
            m = 2 * m + 1;
        } else {
            A = V2;
            B = V21;
            m = 2 * m;
        }
    }
    return A;
}

CV flip(const CV& a, bool f) { return f ? CV{neg(a.v), a.er, a.ei} : a; }

template <class Body>
BC poly(const BC& z, std::uint64_t deg, std::int64_t c0, bool f, const BCC& ctx, Body body) {
    if (z.is_undefined()) return BC::undefined();
    if (deg == 0) return BC(BF(f ? -c0 : c0).rounded(ctx.real()), BF::zero());
    if (!z.is_finite()) return BC::undefined();
    return drive(ctx, [&](std::size_t wp) { return approx(flip(body(wp), f)); });
}

} // namespace

BC chebyshev_t(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::chebyshev_t(z.real(), n, ctx.real()), BF::zero());
    const std::uint64_t k = uabs(n);
    const CV P = cv_exact(scale2(cx(z), 1));
    return poly(z, k, 1, false, ctx, [&](std::size_t w) { return half(lucas_v(k, P, false, w)); });
}

BC chebyshev_u(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::chebyshev_u(z.real(), n, ctx.real()), BF::zero());
    if (n == -1) return z.is_undefined() ? BC::undefined() : BC::zero();
    const bool          f = n < -1;
    const std::uint64_t k = f ? uabs(n) - 2 : static_cast<std::uint64_t>(n);
    const CV P = cv_exact(scale2(cx(z), 1));
    return poly(z, k, 1, f, ctx, [&](std::size_t w) {
        CV A, B;
        lucas_u(k, P, false, w, A, B);
        return B;
    });
}

BC chebyshev_v(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::chebyshev_v(z.real(), n, ctx.real()), BF::zero());
    const std::uint64_t k = n < 0 ? uabs(n) - 1 : static_cast<std::uint64_t>(n);
    const CV P = cv_exact(scale2(cx(z), 1));
    return poly(z, k, 1, false, ctx, [&](std::size_t w) {
        CV A, B;
        lucas_u(k, P, false, w, A, B);
        return cv_sub(B, A, w);
    });
}

BC chebyshev_w(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::chebyshev_w(z.real(), n, ctx.real()), BF::zero());
    const bool          f = n < 0;
    const std::uint64_t k = f ? uabs(n) - 1 : static_cast<std::uint64_t>(n);
    const CV P = cv_exact(scale2(cx(z), 1));
    return poly(z, k, 1, f, ctx, [&](std::size_t w) {
        CV A, B;
        lucas_u(k, P, false, w, A, B);
        return cv_add(B, A, w);
    });
}

BC fibonacci_polynomial(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::fibonacci_polynomial(z.real(), n, ctx.real()), BF::zero());
    const std::uint64_t k = uabs(n);
    const bool          f = n < 0 && (k % 2 == 0);
    const CV P = cv_exact(cx(z));
    return poly(z, k == 0 ? 0 : k - 1, k == 0 ? 0 : 1, f, ctx, [&](std::size_t w) {
        CV A, B;
        lucas_u(k, P, true, w, A, B);
        return A;
    });
}

BC lucas_polynomial(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::lucas_polynomial(z.real(), n, ctx.real()), BF::zero());
    const std::uint64_t k = uabs(n);
    const bool          f = n < 0 && (k % 2 == 1);
    const CV P = cv_exact(cx(z));
    return poly(z, k, 2, f, ctx, [&](std::size_t w) { return lucas_v(k, P, true, w); });
}

BC legendre_p(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::legendre_p(z.real(), n, ctx.real()), BF::zero());
    const std::uint64_t k = n < 0 ? uabs(n) - 1 : static_cast<std::uint64_t>(n);
    const CV Z = cv_exact(cx(z));
    return poly(z, k, 1, false, ctx, [&](std::size_t w) {
        CV p0 = cint(1);
        CV p1 = Z;
        for (std::uint64_t j = 1; j < k; ++j) {
            const CV a = cv_scale(cv_mul(Z, p1, w), BF(2 * j + 1), w);
            const CV b = cv_scale(p0, BF(j), w);
            const CV p2 = cv_div_int(cv_sub(a, b, w), j + 1, w);
            p0 = p1;
            p1 = p2;
        }
        return p1;
    });
}

BC legendre_q(const BC& z, std::int64_t n, const BCC& ctx) {
    if (z.imaginary().is_zero() && z.real().is_finite()) return BC(math::legendre_q(z.real(), n, ctx.real()), BF::zero());
    if (z.is_undefined() || n < 0) return BC::undefined();
    const std::uint64_t k = static_cast<std::uint64_t>(n);
    if (!z.is_finite()) return BC::zero();
    const CX Zc = cx(z);
    if (z.imaginary().is_zero()) {
        const BF ax = z.real().abs();
        if (BF::compare(ax, BF::one()) == BF::ordering::equal) return BC::undefined();
    }
    const CV Z = cv_exact(Zc);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wq = w + 16;
        const CX L  = cx(math::arccoth(z, BCC(BFC::clamp_precision(wq))));
        CV q0 = cv_from(L, l2(L) - static_cast<double>(wq) + 1);
        if (k == 0) return approx(q0);
        CV q1 = cv_sub(cv_mul(Z, q0, w), cint(1), w);
        for (std::uint64_t j = 1; j < k; ++j) {
            const CV a = cv_scale(cv_mul(Z, q1, w), BF(2 * j + 1), w);
            const CV b = cv_scale(q0, BF(j), w);
            const CV q2 = cv_div_int(cv_sub(a, b, w), j + 1, w);
            q0 = q1;
            q1 = q2;
        }
        return approx(q1);
    });
}

} // namespace cxsf
} // namespace math
} // namespace fizmo
