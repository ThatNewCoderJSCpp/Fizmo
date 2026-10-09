#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace rgdetail {

zt_val rg_low(const BigFloat& x, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const zt_val li = pldetail::pl_li2_raw(x, want);
    if (!li.v.is_finite()) return li;
    const BigFloat a  = ln(x.abs(), wc);                                                   
    const BigFloat b  = ln(hz_xadd(BigFloat::one(), x, true), wc);                        
    const BigFloat p  = BigFloat::mul(a, b, wc).scaled_pow2(-1);
    const BigFloat v  = BigFloat::add(li.v, p, wc);
    if (v.is_zero()) return zt_val{v, 1.0e9};
    const double   lv = hz_l2(v);
    const double units = std::exp2(hz_l2(li.v) + li.lost - lv) + std::exp2(hz_l2(p) + 2.0 - lv) + 1.0;
    return rg_combine_units(v, units);
}

zt_val rg_high(const BigFloat& x, std::size_t want) {
    const BigFloatContext wc(want, RoundingMode::nearest_even);
    const BigFloat one = BigFloat::one();
    const BigFloat y   = x.reciprocal(wc);
    if (BigFloat::compare(y, one) != BigFloat::ordering::less) return zt_val{y, 1.0e9};    
    const zt_val li = pldetail::pl_int_raw(2, y, want);
    if (!li.v.is_finite()) return li;
    const BigFloat lx  = ln(x, wc);
    const BigFloat lxm = ln(hz_xadd(x, one, true), wc);                                    
    const BigFloat d   = BigFloat::sub(lxm, lx, wc);                                       
    const BigFloat p   = BigFloat::mul(lx, d, wc).scaled_pow2(-1);
    const BigFloat c   = constants::zeta2(wc).scaled_pow2(1);                              
    const BigFloat v   = BigFloat::add(BigFloat::sub(c, li.v, wc), p, wc);
    if (v.is_zero()) return zt_val{v, 1.0e9};
    const double lv = hz_l2(v);
    const double units = std::exp2(hz_l2(li.v) + li.lost - lv)                             
                       + std::exp2(hz_l2(d) + 1.0 - lv)                                    
                       + std::exp2(hz_l2(lx) + std::log2(std::exp2(hz_l2(lx)) + std::exp2(hz_l2(lxm))) + 1.0 - lv)   
                       + std::exp2(hz_l2(p) + 2.0 - lv)
                       + std::exp2(hz_l2(c) - lv)
                       + 1.0;
    return rg_combine_units(v, units);
}

zt_val rg_raw(const BigFloat& x, std::size_t want) {
    return (BigFloat::compare(x, BigFloat::one()) == BigFloat::ordering::less) ? rg_low(x, want) : rg_high(x, want);
}

std::size_t rg_guard0(const BigFloat& x) {
    const double l = std::fabs(hz_l2(x));
    return 32 + static_cast<std::size_t>(2.0 * std::log2(l + 2.0));
}

bool rg_normalized_exact(const BigFloat& x, BigFloat& out) {
    const BigFloat half = BigFloat::one().scaled_pow2(-1);
    if (BigFloat::compare(x, half) == BigFloat::ordering::equal)                              { out = half;                                   return true; }
    if (BigFloat::compare(x, BigFloat::one(true)) == BigFloat::ordering::equal)               { out = -half;                                  return true; }
    if (BigFloat::compare(x, BigFloat(static_cast<std::uint64_t>(2))) == BigFloat::ordering::equal) { out = BigFloat(static_cast<std::uint64_t>(3)).scaled_pow2(-1); return true; }
    return false;
}

} // namespace rgdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace math {

BigFloat rogers_l(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return x;

    if (x.is_infinite()) {                                                                  
        if (!x.signbit()) return constants::zeta2(ctx).scaled_pow2(1);
        return hzdetail::hz_drive(32, ctx, [](std::size_t want) {
            return ztdetail::zt_val{-constants::zeta2(BigFloatContext(want, RoundingMode::nearest_even)), 1.0};
        });
    }

    if (BigFloat::compare(x, BigFloat::one()) == BigFloat::ordering::equal) return constants::zeta2(ctx);
    return hzdetail::hz_drive(rgdetail::rg_guard0(x), ctx, [&](std::size_t want) { return rgdetail::rg_raw(x, want); });
}

BigFloat rogers_l_normalized(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_zero())      return x;
    if (x.is_infinite())  return x.signbit() ? BigFloat::one(true) : BigFloat(static_cast<std::uint64_t>(2));
    if (BigFloat::compare(x, BigFloat::one()) == BigFloat::ordering::equal) return BigFloat::one();
    BigFloat out;
    if (rgdetail::rg_normalized_exact(x, out)) return out.rounded(ctx);

    return hzdetail::hz_drive(rgdetail::rg_guard0(x), ctx, [&](std::size_t want) -> ztdetail::zt_val {
        const ztdetail::zt_val r = rgdetail::rg_raw(x, want);
        if (!r.v.is_finite()) return r;
        const BigFloatContext wc(want, RoundingMode::nearest_even);
        return ztdetail::zt_val{BigFloat::div(r.v, constants::zeta2(wc), wc), std::log2(std::exp2(r.lost) + 2.0)};
    });
}

BigFloat spence_function(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? BigFloat::nan() : BigFloat::infinity();
    if (BigFloat::compare(x, -BigFloat::one()) == BigFloat::ordering::less) { return BigFloat::nan(); } 
    return -polylog(-x, BigFloat(static_cast<std::uint64_t>(2)), ctx);
}

BigFloat spence_integral(const BigFloat& x, const BigFloatContext& ctx) {
    if (x.is_nan())       return BigFloat::nan();
    if (x.is_undefined()) return BigFloat::undefined();
    if (x.is_infinite())  return x.signbit() ? BigFloat::nan() : BigFloat::infinity(true);   
    if (x.signbit() && !x.is_zero()) return BigFloat::nan();
    return polylog(hzdetail::hz_xadd(BigFloat::one(), x, true), BigFloat(static_cast<std::uint64_t>(2)), ctx);
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo
