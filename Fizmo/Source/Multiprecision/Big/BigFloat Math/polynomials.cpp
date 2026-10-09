#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {
namespace math {
namespace pldetail {

double pl_lsum(double a, double b) {
    if (a == pl_ninf()) return b;
    if (b == pl_ninf()) return a;
    const double hi = (a > b) ? a : b;
    const double lo = (a > b) ? b : a;
    return hi + std::log2(1.0 + std::exp2(lo - hi)) + 1e-9;
}

bool pl_mul_exact(const BigFloat& a, const BigFloat& b, std::size_t w) {
    if (a.is_zero() || b.is_zero()) return true;
    return a.significand().bit_length() + b.significand().bit_length() <= w;
}

bool pl_add_exact(const BigFloat& a, const BigFloat& b, std::size_t w) {
    if (a.is_zero()) return b.is_zero() || b.significand().bit_length() <= w;
    if (b.is_zero()) return a.significand().bit_length() <= w;
    const std::int64_t ta = pl_top(a);
    const std::int64_t tb = pl_top(b);
    const std::int64_t hi = (ta > tb) ? ta : tb;
    const std::int64_t lo = (a.exponent() < b.exponent()) ? a.exponent() : b.exponent();
    return hi - lo + 2 <= static_cast<std::int64_t>(w);
}

pl_val pl_mul(const pl_val& a, const pl_val& b, const BigFloatContext& wc) {
    pl_val r{BigFloat::mul(a.v, b.v, wc), pl_ninf()};
    r.le = pl_lsum(pl_lsum(pl_mag(a.v) + b.le, pl_mag(b.v) + a.le), a.le + b.le);
    if (!pl_mul_exact(a.v, b.v, wc.precision)) r.le = pl_lsum(r.le, pl_mag(r.v) - static_cast<double>(wc.precision));
    return r;
}

pl_val pl_addsub(const pl_val& a, const pl_val& b, bool sub, const BigFloatContext& wc) {
    pl_val r{sub ? BigFloat::sub(a.v, b.v, wc) : BigFloat::add(a.v, b.v, wc), pl_lsum(a.le, b.le)};
    if (!pl_add_exact(a.v, b.v, wc.precision)) r.le = pl_lsum(r.le, pl_mag(r.v) - static_cast<double>(wc.precision));
    return r;
}

void pl_lucas_u(std::uint64_t n, const pl_val& P, bool q_neg, const BigFloatContext& wc, pl_val& A, pl_val& B) {
    A = pl_exact(BigFloat::zero());
    B = pl_exact(BigFloat::one());

    for (int i = pl_msb(n); i >= 0; --i) {
        const pl_val V   = pl_sub(pl_scale2(B, 1), pl_mul(P, A, wc), wc);         
        const pl_val U2  = pl_mul(A, V, wc);                                         
        const pl_val BB  = pl_mul(B, B, wc);
        const pl_val AA  = pl_mul(A, A, wc);
        const pl_val U21 = q_neg ? pl_add(BB, AA, wc) : pl_sub(BB, AA, wc);         

        if ((n >> i) & 1u) {
            const pl_val PU = pl_mul(P, U21, wc);
            B = q_neg ? pl_add(PU, U2, wc) : pl_sub(PU, U2, wc);                    
            A = U21;
        } else {
            A = U2;
            B = U21;
        }
    }
}

pl_val pl_lucas_v(std::uint64_t n, const pl_val& P, bool q_neg, const BigFloatContext& wc) {
    pl_val        A = pl_exact(BigFloat(static_cast<std::int64_t>(2)));
    pl_val        B = P;
    std::uint64_t m = 0;

    for (int i = pl_msb(n); i >= 0; --i) {
        const bool   qm  = q_neg && (m & 1u);                                      
        const pl_val V2  = pl_add_int(pl_mul(A, A, wc), qm ? 2 : -2, wc);          
        const pl_val AB  = pl_mul(A, B, wc);
        const pl_val V21 = qm ? pl_add(AB, P, wc) : pl_sub(AB, P, wc);             

        if ((n >> i) & 1u) {
            const bool qm1 = q_neg && ((m + 1) & 1u);
            B = pl_add_int(pl_mul(B, B, wc), qm1 ? 2 : -2, wc);                    
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

bool pl_safe(const BigFloat& v, std::size_t want, double le, std::size_t prec) {
    BigUInt           sig = v.significand();
    std::int64_t      lsb = v.exponent();
    const std::size_t L   = sig.bit_length();

    if (L < want) {
        sig.shift_left_mutable(want - L);
        lsb -= static_cast<std::int64_t>(want - L);
    }

    const double e = std::ceil(le) - static_cast<double>(lsb);
    if (e + 4.0 >= static_cast<double>(sig.bit_length())) return false;
    const std::size_t err = (e <= 0.0) ? 0 : static_cast<std::size_t>(e);
    return constants::bfdetail::round_is_safe(sig, prec, err + 2);
}

bool pl_special(const BigFloat& x, std::uint64_t deg, std::uint64_t c0, bool flip, const BigFloatContext& ctx, BigFloat& out) {
    if (x.is_nan())       { out = BigFloat::nan();       return true; }
    if (x.is_undefined()) { out = BigFloat::undefined(); return true; }
    if (deg == 0)         { out = BigFloat(c0).with_sign(flip).rounded(ctx); return true; }
    if (x.is_infinite())  { out = BigFloat::infinity((x.signbit() && (deg & 1u)) != flip); return true; }
    return false;
}

std::vector<BigInt> pc_step(const std::vector<BigInt>& cur, const std::vector<BigInt>& prev, bool two_x, bool q_neg) {
    std::vector<BigInt> r(cur.size() + 1, pc_int(0));

    for (std::size_t i = 0; i < cur.size(); ++i) {
        r[i + 1] = cur[i];
        if (two_x) r[i + 1].add_mutable(cur[i]);
    }

    for (std::size_t i = 0; i < prev.size(); ++i) {
        if (q_neg) r[i].add_mutable(prev[i]); else r[i].sub_mutable(prev[i]);
    }

    return r;
}

std::vector<BigInt> pc_run(std::size_t n, std::vector<BigInt> y0, std::vector<BigInt> y1, bool two_x, bool q_neg) {
    if (n == 0) return y0;

    for (std::size_t k = 1; k < n; ++k) {
        std::vector<BigInt> y2 = pc_step(y1, y0, two_x, q_neg);
        y0.swap(y1);
        y1.swap(y2);
    }

    return y1;
}

} // namespace pldetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat chebyshev_t(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    const std::uint64_t k = pldetail::pl_abs(n);                                
    BigFloat out;
    if (pldetail::pl_special(x, k, 1, false, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x.scaled_pow2(1));
    return pldetail::pl_drive(k, false, ctx, [&](const BigFloatContext& wc) {
        return pldetail::pl_scale2(pldetail::pl_lucas_v(k, P, false, wc), -1);  
    });
}

BigFloat chebyshev_u(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    if (n == -1) {                                                                 
        if (x.is_nan())       return BigFloat::nan();
        if (x.is_undefined()) return BigFloat::undefined();
        return BigFloat::zero();
    }

    const bool          flip = n < -1;                                             
    const std::uint64_t k    = flip ? pldetail::pl_abs(n) - 2 : static_cast<std::uint64_t>(n);
    BigFloat out;
    if (pldetail::pl_special(x, k, 1, flip, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x.scaled_pow2(1));
    return pldetail::pl_drive(k, flip, ctx, [&](const BigFloatContext& wc) {
        pldetail::pl_val A, B;
        pldetail::pl_lucas_u(k, P, false, wc, A, B);
        return B;                                                                  
    });
}

BigFloat chebyshev_v(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    const std::uint64_t k = (n < 0) ? pldetail::pl_abs(n) - 1 : static_cast<std::uint64_t>(n);   
    BigFloat out;
    if (pldetail::pl_special(x, k, 1, false, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x.scaled_pow2(1));
    return pldetail::pl_drive(k, false, ctx, [&](const BigFloatContext& wc) {
        pldetail::pl_val A, B;
        pldetail::pl_lucas_u(k, P, false, wc, A, B);
        return pldetail::pl_sub(B, A, wc);                                         
    });
}

BigFloat chebyshev_w(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    const bool          flip = n < 0;                                              
    const std::uint64_t k    = flip ? pldetail::pl_abs(n) - 1 : static_cast<std::uint64_t>(n);
    BigFloat out;
    if (pldetail::pl_special(x, k, 1, flip, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x.scaled_pow2(1));
    return pldetail::pl_drive(k, flip, ctx, [&](const BigFloatContext& wc) {
        pldetail::pl_val A, B;
        pldetail::pl_lucas_u(k, P, false, wc, A, B);
        return pldetail::pl_add(B, A, wc);                                         
    });
}

BigFloat fibonacci_polynomial(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    const std::uint64_t k    = pldetail::pl_abs(n);
    const bool          flip = n < 0 && (k % 2 == 0);                              
    BigFloat out;
    if (pldetail::pl_special(x, (k == 0) ? 0 : k - 1, (k == 0) ? 0 : 1, flip, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x);
    return pldetail::pl_drive(k, flip, ctx, [&](const BigFloatContext& wc) {
        pldetail::pl_val A, B;
        pldetail::pl_lucas_u(k, P, true, wc, A, B);
        return A;                                                                 
    });
}

BigFloat lucas_polynomial(const BigFloat& x, std::int64_t n, const BigFloatContext& ctx) {
    const std::uint64_t k    = pldetail::pl_abs(n);
    const bool          flip = n < 0 && (k % 2 == 1);                             
    BigFloat out;
    if (pldetail::pl_special(x, k, 2, flip, ctx, out)) return out;
    const pldetail::pl_val P = pldetail::pl_exact(x);
    return pldetail::pl_drive(k, flip, ctx, [&](const BigFloatContext& wc) {
        return pldetail::pl_lucas_v(k, P, true, wc);                              
    });
}

std::vector<BigInt> chebyshev_t_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(1)}, {pc_int(0), pc_int(1)}, true, false);
}

std::vector<BigInt> chebyshev_u_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(1)}, {pc_int(0), pc_int(2)}, true, false);
}

std::vector<BigInt> chebyshev_v_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(1)}, {pc_int(-1), pc_int(2)}, true, false);
}

std::vector<BigInt> chebyshev_w_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(1)}, {pc_int(1), pc_int(2)}, true, false);
}

std::vector<BigInt> lucas_polynomial_coefficients(std::size_t n) {
    using pldetail::pc_int;
    return pldetail::pc_run(n, {pc_int(2)}, {pc_int(0), pc_int(1)}, false, true);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
