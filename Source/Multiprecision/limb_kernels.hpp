#ifndef FIZMO_MULTIPRECISION_LIMB_KERNELS_HPP
#define FIZMO_MULTIPRECISION_LIMB_KERNELS_HPP

#include <cstdint>
#include <cstddef>

#if !defined(FIZMO_MUL_THRESHOLDS_RUNTIME) && defined(__has_include)
#if __has_include(<fizmo_mul_tuning.hpp>)
#include <fizmo_mul_tuning.hpp>
#endif
#endif

namespace fizmo {
namespace multiprecision {
namespace mdetail {

inline void mul_wide64(std::uint64_t a, std::uint64_t b, std::uint64_t& hi, std::uint64_t& lo) noexcept {
    const std::uint64_t al = a & 0xFFFFFFFFull, ah = a >> 32;
    const std::uint64_t bl = b & 0xFFFFFFFFull, bh = b >> 32;
    const std::uint64_t p0 = al * bl, p1 = al * bh, p2 = ah * bl, p3 = ah * bh;
    const std::uint64_t m  = p1 + p2;
    const std::uint64_t mc = (m < p1) ? (1ull << 32) : 0ull;
    lo = p0 + (m << 32);
    hi = p3 + (m >> 32) + mc + ((lo < p0) ? 1ull : 0ull);
}

// Numbers approximated after multiple runs on my machine
// 64 GB RAM, DDR5
// 64 Bit Windows 11 Home
// AMD Ryzen 7 9850x3D
// VS-Code, G++ and CLANG using -O3
#ifndef FIZMO_MUL_KARATSUBA_THRESHOLD
#define FIZMO_MUL_KARATSUBA_THRESHOLD   35
#endif
#ifndef FIZMO_MUL_TOOM3_THRESHOLD
#define FIZMO_MUL_TOOM3_THRESHOLD       85
#endif
#ifndef FIZMO_MUL_TOOM4_THRESHOLD
#define FIZMO_MUL_TOOM4_THRESHOLD      125
#endif
#ifndef FIZMO_MUL_NTT_THRESHOLD
#define FIZMO_MUL_NTT_THRESHOLD       3825
#endif

#if defined(FIZMO_MUL_THRESHOLDS_RUNTIME)
struct mul_threshold {
    static inline std::size_t karatsuba = FIZMO_MUL_KARATSUBA_THRESHOLD;
    static inline std::size_t toom3     = FIZMO_MUL_TOOM3_THRESHOLD;
    static inline std::size_t toom4     = FIZMO_MUL_TOOM4_THRESHOLD;
    static inline std::size_t ntt       = FIZMO_MUL_NTT_THRESHOLD;
};
#else
struct mul_threshold {
    static constexpr std::size_t karatsuba = FIZMO_MUL_KARATSUBA_THRESHOLD;
    static constexpr std::size_t toom3     = FIZMO_MUL_TOOM3_THRESHOLD;
    static constexpr std::size_t toom4     = FIZMO_MUL_TOOM4_THRESHOLD;
    static constexpr std::size_t ntt       = FIZMO_MUL_NTT_THRESHOLD;
};
#endif

inline void        mul_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws);
inline std::size_t mul_n_scratch(std::size_t n);

inline std::size_t max_size(std::size_t a, std::size_t b) noexcept { return (a > b) ? a : b; }

inline void copy_limbs(std::uint64_t* d, const std::uint64_t* s, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) d[i] = s[i];
}

inline void zero_limbs(std::uint64_t* d, std::size_t n) noexcept {
    for (std::size_t i = 0; i < n; ++i) d[i] = 0;
}

inline void copy_pad(std::uint64_t* d, const std::uint64_t* s, std::size_t sn, std::size_t dn) noexcept {
    for (std::size_t i = 0;  i < sn; ++i) d[i] = s[i];
    for (std::size_t i = sn; i < dn; ++i) d[i] = 0;
}

inline int cmp_limbs(const std::uint64_t* a, const std::uint64_t* b, std::size_t n) noexcept {
    for (std::size_t i = n; i > 0; --i) {
        if (a[i - 1] != b[i - 1]) return (a[i - 1] < b[i - 1]) ? -1 : 1;
    }
    return 0;
}

inline std::uint64_t add_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n) noexcept {
    std::uint64_t carry = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t a = ap[i];
        const std::uint64_t b = bp[i];
        const std::uint64_t s = a + b;
        std::uint64_t       c = (s < a) ? 1u : 0u;
        const std::uint64_t t = s + carry;
        c |= (t < s) ? 1u : 0u;
        rp[i] = t;
        carry = c;
    }

    return carry;
}

inline std::uint64_t sub_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n) noexcept {
    std::uint64_t borrow = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t a = ap[i];
        const std::uint64_t b = bp[i];
        const std::uint64_t d = a - b;
        std::uint64_t       r = (a < b) ? 1u : 0u;
        const std::uint64_t e = d - borrow;
        r |= (d < borrow) ? 1u : 0u;
        rp[i] = e;
        borrow = r;
    }

    return borrow;
}

inline std::uint64_t add_1(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t b) noexcept {
    std::uint64_t carry = b;
    std::size_t   i     = 0;

    for (; i < n && carry != 0; ++i) {
        const std::uint64_t s = ap[i] + carry;
        carry = (s < carry) ? 1u : 0u;
        rp[i] = s;
    }

    if (rp != ap) { for (; i < n; ++i) rp[i] = ap[i]; }
    return carry;
}

inline std::uint64_t sub_1(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t b) noexcept {
    std::uint64_t borrow = b;
    std::size_t   i      = 0;

    for (; i < n && borrow != 0; ++i) {
        const std::uint64_t a = ap[i];
        rp[i]  = a - borrow;
        borrow = (a < borrow) ? 1u : 0u;
    }

    if (rp != ap) { for (; i < n; ++i) rp[i] = ap[i]; }
    return borrow;
}

inline int abs_sub_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n) noexcept {
    if (cmp_limbs(ap, bp, n) >= 0) { sub_n(rp, ap, bp, n); return 0; }
    sub_n(rp, bp, ap, n);
    return 1;
}

inline void add_at(std::uint64_t* rp, std::size_t rn, const std::uint64_t* sp, std::size_t sn, std::size_t off) noexcept {
    if (off >= rn) return;
    std::size_t m = rn - off;
    if (m > sn) m = sn;
    const std::uint64_t c = add_n(rp + off, rp + off, sp, m);
    if (c != 0 && off + m < rn) add_1(rp + off + m, rp + off + m, rn - off - m, c);
}

inline std::uint64_t lshift_limbs(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, unsigned cnt) noexcept {
    if (n == 0) return 0;
    const unsigned      rc   = 64u - cnt;
    const std::uint64_t high = ap[n - 1] >> rc;
    for (std::size_t i = n - 1; i > 0; --i) rp[i] = (ap[i] << cnt) | (ap[i - 1] >> rc);
    rp[0] = ap[0] << cnt;
    return high;
}

inline std::uint64_t rshift_limbs(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, unsigned cnt) noexcept {
    if (n == 0) return 0;
    const unsigned      lc  = 64u - cnt;
    const std::uint64_t low = ap[0] << lc;
    for (std::size_t i = 0; i + 1 < n; ++i) rp[i] = (ap[i] >> cnt) | (ap[i + 1] << lc);
    rp[n - 1] = ap[n - 1] >> cnt;
    return low;
}

inline unsigned bitlen64(std::uint64_t x) noexcept {
    unsigned r = 0;
    while (x != 0) { ++r; x >>= 1; }
    return r;
}

inline std::uint64_t mul_1(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t b) noexcept {
    std::uint64_t carry = 0;

    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t hi, lo;
        mul_wide64(ap[i], b, hi, lo);
        lo += carry;
        hi += (lo < carry) ? 1u : 0u;
        rp[i] = lo;
        carry = hi;
    }

    return carry;
}

inline std::uint64_t addmul_1(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t b) noexcept {
    std::uint64_t carry = 0;

    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t hi, lo;
        mul_wide64(ap[i], b, hi, lo);
        lo += carry;
        hi += (lo < carry) ? 1u : 0u;
        const std::uint64_t r = rp[i];
        lo += r;
        hi += (lo < r) ? 1u : 0u;
        rp[i] = lo;
        carry = hi;
    }

    return carry;
}

inline std::uint64_t submul_1(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t b) noexcept {
    std::uint64_t borrow = 0;

    for (std::size_t i = 0; i < n; ++i) {
        std::uint64_t hi, lo;
        mul_wide64(ap[i], b, hi, lo);
        lo += borrow;
        hi += (lo < borrow) ? 1u : 0u;
        const std::uint64_t r = rp[i];
        rp[i] = r - lo;
        hi += (r < lo) ? 1u : 0u;
        borrow = hi;
    }

    return borrow;
}

inline std::uint64_t binv64(std::uint64_t d) noexcept {
    std::uint64_t x = d;
    x *= std::uint64_t(2) - d * x;
    x *= std::uint64_t(2) - d * x;
    x *= std::uint64_t(2) - d * x;
    x *= std::uint64_t(2) - d * x;
    x *= std::uint64_t(2) - d * x;
    return x;
}

inline void divexact_by_odd(std::uint64_t* rp, const std::uint64_t* ap, std::size_t n, std::uint64_t d) noexcept {
    const std::uint64_t inv    = binv64(d);
    std::uint64_t       borrow = 0;

    for (std::size_t i = 0; i < n; ++i) {
        const std::uint64_t u  = ap[i];
        const std::uint64_t s  = u - borrow;
        const std::uint64_t nb = (u < borrow) ? 1u : 0u;
        const std::uint64_t q  = s * inv;
        rp[i] = q;
        std::uint64_t hi, lo;
        mul_wide64(q, d, hi, lo);
        (void)lo;
        borrow = hi + nb;
    }
}

inline void mul_2x2(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp) noexcept {
    std::uint64_t h0, l0, h1, l1, h2, l2, h3, l3;
    mul_wide64(ap[0], bp[0], h0, l0);
    mul_wide64(ap[0], bp[1], h1, l1);
    mul_wide64(ap[1], bp[0], h2, l2);
    mul_wide64(ap[1], bp[1], h3, l3);
    rp[0] = l0;
    std::uint64_t c = 0;
    std::uint64_t t = h0 + l1;  c += (t < h0) ? 1u : 0u;
    t += l2;                    c += (t < l2) ? 1u : 0u;
    rp[1] = t;
    std::uint64_t d = 0;
    std::uint64_t u = h1 + h2;  d += (u < h1) ? 1u : 0u;
    u += l3;                    d += (u < l3) ? 1u : 0u;
    u += c;                     d += (u < c)  ? 1u : 0u;
    rp[2] = u;
    rp[3] = h3 + d;
}

inline void mul_basecase(std::uint64_t* rp, const std::uint64_t* ap, std::size_t an, const std::uint64_t* bp, std::size_t bn) noexcept {
    rp[an] = mul_1(rp, ap, an, bp[0]);
    for (std::size_t i = 1; i < bn; ++i) rp[an + i] = addmul_1(rp + i, ap, an, bp[i]);
}

inline void mul_karatsuba(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws) {
    const std::size_t k  = (n + 1) / 2;
    const std::size_t hs = n - k;
    const std::size_t W  = 2 * k;
    std::uint64_t* a1 = ws; ws += k;
    std::uint64_t* b1 = ws; ws += k;
    std::uint64_t* da = ws; ws += k;
    std::uint64_t* db = ws; ws += k;
    std::uint64_t* z0 = ws; ws += W;
    std::uint64_t* z2 = ws; ws += W;
    std::uint64_t* zp = ws; ws += W;
    std::uint64_t* z1 = ws; ws += W + 2;
    copy_pad(a1, ap + k, hs, k);
    copy_pad(b1, bp + k, hs, k);
    const int sa = abs_sub_n(da, ap, a1, k);
    const int sb = abs_sub_n(db, bp, b1, k);
    mul_n(z0, ap, bp, k, ws);
    mul_n(z2, a1, b1, k, ws);
    mul_n(zp, da, db, k, ws);
    copy_limbs(z1, z0, W);
    z1[W]     = add_n(z1, z1, z2, W);
    z1[W + 1] = 0;

    if (sa == sb) z1[W] -= sub_n(z1, z1, zp, W);
    else          z1[W] += add_n(z1, z1, zp, W);

    zero_limbs(rp, 2 * n);
    add_at(rp, 2 * n, z0, W,          0);
    add_at(rp, 2 * n, z1, W + 1,      k);
    add_at(rp, 2 * n, z2, 2 * hs, 2 * k);
}

inline int toom3_eval(
    std::uint64_t* s1, std::uint64_t* m1, std::uint64_t* s2,
    const std::uint64_t* p0, const std::uint64_t* p1, const std::uint64_t* p2,
    std::size_t k, std::uint64_t* t
) noexcept {
    const std::size_t E = k + 1;
    t[k]  = add_n(t, p0, p2, k);
    s1[k] = t[k] + add_n(s1, t, p1, k);
    int sign = 0;

    if (t[k] != 0) {
        m1[k] = t[k] - sub_n(m1, t, p1, k);
    } else if (cmp_limbs(t, p1, k) >= 0) {
        sub_n(m1, t, p1, k); m1[k] = 0;
    } else {
        sub_n(m1, p1, t, k); m1[k] = 0; sign = 1;
    }

    copy_pad(s2, p2, k, E);
    lshift_limbs(s2, s2, E, 1);
    s2[k] += add_n(s2, s2, p1, k);
    lshift_limbs(s2, s2, E, 1);
    s2[k] += add_n(s2, s2, p0, k);
    return sign;
}

inline void mul_toom3(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws) {
    const std::size_t k  = (n + 2) / 3;
    const std::size_t hs = n - 2 * k;
    const std::size_t E  = k + 1;
    const std::size_t W  = 2 * k + 2;
    std::uint64_t* a2 = ws; ws += k; copy_pad(a2, ap + 2 * k, hs, k);
    std::uint64_t* b2 = ws; ws += k; copy_pad(b2, bp + 2 * k, hs, k);
    std::uint64_t* as1 = ws; ws += E;
    std::uint64_t* am1 = ws; ws += E;
    std::uint64_t* as2 = ws; ws += E;
    std::uint64_t* bs1 = ws; ws += E;
    std::uint64_t* bm1 = ws; ws += E;
    std::uint64_t* bs2 = ws; ws += E;
    std::uint64_t* tmp = ws; ws += E;
    const int sga = toom3_eval(as1, am1, as2, ap, ap + k, a2, k, tmp);
    const int sgb = toom3_eval(bs1, bm1, bs2, bp, bp + k, b2, k, tmp);
    const int sgn = sga ^ sgb;
    std::uint64_t* v0   = ws; ws += W;
    std::uint64_t* v1   = ws; ws += W;
    std::uint64_t* vm1  = ws; ws += W;
    std::uint64_t* v2   = ws; ws += W;
    std::uint64_t* vinf = ws; ws += W;
    std::uint64_t* te   = ws; ws += W;
    std::uint64_t* to   = ws; ws += W;
    std::uint64_t* c1   = ws; ws += W;
    std::uint64_t* c2   = ws; ws += W;
    std::uint64_t* c3   = ws; ws += W;
    mul_n(v0,  ap,  bp,  k, ws); v0[2 * k] = 0; v0[2 * k + 1] = 0;
    mul_n(v1,  as1, bs1, E, ws);
    mul_n(vm1, am1, bm1, E, ws);
    mul_n(v2,  as2, bs2, E, ws);
    mul_n(vinf, a2, b2,  k, ws); vinf[2 * k] = 0; vinf[2 * k + 1] = 0;

    if (sgn == 0) { add_n(te, v1, vm1, W); sub_n(to, v1, vm1, W); }
    else          { sub_n(te, v1, vm1, W); add_n(to, v1, vm1, W); }

    rshift_limbs(te, te, W, 1);
    rshift_limbs(to, to, W, 1);
    sub_n(c2, te, v0,   W);
    sub_n(c2, c2, vinf, W);
    sub_n(c3, v2, v0, W);
    submul_1(c3, vinf, W, 16);
    rshift_limbs(c3, c3, W, 1);
    submul_1(c3, c2, W, 2);
    sub_n(c3, c3, to, W);
    divexact_by_odd(c3, c3, W, 3);
    sub_n(c1, to, c3, W);
    zero_limbs(rp, 2 * n);
    add_at(rp, 2 * n, v0,   W,     0);
    add_at(rp, 2 * n, c1,   W,     k);
    add_at(rp, 2 * n, c2,   W, 2 * k);
    add_at(rp, 2 * n, c3,   W, 3 * k);
    add_at(rp, 2 * n, vinf, 2 * hs, 4 * k);
}

inline void toom4_eval(
    std::uint64_t* s1, std::uint64_t* m1,
    std::uint64_t* s2, std::uint64_t* m2, std::uint64_t* s3,
    const std::uint64_t* p0, const std::uint64_t* p1,
    const std::uint64_t* p2, const std::uint64_t* p3,
    std::size_t k, std::uint64_t* t, std::uint64_t* u,
    int& sg1, int& sg2
) noexcept {
    const std::size_t E = k + 1;
    t[k] = add_n(t, p0, p2, k);
    u[k] = add_n(u, p1, p3, k);
    s1[k] = t[k] + u[k] + add_n(s1, t, u, k);
    sg1   = abs_sub_n(m1, t, u, E);
    copy_pad(t, p2, k, E);
    lshift_limbs(t, t, E, 2);
    t[k] += add_n(t, t, p0, k);
    copy_pad(u, p3, k, E);
    lshift_limbs(u, u, E, 2);
    u[k] += add_n(u, u, p1, k);
    lshift_limbs(u, u, E, 1);
    s2[k] = t[k] + u[k] + add_n(s2, t, u, k);
    sg2   = abs_sub_n(m2, t, u, E);
    copy_pad(s3, p3, k, E);
    mul_1(s3, s3, E, 3);   s3[k] += add_n(s3, s3, p2, k);
    mul_1(s3, s3, E, 3);   s3[k] += add_n(s3, s3, p1, k);
    mul_1(s3, s3, E, 3);   s3[k] += add_n(s3, s3, p0, k);
}

inline void mul_toom4(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws) {
    const std::size_t k  = (n + 3) / 4;
    const std::size_t hs = n - 3 * k;
    const std::size_t E  = k + 1;
    const std::size_t W  = 2 * k + 2;
    std::uint64_t* a3 = ws; ws += k; copy_pad(a3, ap + 3 * k, hs, k);
    std::uint64_t* b3 = ws; ws += k; copy_pad(b3, bp + 3 * k, hs, k);
    std::uint64_t* as1 = ws; ws += E;
    std::uint64_t* am1 = ws; ws += E;
    std::uint64_t* as2 = ws; ws += E;
    std::uint64_t* am2 = ws; ws += E;
    std::uint64_t* as3 = ws; ws += E;
    std::uint64_t* bs1 = ws; ws += E;
    std::uint64_t* bm1 = ws; ws += E;
    std::uint64_t* bs2 = ws; ws += E;
    std::uint64_t* bm2 = ws; ws += E;
    std::uint64_t* bs3 = ws; ws += E;
    std::uint64_t* tt  = ws; ws += E;
    std::uint64_t* uu  = ws; ws += E;
    int sa1, sa2, sb1, sb2;
    toom4_eval(as1, am1, as2, am2, as3, ap, ap + k, ap + 2 * k, a3, k, tt, uu, sa1, sa2);
    toom4_eval(bs1, bm1, bs2, bm2, bs3, bp, bp + k, bp + 2 * k, b3, k, tt, uu, sb1, sb2);
    const int sg1 = sa1 ^ sb1;
    const int sg2 = sa2 ^ sb2;
    std::uint64_t* v0   = ws; ws += W;
    std::uint64_t* v1   = ws; ws += W;
    std::uint64_t* vm1  = ws; ws += W;
    std::uint64_t* v2   = ws; ws += W;
    std::uint64_t* vm2  = ws; ws += W;
    std::uint64_t* v3   = ws; ws += W;
    std::uint64_t* vinf = ws; ws += W;
    std::uint64_t* e1   = ws; ws += W;
    std::uint64_t* o1   = ws; ws += W;
    std::uint64_t* e2   = ws; ws += W;
    std::uint64_t* o2   = ws; ws += W;
    std::uint64_t* rr   = ws; ws += W;
    std::uint64_t* gg   = ws; ws += W;
    std::uint64_t* hh   = ws; ws += W;
    std::uint64_t* c1   = ws; ws += W;
    std::uint64_t* c2   = ws; ws += W;
    std::uint64_t* c3   = ws; ws += W;
    std::uint64_t* c4   = ws; ws += W;
    std::uint64_t* c5   = ws; ws += W;
    mul_n(v0,   ap,  bp,  k, ws); v0[2 * k]   = 0; v0[2 * k + 1]   = 0;
    mul_n(v1,   as1, bs1, E, ws);
    mul_n(vm1,  am1, bm1, E, ws);
    mul_n(v2,   as2, bs2, E, ws);
    mul_n(vm2,  am2, bm2, E, ws);
    mul_n(v3,   as3, bs3, E, ws);
    mul_n(vinf, a3,  b3,  k, ws); vinf[2 * k] = 0; vinf[2 * k + 1] = 0;

    if (sg1 == 0) { add_n(e1, v1, vm1, W); sub_n(o1, v1, vm1, W); }
    else          { sub_n(e1, v1, vm1, W); add_n(o1, v1, vm1, W); }

    rshift_limbs(e1, e1, W, 1);
    rshift_limbs(o1, o1, W, 1);

    if (sg2 == 0) { add_n(e2, v2, vm2, W); sub_n(o2, v2, vm2, W); }
    else          { sub_n(e2, v2, vm2, W); add_n(o2, v2, vm2, W); }

    rshift_limbs(e2, e2, W, 1);
    rshift_limbs(o2, o2, W, 2);
    sub_n(hh, e1, v0,   W);
    sub_n(hh, hh, vinf, W);
    sub_n(gg, e2, v0, W);
    submul_1(gg, vinf, W, 64);
    rshift_limbs(gg, gg, W, 2);
    sub_n(c4, gg, hh, W);
    divexact_by_odd(c4, c4, W, 3);
    sub_n(c2, hh, c4, W);
    sub_n(rr, v3, v0, W);
    submul_1(rr, c2,   W,   9);
    submul_1(rr, c4,   W,  81);
    submul_1(rr, vinf, W, 729);
    divexact_by_odd(rr, rr, W, 3);
    sub_n(gg, rr, o1, W);
    rshift_limbs(gg, gg, W, 3);
    sub_n(hh, o2, o1, W);
    divexact_by_odd(hh, hh, W, 3);
    sub_n(c5, gg, hh, W);
    divexact_by_odd(c5, c5, W, 5);
    copy_limbs(c3, hh, W);
    submul_1(c3, c5, W, 5);
    sub_n(c1, o1, c3, W);
    sub_n(c1, c1, c5, W);
    zero_limbs(rp, 2 * n);
    add_at(rp, 2 * n, v0,   W,      0);
    add_at(rp, 2 * n, c1,   W,      k);
    add_at(rp, 2 * n, c2,   W,  2 * k);
    add_at(rp, 2 * n, c3,   W,  3 * k);
    add_at(rp, 2 * n, c4,   W,  4 * k);
    add_at(rp, 2 * n, c5,   W,  5 * k);
    add_at(rp, 2 * n, vinf, 2 * hs, 6 * k);
}

inline std::size_t mul_n_scratch(std::size_t n) {
    if (n < mul_threshold::karatsuba) return 0;
    if (n < mul_threshold::toom3) { const std::size_t k = (n + 1) / 2; return 12 * k +  4 + mul_n_scratch(k); }
    if (n < mul_threshold::toom4) { const std::size_t k = (n + 2) / 3; return 30 * k + 32 + mul_n_scratch(k + 1); }
    const std::size_t k = (n + 3) / 4;
    return 54 * k + 56 + mul_n_scratch(k + 1);
}

inline std::size_t toom4_scratch(std::size_t n) {
    const std::size_t k = (n + 3) / 4;
    return 54 * k + 56 + mul_n_scratch(k + 1);
}

inline void mul_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n, std::uint64_t* ws) {
    if (n < mul_threshold::karatsuba) {
        if (n == 1) { rp[1] = mul_1(rp, ap, 1, bp[0]); return; }
        if (n == 2) { mul_2x2(rp, ap, bp);             return; }
        mul_basecase(rp, ap, n, bp, n);
        return;
    }

    if (n < mul_threshold::toom3) { mul_karatsuba(rp, ap, bp, n, ws); return; }
    if (n < mul_threshold::toom4) { mul_toom3(rp, ap, bp, n, ws);     return; }
    mul_toom4(rp, ap, bp, n, ws);
}

inline void mullo_n(std::uint64_t* rp, const std::uint64_t* ap, const std::uint64_t* bp, std::size_t n) noexcept {
    if (n == 0) return;
    mul_1(rp, ap, n, bp[0]);                                        
    for (std::size_t i = 1; i < n; ++i) addmul_1(rp + i, ap, n - i, bp[i]);
}

} // namespace mdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_LIMB_KERNELS_HPP