#ifndef FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_SELECT_HPP
#define FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_SELECT_HPP

#include "big_uint_mult_detail.hpp"

namespace fizmo {
namespace multiprecision {

namespace mulsel {

template <class Fn>
inline BigUInt run_split(const BigUInt& a, const BigUInt& b, std::size_t floor_n, std::size_t scratch, Fn fn) {
    const std::size_t an = a.limb_count();
    const std::size_t bn = b.limb_count();
    const std::size_t rn = an + bn;
    std::size_t n = (an > bn) ? an : bn;
    if (n < floor_n) n = floor_n;
    budetail::limb_vec pa, pb, r, ws;
    if (!pa.resize_uninit(n))    return BigUInt::undefined();
    if (!pb.resize_uninit(n))    return BigUInt::undefined();
    if (!r.resize_uninit(2 * n)) return BigUInt::undefined();
    if (scratch != 0 && !ws.resize_uninit(scratch)) return BigUInt::undefined();
    mdetail::copy_pad(pa.data(), a.limbs(), an, n);
    mdetail::copy_pad(pb.data(), b.limbs(), bn, n);
    fn(r.data(), pa.data(), pb.data(), n, ws.data());
    r.set_size_unchecked(rn);
    return BigUInt(std::move(r));
}

inline bool trivial(const BigUInt& a, const BigUInt& b, BigUInt& out) {
    if (a.is_undefined() || b.is_undefined()) { out = BigUInt::undefined(); return true; }
    if (a.is_zero() || b.is_zero())           { out = BigUInt::zero();      return true; }
    if (a.limb_count() > BigUInt::max_limbs - b.limb_count()) { out = BigUInt::undefined(); return true; }
    return false;
}

} // namespace mulsel

inline BigUInt multiply_basecase(const BigUInt& a, const BigUInt& b) {
    BigUInt early;
    if (mulsel::trivial(a, b, early)) return early;
    const std::size_t an = a.limb_count();
    const std::size_t bn = b.limb_count();
    budetail::limb_vec r;
    if (!r.resize_uninit(an + bn)) return BigUInt::undefined();

    if (an >= bn) mdetail::mul_basecase(r.data(), a.limbs(), an, b.limbs(), bn);
    else          mdetail::mul_basecase(r.data(), b.limbs(), bn, a.limbs(), an);

    return BigUInt(std::move(r));
}

inline BigUInt multiply_karatsuba(const BigUInt& a, const BigUInt& b) {
    BigUInt early;
    if (mulsel::trivial(a, b, early)) return early;
    std::size_t n = a.limb_count() > b.limb_count() ? a.limb_count() : b.limb_count();
    if (n < 2) n = 2;
    const std::size_t k = (n + 1) / 2;
    return mulsel::run_split(a, b, 2, 12 * k + 4 + mdetail::mul_n_top_scratch(k), [](std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t nn, std::uint64_t* ws) { mdetail::mul_karatsuba(rp, ap, bp, nn, ws); });
}

inline BigUInt multiply_toom3(const BigUInt& a, const BigUInt& b) {
    BigUInt early;
    if (mulsel::trivial(a, b, early)) return early;
    std::size_t n = a.limb_count() > b.limb_count() ? a.limb_count() : b.limb_count();
    if (n < 5) n = 5;
    const std::size_t k = (n + 2) / 3;
    return mulsel::run_split(a, b, 5, 30 * k + 32 + mdetail::mul_n_top_scratch(k + 1), [](std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t nn, std::uint64_t* ws) { mdetail::mul_toom3(rp, ap, bp, nn, ws); });
}

inline BigUInt multiply_toom4(const BigUInt& a, const BigUInt& b) {
    BigUInt early;
    if (mulsel::trivial(a, b, early)) return early;
    std::size_t n = a.limb_count() > b.limb_count() ? a.limb_count() : b.limb_count();
    if (n < 13) n = 13;
    const std::size_t k = (n + 3) / 4;
    return mulsel::run_split(a, b, 13, 54 * k + 56 + mdetail::mul_n_top_scratch(k + 1), [](std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t nn, std::uint64_t* ws) { mdetail::mul_toom4(rp, ap, bp, nn, ws); });
}

inline BigUInt multiply_ntt(const BigUInt& a, const BigUInt& b) {
    BigUInt early;
    if (mulsel::trivial(a, b, early)) return early;
    const std::size_t an = a.limb_count();
    const std::size_t bn = b.limb_count();
    budetail::limb_vec r;
    if (!r.resize_uninit(an + bn)) return BigUInt::undefined();
    if (!mdetail::ntt_mul(r.data(), a.limbs(), an, b.limbs(), bn)) return BigUInt::undefined();
    return BigUInt(std::move(r));
}

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_MUL_SELECT_HPP