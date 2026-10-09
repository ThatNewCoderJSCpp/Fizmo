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

BigUInt isqrt(const BigUInt& v);

BigUInt atan_inv_fixed(std::uint64_t q, std::size_t N);

BigUInt atanh_inv_fixed(std::uint64_t q, std::size_t N);

inline BigUInt fx_recip(const BigUInt& v, std::size_t N) { 
    BigUInt num = unit(2 * N);
    return num / v;
}

inline BigUInt fx_mul(const BigUInt& a, const BigUInt& b, std::size_t N) {
    BigUInt p = a * b;
    p.shift_right_mutable(N);
    return p;
}

BigUInt pi_fixed(std::size_t N);

inline BigUInt inv_pi_fixed(std::size_t N)      { return fx_recip(pi_fixed(N), N); }
inline BigUInt third_pi_fixed(std::size_t N)    { BigUInt v = pi_fixed(N); v.div_small_mutable(3);   return v; }
inline BigUInt sixth_pi_fixed(std::size_t N)    { BigUInt v = pi_fixed(N); v.div_small_mutable(6);   return v; }
inline BigUInt pi_180_fixed(std::size_t N)      { BigUInt v = pi_fixed(N); v.div_small_mutable(180); return v; }
inline BigUInt inv_pi_180_fixed(std::size_t N)  { BigUInt v = inv_pi_fixed(N); v.mul_small_mutable(180); return v; }

BigUInt ln2_fixed(std::size_t N);

BigUInt ln10_fixed(std::size_t N);

inline BigUInt inv_ln2_fixed(std::size_t N)  { return fx_recip(ln2_fixed(N), N); }
inline BigUInt inv_ln10_fixed(std::size_t N) { return fx_recip(ln10_fixed(N), N); }

BigUInt e_fixed(std::size_t N);

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

BigUInt apery_fixed(std::size_t N);

bool round_is_safe(const BigUInt& mag, std::size_t prec, std::size_t err_bits);

typedef BigUInt (*fixed_fn)(std::size_t);

struct fixed_cache {
    BigUInt     value;
    std::size_t bits;
    fixed_cache() : value(BigUInt::zero()), bits(0) {}
};

BigFloat materialize(fixed_fn gen, fixed_cache& cache, bool neg, const BigFloatContext& ctx);

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