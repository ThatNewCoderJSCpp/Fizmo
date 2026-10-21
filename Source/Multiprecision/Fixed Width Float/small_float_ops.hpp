#ifndef FIZMO_MULTIPRECISION_SMALL_FLOAT_OPS_HPP
#define FIZMO_MULTIPRECISION_SMALL_FLOAT_OPS_HPP

#include <cstddef>
#include <cstdint>

#ifndef FIZMO_SMALL_FLOAT_FAST
#define FIZMO_SMALL_FLOAT_FAST 1
#endif

namespace fizmo {
namespace multiprecision {
namespace fdetail {

template <std::size_t TB, std::size_t MB>
struct small_format {
    static constexpr bool value = FIZMO_SMALL_FLOAT_FAST && TB <= 64 && MB >= 2 && MB <= 56 && TB - MB <= 30;
};

struct sf_value {
    bool neg;
    std::uint64_t bits;
};

namespace sfmath {

constexpr int bit_length(std::uint64_t x) noexcept {
#if defined(__GNUC__) || defined(__clang__)
    return x == 0 ? 0 : 64 - __builtin_clzll(x);
#else
    int n = 0;
    if (x >> 32) { x >>= 32; n += 32; }
    if (x >> 16) { x >>= 16; n += 16; }
    if (x >> 8)  { x >>= 8;  n += 8; }
    if (x >> 4)  { x >>= 4;  n += 4; }
    if (x >> 2)  { x >>= 2;  n += 2; }
    if (x >> 1)  { x >>= 1;  n += 1; }
    return n + static_cast<int>(x);
#endif
}

constexpr std::uint64_t low_mask(int n) noexcept {
    return n <= 0 ? 0ull : n >= 64 ? ~0ull : ((std::uint64_t(1) << n) - 1u);
}

constexpr void mul_wide(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) noexcept {
#if defined(__SIZEOF_INT128__)
    const unsigned __int128 p = static_cast<unsigned __int128>(a) * b;
    hi = static_cast<std::uint64_t>(p >> 64);
    lo = static_cast<std::uint64_t>(p);
#else
    const std::uint64_t a0 = a & 0xffffffffu, a1 = a >> 32;
    const std::uint64_t b0 = b & 0xffffffffu, b1 = b >> 32;
    const std::uint64_t p00 = a0 * b0;
    const std::uint64_t p01 = a0 * b1;
    const std::uint64_t p10 = a1 * b0;
    const std::uint64_t p11 = a1 * b1;
    const std::uint64_t mid = (p00 >> 32) + (p01 & 0xffffffffu) + (p10 & 0xffffffffu);
    lo = (mid << 32) | (p00 & 0xffffffffu);
    hi = p11 + (p01 >> 32) + (p10 >> 32) + (mid >> 32);
#endif
}

constexpr std::uint64_t div_wide(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& rem) noexcept {
#if defined(__SIZEOF_INT128__)
    const unsigned __int128 n = (static_cast<unsigned __int128>(hi) << 64) | lo;
    const unsigned __int128 q = n / d;
    rem = static_cast<std::uint64_t>(n - q * d);
    return static_cast<std::uint64_t>(q);
#else
    const std::uint64_t b = std::uint64_t(1) << 32;
    const int s = 64 - bit_length(d);
    d <<= s;
    const std::uint64_t vn1 = d >> 32;
    const std::uint64_t vn0 = d & 0xffffffffu;
    const std::uint64_t un32 = s == 0 ? hi : ((hi << s) | (lo >> (64 - s)));
    const std::uint64_t un10 = lo << s;
    const std::uint64_t un1 = un10 >> 32;
    const std::uint64_t un0 = un10 & 0xffffffffu;
    std::uint64_t q1 = un32 / vn1;
    std::uint64_t rhat = un32 - q1 * vn1;
    while (q1 >= b || q1 * vn0 > b * rhat + un1) {
        --q1;
        rhat += vn1;
        if (rhat >= b) break;
    }
    const std::uint64_t un21 = un32 * b + un1 - q1 * d;
    std::uint64_t q0 = un21 / vn1;
    rhat = un21 - q0 * vn1;
    while (q0 >= b || q0 * vn0 > b * rhat + un0) {
        --q0;
        rhat += vn1;
        if (rhat >= b) break;
    }
    rem = (un21 * b + un0 - q0 * d) >> s;
    return q1 * b + q0;
#endif
}

} // namespace sfmath

template <std::size_t TB, std::size_t MB>
struct small_ops {
    using u64 = std::uint64_t;

    static constexpr int        mb        = static_cast<int>(MB);
    static constexpr int        precision = mb + 1;
    static constexpr int        eb        = static_cast<int>(TB - MB);
    static constexpr long long  bias      = (1ll << (eb - 1)) - 1;
    static constexpr u64        max_biased = (u64(1) << eb) - 1u;
    static constexpr u64        frac_mask = (u64(1) << MB) - 1u;
    static constexpr u64        hidden    = u64(1) << MB;
    static constexpr u64        inf_bits  = max_biased << MB;
    static constexpr u64        nan_bits  = inf_bits | (u64(1) << (MB - 1)) | 1u;
    static constexpr u64        undefined_bits = inf_bits | (u64(1) << (MB - 1)) | 2u;
    static constexpr u64        indeterminate_bits = inf_bits | (u64(1) << (MB - 1)) | 3u;
    static constexpr long long  emin      = 1 - bias;
    static constexpr int        guard     = 62 - precision;

    static constexpr bool is_zero(u64 b) noexcept { return b == 0; }
    static constexpr bool is_infinite(u64 b) noexcept { return b == inf_bits; }
    static constexpr bool is_nan(bool n, u64 b) noexcept { return !n && b == nan_bits; }
    static constexpr bool is_undefined(bool n, u64 b) noexcept { return !n && b == undefined_bits; }
    static constexpr bool is_indeterminate(bool n, u64 b) noexcept { return !n && b == indeterminate_bits; }
    static constexpr bool is_unordered(bool n, u64 b) noexcept { return is_nan(n, b) || is_undefined(n, b) || is_indeterminate(n, b); }
    static constexpr bool is_finite(bool n, u64 b) noexcept { return !is_unordered(n, b) && !is_infinite(b); }

    static constexpr sf_value unordered(bool an, u64 a, bool bn, u64 b) noexcept {
        return (is_undefined(an, a) || is_undefined(bn, b))         ? sf_value{ false, undefined_bits }
             : (is_indeterminate(an, a) || is_indeterminate(bn, b)) ? sf_value{ false, indeterminate_bits }
             : sf_value{ false, nan_bits };
    }

    static constexpr void unpack(u64 b, u64& m, long long& e) noexcept {
        const u64 be = b >> MB;
        if (be == 0) {
            m = b & frac_mask;
            e = emin - mb;
            const int lz = precision - sfmath::bit_length(m);
            m <<= lz;
            e -= lz;
        } else {
            m = (b & frac_mask) | hidden;
            e = static_cast<long long>(be) - bias - mb;
        }
    }

    static constexpr sf_value pack(bool neg, u64 sig, long long e, bool sticky) noexcept {
        if (sig == 0) return sf_value{ neg, 0 };
        const int L = sfmath::bit_length(sig);
        const long long E = e + L - 1;
        long long q = (E < emin ? emin : E) - mb;
        const long long shift = q - e;
        if (shift > 0) {
            if (shift > 64) return sf_value{ neg, 0 };
            const bool rb = ((sig >> (shift - 1)) & 1u) != 0;
            const bool st = sticky || (shift >= 2 && (sig & sfmath::low_mask(static_cast<int>(shift - 1))) != 0);
            sig = shift >= 64 ? 0u : (sig >> shift);
            if (rb && (st || (sig & 1u))) ++sig;
        } else if (shift < 0) {
            sig <<= -shift;
        }
        if (sig >> precision) {
            sig >>= 1;
            ++q;
        }
        if (sig == 0) return sf_value{ neg, 0 };
        if (sig & hidden) {
            const long long biased = q + mb + bias;
            if (biased >= static_cast<long long>(max_biased)) return sf_value{ neg, inf_bits };
            return sf_value{ neg, (static_cast<u64>(biased) << MB) | (sig & frac_mask) };
        }
        return sf_value{ neg, sig };
    }

    static constexpr sf_value pack_wide(bool neg, u64 hi, u64 lo, long long e, bool sticky) noexcept {
        if (hi == 0) return pack(neg, lo, e, sticky);
        const int s = sfmath::bit_length(hi) + 1;
        if (s >= 64) {
            const int extra = s - 64;
            const u64 sig = hi >> extra;
            const bool st = sticky || lo != 0 || (hi & sfmath::low_mask(extra)) != 0;
            return pack(neg, sig, e + s, st);
        }
        const u64 sig = (hi << (64 - s)) | (lo >> s);
        const bool st = sticky || (lo & sfmath::low_mask(s)) != 0;
        return pack(neg, sig, e + s, st);
    }

    static constexpr sf_value add(bool an, u64 a, bool bn, u64 b) noexcept {
        if (is_unordered(an, a) || is_unordered(bn, b)) return unordered(an, a, bn, b);
        if (is_infinite(a) || is_infinite(b)) {
            if (is_infinite(a) && is_infinite(b) && an != bn) return sf_value{ false, indeterminate_bits };
            return is_infinite(a) ? sf_value{ an, a } : sf_value{ bn, b };
        }
        if (a == 0 && b == 0) return sf_value{ an && bn, 0 };
        if (a == 0) return sf_value{ bn, b };
        if (b == 0) return sf_value{ an, a };
        u64 ma = 0, mbv = 0;
        long long ea = 0, ebv = 0;
        unpack(a, ma, ea);
        unpack(b, mbv, ebv);
        bool sa = an, sb = bn;
        if (ea < ebv || (ea == ebv && ma < mbv)) {
            const u64 tm = ma; ma = mbv; mbv = tm;
            const long long te = ea; ea = ebv; ebv = te;
            const bool ts = sa; sa = sb; sb = ts;
        }
        const u64 A = ma << guard;
        u64 B = mbv << guard;
        const long long d = ea - ebv;
        bool sticky = false;
        if (d > 0) {
            if (d >= 63) {
                sticky = B != 0;
                B = 0;
            } else {
                sticky = (B & sfmath::low_mask(static_cast<int>(d))) != 0;
                B >>= d;
            }
        }
        if (sticky) B |= 1u;
        if (sa == sb) return pack(sa, A + B, ea - guard, false);
        const u64 S = A - B;
        if (S == 0) return sf_value{ false, 0 };
        return pack(sa, S, ea - guard, false);
    }

    static constexpr sf_value sub(bool an, u64 a, bool bn, u64 b) noexcept {
        return add(an, a, is_unordered(bn, b) ? bn : !bn, b);
    }

    static constexpr sf_value mul(bool an, u64 a, bool bn, u64 b) noexcept {
        if (is_unordered(an, a) || is_unordered(bn, b)) return unordered(an, a, bn, b);
        const bool sr = an != bn;
        if (is_infinite(a)) return b == 0 ? sf_value{ false, indeterminate_bits } : sf_value{ sr, inf_bits };
        if (is_infinite(b)) return a == 0 ? sf_value{ false, indeterminate_bits } : sf_value{ sr, inf_bits };
        if (a == 0 || b == 0) return sf_value{ sr, 0 };
        u64 ma = 0, mbv = 0;
        long long ea = 0, ebv = 0;
        unpack(a, ma, ea);
        unpack(b, mbv, ebv);
        u64 hi = 0, lo = 0;
        sfmath::mul_wide(ma, mbv, hi, lo);
        return pack_wide(sr, hi, lo, ea + ebv, false);
    }

    static constexpr sf_value div(bool an, u64 a, bool bn, u64 b) noexcept {
        if (is_unordered(an, a) || is_unordered(bn, b)) return unordered(an, a, bn, b);
        const bool sr = an != bn;
        if (b == 0) return a == 0 ? sf_value{ false, indeterminate_bits } : sf_value{ false, undefined_bits };
        if (is_infinite(a)) return is_infinite(b) ? sf_value{ false, indeterminate_bits } : sf_value{ sr, inf_bits };
        if (is_infinite(b) || a == 0) return sf_value{ sr, 0 };
        u64 ma = 0, mbv = 0;
        long long ea = 0, ebv = 0;
        unpack(a, ma, ea);
        unpack(b, mbv, ebv);
        const int k = precision + 2;
        const u64 nhi = k >= 64 ? ma : (ma >> (64 - k));
        const u64 nlo = ma << k;
        u64 rem = 0;
        const u64 q = sfmath::div_wide(nhi, nlo, mbv, rem);
        return pack(sr, q, ea - ebv - k, rem != 0);
    }

    static constexpr bool equal(bool an, u64 a, bool bn, u64 b) noexcept {
        if (is_unordered(an, a) || is_unordered(bn, b)) return false;
        if (a == 0 && b == 0) return true;
        return an == bn && a == b;
    }

    static constexpr bool less(bool an, u64 a, bool bn, u64 b) noexcept {
        if (is_unordered(an, a) || is_unordered(bn, b)) return false;
        if (an && is_infinite(a)) return !(bn && is_infinite(b));
        if (!bn && is_infinite(b)) return !(!an && is_infinite(a));
        if ((!an && is_infinite(a)) || (bn && is_infinite(b))) return false;
        if (an != bn) return !(a == 0 && b == 0) && an;
        return an ? (a > b) : (a < b);
    }
};

} // namespace fdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_SMALL_FLOAT_OPS_HPP
