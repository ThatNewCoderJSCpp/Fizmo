#ifndef FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_HPP
#define FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_HPP

#include <cstdint>
#include <cstddef>
#include <utility>

#include "../big_uint.hpp"
#include "../../limb_kernels.hpp"
#include "big_uint_ntt.hpp"

namespace fizmo {
namespace multiprecision {
namespace mdetail {

inline std::size_t mul_n_top_scratch(std::size_t n) {
    return (n >= mul_threshold::ntt) ? 0 : mul_n_scratch(n);
}

inline void mul_n_top(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws) {
    if (n < mul_threshold::karatsuba) {
        if (n == 1)  { rp[1] = mul_1(rp, ap, 1, bp[0]); return; }
        if (n == 2)  { mul_2x2(rp, ap, bp);             return; }
        mul_basecase(rp, ap, n, bp, n);
        return;
    }

    if (n < mul_threshold::toom3) { mul_karatsuba(rp, ap, bp, n, ws); return; }
    if (n < mul_threshold::toom4) { mul_toom3(rp, ap, bp, n, ws);     return; }
    if (n < mul_threshold::ntt)   { mul_toom4(rp, ap, bp, n, ws);     return; }
    if (ntt_mul(rp, ap, n, bp, n)) return;
    budetail::limb_vec big;
    if (!big.resize_uninit(toom4_scratch(n))) { zero_limbs(rp, 2 * n); return; }
    mul_toom4(rp, ap, bp, n, big.data());
}

inline void mul_limbs(std::uint64_t* rp, const std::uint64_t* ap, std::size_t an, const std::uint64_t* bp, std::size_t bn) {
    if (an < bn) {
        const std::uint64_t* tp = ap; ap = bp; bp = tp;
        const std::size_t    tn = an; an = bn; bn = tn;
    }

    if (bn == 1) { rp[an] = mul_1(rp, ap, an, bp[0]); return; }

    if (bn < mul_threshold::karatsuba) {
        if (an == 2 && bn == 2) { mul_2x2(rp, ap, bp); return; }
        mul_basecase(rp, ap, an, bp, bn);
        return;
    }

    if (an == bn) {
        budetail::limb_vec scr;
        const std::size_t need = mul_n_top_scratch(an);
        if (need != 0 && !scr.resize_uninit(need)) { zero_limbs(rp, 2 * an); return; }
        mul_n_top(rp, ap, bp, an, scr.data());
        return;
    }

    if (bn >= mul_threshold::ntt && ntt_mul(rp, ap, an, bp, bn)) return;
    budetail::limb_vec scr, tmp;
    const std::size_t need = mul_n_top_scratch(bn);
    if (need != 0 && !scr.resize_uninit(need)) { zero_limbs(rp, an + bn); return; }
    if (!tmp.resize_uninit(2 * bn))            { zero_limbs(rp, an + bn); return; }
    std::uint64_t* t = tmp.data();
    mul_n_top(rp, ap, bp, bn, scr.data());
    std::size_t off = bn;

    while (an - off >= bn) {
        mul_n_top(t, ap + off, bp, bn, scr.data());
        const std::uint64_t c = add_n(rp + off, rp + off, t, bn);
        copy_limbs(rp + off + bn, t + bn, bn);
        add_1(rp + off + bn, rp + off + bn, bn, c);
        off += bn;
    }

    const std::size_t m = an - off;

    if (m != 0) {
        budetail::limb_vec tail;
        if (!tail.resize_uninit(m + bn)) return;
        mul_limbs(tail.data(), bp, bn, ap + off, m);    
        const std::uint64_t c = add_n(rp + off, rp + off, tail.data(), bn);
        copy_limbs(rp + off + bn, tail.data() + bn, m);
        add_1(rp + off + bn, rp + off + bn, m, c);
    }
}

inline BigUInt mul(const BigUInt& a, const BigUInt& b) {
    if (a.is_undefined() || b.is_undefined()) return BigUInt::undefined();
    if (a.is_zero() || b.is_zero())           return BigUInt::zero();
    const std::size_t an = a.limb_count();
    const std::size_t bn = b.limb_count();
    if (an > BigUInt::max_limbs - bn) return BigUInt::undefined();
    budetail::limb_vec r;
    if (!r.resize_uninit(an + bn)) return BigUInt::undefined();
    mul_limbs(r.data(), a.limbs(), an, b.limbs(), bn);
    return BigUInt(std::move(r));
}

inline BigUInt sqr(const BigUInt& a) { return mul(a, a); }

} // namespace mdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_HPP