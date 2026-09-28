#ifndef FIZMO_MULTIPRECISION_BIG_BERNOULLI_HPP
#define FIZMO_MULTIPRECISION_BIG_BERNOULLI_HPP

#include "big_float_consts.hpp"

#include <vector>
#include <string>
#include <algorithm>

namespace fizmo {
namespace multiprecision {
namespace constants {

namespace detail {

inline BigUInt bn_gcd(BigUInt a, BigUInt b) {
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    const long long  ta     = a.count_trailing_zeros();
    const long long  tb     = b.count_trailing_zeros();
    const std::size_t shared = static_cast<std::size_t>((ta < tb) ? ta : tb);
    a.shift_right_mutable(static_cast<std::size_t>(ta));
    b.shift_right_mutable(static_cast<std::size_t>(tb));

    for (;;) {
        const int c = a.compare(b);
        if (c == 0) break;

        if (c > 0) {
            a.sub_mutable(b);
            a.shift_right_mutable(static_cast<std::size_t>(a.count_trailing_zeros()));
        } else {
            b.sub_mutable(a);
            b.shift_right_mutable(static_cast<std::size_t>(b.count_trailing_zeros()));
        }
    }

    a.shift_left_mutable(shared);
    return a;
}

static const std::size_t bn_index_cap = 1u << 18;

inline std::vector<BigUInt>& tangent_cache() {
    static thread_local std::vector<BigUInt> cache;
    return cache;
}

inline void tangent_build(std::vector<BigUInt>& cache, std::size_t n) {
    std::vector<BigUInt> v(n + 1, BigUInt::zero());
    v[1] = BigUInt::one();

    for (std::size_t k = 2; k <= n; ++k) {
        v[k] = v[k - 1];
        v[k].mul_small_mutable(static_cast<std::uint64_t>(k - 1));
    }

    BigUInt tmp = BigUInt::zero();

    for (std::size_t k = 2; k <= n; ++k) {
        for (std::size_t j = k; j <= n; ++j) {
            v[j].mul_small_mutable(static_cast<std::uint64_t>(j - k + 2));

            if (j > k) {                       
                tmp = v[j - 1];                
                tmp.mul_small_mutable(static_cast<std::uint64_t>(j - k));
                v[j].add_mutable(tmp);
            }
        }
    }

    cache.assign(std::make_move_iterator(v.begin() + 1), std::make_move_iterator(v.end()));
}

inline const std::vector<BigUInt>& tangent_table(std::size_t n) {
    std::vector<BigUInt>& cache = tangent_cache();
    if (n <= cache.size()) return cache;
    std::size_t target = (cache.size() < 16) ? 16 : cache.size();
    while (target < n) target *= 2;
    if (target > bn_index_cap) target = bn_index_cap;
    tangent_build(cache, target);
    return cache;
}

inline void tangent_reserve(std::size_t n) {
    if (n > bn_index_cap) n = bn_index_cap;
    std::vector<BigUInt>& cache = tangent_cache();
    if (n <= cache.size()) return;
    std::size_t target = n + n / 8 + 16;
    if (target > bn_index_cap) target = bn_index_cap;
    tangent_build(cache, target);
}

inline void zigzag_build(std::vector<BigUInt>& A, std::size_t m) {
    A.assign(m + 1, BigUInt::zero());
    A[0] = BigUInt::one();
    std::vector<BigUInt> b;
    b.reserve(m + 1);
    b.push_back(BigUInt::one());                 // reversed row 0

    for (std::size_t n = 1; n <= m; ++n) {
        for (std::size_t k = 1; k < n; ++k) b[k].add_mutable(b[k - 1]);
        A[n] = b[n - 1];
        std::reverse(b.begin(), b.end());
        b.push_back(BigUInt::zero());
    }
}

static const std::size_t sn_index_cap = 1u << 17;   

inline std::vector<BigUInt>& secant_cache() {
    static thread_local std::vector<BigUInt> cache;
    return cache;
}

inline void secant_build(std::vector<BigUInt>& cache, std::size_t n) {
    std::vector<BigUInt> A;
    zigzag_build(A, 2 * n);
    std::vector<BigUInt>& tc = tangent_cache();
    const bool grow_t = (n > tc.size());
    if (grow_t) tc.assign(n, BigUInt::zero());
    cache.assign(n + 1, BigUInt::zero());
    for (std::size_t k = 0; k <= n; ++k) cache[k] = std::move(A[2 * k]);
    if (grow_t) for (std::size_t i = 1; i <= n; ++i) tc[i - 1] = std::move(A[2 * i - 1]);
}

inline const std::vector<BigUInt>& secant_table(std::size_t n) {
    std::vector<BigUInt>& cache = secant_cache();
    if (n < cache.size()) return cache;
    std::size_t target = (cache.size() < 16) ? 16 : cache.size();
    while (target <= n) target *= 2;
    if (target > sn_index_cap) target = sn_index_cap;
    secant_build(cache, target);
    return cache;
}

inline void secant_reserve(std::size_t n) {
    if (n > sn_index_cap) n = sn_index_cap;
    std::vector<BigUInt>& cache = secant_cache();
    if (n < cache.size()) return;
    std::size_t target = n + n / 8 + 16;
    if (target > sn_index_cap) target = sn_index_cap;
    secant_build(cache, target);
}

static const std::size_t en_index_cap = 1u << 14;
static const std::size_t en_tri_cap   = 256;  

inline std::size_t en_half(std::size_t n) { return (n + 1) / 2; }  

inline const BigUInt& en_at(const std::vector<BigUInt>& h, std::size_t n, std::size_t k) {
    return (k < h.size()) ? h[k] : h[n - 1 - k];
}

inline std::vector<BigUInt> en_step(const std::vector<BigUInt>& prev, std::size_t n) {
    std::vector<BigUInt> h(en_half(n), BigUInt::zero());
    if (n == 1) { h[0] = BigUInt::one(); return h; }

    for (std::size_t k = 0; k < h.size(); ++k) {
        BigUInt t = en_at(prev, n - 1, k);
        t.mul_small_mutable(static_cast<std::uint64_t>(k + 1));

        if (k >= 1) {
            BigUInt u = en_at(prev, n - 1, k - 1);
            u.mul_small_mutable(static_cast<std::uint64_t>(n - k));
            t.add_mutable(u);
        }

        h[k] = std::move(t);
    }

    return h;
}

inline std::vector<std::vector<BigUInt>>& eulerian_tri() {
    static thread_local std::vector<std::vector<BigUInt>> t = [] {
        std::vector<std::vector<BigUInt>> v;
        v.reserve(en_tri_cap + 1);                                     
        v.emplace_back();                                              
        return v;
    }();
    return t;
}

struct en_row_cache {
    std::size_t          n = 0;
    std::vector<BigUInt> h;
};

inline en_row_cache& eulerian_last() {
    static thread_local en_row_cache c;
    return c;
}

inline const std::vector<BigUInt>& eulerian_half_row(std::size_t n) {
    std::vector<std::vector<BigUInt>>& tri = eulerian_tri();
    const std::size_t lim = (n < en_tri_cap) ? n : en_tri_cap;
    while (tri.size() <= lim) tri.push_back(en_step(tri.back(), tri.size()));
    if (n <= en_tri_cap) return tri[n];
    en_row_cache& c = eulerian_last();
    if (c.n == n) return c.h;
    if (c.n > n || c.n < en_tri_cap) { c.h = tri[en_tri_cap]; c.n = en_tri_cap; }
    while (c.n < n) { c.h = en_step(c.h, c.n + 1); ++c.n; }
    return c.h;
}

} // namespace detail

inline BigUInt tangent_number(std::size_t n) {
    if (n == 0)                     return BigUInt::zero();
    if (n > detail::bn_index_cap)   return BigUInt::undefined();
    return detail::tangent_table(n)[n - 1];
}

inline BigUInt secant_number(std::size_t k) {
    if (k > detail::sn_index_cap) return BigUInt::undefined();
    return detail::secant_table(k)[k];
}

inline BigInt euler_number(std::size_t n) {
    if (n % 2 == 1) return BigInt::zero();
    const std::size_t k = n / 2;
    if (k > detail::sn_index_cap) return BigInt::undefined();
    BigUInt m = secant_number(k);
    if (m.is_undefined()) return BigInt::undefined();
    return BigInt::from_magnitude(std::move(m), k % 2 == 1);
}

inline BigUInt eulerian_number(std::size_t n, std::size_t k) {
    if (n == 0) return (k == 0) ? BigUInt::one() : BigUInt::zero();
    if (k >= n) return BigUInt::zero();
    if (n > detail::en_index_cap) return BigUInt::undefined();
    return detail::en_at(detail::eulerian_half_row(n), n, k);
}

inline std::vector<BigUInt> eulerian_row(std::size_t n) {             
    if (n == 0) return std::vector<BigUInt>(1, BigUInt::one());
    if (n > detail::en_index_cap) return std::vector<BigUInt>();
    const std::vector<BigUInt>& h = detail::eulerian_half_row(n);
    std::vector<BigUInt> r(n);
    for (std::size_t k = 0; k < n; ++k) r[k] = detail::en_at(h, n, k);
    return r;
}

// B_1 = -1/2 
class BigBernoulliNumber {
private:
    BigInt      m_num;
    BigUInt     m_den;
    std::size_t m_index;
    bool        m_valid;

    BigBernoulliNumber(BigInt num, BigUInt den, std::size_t k, bool valid) : m_num(std::move(num)), m_den(std::move(den)), m_index(k), m_valid(valid) {}

public:
    BigBernoulliNumber() : m_num(BigInt::zero()), m_den(BigUInt::one()), m_index(0), m_valid(false) {}

    static BigBernoulliNumber make(std::size_t k) {
        if (k == 0) return BigBernoulliNumber(BigInt(1), BigUInt::one(), 0, true);
        if (k == 1) return BigBernoulliNumber(BigInt(-1), BigUInt(std::uint64_t(2)), 1, true);
        if (k % 2 == 1) return BigBernoulliNumber(BigInt::zero(), BigUInt::one(), k, true);
        const std::size_t n = k / 2;
        if (n > detail::bn_index_cap) return BigBernoulliNumber(BigInt::undefined(), BigUInt::one(), k, false);
        BigUInt num = tangent_number(n);
        if (num.is_undefined()) return BigBernoulliNumber(BigInt::undefined(), BigUInt::one(), k, false);
        num.mul_small_mutable(static_cast<std::uint64_t>(k));
        BigUInt p = constants::bfdetail::unit(k);          
        BigUInt q = p;
        q.sub_small_mutable(1);
        BigUInt den = p * q;
        const BigUInt g = detail::bn_gcd(num, den);

        if (!g.is_one() && !g.is_zero()) {
            num = num / g;
            den = den / g;
        }

        const bool neg = (n % 2 == 0);
        return BigBernoulliNumber(BigInt::from_magnitude(std::move(num), neg), std::move(den), k, true);
    }

    const BigInt&  numerator()   const noexcept { return m_num; }
    const BigUInt& denominator() const noexcept { return m_den; }
    std::size_t    index()       const noexcept { return m_index; }
    bool           is_valid()    const noexcept { return m_valid; }
    bool           is_zero()     const noexcept { return m_valid && m_num.magnitude().is_zero(); }
    bool           is_negative() const noexcept { return m_valid && m_num.is_negative(); }

    BigFloat to_bigfloat(const BigFloatContext& ctx) const {
        if (!m_valid) return BigFloat::undefined();
        return BigFloat::div(BigFloat(m_num), BigFloat(m_den), ctx);
    }

    BigFloat to_bigfloat() const { return to_bigfloat(BigFloatContext::current()); }

    std::string to_string() const {
        if (!m_valid) return "undefined";
        std::string s = BigFloat(m_num).to_string(BigFloat::no_digit_limit, -1000000);
        if (m_den.is_one()) return s;
        s.push_back('/');
        return s + BigFloat(m_den).to_string(BigFloat::no_digit_limit, -1000000);
    }

    inline friend std::ostream& operator<<(std::ostream& os, const BigBernoulliNumber& b) {
        return os << b.to_string();
    }
};

inline BigBernoulliNumber bernoulli_number(std::size_t k) { return BigBernoulliNumber::make(k); }

inline BigFloat bernoulli(std::size_t k, const BigFloatContext& ctx) {
    return BigBernoulliNumber::make(k).to_bigfloat(ctx);
}

inline BigFloat bernoulli(std::size_t k) { return bernoulli(k, BigFloatContext::current()); }

inline BigFloat bernoulli_float(std::size_t k, const BigFloatContext& ctx) {
    if (k == 0)     return BigFloat::one();
    if (k == 1)     return BigFloat::one(true).scaled_pow2(-1);
    if (k % 2 == 1) return BigFloat::zero();
    const std::size_t n = k / 2;
    if (n > detail::bn_index_cap) return BigFloat::undefined();
    BigUInt num = tangent_number(n);
    if (num.is_undefined()) return BigFloat::undefined();
    num.mul_small_mutable(static_cast<std::uint64_t>(k));
    BigUInt den = constants::bfdetail::unit(k);
    den.sub_small_mutable(1);
    const BigFloat r = BigFloat::div(BigFloat(num), BigFloat(den), ctx).scaled_pow2(-static_cast<std::int64_t>(k));
    return r.with_sign(n % 2 == 0);
}

inline BigFloat bernoulli_float(std::size_t k) { return bernoulli_float(k, BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_BERNOULLI_HPP