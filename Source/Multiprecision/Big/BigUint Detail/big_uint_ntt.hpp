#ifndef FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_NTT_HPP
#define FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_NTT_HPP

#include "big_uint_mul_low.hpp"

namespace fizmo {
namespace multiprecision {
namespace mdetail {
namespace nttdetail {

static constexpr unsigned int P_TWO_ADIC   = 57;
static constexpr unsigned int P_ODD_PART   = 29;
static constexpr std::uint64_t P = P_ODD_PART * (1ull << P_TWO_ADIC) + 1;  

struct mont_ctx {
    std::uint64_t p;        // the modulus
    std::uint64_t np;       // -p^-1 mod 2^64
    std::uint64_t one;      // 2^64 mod p         
    std::uint64_t r2;       // 2^128 mod p
    std::uint64_t root;     // primitive 2^57-th root of unity
    std::uint64_t iroot;    // its inverse
};

inline std::uint64_t mont_mul(std::uint64_t a, std::uint64_t b, std::uint64_t p, std::uint64_t np) noexcept {
    std::uint64_t hi, lo;
    budetail::mul_wide(a, b, hi, lo);
    const std::uint64_t m = lo * np;
    std::uint64_t mh, ml;
    budetail::mul_wide(m, p, mh, ml);
    (void)ml;                                       
    std::uint64_t t = hi + mh + ((lo != 0) ? 1u : 0u);  
    if (t >= p) t -= p;
    return t;
}

inline std::uint64_t mod_add(std::uint64_t a, std::uint64_t b, std::uint64_t p) noexcept {
    const std::uint64_t s = a + b;                  
    return (s >= p) ? (s - p) : s;
}

inline std::uint64_t mod_sub(std::uint64_t a, std::uint64_t b, std::uint64_t p) noexcept {
    return (a >= b) ? (a - b) : (a + p - b);
}

inline std::uint64_t mont_pow(std::uint64_t base, std::uint64_t e, const mont_ctx& m) noexcept {
    std::uint64_t r = m.one;
    std::uint64_t b = base;

    while (e != 0) {
        if ((e & 1u) != 0) r = mont_mul(r, b, m.p, m.np);
        b = mont_mul(b, b, m.p, m.np);
        e >>= 1;
    }

    return r;
}

inline mont_ctx make_mont_ctx() noexcept {
    mont_ctx m;
    m.p  = P;
    m.np = std::uint64_t(0) - binv64(P);
    std::uint64_t x = 1;
    for (int i = 0; i < 64; ++i) { x += x; if (x >= P) x -= P; }
    m.one = x;
    for (int i = 0; i < 64; ++i) { x += x; if (x >= P) x -= P; }
    m.r2 = x;
    const std::uint64_t e2  = (P - 1) / 2;
    const std::uint64_t e29 = (P - 1) / P_ODD_PART;

    for (std::uint64_t g = 2; ; ++g) {
        const std::uint64_t gm = mont_mul(g, m.r2, m.p, m.np);
        if (mont_pow(gm, e2,  m) == m.one) continue;
        if (mont_pow(gm, e29, m) == m.one) continue;
        m.root = mont_pow(gm, P_ODD_PART, m);        
        break;
    }

    m.iroot = mont_pow(m.root, (std::uint64_t(1) << P_TWO_ADIC) - 1, m);
    return m;
}

inline const mont_ctx& ctx() {
    static const mont_ctx m = make_mont_ctx();
    return m;
}

inline bool self_check() {
    const mont_ctx& m = ctx();
    const std::uint64_t bases[12] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 37 };
    const std::uint64_t pm1 = mont_mul(m.p - 1, m.r2, m.p, m.np);

    for (int i = 0; i < 12; ++i) {
        std::uint64_t x = mont_pow(mont_mul(bases[i] % m.p, m.r2, m.p, m.np), P_ODD_PART, m);
        if (x == m.one || x == pm1) continue;
        bool ok = false;

        for (unsigned r = 1; r < P_TWO_ADIC; ++r) {
            x = mont_mul(x, x, m.p, m.np);
            if (x == pm1) { ok = true; break; }
        }

        if (!ok) return false;                      
    }

    std::uint64_t w = m.root;

    for (unsigned i = 0; i + 1 < P_TWO_ADIC; ++i) {
        if (w == m.one) return false;                
        w = mont_mul(w, w, m.p, m.np);
    }

    if (w == m.one) return false;
    w = mont_mul(w, w, m.p, m.np);
    if (w != m.one) return false;                    
    return mont_mul(m.root, m.iroot, m.p, m.np) == m.one;
}

inline void ntt_dif(std::uint64_t* a, std::size_t N, const std::uint64_t* rt, const mont_ctx& m) noexcept {
    for (std::size_t len = N; len >= 2; len >>= 1) {
        const std::size_t h      = len >> 1;
        const std::size_t stride = N / len;

        for (std::size_t i = 0; i < N; i += len) {
            for (std::size_t j = 0; j < h; ++j) {
                const std::uint64_t u = a[i + j];
                const std::uint64_t v = a[i + j + h];
                a[i + j]     = mod_add(u, v, m.p);
                a[i + j + h] = mont_mul(mod_sub(u, v, m.p), rt[j * stride], m.p, m.np);
            }
        }
    }
}

inline void ntt_dit(std::uint64_t* a, std::size_t N, const std::uint64_t* irt, const mont_ctx& m) noexcept {
    for (std::size_t len = 2; len <= N; len <<= 1) {
        const std::size_t h      = len >> 1;
        const std::size_t stride = N / len;

        for (std::size_t i = 0; i < N; i += len) {
            for (std::size_t j = 0; j < h; ++j) {
                const std::uint64_t u = a[i + j];
                const std::uint64_t v = mont_mul(a[i + j + h], irt[j * stride], m.p, m.np);
                a[i + j]     = mod_add(u, v, m.p);
                a[i + j + h] = mod_sub(u, v, m.p);
            }
        }
    }
}

inline std::uint64_t get_bits(const std::uint64_t* p, std::size_t n, std::size_t bitpos, unsigned d) noexcept {
    const std::size_t w   = bitpos >> 6;
    const unsigned    off = static_cast<unsigned>(bitpos & 63);
    if (w >= n) return 0;
    std::uint64_t v = p[w] >> off;
    if (off != 0 && w + 1 < n) v |= p[w + 1] << (64u - off);
    return v & ((std::uint64_t(1) << d) - 1);        
}

inline void add_at_bit(std::uint64_t* rp, std::size_t rn, std::size_t bitpos, std::uint64_t v) noexcept {
    if (v == 0) return;
    std::size_t    w   = bitpos >> 6;
    const unsigned off = static_cast<unsigned>(bitpos & 63);
    if (w >= rn) return;
    const std::uint64_t lo = v << off;
    const std::uint64_t hi = (off == 0) ? std::uint64_t(0) : (v >> (64u - off));
    std::uint64_t carry;

    { 
        const std::uint64_t t = rp[w] + lo; 
        carry = (t < lo) ? 1u : 0u; 
        rp[w] = t; 
    }

    ++w;
    std::uint64_t add = hi + carry;                  

    while (add != 0 && w < rn) {
        const std::uint64_t t = rp[w] + add;
        add   = (t < add) ? 1u : 0u;
        rp[w] = t;
        ++w;
    }
}

inline bool choose_params(std::size_t bits_a, std::size_t bits_b, unsigned& d_out, std::size_t& N_out, unsigned& ln_out) noexcept {
    for (unsigned d = 30; d >= 8; --d) {
        const std::uint64_t maxdig = (std::uint64_t(1) << d) - 1;
        const std::uint64_t sq     = maxdig * maxdig;          
        const std::size_t La   = (bits_a + d - 1) / d;
        const std::size_t Lb   = (bits_b + d - 1) / d;
        const std::size_t minL = (La < Lb) ? La : Lb;
        if (La == 0 || Lb == 0) continue;
        if (static_cast<std::uint64_t>(minL) > (P - 1) / sq) continue;   
        const std::size_t need = La + Lb;             
        std::size_t N  = 2;
        unsigned    ln = 1;
        while (N < need) { N <<= 1; ++ln; }
        if (ln > P_TWO_ADIC) continue;
        d_out = d; N_out = N; ln_out = ln;
        return true;
    }

    return false;
}

} // namespace nttdetail

inline bool ntt_mul(std::uint64_t* rp, const std::uint64_t* ap, std::size_t an, const std::uint64_t* bp, std::size_t bn) {
    using namespace nttdetail;
    if (an == 0 || bn == 0) return false;
    const std::size_t bits_a = (an - 1) * 64 + bitlen64(ap[an - 1]);
    const std::size_t bits_b = (bn - 1) * 64 + bitlen64(bp[bn - 1]);
    if (bits_a == 0 || bits_b == 0) return false;
    unsigned    d, ln;
    std::size_t N;
    if (!choose_params(bits_a, bits_b, d, N, ln)) return false;
    const mont_ctx& m = ctx();
    const std::size_t La = (bits_a + d - 1) / d;
    const std::size_t Lb = (bits_b + d - 1) / d;
    const bool        squaring = (ap == bp) && (an == bn);
    budetail::limb_vec fa, fb, rt, irt;
    if (!fa.resize_uninit(N))                        return false;
    if (!squaring && !fb.resize_uninit(N))           return false;
    if (!rt.resize_uninit(N / 2))                    return false;
    if (!irt.resize_uninit(N / 2))                   return false;

    {
        const std::uint64_t w  = mont_pow(m.root,  std::uint64_t(1) << (P_TWO_ADIC - ln), m);
        const std::uint64_t iw = mont_pow(m.iroot, std::uint64_t(1) << (P_TWO_ADIC - ln), m);
        std::uint64_t* r  = rt.data();
        std::uint64_t* ir = irt.data();
        r[0] = m.one; ir[0] = m.one;

        for (std::size_t j = 1; j < N / 2; ++j) {
            r[j]  = mont_mul(r[j - 1],  w,  m.p, m.np);
            ir[j] = mont_mul(ir[j - 1], iw, m.p, m.np);
        }
    }

    std::uint64_t* A = fa.data();
    for (std::size_t j = 0; j < La; ++j) A[j] = get_bits(ap, an, j * d, d);
    for (std::size_t j = La; j < N; ++j) A[j] = 0;
    ntt_dif(A, N, rt.data(), m);

    if (squaring) {
        for (std::size_t j = 0; j < N; ++j) A[j] = mont_mul(A[j], A[j], m.p, m.np);
    } else {
        std::uint64_t* B = fb.data();
        for (std::size_t j = 0; j < Lb; ++j) B[j] = get_bits(bp, bn, j * d, d);
        for (std::size_t j = Lb; j < N; ++j) B[j] = 0;
        ntt_dif(B, N, rt.data(), m);
        for (std::size_t j = 0; j < N; ++j) A[j] = mont_mul(A[j], B[j], m.p, m.np);
    }

    ntt_dit(A, N, irt.data(), m);

    {
        const std::uint64_t inv2      = (m.p + 1) / 2;
        const std::uint64_t inv2_mont = mont_mul(inv2, m.r2, m.p, m.np);
        const std::uint64_t ninv_mont = mont_pow(inv2_mont, ln, m);        // = N^-1 * R
        const std::uint64_t scale     = mont_mul(ninv_mont, m.r2, m.p, m.np); // = N^-1 * R^2
        for (std::size_t j = 0; j < N; ++j) A[j] = mont_mul(A[j], scale, m.p, m.np);
    }

    const std::size_t rn    = an + bn;
    const std::size_t ncoef = La + Lb - 1;
    zero_limbs(rp, rn);
    for (std::size_t j = 0; j < ncoef; ++j) add_at_bit(rp, rn, j * d, A[j]);
    return true;
}

} // namespace mdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_UNSIGNED_INTEGER_NTT_HPP