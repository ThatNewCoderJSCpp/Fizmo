#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_CONSTANTS_HPP

#include "../big_float.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace bfdetail {

inline BigUInt unit(std::size_t N) {
    BigUInt r = BigUInt::one();
    r.shift_left_mutable(N);
    return r;
}

inline BigUInt isqrt(const BigUInt& v) {                  
    if (v.is_undefined()) return v;
    if (v.is_zero())      return BigUInt::zero();
    BigUInt x = unit((v.bit_length() + 1) / 2);           

    for (;;) {
        BigUInt y = v / x;
        y.add_mutable(x);
        y.shift_right_mutable(1);
        if (!(y < x)) break;
        x = std::move(y);
    }

    return x;
}

inline BigUInt atan_inv_fixed(std::uint64_t q, std::size_t N) {
    BigUInt term = unit(N);
    term.div_small_mutable(q);
    BigUInt pos = term;
    BigUInt neg = BigUInt::zero();
    const std::uint64_t qq = q * q;
    bool sub = true;

    for (std::uint64_t k = 3; ; k += 2, sub = !sub) {
        term.div_small_mutable(qq);
        if (term.is_zero()) break;
        BigUInt t = term;
        t.div_small_mutable(k);
        if (t.is_zero()) break;
        if (sub) neg.add_mutable(t); else pos.add_mutable(t);
    }

    pos.sub_mutable(neg);
    return pos;
}

inline BigUInt atanh_inv_fixed(std::uint64_t q, std::size_t N) {
    BigUInt term = unit(N);
    term.div_small_mutable(q);
    BigUInt sum = term;
    const std::uint64_t qq = q * q;

    for (std::uint64_t k = 3; ; k += 2) {
        term.div_small_mutable(qq);
        if (term.is_zero()) break;
        BigUInt t = term;
        t.div_small_mutable(k);
        if (t.is_zero()) break;
        sum.add_mutable(t);
    }

    return sum;
}

inline BigUInt fx_recip(const BigUInt& v, std::size_t N) { 
    BigUInt num = unit(2 * N);
    return num / v;
}

inline BigUInt fx_mul(const BigUInt& a, const BigUInt& b, std::size_t N) {
    BigUInt p = a * b;
    p.shift_right_mutable(N);
    return p;
}

inline BigUInt pi_fixed(std::size_t N) {                   
    BigUInt a = atan_inv_fixed(5, N);
    a.mul_small_mutable(16);
    BigUInt b = atan_inv_fixed(239, N);
    b.mul_small_mutable(4);
    a.sub_mutable(b);
    return a;
}

inline BigUInt inv_pi_fixed(std::size_t N)      { return fx_recip(pi_fixed(N), N); }
inline BigUInt third_pi_fixed(std::size_t N)    { BigUInt v = pi_fixed(N); v.div_small_mutable(3);   return v; }
inline BigUInt sixth_pi_fixed(std::size_t N)    { BigUInt v = pi_fixed(N); v.div_small_mutable(6);   return v; }
inline BigUInt pi_180_fixed(std::size_t N)      { BigUInt v = pi_fixed(N); v.div_small_mutable(180); return v; }
inline BigUInt inv_pi_180_fixed(std::size_t N)  { BigUInt v = inv_pi_fixed(N); v.mul_small_mutable(180); return v; }

inline BigUInt ln2_fixed(std::size_t N) {
    BigUInt a = atanh_inv_fixed(31, N);  a.mul_small_mutable(14);
    BigUInt b = atanh_inv_fixed(49, N);  b.mul_small_mutable(10);
    BigUInt c = atanh_inv_fixed(161, N); c.mul_small_mutable(6);
    a.add_mutable(b);
    a.add_mutable(c);
    return a;
}

inline BigUInt ln10_fixed(std::size_t N) {
    BigUInt a = atanh_inv_fixed(31, N);  a.mul_small_mutable(46);
    BigUInt b = atanh_inv_fixed(49, N);  b.mul_small_mutable(34);
    BigUInt c = atanh_inv_fixed(161, N); c.mul_small_mutable(20);
    a.add_mutable(b);
    a.add_mutable(c);
    return a;
}

inline BigUInt inv_ln2_fixed(std::size_t N)  { return fx_recip(ln2_fixed(N), N); }
inline BigUInt inv_ln10_fixed(std::size_t N) { return fx_recip(ln10_fixed(N), N); }

inline BigUInt e_fixed(std::size_t N) {
    BigUInt term = unit(N);
    BigUInt sum  = term;

    for (std::uint64_t k = 1; ; ++k) {
        term.add_small_mutable(k / 2);                     
        term.div_small_mutable(k);
        if (term.is_zero()) break;
        sum.add_mutable(term);
    }

    return sum;
}

inline BigUInt inv_e_fixed(std::size_t N) { return fx_recip(e_fixed(N), N); }

inline BigUInt sqrt_int_fixed(std::uint64_t n, std::size_t N) {
    BigUInt v = unit(2 * N);
    v.mul_small_mutable(n);
    return isqrt(v);                                       
}

inline BigUInt inv_sqrt_int_fixed(std::uint64_t n, std::size_t N) {
    BigUInt v = unit(2 * N);
    v.div_small_mutable(n);
    return isqrt(v);                                       
}

inline BigUInt sqrt2_fixed(std::size_t N) { return sqrt_int_fixed(2, N); }
inline BigUInt sqrt3_fixed(std::size_t N) { return sqrt_int_fixed(3, N); }
inline BigUInt sqrt5_fixed(std::size_t N) { return sqrt_int_fixed(5, N); }

inline BigUInt inv_sqrt2_fixed(std::size_t N) { return inv_sqrt_int_fixed(2, N); }
inline BigUInt inv_sqrt3_fixed(std::size_t N) { return inv_sqrt_int_fixed(3, N); }
inline BigUInt inv_sqrt5_fixed(std::size_t N) { return inv_sqrt_int_fixed(5, N); }

inline BigUInt phi_fixed(std::size_t N) {
    BigUInt v = sqrt5_fixed(N);
    v.add_mutable(unit(N));
    v.shift_right_mutable(1);
    return v;
}

inline BigUInt inv_phi_fixed(std::size_t N) {
    BigUInt v = sqrt5_fixed(N);
    v.sub_mutable(unit(N));
    v.shift_right_mutable(1);
    return v;
}

inline BigUInt sqrt_pi_fixed(std::size_t N) {
    BigUInt v = pi_fixed(N);
    v.shift_left_mutable(N);                               
    return isqrt(v);
}

inline BigUInt inv_sqrt_pi_fixed(std::size_t N) { return fx_recip(sqrt_pi_fixed(N), N); }

inline BigUInt zeta2_fixed(std::size_t N) {                
    const BigUInt p = pi_fixed(N);
    BigUInt v = fx_mul(p, p, N);
    v.div_small_mutable(6);
    return v;
}

inline BigUInt apery_fixed(std::size_t N) {                
    BigUInt term = unit(N);
    term.div_small_mutable(2);
    BigUInt pos = term;
    BigUInt neg = BigUInt::zero();

    for (std::uint64_t k = 1; ; ++k) {
        term.mul_small_mutable(k * k * k);
        term.div_small_mutable(2);
        term.div_small_mutable(k + 1);
        term.div_small_mutable(k + 1);
        term.div_small_mutable(2 * k + 1);
        if (term.is_zero()) break;
        if (k & 1u) neg.add_mutable(term); else pos.add_mutable(term);
    }

    pos.sub_mutable(neg);
    pos.mul_small_mutable(5);
    pos.shift_right_mutable(1);
    return pos;
}

inline bool round_is_safe(const BigUInt& mag, std::size_t prec, std::size_t err_bits) {
    const std::size_t L = mag.bit_length();
    if (L <= prec) return true;
    const std::size_t drop = L - prec;
    if (drop <= err_bits + 2) return false;
    BigUInt mask = unit(drop);
    mask.sub_small_mutable(1);
    BigUInt low = mag;
    low.and_mutable(mask);
    const BigUInt eps  = unit(err_bits);
    const BigUInt half = unit(drop - 1);
    if (low < eps) return false;                          
    BigUInt top = mask;
    top.sub_mutable(eps);
    if (top < low) return false;                          

    if (low < half) { BigUInt d = half; d.sub_mutable(low);  if (d < eps) return false; }
    else            { BigUInt d = low;  d.sub_mutable(half); if (d < eps) return false; }

    return true;
}

typedef BigUInt (*fixed_fn)(std::size_t);

struct fixed_cache {
    BigUInt     value;
    std::size_t bits;
    fixed_cache() : value(BigUInt::zero()), bits(0) {}
};

inline BigFloat materialize(fixed_fn gen, fixed_cache& cache, bool neg, const BigFloatContext& ctx) {
    const std::size_t slack = 32;                          
    const std::size_t err   = 6;                           
    std::size_t work = ctx.precision + 32;

    for (;;) {
        if (cache.bits < work) {
            BigUInt v = gen(work + slack);
            v.shift_right_mutable(slack);
            if (v.is_undefined()) return BigFloat::undefined();
            cache.value = std::move(v);
            cache.bits  = work;
        }

        if (round_is_safe(cache.value, ctx.precision, err)) {
            const BigFloat x(cache.value, neg, -static_cast<std::int64_t>(cache.bits));
            return x.rounded(ctx);
        }

        if (work >= BigFloatContext::max_prec / 2) {       
            const BigFloat x(cache.value, neg, -static_cast<std::int64_t>(cache.bits));
            return x.rounded(ctx);
        }

        work *= 2;
    }
}

} // namespace bfdetail

#define FIZMO_BIGFLOAT_CONST(NAME)                                                    \
    inline BigFloat NAME(const BigFloatContext& ctx) {                                \
        static thread_local bfdetail::fixed_cache cache;                              \
        return bfdetail::materialize(&bfdetail::NAME##_fixed, cache, false, ctx);     \
    }                                                                                 \
    inline BigFloat NAME() { return NAME(BigFloatContext::current()); }

#define FIZMO_BIGFLOAT_CONST_SCALED(NAME, BASE, K)                                    \
    inline BigFloat NAME(const BigFloatContext& ctx) { return BASE(ctx).scaled_pow2(K); } \
    inline BigFloat NAME() { return NAME(BigFloatContext::current()); }

#define FIZMO_BIGFLOAT_CONST_NEG(NAME, GEN)                                           \
    inline BigFloat NAME(const BigFloatContext& ctx) {                                \
        static thread_local bfdetail::fixed_cache cache;                              \
        return bfdetail::materialize(&bfdetail::GEN##_fixed, cache, true, ctx);       \
    }                                                                                 \
    inline BigFloat NAME() { return NAME(BigFloatContext::current()); }

FIZMO_BIGFLOAT_CONST(pi)
FIZMO_BIGFLOAT_CONST(inv_pi)
FIZMO_BIGFLOAT_CONST(third_pi)
FIZMO_BIGFLOAT_CONST(sixth_pi)
FIZMO_BIGFLOAT_CONST(pi_180)
FIZMO_BIGFLOAT_CONST(inv_pi_180)
FIZMO_BIGFLOAT_CONST(ln2)
FIZMO_BIGFLOAT_CONST(ln10)
FIZMO_BIGFLOAT_CONST(inv_ln2)
FIZMO_BIGFLOAT_CONST(inv_ln10)
FIZMO_BIGFLOAT_CONST(e)
FIZMO_BIGFLOAT_CONST(inv_e)
FIZMO_BIGFLOAT_CONST(phi)
FIZMO_BIGFLOAT_CONST_NEG(psi, inv_phi)
FIZMO_BIGFLOAT_CONST(inv_phi)
FIZMO_BIGFLOAT_CONST(sqrt2)
FIZMO_BIGFLOAT_CONST(sqrt3)
FIZMO_BIGFLOAT_CONST(sqrt5)
FIZMO_BIGFLOAT_CONST(inv_sqrt2)
FIZMO_BIGFLOAT_CONST(inv_sqrt3)
FIZMO_BIGFLOAT_CONST(inv_sqrt5)
FIZMO_BIGFLOAT_CONST(sqrt_pi)
FIZMO_BIGFLOAT_CONST(inv_sqrt_pi)
FIZMO_BIGFLOAT_CONST(zeta2)
FIZMO_BIGFLOAT_CONST(apery)

FIZMO_BIGFLOAT_CONST_SCALED(two_pi,            pi,           1)
FIZMO_BIGFLOAT_CONST_SCALED(half_pi,           pi,          -1)
FIZMO_BIGFLOAT_CONST_SCALED(quarter_pi,        pi,          -2)
FIZMO_BIGFLOAT_CONST_SCALED(two_inv_pi,        inv_pi,       1)
FIZMO_BIGFLOAT_CONST_SCALED(inv_two_pi,        inv_pi,      -1)
FIZMO_BIGFLOAT_CONST_SCALED(two_inv_sqrt_pi,   inv_sqrt_pi,  1)

#undef FIZMO_BIGFLOAT_CONST
#undef FIZMO_BIGFLOAT_CONST_SCALED
#undef FIZMO_BIGFLOAT_CONST_NEG

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_CONSTANTS_HPP