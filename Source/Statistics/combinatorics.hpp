#ifndef FIZMO_STATISTICS_COMBINATORICS_HPP
#define FIZMO_STATISTICS_COMBINATORICS_HPP

#include "common.hpp"

#include <initializer_list>

namespace fizmo {
namespace statistics {

inline BigInt factorial(std::uint64_t n) {
    BigInt r(1);
    for (std::uint64_t i = 2; i <= n; ++i) r *= BigInt(i);
    return r;
}

inline BigInt ncr(std::uint64_t n, std::uint64_t r) {
    if (r > n) return BigInt(0);
    return math::combination(BigInt(n), static_cast<std::int64_t>(r));
}

inline BigInt npr(std::uint64_t n, std::uint64_t r) {
    if (r > n) return BigInt(0);
    return math::permutation(BigInt(n), static_cast<std::int64_t>(r));
}

inline BigInt combinations(std::uint64_t n, std::uint64_t r) { return ncr(n, r); }
inline BigInt permutations(std::uint64_t n, std::uint64_t r) { return npr(n, r); }

inline BigInt integer_power(std::uint64_t base, std::uint64_t exponent) {
    BigInt result(1);
    BigInt b(base);

    while (exponent > 0) {
        if (exponent & 1u) result *= b;
        exponent >>= 1;
        if (exponent > 0) b *= b;
    }

    return result;
}

inline BigInt lock_combinations(std::uint64_t positions, std::uint64_t symbols) {
    if (symbols == 0) return BigInt(positions == 0 ? 1 : 0);
    return ncr(positions + symbols - 1, symbols - 1);
}

inline BigInt lock_permutations(std::uint64_t positions, std::uint64_t symbols) { return integer_power(symbols, positions); }

inline BigInt combinations_with_repetition(std::uint64_t n, std::uint64_t r) { return lock_combinations(r, n); }
inline BigInt permutations_with_repetition(std::uint64_t n, std::uint64_t r) { return integer_power(n, r); }

inline BigInt multinomial(const std::vector<std::uint64_t>& groups) {
    std::uint64_t total = 0;
    BigInt        r(1);

    for (std::uint64_t g : groups) {
        total += g;
        r *= ncr(total, g);
    }

    return r;
}

inline BigInt multinomial(std::initializer_list<std::uint64_t> groups) { return multinomial(std::vector<std::uint64_t>(groups)); }

inline BigInt derangements(std::uint64_t n) {
    if (n == 0) return BigInt(1);
    BigInt prev(1);
    BigInt cur(0);

    for (std::uint64_t i = 2; i <= n; ++i) {
        BigInt next = BigInt(i - 1) * (cur + prev);
        prev = cur;
        cur  = next;
    }

    return cur;
}

template <class T = double>
inline T binomial_coefficient(const T& n, const T& k) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return math::combination(sdetail::big(n), sdetail::big(k), c); });
}

template <class T = double>
inline T probability_of_all(const std::vector<T>& independent_probabilities) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        BigFloat              acc = BigFloat::one();
        for (const T& p : independent_probabilities) acc = BigFloat::mul(acc, sdetail::big(p), exact);
        return acc.rounded(c);
    });
}

template <class T = double>
inline T probability_of_any(const std::vector<T>& independent_probabilities) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        BigFloat              none = BigFloat::one();
        for (const T& p : independent_probabilities) none = BigFloat::mul(none, BigFloat::sub(BigFloat::one(), sdetail::big(p), exact), exact);
        return BigFloat::sub(BigFloat::one(), none, c);
    });
}

template <class T = double>
inline T probability_of_union(const T& a, const T& b, const T& both) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        return BigFloat::sub(BigFloat::add(sdetail::big(a), sdetail::big(b), exact), sdetail::big(both), c);
    });
}

template <class T = double>
inline T conditional_probability(const T& both, const T& given) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::div(sdetail::big(both), sdetail::big(given), c); });
}

template <class T = double>
inline T bayes(const T& likelihood, const T& prior, const T& evidence) {
    return sdetail::compute<T>([&](const BigFloatContext& c) {
        const BigFloatContext exact(BigFloatContext::max_prec, math::RoundingMode::nearest_even);
        return BigFloat::div(BigFloat::mul(sdetail::big(likelihood), sdetail::big(prior), exact), sdetail::big(evidence), c);
    });
}

template <class T = double>
inline T complement(const T& p) {
    return sdetail::compute<T>([&](const BigFloatContext& c) { return BigFloat::sub(BigFloat::one(), sdetail::big(p), c); });
}

} // namespace statistics
} // namespace fizmo

#endif // FIZMO_STATISTICS_COMBINATORICS_HPP
