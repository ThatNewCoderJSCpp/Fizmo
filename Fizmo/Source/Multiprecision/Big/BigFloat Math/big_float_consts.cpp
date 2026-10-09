#include "fizmo_library.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {
namespace bfdetail {

BigUInt isqrt(const BigUInt& v) {                  
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

BigUInt atan_inv_fixed(std::uint64_t q, std::size_t N) {
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

BigUInt atanh_inv_fixed(std::uint64_t q, std::size_t N) {
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

BigUInt pi_fixed(std::size_t N) {                   
    BigUInt a = atan_inv_fixed(5, N);
    a.mul_small_mutable(16);
    BigUInt b = atan_inv_fixed(239, N);
    b.mul_small_mutable(4);
    a.sub_mutable(b);
    return a;
}

BigUInt ln2_fixed(std::size_t N) {
    BigUInt a = atanh_inv_fixed(31, N);  a.mul_small_mutable(14);
    BigUInt b = atanh_inv_fixed(49, N);  b.mul_small_mutable(10);
    BigUInt c = atanh_inv_fixed(161, N); c.mul_small_mutable(6);
    a.add_mutable(b);
    a.add_mutable(c);
    return a;
}

BigUInt ln10_fixed(std::size_t N) {
    BigUInt a = atanh_inv_fixed(31, N);  a.mul_small_mutable(46);
    BigUInt b = atanh_inv_fixed(49, N);  b.mul_small_mutable(34);
    BigUInt c = atanh_inv_fixed(161, N); c.mul_small_mutable(20);
    a.add_mutable(b);
    a.add_mutable(c);
    return a;
}

BigUInt e_fixed(std::size_t N) {
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

BigUInt apery_fixed(std::size_t N) {                
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

bool round_is_safe(const BigUInt& mag, std::size_t prec, std::size_t err_bits) {
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

BigFloat materialize(fixed_fn gen, fixed_cache& cache, bool neg, const BigFloatContext& ctx) {
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
} // namespace constants
} // namespace multiprecision
} // namespace fizmo
