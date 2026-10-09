#ifndef FIZMO_MULTIPRECISION_BIG_AIRY_HPP
#define FIZMO_MULTIPRECISION_BIG_AIRY_HPP

#include "gamma.hpp"
#include "trig.hpp"
#include "pow_nth_root.hpp"
#include "sqrt_cbrt.hpp"
#include "logarithms.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <type_traits>

namespace fizmo {
namespace multiprecision {
namespace math {

namespace aydetail {

using BF  = BigFloat;
using BFC = BigFloatContext;

enum class ay_kind : int { ai = 0, bi, gi, hi };

struct ay_val {
    BF     v;
    double lost;                                                       
};

static const double ay_inf = std::numeric_limits<double>::infinity();

inline BFC    ay_ctx(std::size_t p) { return BFC(BFC::clamp_precision(p), RoundingMode::nearest_even); }
inline ay_val ay_undef()            { return ay_val{BF::undefined(), 0.0}; }

 double ay_l2lo(const BF& x);

inline double ay_l2hi(const BF& x) { return ay_l2lo(x) + 1.0; }       

 double ay_lsum(double a, double b);

 bool ay_safe(const BF& v, std::size_t want, double lost, std::size_t prec);

template <typename Eval>
inline BF ay_drive(std::size_t guard, const BFC& ctx, Eval eval) {
    for (;;) {
        const std::size_t want = BFC::clamp_precision(ctx.precision + guard);
        const ay_val      r    = eval(want);
        if (!r.v.is_finite()) return r.v;
        if (ay_safe(r.v, want, r.lost, ctx.precision)) return r.v.rounded(ctx);
        if (guard >= (1u << 22) || ctx.precision + guard >= BFC::max_prec / 4) return r.v.rounded(ctx);
        guard *= 2;
    }
}

struct ay_consts {
    BF          c1, c2;
    std::size_t prec = 0;
};

 const ay_consts& ay_constants(std::size_t want);

struct ay_ser {
    BF     f, g, h;
    double E  = -ay_inf;
    bool   ok = false;
};

 ay_ser ay_series(const BF& x, bool need_h, std::size_t want);

 ay_val ay_from_series(ay_kind k, const BF& x, std::size_t want);

 bool ay_u_available(double zeta, double target);

 bool ay_w_available(double ly, double target, bool gi_bound);

 BF ay_u_next(const BF& w, const BF& iz, std::uint64_t k, const BFC& wc);

 ay_val ay_ai_pos_asym(const BF& x, std::size_t want);

 ay_val ay_neg_asym(const BF& y, bool bi, std::size_t want);

 ay_val ay_hi_neg_asym(const BF& y, std::size_t want);

 ay_val ay_gi_pos_asym(const BF& x, std::size_t want);

struct ay_regime {
    bool   neg;
    double lx;        
    double zeta;      
};

 ay_regime ay_regime_of(const BF& x);

 bool ay_use_asym(ay_kind k, const ay_regime& g, double target);

 ay_val ay_eval(ay_kind k, const BF& x, std::size_t want);

 std::size_t ay_guard0(ay_kind k, const BF& x, std::size_t prec);

 BF ay_public(ay_kind k, const BF& x, const BFC& ctx);

} // namespace aydetail

inline BigFloat airy_ai(const BigFloat& x, const BigFloatContext& ctx)   { return aydetail::ay_public(aydetail::ay_kind::ai, x, ctx); }
inline BigFloat airy_bi(const BigFloat& x, const BigFloatContext& ctx)   { return aydetail::ay_public(aydetail::ay_kind::bi, x, ctx); }
inline BigFloat scorer_gi(const BigFloat& x, const BigFloatContext& ctx) { return aydetail::ay_public(aydetail::ay_kind::gi, x, ctx); }
inline BigFloat scorer_hi(const BigFloat& x, const BigFloatContext& ctx) { return aydetail::ay_public(aydetail::ay_kind::hi, x, ctx); }

FIZMO_MP_TRIG_FORWARD(airy_ai)
FIZMO_MP_TRIG_FORWARD(airy_bi)
FIZMO_MP_TRIG_FORWARD(scorer_gi)
FIZMO_MP_TRIG_FORWARD(scorer_hi)

#define FIZMO_MP_AIRY_ARITH_FORWARD(FN)                                                        \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x, const BigFloatContext& c) { return FN(BigFloat(x), c); }            \
    template <typename T, typename std::enable_if<std::is_arithmetic<T>::value, int>::type = 0> \
    inline BigFloat FN(T x) { return FN(BigFloat(x), BigFloatContext::current()); }

FIZMO_MP_AIRY_ARITH_FORWARD(airy_ai)
FIZMO_MP_AIRY_ARITH_FORWARD(airy_bi)
FIZMO_MP_AIRY_ARITH_FORWARD(scorer_gi)
FIZMO_MP_AIRY_ARITH_FORWARD(scorer_hi)

#undef FIZMO_MP_AIRY_ARITH_FORWARD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_AIRY_HPP