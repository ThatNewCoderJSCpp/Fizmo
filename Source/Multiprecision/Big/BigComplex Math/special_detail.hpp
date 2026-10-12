#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_DETAIL_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_DETAIL_HPP

#include "inv_elementary.hpp"

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace fizmo {
namespace math {
namespace cxsf {

using BF  = BigFloat;
using BFC = BigFloatContext;
using BC  = BigComplex;
using BCC = BigComplexContext;

constexpr double ninf = -std::numeric_limits<double>::infinity();
constexpr double pinf = std::numeric_limits<double>::infinity();

struct CX {
    BF re;
    BF im;

    CX() : re(BF::zero()), im(BF::zero()) {}
    CX(BF r) : re(std::move(r)), im(BF::zero()) {}
    CX(BF r, BF i) : re(std::move(r)), im(std::move(i)) {}
};

struct Approx {
    CX     v;
    double er        = ninf;
    double ei        = ninf;
    bool   undefined = false;
};

inline Approx approx(CX v, double err) {
    Approx a;
    a.v  = std::move(v);
    a.er = err;
    a.ei = err;
    return a;
}

inline Approx approx_undefined() {
    Approx a;
    a.undefined = true;
    return a;
}

struct CV {
    CX     v;
    double er = ninf;
    double ei = ninf;
};

inline CV cv_exact(CX v) { return CV{std::move(v), ninf, ninf}; }
inline Approx approx(const CV& c) {
    Approx a;
    a.v  = c.v;
    a.er = c.er;
    a.ei = c.ei;
    return a;
}

CV cv_add(const CV& a, const CV& b, std::size_t w);
CV cv_sub(const CV& a, const CV& b, std::size_t w);
CV cv_mul(const CV& a, const CV& b, std::size_t w);
CV cv_scale(const CV& a, const BF& s, std::size_t w);
CV cv_div_int(const CV& a, std::uint64_t d, std::size_t w);
CV cv_from(const CX& v, double err);

BFC wctx(std::size_t w);

double l2(const BF& x);
double l2(const CX& z);
double lsum(double a, double b);

CX cx(const BC& z);
BC bc(const CX& z);

inline bool is_zero(const CX& z) { return z.re.is_zero() && z.im.is_zero(); }
inline bool is_real(const CX& z) { return z.im.is_zero(); }
inline bool finite(const CX& z) { return z.re.is_finite() && z.im.is_finite(); }

CX add(const CX& a, const CX& b, std::size_t w);
CX sub(const CX& a, const CX& b, std::size_t w);
CX mul(const CX& a, const CX& b, std::size_t w);
CX mul(const CX& a, const BF& b, std::size_t w);
CX div(const CX& a, const CX& b, std::size_t w);
CX div(const CX& a, const BF& b, std::size_t w);
CX sqr(const CX& a, std::size_t w);
CX recip(const CX& a, std::size_t w);
CX neg(const CX& a);
CX conj(const CX& a);
CX mul_i(const CX& a);
CX div_i(const CX& a);
CX scale2(const CX& a, std::int64_t k);
CX add_real(const CX& a, const BF& b, std::size_t w);

CX exp(const CX& a, std::size_t w);
CX ln(const CX& a, std::size_t w);
CX sqrt(const CX& a, std::size_t w);
CX sin(const CX& a, std::size_t w);
CX cos(const CX& a, std::size_t w);
CX pow(const CX& a, const CX& b, std::size_t w);
CX pow_int(const CX& a, std::int64_t n, std::size_t w);

BF pi(std::size_t w);

void sincospi(const BF& x, std::size_t w, BF& s, BF& c);
CV sinpi(const CX& z, std::size_t w);
CV cospi(const CX& z, std::size_t w);
std::size_t extra_bits(const BF& x);
std::size_t extra_bits(const CX& z);

bool settle(const BF& x, double err, const BFC& c, bool last, BF& out);

BC finish_undefined();

template <class Eval>
BC drive(const BCC& ctx, Eval&& eval, std::size_t guard = 24, std::size_t cap = 2048) {
    const std::size_t prec = ctx.precision();
    for (;;) {
        const std::size_t wp   = BFC::clamp_precision(prec + guard);
        const bool        last = guard >= cap || wp >= BFC::max_prec / 4;
        const Approx      a    = eval(wp);
        if (a.undefined) return finish_undefined();
        if (!finite(a.v)) return BC(a.v.re.rounded(ctx.real()), a.v.im.rounded(ctx.imaginary()));
        BF re, im;
        const bool okr = settle(a.v.re, a.er, ctx.real(), last, re);
        const bool oki = settle(a.v.im, a.ei, ctx.imaginary(), last, im);
        if ((okr && oki) || last) return BC(re, im);
        guard *= 2;
    }
}

class PowTable {
public:
    PowTable(const CX& s, std::size_t w) : s_(s), w_(w) {}
    const CX& get(std::uint64_t n);

private:
    void grow(std::uint64_t n);
    CX                         s_;
    std::size_t                w_;
    std::vector<std::uint32_t> spf_;
    std::vector<CX>            t_;
};

CX real_pow_neg(const BF& x, const CX& s, std::size_t w);

struct Term {
    double l2max = ninf;
    double l2sum = ninf;
    void add(double l) {
        if (l > l2max) l2max = l;
        l2sum = lsum(l2sum, l);
    }
};

} // namespace cxsf
} // namespace math
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_SPECIAL_DETAIL_HPP
