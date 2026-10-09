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

 BigUInt bn_gcd(BigUInt a, BigUInt b);

static const std::size_t bn_index_cap = 1u << 18;

inline std::vector<BigUInt>& tangent_cache() {
    static thread_local std::vector<BigUInt> cache;
    return cache;
}

 void tangent_build(std::vector<BigUInt>& cache, std::size_t n);

 const std::vector<BigUInt>& tangent_table(std::size_t n);

 void tangent_reserve(std::size_t n);

 void zigzag_build(std::vector<BigUInt>& A, std::size_t m);

static const std::size_t sn_index_cap = 1u << 17;   

inline std::vector<BigUInt>& secant_cache() {
    static thread_local std::vector<BigUInt> cache;
    return cache;
}

 void secant_build(std::vector<BigUInt>& cache, std::size_t n);

 const std::vector<BigUInt>& secant_table(std::size_t n);

 void secant_reserve(std::size_t n);

static const std::size_t en_index_cap = 1u << 14;
static const std::size_t en_tri_cap   = 256;  

inline std::size_t en_half(std::size_t n) { return (n + 1) / 2; }  

inline const BigUInt& en_at(const std::vector<BigUInt>& h, std::size_t n, std::size_t k) {
    return (k < h.size()) ? h[k] : h[n - 1 - k];
}

 std::vector<BigUInt> en_step(const std::vector<BigUInt>& prev, std::size_t n);

 std::vector<std::vector<BigUInt>>& eulerian_tri();

struct en_row_cache {
    std::size_t          n = 0;
    std::vector<BigUInt> h;
};

inline en_row_cache& eulerian_last() {
    static thread_local en_row_cache c;
    return c;
}

 const std::vector<BigUInt>& eulerian_half_row(std::size_t n);

} // namespace detail

 BigUInt tangent_number(std::size_t n);

inline BigUInt secant_number(std::size_t k) {
    if (k > detail::sn_index_cap) return BigUInt::undefined();
    return detail::secant_table(k)[k];
}

 BigInt euler_number(std::size_t n);

 BigUInt eulerian_number(std::size_t n, std::size_t k);

 std::vector<BigUInt> eulerian_row(std::size_t n);

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

    static BigBernoulliNumber make(std::size_t k);

    const BigInt&  numerator()   const noexcept { return m_num; }
    const BigUInt& denominator() const noexcept { return m_den; }
    std::size_t    index()       const noexcept { return m_index; }
    bool           is_valid()    const noexcept { return m_valid; }
    bool           is_zero()     const noexcept { return m_valid && m_num.magnitude().is_zero(); }
    bool           is_negative() const noexcept { return m_valid && m_num.is_negative(); }

    BigFloat to_bigfloat(const BigFloatContext& ctx) const;

    BigFloat to_bigfloat() const { return to_bigfloat(BigFloatContext::current()); }

    std::string to_string() const;

    inline friend std::ostream& operator<<(std::ostream& os, const BigBernoulliNumber& b) {
        return os << b.to_string();
    }
};

inline BigBernoulliNumber bernoulli_number(std::size_t k) { return BigBernoulliNumber::make(k); }

inline BigFloat bernoulli(std::size_t k, const BigFloatContext& ctx) {
    return BigBernoulliNumber::make(k).to_bigfloat(ctx);
}

inline BigFloat bernoulli(std::size_t k) { return bernoulli(k, BigFloatContext::current()); }

 BigFloat bernoulli_float(std::size_t k, const BigFloatContext& ctx);

inline BigFloat bernoulli_float(std::size_t k) { return bernoulli_float(k, BigFloatContext::current()); }

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_BERNOULLI_HPP