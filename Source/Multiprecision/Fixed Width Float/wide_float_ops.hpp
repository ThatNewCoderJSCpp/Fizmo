#ifndef FIZMO_MULTIPRECISION_WIDE_FLOAT_OPS_HPP
#define FIZMO_MULTIPRECISION_WIDE_FLOAT_OPS_HPP

#include <cstddef>
#include <cstdint>
#include <cmath>
#include <vector>
#include "../Fixed Width int/integer.hpp"
#include "../limb_kernels.hpp"

#ifndef FIZMO_WIDE_FLOAT_FAST
#define FIZMO_WIDE_FLOAT_FAST 1
#endif

#if !defined(FIZMO_NO_ASM_MUL) && (defined(__x86_64__) || defined(_M_X64))
#define FIZMO_WMATH_ADC 1
#if defined(_MSC_VER) && !defined(__clang__)
#include <intrin.h>
#else
#include <x86intrin.h>
#endif // _MSC_VER
#endif // FIZMO_NO_ASM_MUL

namespace fizmo {
namespace multiprecision {
namespace fdetail {

template <std::size_t TB, std::size_t MB>
struct wide_format {
    static constexpr bool value = FIZMO_WIDE_FLOAT_FAST && TB >= 128 && TB % 64 == 0 && TB - MB <= 62 && MB >= 64;
};

namespace wmath {

inline void mul64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) noexcept {
    mdetail::mul_wide64(a, b, hi, lo);
}

inline std::uint64_t div_2by1(std::uint64_t hi, std::uint64_t lo, std::uint64_t d, std::uint64_t& rem) noexcept {
#if !defined(FIZMO_NO_ASM_MUL) && (defined(__GNUC__) || defined(__clang__)) && defined(__x86_64__)
    std::uint64_t q, r;
    __asm__ __volatile__("divq %4" : "=a"(q), "=d"(r) : "a"(lo), "d"(hi), "rm"(d) : "cc");
    rem = r;
    return q;
#else
    if (d <= 0xFFFFFFFFull) {
        std::uint64_t cur = (hi << 32) | (lo >> 32);
        const std::uint64_t qh = cur / d;
        cur -= qh * d;
        cur = (cur << 32) | (lo & 0xFFFFFFFFull);
        const std::uint64_t ql = cur / d;
        rem = cur - ql * d;
        return (qh << 32) | ql;
    }
    std::uint64_t q = 0, r = hi;
    for (int i = 63; i >= 0; --i) {
        const bool top = (r >> 63) != 0;
        r = (r << 1) | ((lo >> i) & 1u);
        if (top || r >= d) { r -= d; q |= std::uint64_t(1) << i; }
    }
    rem = r;
    return q;
#endif // FIZMO_NO_ASM_MUL
}

inline std::uint64_t div_small(std::uint64_t* r, const std::uint64_t* a, std::size_t n, std::uint64_t d) noexcept {
    if (n < 3) {
        std::uint64_t rem = 0;
        for (std::size_t i = n; i-- > 0;) r[i] = div_2by1(rem, a[i], d, rem);
        return rem;
    }
    const unsigned sh = static_cast<unsigned>(clz64(d));
    const std::uint64_t dn = d << sh;
    std::uint64_t junk;
    const std::uint64_t inv = div_2by1(~dn, ~std::uint64_t(0), dn, junk);
    std::uint64_t rr = (sh == 0) ? 0 : (a[n - 1] >> (64 - sh));
    for (std::size_t i = n; i-- > 0;) {
        const std::uint64_t cur = a[i];
        const std::uint64_t nlo = (sh == 0) ? cur : ((cur << sh) | ((i > 0) ? (a[i - 1] >> (64 - sh)) : 0));
        std::uint64_t q1, q0;
        mul64(inv, rr, q1, q0);
        const std::uint64_t t0 = q0 + nlo;
        q1 += rr + 1 + ((t0 < q0) ? 1u : 0u);
        q0 = t0;
        std::uint64_t rm = nlo - q1 * dn;
        if (rm > q0) { --q1; rm += dn; }
        if (rm >= dn) { ++q1; rm -= dn; }
        r[i] = q1;
        rr = rm;
    }
    return rr >> sh;
}

inline long long top_bit(const std::uint64_t* a, std::size_t n) noexcept {
    for (std::size_t i = n; i-- > 0;) {
        if (a[i]) return static_cast<long long>(i * 64) + 63 - clz64(a[i]);
    }
    return -1;
}

inline bool is_zero(const std::uint64_t* a, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) if (a[i]) return false;
    return true;
}

inline bool get_bit(const std::uint64_t* a, std::size_t n, long long i) noexcept {
    if (i < 0) return false;
    const std::size_t w = static_cast<std::size_t>(i) / 64;
    return w < n && ((a[w] >> (static_cast<std::size_t>(i) % 64)) & 1u) != 0;
}

inline bool any_below(const std::uint64_t* a, std::size_t n, long long bits) noexcept {
    if (bits <= 0) return false;
    const std::size_t full = static_cast<std::size_t>(bits) / 64;
    const std::size_t rest = static_cast<std::size_t>(bits) % 64;
    for (std::size_t i = 0; i < full && i < n; ++i) if (a[i]) return true;
    if (rest && full < n && (a[full] & ((std::uint64_t(1) << rest) - 1u))) return true;
    return false;
}

inline void shl(std::uint64_t* a, std::size_t n, std::size_t s) noexcept {
    if (s == 0) return;
    const std::size_t w = s / 64, b = s % 64;
    for (std::size_t i = n; i-- > 0;) {
        std::uint64_t v = 0;
        if (i >= w) {
            v = a[i - w] << b;
            if (b && i >= w + 1) v |= a[i - w - 1] >> (64 - b);
        }
        a[i] = v;
    }
}

inline void shr(std::uint64_t* a, std::size_t n, std::size_t s) noexcept {
    if (s == 0) return;
    const std::size_t w = s / 64, b = s % 64;
    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t v = 0;
        if (i + w < n) {
            v = a[i + w] >> b;
            if (b && i + w + 1 < n) v |= a[i + w + 1] << (64 - b);
        }
        a[i] = v;
    }
}

inline void add_one(std::uint64_t* a, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) { if (++a[i] != 0) return; }
}

inline void add(std::uint64_t* r, const std::uint64_t* a, const std::uint64_t* b, std::size_t n) noexcept {
#if defined(FIZMO_WMATH_ADC)
    unsigned char c = 0;
    for (std::size_t i = 0; i < n; ++i) {
        unsigned long long t;
        c = _addcarry_u64(c, a[i], b[i], &t);
        r[i] = t;
    }
#else
    std::uint64_t c = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t s = a[i] + c;
        const std::uint64_t c1 = s < c ? 1u : 0u;
        r[i] = s + b[i];
        c = c1 + (r[i] < s ? 1u : 0u);
    }
#endif // FIZMO_WMATH_ADC
}

inline void sub(std::uint64_t* r, const std::uint64_t* a, const std::uint64_t* b, std::size_t n) noexcept {
#if defined(FIZMO_WMATH_ADC)
    unsigned char c = 0;
    for (std::size_t i = 0; i < n; ++i) {
        unsigned long long t;
        c = _subborrow_u64(c, a[i], b[i], &t);
        r[i] = t;
    }
#else
    std::uint64_t br = 0;
    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t d = a[i] - b[i];
        const std::uint64_t b1 = a[i] < b[i] ? 1u : 0u;
        r[i] = d - br;
        br = b1 + (d < br ? 1u : 0u);
    }
#endif // FIZMO_WMATH_ADC
}

inline int compare(const std::uint64_t* a, const std::uint64_t* b, std::size_t n) noexcept {
    for (std::size_t i = n; i-- > 0;) {
        if (a[i] != b[i]) return a[i] < b[i] ? -1 : 1;
    }
    return 0;
}

inline void mul(std::uint64_t* r, const std::uint64_t* a, std::size_t na, const std::uint64_t* b, std::size_t nb) noexcept {
    if (na == nb && na >= mdetail::mul_threshold::karatsuba) {
        thread_local std::vector<std::uint64_t> scratch;
        const std::size_t need = mdetail::mul_n_scratch(na) + 1;
        if (scratch.size() < need) scratch.resize(need);
        mdetail::mul_n(r, a, b, na, scratch.data());
        return;
    }
    for (std::size_t i = 0; i < na + nb; ++i) r[i] = 0;
    for (std::size_t i = 0; i < na; ++i) {
        if (!a[i]) continue;
        std::uint64_t carry = 0;
        for (std::size_t j = 0; j < nb; ++j) {
            std::uint64_t hi, lo;
            mul64(a[i], b[j], hi, lo);
            lo += carry;
            hi += lo < carry ? 1u : 0u;
            const std::uint64_t t = r[i + j] + lo;
            hi += t < lo ? 1u : 0u;
            r[i + j] = t;
            carry = hi;
        }
        r[i + nb] = carry;
    }
}

inline std::uint64_t get_field(const std::uint64_t* a, std::size_t pos, std::size_t count) noexcept {
    const std::size_t w = pos / 64, b = pos % 64;
    std::uint64_t v = a[w] >> b;
    if (b + count > 64) v |= a[w + 1] << (64 - b);
    return count >= 64 ? v : (v & ((std::uint64_t(1) << count) - 1u));
}

inline bool divide(std::uint64_t* q, std::size_t nq, const std::uint64_t* num, std::size_t nn, const std::uint64_t* den, std::size_t nd) noexcept {
    constexpr std::size_t cap = 2 * (4096 / 64) + 4;
    std::uint32_t u[2 * cap], v[2 * cap], qq[2 * cap], rr[2 * cap], un[2 * cap + 1], vn[2 * cap];
    for (std::size_t i = 0; i < nn; ++i) { u[2 * i] = static_cast<std::uint32_t>(num[i]); u[2 * i + 1] = static_cast<std::uint32_t>(num[i] >> 32); }
    for (std::size_t i = 0; i < nd; ++i) { v[2 * i] = static_cast<std::uint32_t>(den[i]); v[2 * i + 1] = static_cast<std::uint32_t>(den[i] >> 32); }
    int m = static_cast<int>(2 * nn), n = static_cast<int>(2 * nd);
    while (m > 1 && u[m - 1] == 0u) --m;
    while (n > 1 && v[n - 1] == 0u) --n;
    for (std::size_t i = 0; i < nq; ++i) q[i] = 0;
    if (m < n) return !is_zero(num, nn);
    for (int i = 0; i < m; ++i) qq[i] = 0u;
    for (int i = 0; i < n; ++i) rr[i] = 0u;
    udiv::knuth_divide(qq, rr, u, v, m, n, un, vn);
    for (int i = 0; i <= m - n; ++i) {
        const std::size_t w = static_cast<std::size_t>(i) / 2;
        if (w < nq) q[w] |= static_cast<std::uint64_t>(qq[i]) << (32 * (i % 2));
    }
    for (int i = 0; i < n; ++i) if (rr[i]) return true;
    return false;
}

} // namespace wmath

template <std::size_t TB, std::size_t MB>
struct wide_ops {
    static constexpr std::size_t L    = TB / 64;
    static constexpr std::size_t W    = 2 * L + 2;
    static constexpr std::size_t EB   = TB - MB;
    static constexpr long long   bias = (1LL << (EB - 1)) - 1;
    static constexpr std::uint64_t emax = (std::uint64_t(1) << EB) - 1u;
    static constexpr long long   MI   = static_cast<long long>(MB);
    static constexpr long long   min_exp = 1 - bias;

    struct unpacked {
        std::uint64_t m[L];
        long long     e;
    };

    static std::uint64_t exponent_field(const std::uint64_t* bits) noexcept { return wmath::get_field(bits, MB, EB); }

    static bool mantissa_zero(const std::uint64_t* bits) noexcept {
        const std::size_t full = MB / 64, rest = MB % 64;
        for (std::size_t i = 0; i < full; ++i) if (bits[i]) return false;
        return !(rest && (bits[full] & ((std::uint64_t(1) << rest) - 1u)));
    }

    static bool finite_nonzero(const std::uint64_t* bits) noexcept {
        const std::uint64_t e = exponent_field(bits);
        if (e == emax) return false;
        return e != 0 || !mantissa_zero(bits);
    }

    static void unpack(const std::uint64_t* bits, unpacked& u) noexcept {
        const std::size_t full = MB / 64, rest = MB % 64;
        for (std::size_t i = 0; i < L; ++i) u.m[i] = 0;
        for (std::size_t i = 0; i < full; ++i) u.m[i] = bits[i];
        if (rest) u.m[full] = bits[full] & ((std::uint64_t(1) << rest) - 1u);
        const std::uint64_t e = exponent_field(bits);
        if (e == 0) {
            u.e = min_exp - MI;
        } else {
            u.m[full] |= std::uint64_t(1) << rest;
            u.e = static_cast<long long>(e) - bias - MI;
        }
    }

    enum class kind { zero, finite, infinite };

    static kind pack(std::uint64_t* buf, std::size_t n, long long e, bool sticky, std::uint64_t* out) noexcept {
        for (std::size_t i = 0; i < L; ++i) out[i] = 0;
        const long long lead = wmath::top_bit(buf, n);
        if (lead < 0) return kind::zero;
        long long E = e + lead;
        if (E < min_exp) E = min_exp;
        const long long drop = E - MI - e;

        if (drop > 0) {
            if (drop > static_cast<long long>(n * 64)) return kind::zero;
            const bool rb = wmath::get_bit(buf, n, drop - 1);
            if (!sticky && drop >= 2) sticky = wmath::any_below(buf, n, drop - 1);
            wmath::shr(buf, n, static_cast<std::size_t>(drop));
            if (rb && (sticky || (buf[0] & 1u))) wmath::add_one(buf, n);
        } else if (drop < 0) {
            wmath::shl(buf, n, static_cast<std::size_t>(-drop));
        }

        if (wmath::get_bit(buf, n, MI + 1)) { wmath::shr(buf, n, 1); ++E; }
        if (wmath::is_zero(buf, n)) return kind::zero;

        const std::size_t full = MB / 64, rest = MB % 64;
        const bool normal = wmath::get_bit(buf, n, MI);
        for (std::size_t i = 0; i < full; ++i) out[i] = buf[i];
        if (rest) out[full] = buf[full] & ((std::uint64_t(1) << rest) - 1u);
        if (!normal) return kind::finite;

        const long long biased = E + bias;
        if (biased >= static_cast<long long>(emax)) return kind::infinite;
        const std::uint64_t be = static_cast<std::uint64_t>(biased);
        out[full] |= be << rest;
        if (rest + EB > 64 && full + 1 < L) out[full + 1] |= be >> (64 - rest);
        return kind::finite;
    }

    static kind add(bool sa, const std::uint64_t* a, bool sb, const std::uint64_t* b, bool& sr, std::uint64_t* out) noexcept {
        unpacked ua, ub;
        unpack(a, ua);
        unpack(b, ub);
        std::uint64_t wa[W] = {}, wb[W] = {};
        for (std::size_t i = 0; i < L; ++i) { wa[i] = ua.m[i]; wb[i] = ub.m[i]; }
        long long er;
        if (ua.e > ub.e) {
            const long long d = ua.e - ub.e;
            if (d >= static_cast<long long>(TB) + 4) { sr = sa; return pack(wa, L, ua.e, false, out); }
            wmath::shl(wa, W, static_cast<std::size_t>(d));
            er = ub.e;
        } else if (ub.e > ua.e) {
            const long long d = ub.e - ua.e;
            if (d >= static_cast<long long>(TB) + 4) { sr = sb; return pack(wb, L, ub.e, false, out); }
            wmath::shl(wb, W, static_cast<std::size_t>(d));
            er = ua.e;
        } else {
            er = ua.e;
        }

        std::uint64_t r[W];
        if (sa == sb) {
            wmath::add(r, wa, wb, W);
            sr = sa;
            return pack(r, W, er, false, out);
        }
        const int c = wmath::compare(wa, wb, W);
        if (c == 0) { sr = false; for (std::size_t i = 0; i < L; ++i) out[i] = 0; return kind::zero; }
        if (c > 0) { wmath::sub(r, wa, wb, W); sr = sa; }
        else       { wmath::sub(r, wb, wa, W); sr = sb; }
        return pack(r, W, er, false, out);
    }

    static kind mul(const std::uint64_t* a, const std::uint64_t* b, std::uint64_t* out) noexcept {
        unpacked ua, ub;
        unpack(a, ua);
        unpack(b, ub);
        std::uint64_t r[W] = {};
        wmath::mul(r, ua.m, L, ub.m, L);
        return pack(r, W, ua.e + ub.e, false, out);
    }

    static void normalize(unpacked& u) noexcept {
        const long long lead = wmath::top_bit(u.m, L);
        if (lead >= 0 && lead < MI) {
            wmath::shl(u.m, L, static_cast<std::size_t>(MI - lead));
            u.e -= MI - lead;
        }
    }

    static bool square_above(const std::uint64_t* x, const std::uint64_t* n) noexcept {
        const long long hx = wmath::top_bit(x, W);
        if (hx < 0) return false;
        const std::size_t nx = static_cast<std::size_t>(hx / 64 + 1);
        if (2 * nx > W + 1) return true;
        std::uint64_t sq[2 * W] = {};
        wmath::mul(sq, x, nx, x, nx);
        return wmath::compare(sq, n, W) > 0;
    }

    static void isqrt(const std::uint64_t* n, std::uint64_t* x) noexcept {
        for (std::size_t i = 0; i < W; ++i) x[i] = 0;
        const long long nbits = wmath::top_bit(n, W) + 1;
        if (nbits <= 0) return;

        if (nbits <= 126) {
            const long double v = static_cast<long double>(n[0]) + static_cast<long double>(n[1]) * 18446744073709551616.0L;
            x[0] = static_cast<std::uint64_t>(std::sqrt(v));
        } else {
            const long long sh = 2 * (nbits / 4);
            std::uint64_t m[W];
            for (std::size_t i = 0; i < W; ++i) m[i] = n[i];
            wmath::shr(m, W, static_cast<std::size_t>(sh));
            isqrt(m, x);
            wmath::add_one(x, W);
            wmath::shl(x, W, static_cast<std::size_t>(sh / 2));
            std::uint64_t q[W], y[W];
            wmath::divide(q, W, n, W, x, W);
            wmath::add(y, x, q, W);
            wmath::shr(y, W, 1);
            for (std::size_t i = 0; i < W; ++i) x[i] = y[i];
        }

        std::uint64_t one[W] = {};
        one[0] = 1;
        while (square_above(x, n)) wmath::sub(x, x, one, W);
        for (;;) {
            std::uint64_t t[W];
            for (std::size_t i = 0; i < W; ++i) t[i] = x[i];
            wmath::add_one(t, W);
            if (square_above(t, n)) break;
            for (std::size_t i = 0; i < W; ++i) x[i] = t[i];
        }
    }

    static kind sqrt(const std::uint64_t* a, std::uint64_t* out) noexcept {
        unpacked ua;
        unpack(a, ua);
        normalize(ua);
        long long k = MI + 5;
        if (((ua.e - k) % 2 + 2) % 2 != 0) ++k;
        std::uint64_t n[W] = {};
        for (std::size_t i = 0; i < L; ++i) n[i] = ua.m[i];
        wmath::shl(n, W, static_cast<std::size_t>(k));
        std::uint64_t x[W];
        isqrt(n, x);
        std::uint64_t sq[2 * W] = {};
        const std::size_t nx = static_cast<std::size_t>(wmath::top_bit(x, W) / 64 + 1);
        wmath::mul(sq, x, nx, x, nx);
        bool sticky = false;
        for (std::size_t i = 0; i < W; ++i) if (sq[i] != n[i]) { sticky = true; break; }
        return pack(x, W, (ua.e - k) / 2, sticky, out);
    }

    static kind div(const std::uint64_t* a, const std::uint64_t* b, std::uint64_t* out) noexcept {
        unpacked ua, ub;
        unpack(a, ua);
        unpack(b, ub);
        normalize(ua);
        normalize(ub);
        std::uint64_t num[W] = {};
        for (std::size_t i = 0; i < L; ++i) num[i] = ua.m[i];
        wmath::shl(num, W, static_cast<std::size_t>(MI + 2));
        std::uint64_t q[W] = {};
        const bool rem = wmath::divide(q, W, num, W, ub.m, L);
        return pack(q, W, ua.e - ub.e - (MI + 2), rem, out);
    }
};

} // namespace fdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_WIDE_FLOAT_OPS_HPP
