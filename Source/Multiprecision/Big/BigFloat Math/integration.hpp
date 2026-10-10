#ifndef FIZMO_BIGFLOAT_INTEGRATION_HPP
#define FIZMO_BIGFLOAT_INTEGRATION_HPP

#include "../../../Misc Math/integration_interval.hpp"         
#include "../big_function.hpp"      
#include "logarithms.hpp"
#include "trig.hpp"
#include "htrig.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <utility>
#include <vector>

namespace fizmo {
namespace math {
namespace integration {

namespace bfidetail {

using BF       = ::fizmo::multiprecision::BigFloat;
using BFC      = ::fizmo::multiprecision::BigFloatContext;
using BFF      = ::fizmo::multiprecision::BigFloatFunction;
using bf_nodes = std::vector<std::pair<BF, BF>>;                  
namespace mpm  = ::fizmo::multiprecision::math;
namespace mpc  = ::fizmo::multiprecision::constants;

static const double bf_inf = std::numeric_limits<double>::infinity();

inline BFC bf_ctx(std::size_t p) { return BFC(BFC::clamp_precision(p), ::fizmo::multiprecision::RoundingMode::nearest_even); }

inline std::size_t bf_bitlen(std::uint64_t v) noexcept {
    std::size_t n = 0;
    while (v != 0) { ++n; v >>= 1; }
    return n;
}

inline double bf_l2(const BF& x) {                                
    if (x.is_zero())    return -bf_inf;
    return static_cast<double>(x.get_exp_base2());
}

inline BF bf_pow2(double l) {                                      
    if (l == -bf_inf) return BF::zero();
    if (!(l < 1.0e18)) return BF::infinity();
    return BF::one().scaled_pow2(static_cast<std::int64_t>(std::ceil(l)));
}

inline BF bf_mid(const BF& a, const BF& b, const BFC& c) { return BF::add(a, b, c).scaled_pow2(-1); }
inline BF bf_one_minus(const BF& t, const BFC& c)        { return BF::sub(BF::one(), t, bf_ctx(c.precision + 4)); } 
inline BF bf_one_plus(const BF& t, const BFC& c)         { return BF::add(BF::one(), t, bf_ctx(c.precision + 4)); }

inline bool bf_ok(const IntegrationConfig<BF>& cfg, const BF& err, const BF& val) {
    return val.is_finite() && err.is_finite() && cfg.accepts(err, val);
}

inline double bf_de_tmax(std::size_t wp) {
    return std::log(2.0 * (static_cast<double>(wp) + 20.0) * 0.6931471805599453 / 3.141592653589793) + 0.25;
}

inline std::size_t bf_default_gl_n(std::size_t prec) {
    return std::max<std::size_t>(8, std::min<std::size_t>(128, prec / 12));
}

inline bool bf_half_map(InfiniteTransform tr, const BF& t, const BFC& c, BF& u, BF& du) {
    switch (tr) {
        case InfiniteTransform::Exponential: {                     
            const BF om = bf_one_minus(t, c);
            if (om.is_zero() || om.signbit()) return false;
            u  = -mpm::ln(om, c);
            du = om.reciprocal(c);
            return true;
        }

        case InfiniteTransform::Tangent: {                        
            const BF om = bf_one_minus(t, c);
            if (om.is_zero() || om.signbit()) return false;
            const BF hp = mpc::half_pi(c);
            const BF q  = BF::mul(hp, om, c);
            const BF s  = mpm::sin(q, c);
            u  = BF::div(mpm::cos(q, c), s, c);
            du = BF::div(hp, BF::mul(s, s, c), c);
            return true;
        }

        case InfiniteTransform::TanhSinh: {                        
            const BF hp = mpc::half_pi(c);
            u  = mpm::exp(BF::mul(hp, mpm::sinh(t, c), c), c);
            du = BF::mul(BF::mul(hp, mpm::cosh(t, c), c), u, c);
            return true;
        }

        default: {                                                 
            const BF om = bf_one_minus(t, c);
            if (om.is_zero() || om.signbit()) return false;
            const BF r = om.reciprocal(c);
            u  = BF::mul(t, r, c);
            du = BF::mul(r, r, c);
            return true;
        }
    }
}

inline void bf_half_bounds(InfiniteTransform tr, std::size_t wp, BF& lo, BF& hi) {
    if (tr == InfiniteTransform::TanhSinh) {
        hi = BF(bf_de_tmax(wp));
        lo = -hi;
        return;
    }

    lo = BF::zero();
    hi = BF::one();
}

inline bool bf_full_map(InfiniteTransform tr, const BF& t, const BFC& c, BF& x, BF& dx) {
    switch (tr) {
        case InfiniteTransform::Tangent: {                         
            const BF om = bf_one_minus(t, c);
            if (t.is_zero() || om.is_zero() || t.signbit() || om.signbit()) return false;
            const BF   pi  = mpc::pi(c);
            const bool low = BF::compare(t, om) != BF::ordering::greater;              
            const BF   q   = BF::mul(pi, low ? t : om, c);
            const BF   s   = mpm::sin(q, c);                                           
            const BF   cs  = low ? mpm::cos(q, c) : -mpm::cos(q, c);                   
            x  = -BF::div(cs, s, c);
            dx = BF::div(pi, BF::mul(s, s, c), c);
            return true;
        }

        case InfiniteTransform::Rational: {                       
            const BF d = BF::mul(bf_one_minus(t, c), bf_one_plus(t, c), c);
            if (d.is_zero() || d.signbit()) return false;
            const BF r = d.reciprocal(c);
            x  = BF::mul(t, r, c);
            dx = BF::mul(bf_one_plus(BF::mul(t, t, c), c), BF::mul(r, r, c), c);
            return true;
        }

        case InfiniteTransform::TanhSinh: {                        
            const BF hp = mpc::half_pi(c);
            const BF s  = BF::mul(hp, mpm::sinh(t, c), c);
            x  = mpm::sinh(s, c);
            dx = BF::mul(BF::mul(hp, mpm::cosh(t, c), c), mpm::cosh(s, c), c);
            return true;
        }

        default: {                                               
            const BF om = bf_one_minus(t, c);
            if (t.is_zero() || om.is_zero() || t.signbit() || om.signbit()) return false;
            x  = BF::sub(mpm::ln(t, c), mpm::ln(om, c), c);
            dx = BF::mul(t, om, c).reciprocal(c);
            return true;
        }
    }
}

inline void bf_full_bounds(InfiniteTransform tr, std::size_t wp, BF& lo, BF& hi) {
    if (tr == InfiniteTransform::TanhSinh) {
        hi = BF(bf_de_tmax(wp));
        lo = -hi;
        return;
    }

    hi = BF::one();
    lo = (tr == InfiniteTransform::Rational) ? -hi : BF::zero();
}

struct bf_mapped {
    BFF g;
    BF  a, b;
};

inline bf_mapped bf_map(const BFF& f, const Interval<BF>& iv, InfiniteTransform tr, std::size_t wp) {
    const BFF* fp = &f;
    bf_mapped  m;

    switch (bound_type_of(iv)) {
        case BoundType::Finite:
            m.g = BFF([fp](const BF& x, const BFC& c) { return (*fp)(x, c); });
            m.a = iv.lower().value();
            m.b = iv.upper().value();
            return m;

        case BoundType::RightInfinite: {
            const BF base = iv.lower().value();
            m.g = BFF([fp, base, tr](const BF& t, const BFC& c) {
                BF u, du;
                if (!bf_half_map(tr, t, c, u, du)) return BF::zero();
                return BF::mul((*fp)(BF::add(base, u, c), c), du, c);
            });
            bf_half_bounds(tr, wp, m.a, m.b);
            return m;
        }

        case BoundType::LeftInfinite: {
            const BF base = iv.upper().value();
            m.g = BFF([fp, base, tr](const BF& t, const BFC& c) {
                BF u, du;
                if (!bf_half_map(tr, t, c, u, du)) return BF::zero();
                return BF::mul((*fp)(BF::sub(base, u, c), c), du, c);
            });
            bf_half_bounds(tr, wp, m.a, m.b);
            return m;
        }

        default:
            m.g = BFF([fp, tr](const BF& t, const BFC& c) {
                BF x, dx;
                if (!bf_full_map(tr, t, c, x, dx)) return BF::zero();
                return BF::mul((*fp)(x, c), dx, c);
            });
            bf_full_bounds(tr, wp, m.a, m.b);
            return m;
    }
}

inline InfiniteTransform bf_resolve(BoundType bound, IntegrationTechnique technique) noexcept {
    if (bound == BoundType::Finite) return InfiniteTransform::Rational;
    if (technique == IntegrationTechnique::TanhSinh) return InfiniteTransform::TanhSinh;

    if (bound == BoundType::FullyInfinite) {
        switch (technique) {
            case IntegrationTechnique::GaussLegendre:
            case IntegrationTechnique::GaussKronrod:
            case IntegrationTechnique::ClenshawCurtis:
            case IntegrationTechnique::Romberg:
            case IntegrationTechnique::Trapezoidal:
                return InfiniteTransform::TanhSinh;
            default:
                return InfiniteTransform::LogRational;
        }
    }

    switch (technique) {
        case IntegrationTechnique::GaussLegendre:
        case IntegrationTechnique::GaussKronrod:
        case IntegrationTechnique::ClenshawCurtis:
            return InfiniteTransform::TanhSinh;
        case IntegrationTechnique::Romberg:
        case IntegrationTechnique::Trapezoidal:
            return InfiniteTransform::Exponential;
        default:
            return InfiniteTransform::Rational;
    }
}

inline const bf_nodes& bf_gl_nodes(std::size_t n, std::size_t prec) {
    static thread_local std::map<std::pair<std::size_t, std::size_t>, bf_nodes> cache;
    const std::pair<std::size_t, std::size_t> key(n, prec);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    bf_nodes&         out = cache[key];
    const std::size_t gp  = prec + 16 + 2 * bf_bitlen(n);
    const BFC         wc  = bf_ctx(gp);
    out.resize(n);

    auto legendre = [n](const BF& z, const BFC& c, BF& pn, BF& pn1) {                  
        BF p0 = BF::one();
        BF p1 = z;

        for (std::size_t j = 1; j < n; ++j) {
            BF p2 = BF::div(
                BF::sub(
                    BF::mul(
                        BF(static_cast<std::uint64_t>(2 * j + 1)), 
                        BF::mul(
                            z, 
                            p1, 
                            c), 
                        c
                    ),
                    BF::mul(
                        BF(static_cast<std::uint64_t>(j)), 
                        p0, 
                        c
                    ), 
                    c
                ),
                BF(static_cast<std::uint64_t>(j + 1)), 
                c
            );

            p0 = std::move(p1);
            p1 = std::move(p2);
        }

        pn  = std::move(p1);
        pn1 = std::move(p0);
    };

    const std::size_t m  = (n + 1) / 2;
    const BF          nb = BF(static_cast<std::uint64_t>(n));

    for (std::size_t i = 0; i < m; ++i) {
        double zd = std::cos(3.141592653589793 * (static_cast<double>(i) + 0.75) / (static_cast<double>(n) + 0.5));

        for (int it2 = 0; it2 < 4; ++it2) {                                              
            double p0 = 1.0;
            double p1 = zd;

            for (std::size_t j = 1; j < n; ++j) {
                const double p2 = ((2.0 * j + 1.0) * zd * p1 - static_cast<double>(j) * p0) / (static_cast<double>(j) + 1.0);
                p0 = p1;
                p1 = p2;
            }

            zd -= p1 / (static_cast<double>(n) * (zd * p1 - p0) / (zd * zd - 1.0));
        }

        BF          z(zd);
        std::size_t p       = 64;
        int         at_full = 0;

        while (at_full < 2) {                                                           
            p = std::min<std::size_t>(2 * p, gp);
            if (p == gp) ++at_full;
            const BFC c = bf_ctx(p);
            BF pn, pn1;
            legendre(z, c, pn, pn1);
            const BF dp = BF::div(BF::mul(nb, BF::sub(BF::mul(z, pn, c), pn1, c), c), BF::sub(BF::mul(z, z, c), BF::one(), c), c);
            z = BF::sub(z, BF::div(pn, dp, c), c);
        }

        BF pn, pn1;
        legendre(z, wc, pn, pn1);
        const BF omz2 = BF::sub(BF::one(), BF::mul(z, z, wc), wc);
        const BF dp   = BF::div(BF::mul(nb, BF::sub(BF::mul(z, pn, wc), pn1, wc), wc), -omz2, wc);
        const BF w    = BF::div(BF(static_cast<std::uint64_t>(2)), BF::mul(omz2, BF::mul(dp, dp, wc), wc), wc);
        out[i]         = std::make_pair(-z, w);
        out[n - 1 - i] = std::make_pair(z, w);
    }

    return out;
}

inline const bf_nodes& bf_cc_nodes(std::size_t N, std::size_t prec) {
    static thread_local std::map<std::pair<std::size_t, std::size_t>, bf_nodes> cache;
    const std::pair<std::size_t, std::size_t> key(N, prec);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    bf_nodes&  out = cache[key];
    const BFC  wc  = bf_ctx(prec + 16 + bf_bitlen(N));
    const BF   pi  = mpc::pi(wc);
    const BF   Nb  = BF(static_cast<std::uint64_t>(N));
    std::vector<BF> cosv(N + 1);

    for (std::size_t k = 0; k <= N; ++k) {
        cosv[k] = mpm::cos(BF::div(BF::mul(pi, BF(static_cast<std::uint64_t>(k)), wc), Nb, wc), wc);
    }

    auto cos_at = [&](std::uint64_t r) -> const BF& {                                  
        r %= 2 * N;
        if (r > N) r = 2 * N - r;
        return cosv[static_cast<std::size_t>(r)];
    };

    out.resize(N + 1);

    for (std::size_t k = 0; k <= N; ++k) {
        BF s = BF::zero();

        for (std::size_t j = 1; j <= N / 2; ++j) {
            BF term = BF::div(cos_at(static_cast<std::uint64_t>(2 * j) * k), BF(static_cast<std::uint64_t>(4 * j * j - 1)), wc);
            if (2 * j != N) term = term.scaled_pow2(1);
            s = BF::add(s, term, wc);
        }

        BF w = BF::div(BF::sub(BF::one(), s, wc), Nb, wc);
        if (k != 0 && k != N) w = w.scaled_pow2(1);
        out[k] = std::make_pair(cosv[k], w);
    }

    return out;
}

struct bf_de_node {
    BF E;
    BF hch;
};

inline const std::vector<bf_de_node>& bf_de_level(std::size_t prec, std::size_t level) {
    static thread_local std::map<std::pair<std::size_t, std::size_t>, std::vector<bf_de_node>> cache;
    const std::pair<std::size_t, std::size_t> key(prec, level);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    std::vector<bf_de_node>& out  = cache[key];
    const BFC                c    = bf_ctx(prec + 8);
    const BF                 hp   = mpc::half_pi(c);
    const double             tmax = bf_de_tmax(prec);
    const double             step = std::ldexp(1.0, -static_cast<int>(level));
    const std::uint64_t      inc  = (level == 0) ? 1 : 2;

    for (std::uint64_t i = (level == 0) ? 0 : 1; static_cast<double>(i) * step <= tmax; i += inc) {
        const BF   t = BF(i).scaled_pow2(-static_cast<std::int64_t>(level));
        bf_de_node nd;
        nd.E   = mpm::exp(BF::mul(hp, mpm::sinh(t, c), c), c);
        nd.hch = BF::mul(hp, mpm::cosh(t, c), c);
        out.push_back(std::move(nd));
    }

    return out;
}

inline std::size_t bf_default_gk_n(std::size_t prec) {
    return std::max<std::size_t>(7, std::min<std::size_t>(64, prec / 16));
}

inline double bf_to_double(const BF& x) {                                  
    if (x.is_zero())    return 0.0;
    if (!x.is_finite()) return x.signbit() ? -bf_inf : bf_inf;
    ::fizmo::multiprecision::BigUInt sig = x.significand();
    const std::size_t L = sig.bit_length();
    std::int64_t      e = x.exponent();

    if (L > 64) {
        sig.shift_right_mutable(L - 64);
        e += static_cast<std::int64_t>(L - 64);
    }

    if (e < -4000) e = -4000;
    if (e >  4000) e =  4000;
    const double v = std::ldexp(static_cast<double>(sig.get_lowest_bits()), static_cast<int>(e));
    return x.signbit() ? -v : v;
}

inline void bf_tqli(std::vector<double>& d, std::vector<double>& e) {
    const int    n   = static_cast<int>(d.size());
    const double eps = std::numeric_limits<double>::epsilon();

    for (int l = 0; l < n; ++l) {
        int iter = 0;
        int m    = l;

        do {
            for (m = l; m < n - 1; ++m) {
                const double dd = std::fabs(d[m]) + std::fabs(d[m + 1]);
                if (std::fabs(e[m]) <= eps * dd) break;
            }

            if (m != l) {
                if (iter++ == 64) break;
                double g = (d[l + 1] - d[l]) / (2.0 * e[l]);
                double r = std::hypot(g, 1.0);
                g = d[m] - d[l] + e[l] / (g + std::copysign(r, g));
                double s = 1.0, c = 1.0, p = 0.0;
                int    i = m - 1;

                for (; i >= l; --i) {
                    const double f = s * e[i];
                    const double b = c * e[i];
                    r = std::hypot(f, g);
                    e[i + 1] = r;
                    if (r == 0.0) { d[i + 1] -= p; e[m] = 0.0; break; }
                    s = f / r;
                    c = g / r;
                    g = d[i + 1] - p;
                    r = (d[i] - g) * s + 2.0 * c * b;
                    p = s * r;
                    d[i + 1] = g + p;
                    g = c * r - b;
                }

                if (r == 0.0 && i >= l) continue;
                d[l] -= p;
                e[l]  = g;
                e[m]  = 0.0;
            }
        } while (m != l);
    }
}

inline void bf_laurie(
    std::size_t N, const std::vector<BF>& a0, const std::vector<BF>& b0, const BFC& c,
    std::vector<BF>& a_out, std::vector<BF>& b_out
) {
    const BF zero = BF::zero();
    std::vector<BF> A(2 * N + 2, zero), B(2 * N + 2, zero);
    for (std::size_t k = 0; k <= (3 * N) / 2;     ++k) A[k + 1] = a0[k];
    for (std::size_t k = 0; k <= (3 * N + 1) / 2; ++k) B[k + 1] = b0[k];
    const std::size_t ns = N / 2 + 2;
    std::vector<BF> s(ns + 1, zero), t(ns + 1, zero);
    std::vector<BF> terms;
    std::vector<std::size_t> js;
    t[2] = B[N + 2];

    for (std::size_t m = 0; m + 2 <= N; ++m) {                              
        const std::size_t kmax = (m + 1) / 2;
        terms.clear();

        for (std::size_t k = kmax + 1; k-- > 0; ) {                         
            const std::size_t l = m - k;
            BF term = BF::mul(BF::sub(A[k + N + 2], A[l + 1], c), t[k + 2], c);
            term = BF::add(term, BF::mul(B[k + N + 2], s[k + 1], c), c);
            term = BF::sub(term, BF::mul(B[l + 1], s[k + 2], c), c);
            terms.push_back(std::move(term));
        }

        BF acc = zero;
        std::size_t idx = 0;

        for (std::size_t k = kmax + 1; k-- > 0; ++idx) {                     
            acc = BF::add(acc, terms[idx], c);
            s[k + 2] = acc;
        }

        std::swap(s, t);
    }

    for (std::size_t j = N / 2 + 1; j-- > 0; ) s[j + 2] = s[j + 1];          

    for (std::size_t m = N - 1; m + 3 <= 2 * N; ++m) {                       
        const std::size_t klo = m + 1 - N;
        const std::size_t khi = (m - 1) / 2;
        terms.clear();
        js.clear();

        for (std::size_t k = klo; k <= khi; ++k) {
            const std::size_t l = m - k;
            const std::size_t j = N - 1 - l;
            BF term = -BF::mul(BF::sub(A[k + N + 2], A[l + 1], c), t[j + 2], c);
            term = BF::sub(term, BF::mul(B[k + N + 2], s[j + 2], c), c);
            term = BF::add(term, BF::mul(B[l + 2], s[j + 3], c), c);
            terms.push_back(std::move(term));
            js.push_back(j);
        }

        BF acc = zero;

        for (std::size_t i = 0; i < terms.size(); ++i) {
            acc = BF::add(acc, terms[i], c);
            s[js[i] + 2] = acc;
        }

        const std::size_t jl = js.back();
        const std::size_t kk = (m + 1) / 2;

        if (m % 2 == 0) A[kk + N + 2] = BF::add(A[kk + 1], BF::div(BF::sub(s[jl + 2], BF::mul(B[kk + N + 2], s[jl + 3], c), c), t[jl + 3], c), c);
        else            B[kk + N + 2] = BF::div(s[jl + 2], s[jl + 3], c);

        std::swap(s, t);
    }

    A[2 * N + 1] = BF::sub(A[N], BF::div(BF::mul(B[2 * N + 1], s[2], c), t[2], c), c);
    a_out.assign(A.begin() + 1, A.begin() + static_cast<std::ptrdiff_t>(2 * N + 2));
    b_out.assign(B.begin() + 1, B.begin() + static_cast<std::ptrdiff_t>(2 * N + 2));
}

struct bf_gk_rule {
    std::vector<BF> x;                                                      
    std::vector<BF> wk;                                                     
    std::vector<BF> wg;                                                     
};

inline const bf_gk_rule& bf_gk_nodes(std::size_t N, std::size_t prec) {
    static thread_local std::map<std::pair<std::size_t, std::size_t>, bf_gk_rule> cache;
    if (N == 0) N = 1;
    const std::pair<std::size_t, std::size_t> key(N, prec);
    auto it = cache.find(key);
    if (it != cache.end()) return it->second;
    bf_gk_rule&       R   = cache[key];
    const std::size_t M   = 2 * N + 1;
    const std::size_t gp  = prec + 64 + 4 * bf_bitlen(N);
    const BFC         c   = bf_ctx(gp);
    const BF          one = BF::one();
    const std::size_t nb = (3 * N + 1) / 2 + 1;
    std::vector<BF> a0(nb, BF::zero()), b0(nb);
    b0[0] = BF(static_cast<std::uint64_t>(2));

    for (std::size_t k = 1; k < nb; ++k) {
        const std::uint64_t kk = static_cast<std::uint64_t>(k) * k;
        b0[k] = BF::div(BF(kk), BF(4 * kk - 1), c);
    }

    std::vector<BF> a, b;
    bf_laurie(N, a0, b0, c, a, b);
    std::fill(a.begin(), a.end(), BF::zero());                              
    std::vector<double> d(M, 0.0), e(M, 0.0);
    for (std::size_t i = 0; i + 1 < M; ++i) e[i] = std::sqrt(bf_to_double(b[i + 1]));
    bf_tqli(d, e);
    std::sort(d.begin(), d.end());

    auto pi_eval = [&](const BF& z, const BFC& cc, BF& p, BF& dp) {          
        BF p0 = one, p1 = z, d0 = BF::zero(), d1 = one;

        for (std::size_t k = 1; k < M; ++k) {
            BF p2 = BF::sub(BF::mul(z, p1, cc), BF::mul(b[k], p0, cc), cc);
            BF d2 = BF::add(p1, BF::sub(BF::mul(z, d1, cc), BF::mul(b[k], d0, cc), cc), cc);
            p0 = std::move(p1); p1 = std::move(p2);
            d0 = std::move(d1); d1 = std::move(d2);
        }

        p  = std::move(p1);
        dp = std::move(d1);
    };

    auto christoffel = [&](const BF& z) {                                    
        BF p0   = one;
        BF p1   = z;
        BF norm = b[0];
        BF s    = norm.reciprocal(c);
        norm = BF::mul(norm, b[1], c);
        s    = BF::add(s, BF::div(BF::mul(p1, p1, c), norm, c), c);

        for (std::size_t k = 1; k + 1 < M; ++k) {
            BF p2 = BF::sub(BF::mul(z, p1, c), BF::mul(b[k], p0, c), c);
            norm = BF::mul(norm, b[k + 1], c);
            s    = BF::add(s, BF::div(BF::mul(p2, p2, c), norm, c), c);
            p0 = std::move(p1);
            p1 = std::move(p2);
        }

        return s.reciprocal(c);
    };

    const bf_nodes& gl = bf_gl_nodes(N, prec);
    R.x.assign(M, BF::zero());
    R.wk.assign(M, BF::zero());
    R.wg.resize(N);
    for (std::size_t j = 0; j < N; ++j) R.wg[j] = gl[j].second;

    for (std::size_t i = N; i < M; ++i) {                                   
        BF z;

        if (i % 2 == 1) {
            z = gl[(i - 1) / 2].first;                                      
        } else if (i == N) {
            z = BF::zero();                                                 
        } else {
            z = BF(d[i]);
            std::size_t p       = 64;
            int         at_full = 0;

            while (at_full < 2) {                                           
                p = std::min<std::size_t>(2 * p, gp);
                if (p == gp) ++at_full;
                const BFC cc = bf_ctx(p);
                BF pv, dv;
                pi_eval(z, cc, pv, dv);
                z = BF::sub(z, BF::div(pv, dv, cc), cc);
            }
        }

        const BF w = christoffel(z);
        R.x[i]          = z;
        R.wk[i]         = w;
        R.x[M - 1 - i]  = -z;
        R.wk[M - 1 - i] = w;
    }

    return R;
}

inline void bf_gk_eval(const BFF& g, const bf_gk_rule& R, const BF& a, const BF& b, const BFC& wc, BF& K, BF& err) {
    const std::size_t M   = R.x.size();
    const std::size_t N   = R.wg.size();
    const BFC         ac  = bf_ctx(wc.precision + 16);
    const BF          m   = bf_mid(a, b, wc);
    const BF          hl  = BF::sub(b, a, wc).scaled_pow2(-1);
    const BF          ahl = hl.abs();
    std::vector<BF> xs(M), ys(M);
    for (std::size_t i = 0; i < M; ++i) xs[i] = BF::add(m, BF::mul(hl, R.x[i], wc), wc);
    g.evaluate(xs.data(), ys.data(), M, wc);
    BF ks = BF::zero(), gs = BF::zero(), ra = BF::zero();

    for (std::size_t i = 0; i < M; ++i) {
        if (!ys[i].is_finite()) {
            K   = BF::nan();
            err = BF::infinity();
            return;
        }

        const BF t = BF::mul(R.wk[i], ys[i], wc);
        ks = BF::add(ks, t, ac);
        ra = BF::add(ra, t.abs(), ac);
    }

    for (std::size_t j = 0; j < N; ++j) gs = BF::add(gs, BF::mul(R.wg[j], ys[2 * j + 1], wc), ac);

    const BF mean = ks.scaled_pow2(-1);                                     
    BF rs = BF::zero();
    for (std::size_t i = 0; i < M; ++i) rs = BF::add(rs, BF::mul(R.wk[i], BF::sub(ys[i], mean, wc).abs(), wc), ac);
    K = BF::mul(hl, ks, wc);
    const BF G      = BF::mul(hl, gs, wc);
    const BF resabs = BF::mul(ahl, ra, wc);
    const BF resasc = BF::mul(ahl, rs, wc);
    const BF diff   = BF::sub(K, G, wc).abs();
    const double ld = diff.is_zero()   ? -bf_inf : bf_l2(diff)   + 1.0;
    const double la = resasc.is_zero() ? -bf_inf : bf_l2(resasc) + 1.0;
    double le = ld;
    if (ld != -bf_inf && la != -bf_inf) le = la + std::min(0.0, 1.5 * (7.643856189774724 + ld - la));
    const double lfloor = resabs.is_zero() ? -bf_inf : bf_l2(resabs) + 1.0 - static_cast<double>(wc.precision) + 6.0;
    if (le < lfloor) le = lfloor;
    err = bf_pow2(le);
}

struct bf_gk_part {
    BF            a, b, value, err;
    double        lerr;
    std::uint64_t depth;
};

inline IntegrationResult<BF> bf_gk_adaptive(
    const BFF& g, const bf_gk_rule& R, const BF& a, const BF& b,
    const IntegrationConfig<BF>& cfg, const BFC& wc
) {
    const BFC ac = bf_ctx(wc.precision + 32);

    auto make = [&](const BF& pa, const BF& pb, std::uint64_t depth) {
        bf_gk_part p;
        p.a     = pa;
        p.b     = pb;
        p.depth = depth;
        bf_gk_eval(g, R, pa, pb, wc, p.value, p.err);
        p.lerr = p.err.is_finite() ? bf_l2(p.err) : bf_inf;
        return p;
    };

    auto cmp = [](const bf_gk_part& x, const bf_gk_part& y) { return x.lerr < y.lerr; };
    std::vector<bf_gk_part> work;
    work.push_back(make(a, b, 0));
    BF tval = work[0].value;
    BF terr = work[0].err;
    std::uint64_t       splits = 0;
    const std::uint64_t cap    = cfg.max_subdivisions;
    const std::uint64_t gcap   = (cfg.strategy == AdaptiveStrategy::Global) ? cap
                               : (cfg.strategy == AdaptiveStrategy::Hybrid) ? std::min<std::uint64_t>(cfg.global_levels, cap) : 0;

    while (splits < gcap && !bf_ok(cfg, terr, tval)) {                      
        std::pop_heap(work.begin(), work.end(), cmp);
        bf_gk_part p = std::move(work.back());
        work.pop_back();

        if (p.depth >= cfg.max_depth) {
            work.push_back(std::move(p));
            std::push_heap(work.begin(), work.end(), cmp);
            break;
        }

        const BF   mid = bf_mid(p.a, p.b, wc);
        bf_gk_part L   = make(p.a, mid, p.depth + 1);
        bf_gk_part Rt  = make(mid, p.b, p.depth + 1);
        tval = BF::add(BF::sub(tval, p.value, ac), BF::add(L.value, Rt.value, ac), ac);
        terr = BF::add(BF::sub(terr, p.err,   ac), BF::add(L.err,   Rt.err,   ac), ac);
        work.push_back(std::move(L));
        std::push_heap(work.begin(), work.end(), cmp);
        work.push_back(std::move(Rt));
        std::push_heap(work.begin(), work.end(), cmp);
        ++splits;
    }

    if (cfg.strategy != AdaptiveStrategy::Global) {                          
        const BF span = BF::sub(b, a, wc);
        const BF tol  = cfg.tolerance_for(tval);
        std::vector<bf_gk_part> stack(std::move(work));
        std::vector<bf_gk_part> done;

        while (!stack.empty()) {
            bf_gk_part p = std::move(stack.back());
            stack.pop_back();
            const BF   share = BF::div(BF::mul(tol, BF::sub(p.b, p.a, wc), wc), span, wc);
            const bool fine  = p.err.is_finite() && !(share < p.err);

            if (fine || splits >= cap || p.depth >= cfg.max_depth) {
                done.push_back(std::move(p));
                continue;
            }

            const BF mid = bf_mid(p.a, p.b, wc);
            stack.push_back(make(mid, p.b, p.depth + 1));
            stack.push_back(make(p.a, mid, p.depth + 1));
            ++splits;
        }

        work = std::move(done);
    }

    BF val = BF::zero();
    BF err = BF::zero();

    for (const bf_gk_part& p : work) {
        val = BF::add(val, p.value, ac);
        err = BF::add(err, p.err,   ac);
    }

    IntegrationResult<BF> r;
    r.value          = val;
    r.error_estimate = err;
    r.subdivisions   = splits;
    r.converged      = bf_ok(cfg, err, val);
    return r;
}

enum class bf_rule : std::uint8_t { left, right, midpoint, trapezoid, simpson, gauss };

inline bf_rule bf_rule_for(IntegrationTechnique t) noexcept {
    switch (t) {
        case IntegrationTechnique::Simpson:      return bf_rule::simpson;
        case IntegrationTechnique::Trapezoidal:  return bf_rule::trapezoid;
        case IntegrationTechnique::LeftRect:     return bf_rule::left;
        case IntegrationTechnique::RightRect:    return bf_rule::right;
        case IntegrationTechnique::MidpointRect: return bf_rule::midpoint;
        default:                                 return bf_rule::gauss;
    }
}

struct bf_rule_eval {
    const BFF*      g;
    bf_rule         rule;
    const bf_nodes* gl;
    BFC             wc;

    unsigned order() const noexcept {                                                   
        switch (rule) {
            case bf_rule::left:
            case bf_rule::right:     return 1;
            case bf_rule::midpoint:
            case bf_rule::trapezoid: return 2;
            case bf_rule::simpson:   return 4;
            default:                 return static_cast<unsigned>(2 * gl->size());
        }
    }

    BF operator()(const BF& a, const BF& b) const {
        const BF h = BF::sub(b, a, wc);

        switch (rule) {
            case bf_rule::left:      return BF::mul(h, (*g)(a, wc), wc);
            case bf_rule::right:     return BF::mul(h, (*g)(b, wc), wc);
            case bf_rule::midpoint:  return BF::mul(h, (*g)(bf_mid(a, b, wc), wc), wc);
            case bf_rule::trapezoid: return BF::mul(h, BF::add((*g)(a, wc), (*g)(b, wc), wc).scaled_pow2(-1), wc);

            case bf_rule::simpson: {
                const BF s = BF::add(BF::add((*g)(a, wc), (*g)(b, wc), wc), (*g)(bf_mid(a, b, wc), wc).scaled_pow2(2), wc);
                return BF::div(BF::mul(h, s, wc), BF(static_cast<std::uint64_t>(6)), wc);
            }

            default: {
                const std::size_t n  = gl->size();
                const BF          m  = bf_mid(a, b, wc);
                const BF          hl = h.scaled_pow2(-1);
                const BFC         ac = bf_ctx(wc.precision + 16);
                std::vector<BF> xs(n), ys(n);
                for (std::size_t i = 0; i < n; ++i) xs[i] = BF::add(m, BF::mul(hl, (*gl)[i].first, wc), wc);
                g->evaluate(xs.data(), ys.data(), n, wc);
                BF s = BF::zero();
                for (std::size_t i = 0; i < n; ++i) s = BF::add(s, BF::mul((*gl)[i].second, ys[i], wc), ac);
                return BF::mul(hl, s, wc);
            }
        }
    }
};

struct bf_panel {
    BF            a, b;
    BF            coarse, left, right;                     
    BF            value, err;                              
    double        lerr;
    std::uint64_t depth;
};

inline bf_panel bf_make_panel(const bf_rule_eval& q, const BF& a, const BF& b, BF coarse, const BF& den, std::uint64_t depth) {
    const BFC& wc = q.wc;
    bf_panel   p;
    p.a      = a;
    p.b      = b;
    p.coarse = std::move(coarse);
    p.depth  = depth;
    const BF m = bf_mid(a, b, wc);
    p.left  = q(a, m);
    p.right = q(m, b);
    const BF fine = BF::add(p.left, p.right, wc);
    const BF corr = BF::div(BF::sub(fine, p.coarse, wc), den, wc);
    p.value = BF::add(fine, corr, wc);
    p.err   = corr.abs();
    p.lerr  = bf_l2(p.err);
    return p;
}

inline IntegrationResult<BF> bf_adaptive(const bf_rule_eval& q, const BF& a, const BF& b, const IntegrationConfig<BF>& cfg) {
    const BFC& wc  = q.wc;
    const BFC  ac  = bf_ctx(wc.precision + 32);
    const BF   den = BF::sub(BF::one().scaled_pow2(static_cast<std::int64_t>(q.order())), BF::one(), wc);
    auto cmp = [](const bf_panel& x, const bf_panel& y) { return x.lerr < y.lerr; };
    std::vector<bf_panel> work;
    work.push_back(bf_make_panel(q, a, b, q(a, b), den, 0));
    BF tval = work[0].value;
    BF terr = work[0].err;
    std::uint64_t       splits = 0;
    const std::uint64_t cap    = cfg.max_subdivisions;
    const std::uint64_t gcap   = (cfg.strategy == AdaptiveStrategy::Global) ? cap : (cfg.strategy == AdaptiveStrategy::Hybrid) ? std::min<std::uint64_t>(cfg.global_levels, cap) : 0;

    while (splits < gcap && !bf_ok(cfg, terr, tval)) {
        std::pop_heap(work.begin(), work.end(), cmp);
        bf_panel p = std::move(work.back());
        work.pop_back();

        if (p.depth >= cfg.max_depth) {
            work.push_back(std::move(p));
            std::push_heap(work.begin(), work.end(), cmp);
            break;
        }

        const BF m = bf_mid(p.a, p.b, wc);
        bf_panel L = bf_make_panel(q, p.a, m, p.left,  den, p.depth + 1);
        bf_panel R = bf_make_panel(q, m, p.b, p.right, den, p.depth + 1);
        tval = BF::add(BF::sub(tval, p.value, ac), BF::add(L.value, R.value, ac), ac);
        terr = BF::add(BF::sub(terr, p.err,   ac), BF::add(L.err,   R.err,   ac), ac);
        work.push_back(std::move(L));
        std::push_heap(work.begin(), work.end(), cmp);
        work.push_back(std::move(R));
        std::push_heap(work.begin(), work.end(), cmp);
        ++splits;
    }

    if (cfg.strategy != AdaptiveStrategy::Global) {
        const BF span = BF::sub(b, a, wc);
        const BF tol  = cfg.tolerance_for(tval);
        std::vector<bf_panel> stack(std::move(work));
        std::vector<bf_panel> done;

        while (!stack.empty()) {
            bf_panel p = std::move(stack.back());
            stack.pop_back();
            const BF   share = BF::div(BF::mul(tol, BF::sub(p.b, p.a, wc), wc), span, wc);
            const bool fine  = p.err.is_finite() && !(share < p.err);

            if (fine || splits >= cap || p.depth >= cfg.max_depth) {
                done.push_back(std::move(p));
                continue;
            }

            const BF m = bf_mid(p.a, p.b, wc);
            stack.push_back(bf_make_panel(q, m, p.b, p.right, den, p.depth + 1));
            stack.push_back(bf_make_panel(q, p.a, m, p.left,  den, p.depth + 1));
            ++splits;
        }

        work = std::move(done);
    }

    BF val = BF::zero();
    BF err = BF::zero();

    for (const bf_panel& p : work) {
        val = BF::add(val, p.value, ac);
        err = BF::add(err, p.err,   ac);
    }

    IntegrationResult<BF> r;
    r.value          = val;
    r.error_estimate = err;
    r.subdivisions   = splits;
    r.converged      = bf_ok(cfg, err, val);
    return r;
}

inline IntegrationResult<BF> bf_clenshaw_curtis(const BFF& g, const BF& a, const BF& b, const IntegrationConfig<BF>& cfg, const BFC& wc) {
    const BFC ac = bf_ctx(wc.precision + 16);
    const BF  m  = bf_mid(a, b, wc);
    const BF  hl = BF::sub(b, a, wc).scaled_pow2(-1);
    std::size_t Nmax = 32;
    while (Nmax < cfg.max_subdivisions && Nmax < (1u << 12)) Nmax <<= 1;

    auto quad = [&](std::size_t N, const std::vector<BF>& fv) {
        const bf_nodes& nw = bf_cc_nodes(N, wc.precision);
        BF s = BF::zero();
        for (std::size_t k = 0; k <= N; ++k) s = BF::add(s, BF::mul(nw[k].second, fv[k], wc), ac);
        return BF::mul(hl, s, wc);
    };

    std::size_t     N = 16;
    std::vector<BF> fv(N + 1);

    {
        const bf_nodes& nw = bf_cc_nodes(N, wc.precision);
        std::vector<BF> xs(N + 1);
        for (std::size_t k = 0; k <= N; ++k) xs[k] = BF::add(m, BF::mul(hl, nw[k].first, wc), wc);
        g.evaluate(xs.data(), fv.data(), N + 1, wc);
    }

    IntegrationResult<BF> r;
    r.value          = quad(N, fv);
    r.error_estimate = BF::infinity();

    while (N < Nmax) {
        const std::size_t N2  = 2 * N;
        const bf_nodes&   nw2 = bf_cc_nodes(N2, wc.precision);
        std::vector<BF>   fv2(N2 + 1);
        std::vector<BF>   xs, ys;
        for (std::size_t k = 0; k <= N; ++k) fv2[2 * k] = std::move(fv[k]);               
        for (std::size_t k = 1; k < N2; k += 2) xs.push_back(BF::add(m, BF::mul(hl, nw2[k].first, wc), wc));
        ys.resize(xs.size());
        g.evaluate(xs.data(), ys.data(), xs.size(), wc);
        for (std::size_t i = 0; i < ys.size(); ++i) fv2[2 * i + 1] = std::move(ys[i]);
        const BF Q2 = quad(N2, fv2);
        r.error_estimate = BF::sub(Q2, r.value, wc).abs();
        r.value = Q2;
        fv.swap(fv2);
        N = N2;
        ++r.subdivisions;

        if (bf_ok(cfg, r.error_estimate, r.value)) {
            r.converged = true;
            break;
        }
    }

    return r;
}

inline IntegrationResult<BF> bf_romberg(const BFF& g, const BF& a, const BF& b, const IntegrationConfig<BF>& cfg, const BFC& wc) {
    const BFC         ac   = bf_ctx(wc.precision + 16);
    const BF          h    = BF::sub(b, a, wc);
    const std::size_t kmax = static_cast<std::size_t>(std::min<std::uint64_t>(cfg.max_depth, bf_bitlen(std::max<std::uint64_t>(cfg.max_subdivisions, 4)) + 1));
    std::vector<BF> prev(1), cur;
    prev[0] = BF::mul(h.scaled_pow2(-1), BF::add(g(a, wc), g(b, wc), wc), wc);
    IntegrationResult<BF> r;
    r.value          = prev[0];
    r.error_estimate = BF::infinity();

    for (std::size_t k = 1; k <= kmax; ++k) {
        const std::uint64_t cnt = 1ull << (k - 1);
        const BF            hk  = h.scaled_pow2(-static_cast<std::int64_t>(k));
        std::vector<BF> xs(static_cast<std::size_t>(cnt)), ys(static_cast<std::size_t>(cnt));
        for (std::uint64_t i = 0; i < cnt; ++i) xs[i] = BF::add(a, BF::mul(hk, BF(2 * i + 1), wc), wc);
        g.evaluate(xs.data(), ys.data(), xs.size(), wc);
        BF s = BF::zero();
        for (const BF& y : ys) s = BF::add(s, y, ac);
        cur.assign(k + 1, BF::zero());
        cur[0] = BF::add(prev[0].scaled_pow2(-1), BF::mul(hk, s, wc), wc);

        for (std::size_t j = 1; j <= k; ++j) {                                           
            const BF den = BF::sub(BF::one().scaled_pow2(static_cast<std::int64_t>(2 * j)), BF::one(), wc);
            cur[j] = BF::add(cur[j - 1], BF::div(BF::sub(cur[j - 1], prev[j - 1], wc), den, wc), wc);
        }

        r.error_estimate = BF::sub(cur[k], prev[k - 1], wc).abs();
        r.value          = cur[k];
        r.subdivisions   = k;
        prev.swap(cur);

        if (k >= 3 && bf_ok(cfg, r.error_estimate, r.value)) {
            r.converged = true;
            break;
        }
    }

    return r;
}

inline IntegrationResult<BF> bf_tanh_sinh(const BFF& f, const Interval<BF>& iv, const IntegrationConfig<BF>& cfg, const BFC& wc) {
    const BoundType bt  = bound_type_of(iv);
    const BFC       ac  = bf_ctx(wc.precision + 16);
    const BF        one = BF::one();
    const BF        two = one.scaled_pow2(1);
    BF a, b, hl;
    if (bt == BoundType::Finite || bt == BoundType::RightInfinite) a = iv.lower().value();
    if (bt == BoundType::Finite || bt == BoundType::LeftInfinite)  b = iv.upper().value();
    if (bt == BoundType::Finite) hl = BF::sub(b, a, wc).scaled_pow2(-1);

    BF   S = BF::zero();
    auto add_point = [&](const BF& w, const BF& x) {                                     
        const BF v = f(x, wc);
        if (v.is_finite()) S = BF::add(S, BF::mul(w, v, wc), ac);
    };

    const std::size_t kmax = static_cast<std::size_t>(std::min<std::uint64_t>(cfg.max_depth, 14));
    IntegrationResult<BF> r;
    r.error_estimate = BF::infinity();
    BF     I_prev;
    double ld_prev = bf_inf;

    for (std::size_t k = 0; k <= kmax; ++k) {
        const std::vector<bf_de_node>& lev = bf_de_level(wc.precision, k);

        for (std::size_t i = 0; i < lev.size(); ++i) {
            const bf_de_node& nd     = lev[i];
            const bool        center = (k == 0 && i == 0);

            switch (bt) {
                case BoundType::Finite: {                          
                    const BF E2 = BF::mul(nd.E, nd.E, wc);
                    const BF d  = BF::add(E2, one, wc);
                    const BF w  = BF::div(BF::mul(nd.hch, E2.scaled_pow2(2), wc), BF::mul(d, d, wc), wc);  
                    if (center) { add_point(w, BF::add(a, hl, wc)); break; }
                    const BF off = BF::mul(hl, BF::div(two, d, wc), wc);
                    add_point(w, BF::add(a, off, wc));
                    add_point(w, BF::sub(b, off, wc));
                    break;
                }

                case BoundType::RightInfinite:
                case BoundType::LeftInfinite: {                    
                    const bool right = (bt == BoundType::RightInfinite);
                    const BF&  base  = right ? a : b;
                    if (center) { add_point(nd.hch, right ? BF::add(base, one, wc) : BF::sub(base, one, wc)); break; }
                    const BF ie = nd.E.reciprocal(wc);
                    add_point(BF::mul(nd.hch, nd.E, wc), right ? BF::add(base, nd.E, wc) : BF::sub(base, nd.E, wc));
                    add_point(BF::mul(nd.hch, ie,   wc), right ? BF::add(base, ie,   wc) : BF::sub(base, ie,   wc));
                    break;
                }

                default: {                                        
                    if (center) { add_point(nd.hch, BF::zero()); break; }
                    const BF ie = nd.E.reciprocal(wc);
                    const BF sh = BF::sub(nd.E, ie, wc).scaled_pow2(-1);
                    const BF w  = BF::mul(nd.hch, BF::add(nd.E, ie, wc).scaled_pow2(-1), wc);
                    add_point(w, sh);
                    add_point(w, -sh);
                    break;
                }
            }
        }

        BF I = S.scaled_pow2(-static_cast<std::int64_t>(k)).rounded(wc);
        if (bt == BoundType::Finite) I = BF::mul(I, hl, wc);
        r.value        = I;
        r.subdivisions = k;

        if (k >= 1) {                                              
            const double ld  = bf_l2(BF::sub(I, I_prev, wc));
            double       est = ld;
            if (k >= 2 && ld < ld_prev) est = std::min(ld, 2.0 * ld - ld_prev);
            const double floor_l = bf_l2(I) - static_cast<double>(wc.precision) + 4.0;
            if (est < floor_l) est = floor_l;
            r.error_estimate = bf_pow2(est);

            if (k >= 3 && bf_ok(cfg, r.error_estimate, I)) {
                r.converged = true;
                return r;
            }

            ld_prev = ld;
        }

        I_prev = I;
    }

    return r;
}

} // namespace bfidetail

class BigFloatIntegration {
public:
    using value_type = ::fizmo::multiprecision::BigFloat;
    using Function   = ::fizmo::multiprecision::BigFloatFunction;
    using Config     = IntegrationConfig<value_type>;
    using Result     = IntegrationResult<value_type>;
    using Range      = Interval<value_type>;

    // n: Gauss-Legendre points per panel (0 = chosen from precision)
    // Ignored by the other techniques
    static Result integrate(
        const Function& f, const Range& iv,        
        IntegrationTechnique technique = IntegrationTechnique::TanhSinh,
        const Config& cfg = Config(), std::size_t n = 0
    ) {
        return run(f, iv, technique, cfg, n);
    }

    static Result integrate(
        const Function& f, const value_type& a, const value_type& b,
        IntegrationTechnique technique = IntegrationTechnique::TanhSinh,
        const Config& cfg = Config(), std::size_t n = 0
    ) {
        if (a.is_nan() || b.is_nan() || a.is_undefined() || b.is_undefined()) {
            Result r;
            r.method         = technique;
            r.context        = cfg.context;
            r.value          = value_type::undefined();
            r.error_estimate = value_type::undefined();
            return r;
        }

        const value_type::ordering o = value_type::compare(a, b);
        if (o == value_type::ordering::equal) return Result::converged_zero(technique);

        if (o == value_type::ordering::greater) {
            Result r = run(f, Range::closed(b, a), technique, cfg, n);
            r.value  = -r.value;
            return r;
        }

        return run(f, Range::closed(a, b), technique, cfg, n);
    }

#define FIZMO_BFI_TECHNIQUE(NAME, TECH)                                                                                  \
    static Result NAME(const Function& f, const Range& iv, const Config& cfg = Config())                                 \
        { return integrate(f, iv, TECH, cfg); }                                                                           \
    static Result NAME(const Function& f, const value_type& a, const value_type& b, const Config& cfg = Config())        \
        { return integrate(f, a, b, TECH, cfg); }

    FIZMO_BFI_TECHNIQUE(integrate_simpson,         IntegrationTechnique::Simpson)
    FIZMO_BFI_TECHNIQUE(integrate_trapezoidal,     IntegrationTechnique::Trapezoidal)
    FIZMO_BFI_TECHNIQUE(integrate_left_rect,       IntegrationTechnique::LeftRect)
    FIZMO_BFI_TECHNIQUE(integrate_right_rect,      IntegrationTechnique::RightRect)
    FIZMO_BFI_TECHNIQUE(integrate_midpoint_rect,   IntegrationTechnique::MidpointRect)
    FIZMO_BFI_TECHNIQUE(integrate_romberg,         IntegrationTechnique::Romberg)
    FIZMO_BFI_TECHNIQUE(integrate_clenshaw_curtis, IntegrationTechnique::ClenshawCurtis)
    FIZMO_BFI_TECHNIQUE(integrate_tanh_sinh,       IntegrationTechnique::TanhSinh)

#undef FIZMO_BFI_TECHNIQUE

    static Result integrate_gauss_legendre(const Function& f, const Range& iv, std::size_t n = 0, const Config& cfg = Config()) {
        return integrate(f, iv, IntegrationTechnique::GaussLegendre, cfg, n);
    }

    static Result integrate_gauss_legendre(const Function& f, const Range& iv, const Config& cfg) {
        return integrate(f, iv, IntegrationTechnique::GaussLegendre, cfg, 0);
    }

    static Result integrate_gauss_legendre(const Function& f, const value_type& a, const value_type& b, std::size_t n = 0, const Config& cfg = Config()) {
        return integrate(f, a, b, IntegrationTechnique::GaussLegendre, cfg, n);
    }

    static Result integrate_gauss_legendre(const Function& f, const value_type& a, const value_type& b, const Config& cfg) {
        return integrate(f, a, b, IntegrationTechnique::GaussLegendre, cfg, 0);
    }

    static const bfidetail::bf_nodes& gauss_legendre_nodes(std::size_t n, std::size_t precision)  { return bfidetail::bf_gl_nodes(n, precision); }
    static const bfidetail::bf_nodes& clenshaw_curtis_nodes(std::size_t N, std::size_t precision) { return bfidetail::bf_cc_nodes(N, precision); }

    // n: 2N+1 points (N = 7 -> GK15, 10 -> GK21, 30 -> GK61). 0 = from precision.
    static Result integrate_gauss_kronrod(const Function& f, const Range& iv, std::size_t n = 0, const Config& cfg = Config()) {
        return integrate(f, iv, IntegrationTechnique::GaussKronrod, cfg, n);
    }

    static Result integrate_gauss_kronrod(const Function& f, const Range& iv, const Config& cfg) {
        return integrate(f, iv, IntegrationTechnique::GaussKronrod, cfg, 0);
    }

    static Result integrate_gauss_kronrod(const Function& f, const value_type& a, const value_type& b, std::size_t n = 0, const Config& cfg = Config()) {
        return integrate(f, a, b, IntegrationTechnique::GaussKronrod, cfg, n);
    }

    static Result integrate_gauss_kronrod(const Function& f, const value_type& a, const value_type& b, const Config& cfg) {
        return integrate(f, a, b, IntegrationTechnique::GaussKronrod, cfg, 0);
    }

    static const bfidetail::bf_gk_rule& gauss_kronrod_nodes(std::size_t n, std::size_t precision) { return bfidetail::bf_gk_nodes(n, precision); }

private:
    static Result run(const Function& f, const Range& iv, IntegrationTechnique technique, const Config& cfg, std::size_t n) {
        using namespace bfidetail;
        if (!f) throw std::bad_function_call();
        Result r;
        r.method  = (technique == IntegrationTechnique::General) ? IntegrationTechnique::TanhSinh : technique;
        r.context = cfg.context;

        if (!iv.is_valid()) {
            r.value          = value_type::undefined();
            r.error_estimate = value_type::undefined();
            return r;
        }

        if (iv.is_empty() || iv.is_degenerate()) {
            r.converged = true;
            return r;
        }

        const std::uint64_t start = f.call_count();
        const std::size_t   prec  = cfg.context.precision;
        const std::size_t   nn    = (n != 0) ? n : (r.method == IntegrationTechnique::GaussKronrod) ? bf_default_gk_n(prec) : bf_default_gl_n(prec);
        const BFC           wc    = bf_ctx(prec + 32 + 2 * bf_bitlen(nn));
        Result raw;

        if (r.method == IntegrationTechnique::TanhSinh) {
            raw = bf_tanh_sinh(f, iv, cfg, wc);
        } else {
            InfiniteTransform tr = cfg.transform;
            if (tr == InfiniteTransform::Auto) tr = bf_resolve(bound_type_of(iv), r.method);
            const bf_mapped m = bf_map(f, iv, tr, wc.precision);

            switch (r.method) {
                case IntegrationTechnique::Romberg:        raw = bf_romberg(m.g, m.a, m.b, cfg, wc);                                break;
                case IntegrationTechnique::ClenshawCurtis: raw = bf_clenshaw_curtis(m.g, m.a, m.b, cfg, wc);                        break;
                case IntegrationTechnique::GaussKronrod:   raw = bf_gk_adaptive(m.g, bf_gk_nodes(nn, wc.precision), m.a, m.b, cfg, wc); break;

                default: {
                    const bf_rule   rule = bf_rule_for(r.method);
                    const bf_nodes* gl   = (rule == bf_rule::gauss) ? &bf_gl_nodes(nn, wc.precision) : nullptr;
                    raw = bf_adaptive(bf_rule_eval{&m.g, rule, gl, wc}, m.a, m.b, cfg);
                    break;
                }
            }
        }

        r.value                = raw.value.rounded(cfg.context);
        r.error_estimate       = raw.error_estimate.rounded(cfg.context);
        r.converged            = raw.converged;
        r.subdivisions         = raw.subdivisions;
        r.function_evaluations = f.call_count() - start;
        return r;
    }
};

} // namespace integration
} // namespace math
} // namespace fizmo

#endif // FIZMO_BIGFLOAT_INTEGRATION_HPP