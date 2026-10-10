#ifndef FIZMO_MULTIPRECISION_INTEGER_MUL_HPP
#define FIZMO_MULTIPRECISION_INTEGER_MUL_HPP

#include <cstdint>
#include <cstddef>
#include <vector>

#include "integer.hpp"
#include "../limb_kernels.hpp"

#ifndef FIZMO_MP_MAX_MUL_STACK_LIMBS
#define FIZMO_MP_MAX_MUL_STACK_LIMBS 4096   
#endif

namespace fizmo {
namespace multiprecision {
namespace imdetail {

template <std::size_t Bits> struct limb_io;

template <> struct limb_io<64> {
    static constexpr std::size_t count = 1;
    static void      store(const umag<64>& x, std::uint64_t* p) noexcept { p[0] = x.to_u64(); }
    static umag<64>  load (const std::uint64_t* p)              noexcept { return umag<64>(p[0]); }
};

template <std::size_t Bits> struct limb_io {
    static constexpr std::size_t count = Bits / 64;
    typedef limb_io<Bits / 2> half_io;

    static void store(const umag<Bits>& x, std::uint64_t* p) noexcept {
        half_io::store(x.get_low_bits(),  p);
        half_io::store(x.get_high_bits(), p + count / 2);
    }

    static umag<Bits> load(const std::uint64_t* p) noexcept {
        return umag<Bits>::from_bits(half_io::load(p), half_io::load(p + count / 2));
    }
};

template <std::size_t Bits> constexpr std::size_t limb_io<Bits>::count;

constexpr std::size_t max2(std::size_t a, std::size_t b) noexcept { return a > b ? a : b; }
constexpr std::size_t max3(std::size_t a, std::size_t b, std::size_t c) noexcept { return max2(max2(a, b), c); }

constexpr std::size_t scratch_bound(std::size_t n) noexcept {
    return (n <= 16) ? std::size_t(1024)
         : max3(12 * ((n + 1) / 2) +  4 + scratch_bound((n + 1) / 2),
                30 * ((n + 2) / 3) + 32 + scratch_bound((n + 2) / 3 + 1),
                54 * ((n + 3) / 4) + 56 + scratch_bound((n + 3) / 4 + 1));
}

template <std::size_t N, bool OnStack = (N <= std::size_t(FIZMO_MP_MAX_MUL_STACK_LIMBS))>
struct scratch_buf {
    std::uint64_t  buf[N];
    std::uint64_t* ptr() noexcept { return buf; }
};

template <std::size_t N>
struct scratch_buf<N, false> {
    std::vector<std::uint64_t> v;
    scratch_buf() : v(N) {}
    std::uint64_t* ptr() noexcept { return &v[0]; }
};

template <std::size_t W, bool HasLimbs = (W >= 64 && W % 64 == 0)>
struct mul_engine;

template <std::size_t W>
struct mul_engine<W, false> {
    static void basecase (const umag<W>& a, const umag<W>& b, umag<W>& h, umag<W>& l) noexcept { wide_mul<W>::mul(a, b, h, l); }
    static void karatsuba(const umag<W>& a, const umag<W>& b, umag<W>& h, umag<W>& l) noexcept { wide_mul<W>::mul(a, b, h, l); }
    static void toom3    (const umag<W>& a, const umag<W>& b, umag<W>& h, umag<W>& l) noexcept { wide_mul<W>::mul(a, b, h, l); }
    static void toom4    (const umag<W>& a, const umag<W>& b, umag<W>& h, umag<W>& l) noexcept { wide_mul<W>::mul(a, b, h, l); }
    static void select   (const umag<W>& a, const umag<W>& b, umag<W>& h, umag<W>& l) noexcept { wide_mul<W>::mul(a, b, h, l); }
};

template <std::size_t W>
struct mul_engine<W, true> {
    static constexpr std::size_t n     = W / 64;
    static constexpr std::size_t ws    = scratch_bound(n);
    static constexpr std::size_t total = 4 * n + ws;  

    typedef limb_io<W> io;

    static void unpack(std::uint64_t* p, const umag<W>& a, const umag<W>& b) noexcept {
        io::store(a, p);
        io::store(b, p + n);
    }

    static void pack(const std::uint64_t* r, umag<W>& high, umag<W>& low) noexcept {
        low  = io::load(r);
        high = io::load(r + n);
    }

    static void basecase(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const std::size_t N = n;
        scratch_buf<total> s;
        std::uint64_t* p = s.ptr();
        unpack(p, a, b);
        mdetail::mul_basecase(p + 2 * N, p, N, p + N, N);
        pack(p + 2 * N, high, low);
    }

    static void karatsuba(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const std::size_t N = n;
        if (N < 2) { basecase(a, b, high, low); return; }
        scratch_buf<total> s;
        std::uint64_t* p = s.ptr();
        unpack(p, a, b);
        mdetail::mul_karatsuba(p + 2 * N, p, p + N, N, p + 4 * N);
        pack(p + 2 * N, high, low);
    }

    static void toom3(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const std::size_t N = n;
        if (N < 5) { karatsuba(a, b, high, low); return; }
        scratch_buf<total> s;
        std::uint64_t* p = s.ptr();
        unpack(p, a, b);
        mdetail::mul_toom3(p + 2 * N, p, p + N, N, p + 4 * N);
        pack(p + 2 * N, high, low);
    }

    static void toom4(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const std::size_t N = n;
        if (N < 13) { toom3(a, b, high, low); return; }
        scratch_buf<total> s;
        std::uint64_t* p = s.ptr();
        unpack(p, a, b);
        mdetail::mul_toom4(p + 2 * N, p, p + N, N, p + 4 * N);
        pack(p + 2 * N, high, low);
    }

    static void select(const umag<W>& a, const umag<W>& b, umag<W>& high, umag<W>& low) noexcept {
        const std::size_t N = n;
        if (a.is_zero() || b.is_zero()) { high = umag<W>(); low = umag<W>(); return; }

        if (N < mdetail::mul_threshold::karatsuba) { basecase (a, b, high, low); return; }
        if (N < mdetail::mul_threshold::toom3)     { karatsuba(a, b, high, low); return; }
        if (N < mdetail::mul_threshold::toom4)     { toom3    (a, b, high, low); return; }

        if (N >= mdetail::mul_threshold::ntt) {
            scratch_buf<total> s;
            std::uint64_t* p = s.ptr();
            unpack(p, a, b);
            if (mdetail::ntt_mul(p + 2 * N, p, N, p + N, N)) { pack(p + 2 * N, high, low); return; }
        }

        toom4(a, b, high, low);
    }
};

template <std::size_t W> constexpr std::size_t mul_engine<W, true>::n;
template <std::size_t W> constexpr std::size_t mul_engine<W, true>::ws;
template <std::size_t W> constexpr std::size_t mul_engine<W, true>::total;

} // namespace imdetail

template <std::size_t W> void mul_basecase (const umag<W>& a, const umag<W>& b, umag<W>& hi, umag<W>& lo) noexcept { imdetail::mul_engine<W>::basecase (a, b, hi, lo); }
template <std::size_t W> void mul_karatsuba(const umag<W>& a, const umag<W>& b, umag<W>& hi, umag<W>& lo) noexcept { imdetail::mul_engine<W>::karatsuba(a, b, hi, lo); }
template <std::size_t W> void mul_toom3    (const umag<W>& a, const umag<W>& b, umag<W>& hi, umag<W>& lo) noexcept { imdetail::mul_engine<W>::toom3    (a, b, hi, lo); }
template <std::size_t W> void mul_toom4    (const umag<W>& a, const umag<W>& b, umag<W>& hi, umag<W>& lo) noexcept { imdetail::mul_engine<W>::toom4    (a, b, hi, lo); }
template <std::size_t W> void mul_auto     (const umag<W>& a, const umag<W>& b, umag<W>& hi, umag<W>& lo) noexcept { imdetail::mul_engine<W>::select   (a, b, hi, lo); }

template <std::size_t W>
umag<W * 2> mul_wide(const umag<W>& a, const umag<W>& b) noexcept {
    umag<W> hi, lo;
    mul_auto(a, b, hi, lo);
    return combine_product<W>(lo, hi);
}

template <std::size_t W>
umag<W> mul_low(const umag<W>& a, const umag<W>& b) noexcept {
    umag<W> hi, lo;
    mul_auto(a, b, hi, lo);
    return lo;
}

template <std::size_t Bits>
integer<Bits, sign::is_unsigned> mul_low(const integer<Bits, sign::is_unsigned>& a, const integer<Bits, sign::is_unsigned>& b) noexcept {
    umag<Bits> hi, lo;
    mul_auto(a.magnitude(), b.magnitude(), hi, lo);
    return integer<Bits, sign::is_unsigned>::from_magnitude(lo);
}

template <std::size_t Bits>
integer<Bits * 2, sign::is_unsigned> mul_wide(const integer<Bits, sign::is_unsigned>& a, const integer<Bits, sign::is_unsigned>& b) noexcept {
    umag<Bits> hi, lo;
    mul_auto(a.magnitude(), b.magnitude(), hi, lo);
    return integer<Bits * 2, sign::is_unsigned>::from_magnitude(combine_product<Bits>(lo, hi));
}

template <std::size_t Bits>
integer<Bits, sign::is_signed> mul_low(const integer<Bits, sign::is_signed>& a, const integer<Bits, sign::is_signed>& b) noexcept {
    umag<Bits> hi, lo;
    mul_auto(a.magnitude(), b.magnitude(), hi, lo);
    return integer<Bits, sign::is_signed>::from_magnitude(lo, a.is_negative() != b.is_negative());
}

template <std::size_t Bits>
integer<Bits * 2, sign::is_signed> mul_wide(const integer<Bits, sign::is_signed>& a, const integer<Bits, sign::is_signed>& b) noexcept {
    umag<Bits> hi, lo;
    mul_auto(a.magnitude(), b.magnitude(), hi, lo);
    return integer<Bits * 2, sign::is_signed>::from_magnitude(combine_product<Bits>(lo, hi), a.is_negative() != b.is_negative());
}

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_INTEGER_MUL_HPP