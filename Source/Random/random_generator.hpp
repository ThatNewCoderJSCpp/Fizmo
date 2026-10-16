#ifndef FIZMO_RANDOM_GENERATOR_HPP
#define FIZMO_RANDOM_GENERATOR_HPP

#include "random_mp.hpp"
#include "random_text.hpp"

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <random>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace fizmo {

class RandomGenerator {
public:
    using result_type = std::uint64_t;

    static constexpr result_type min() noexcept { return std::numeric_limits<result_type>::min(); }
    static constexpr result_type max() noexcept { return std::numeric_limits<result_type>::max(); }

    RandomGenerator() : m_engine(random_int<std::uint64_t>()) {}
    explicit RandomGenerator(std::uint64_t seed) : m_engine(seed) {}

    void seed(std::uint64_t value) { m_engine.seed(value); }
    void reseed() { m_engine.seed(random_int<std::uint64_t>()); }

    result_type operator()() { return m_engine(); }
    result_type next() { return m_engine(); }

    std::uint64_t below(std::uint64_t n) {
        if (n == 0) throw std::domain_error("RandomGenerator::below: the bound must be positive");
        const std::uint64_t threshold = (std::uint64_t(0) - n) % n;
        for (;;) {
            const std::uint64_t v = m_engine();
            if (v >= threshold) return v % n;
        }
    }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value && !std::is_same<typename std::remove_cv<T>::type, bool>::value>::type>
    T random_int(T a = T(0), T b = std::numeric_limits<T>::max()) {
        if (a > b) { const T t = a; a = b; b = t; }
        if (a == b) return a;
        using U = typename std::make_unsigned<T>::type;
        const std::uint64_t span = static_cast<std::uint64_t>(static_cast<U>(static_cast<U>(b) - static_cast<U>(a)));
        const std::uint64_t r = span == std::numeric_limits<std::uint64_t>::max() ? m_engine() : below(span + 1);
        return static_cast<T>(static_cast<U>(static_cast<U>(a) + static_cast<U>(r)));
    }

    template <std::size_t Bits, multiprecision::sign S>
    multiprecision::integer<Bits, S> random_int(const multiprecision::integer<Bits, S>& a, const multiprecision::integer<Bits, S>& b) {
        multiprecision::BigInt lo(a), hi(b);
        if (hi < lo) std::swap(lo, hi);
        const multiprecision::BigUInt span = (hi - lo).magnitude();
        multiprecision::BigUInt r = multiprecision::BigUInt::zero();

        if (!span.is_zero()) {
            const std::size_t bits = span.bit_length();
            for (;;) {
                r = random_bits(bits);
                if (r.compare(span) <= 0) break;
            }
        }

        return detail::integer_from_big<Bits>(lo + multiprecision::BigInt(r), std::integral_constant<multiprecision::sign, S>());
    }

    template <typename F>
    typename std::enable_if<std::is_floating_point<F>::value, F>::type random_real(F a, F b) {
        if (a == b) return a;
        if (b < a) std::swap(a, b);
        std::uniform_real_distribution<F> dist(a, b);
        return dist(m_engine);
    }

    template <typename F>
    typename std::enable_if<detail::is_floatmp_type<F>::value, F>::type random_real(const F& a, const F& b) {
        const multiprecision::BigFloat fa(a), fb(b);
        if (!fa.is_finite() || !fb.is_finite()) throw std::domain_error("RandomGenerator::random_real: bounds must be finite");
        if (fa == fb) return a;
        const bool swap = fb < fa;
        const multiprecision::BigFloat& lo = swap ? fb : fa;
        const multiprecision::BigFloat& hi = swap ? fa : fb;
        const std::size_t p = F::mantissa_bits + 1;
        const multiprecision::BigFloat u(random_bits(p), false, -static_cast<std::int64_t>(p));
        const multiprecision::BigFloatContext ctx(p + 64);
        const multiprecision::BigFloat r = multiprecision::BigFloat::add(lo, multiprecision::BigFloat::mul(multiprecision::BigFloat::sub(hi, lo, ctx), u, ctx), ctx);
        F out = static_cast<F>(r);
        if (!(out < static_cast<F>(hi))) out = static_cast<F>(lo);
        return out;
    }

    template <typename F>
    typename std::enable_if<std::is_floating_point<F>::value || detail::is_floatmp_type<F>::value, F>::type random_unit() {
        return random_real<F>(F(0), F(1));
    }

    bool random_bool() { return (m_engine() >> 63) != 0; }

    bool random_bool(double probability) {
        if (!(probability > 0.0)) return false;
        if (probability >= 1.0) return true;
        return static_cast<double>(m_engine() >> 11) * 0x1.0p-53 < probability;
    }

    char32_t random_char() {
        return static_cast<char32_t>(detail::random_alphanumeric[static_cast<std::size_t>(below(detail::random_alphanumeric_count))]);
    }

    char32_t random_char(char32_t lo, char32_t hi) {
        return detail::random_code_point([this](std::uint64_t n) { return below(n); }, lo, hi);
    }

    char32_t random_char(const std::string& alphabet) {
        return detail::random_alphabet_char([this](std::uint64_t n) { return below(n); }, detail::random_decode_utf8(alphabet));
    }

    std::string random_string(std::size_t length) {
        return detail::random_alphanumeric_string([this](std::uint64_t n) { return below(n); }, length);
    }

    std::string random_string(std::size_t length, const std::string& alphabet) {
        return detail::random_alphabet_string([this](std::uint64_t n) { return below(n); }, length, alphabet);
    }

    template <typename RandomIt>
    void shuffle(RandomIt first, RandomIt last) {
        using diff_t = typename std::iterator_traits<RandomIt>::difference_type;
        const diff_t n = last - first;
        for (diff_t i = n - 1; i > 0; --i) {
            const diff_t j = static_cast<diff_t>(below(static_cast<std::uint64_t>(i) + 1));
            if (j != i) { using std::swap; swap(first[i], first[j]); }
        }
    }

    multiprecision::BigUInt random_bits(std::size_t bits) {
        if (bits == 0) return multiprecision::BigUInt::zero();
        const std::size_t limbs = (bits + 63) / 64;
        std::vector<std::uint64_t> buf(limbs);
        for (std::uint64_t& limb : buf) limb = m_engine();
        const std::size_t top = bits % 64;
        if (top != 0) buf[limbs - 1] &= (std::uint64_t(1) << top) - 1;
        return multiprecision::BigUInt::from_limbs(buf.data(), limbs);
    }

private:
    std::mt19937_64 m_engine;
};

} // namespace fizmo

#endif // FIZMO_RANDOM_GENERATOR_HPP
