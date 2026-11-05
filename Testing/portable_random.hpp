#ifndef FIZMO_TESTING_PORTABLE_RANDOM_HPP
#define FIZMO_TESTING_PORTABLE_RANDOM_HPP

#include <cmath>
#include <cstdint>
#include <limits>
#include <random>

namespace fizmo_test {

template <class Gen>
inline std::uint64_t random_bits64(Gen& g) {
    if (Gen::max() - Gen::min() >= 0xFFFFFFFFFFFFFFFFull) return static_cast<std::uint64_t>(g() - Gen::min());
    const std::uint64_t high = static_cast<std::uint64_t>(g() - Gen::min()) & 0xFFFFFFFFull;
    const std::uint64_t low = static_cast<std::uint64_t>(g() - Gen::min()) & 0xFFFFFFFFull;
    return (high << 32) | low;
}

template <class Gen>
inline std::uint64_t random_below(Gen& g, std::uint64_t count) {
    if (count == 0) return random_bits64(g);
    const std::uint64_t threshold = (0 - count) % count;
    for (;;) {
        const std::uint64_t r = random_bits64(g);
        if (r >= threshold) return r % count;
    }
}

template <class Int, class Gen>
inline Int random_int(Gen& g, Int lo, Int hi) {
    const std::uint64_t span = static_cast<std::uint64_t>(hi) - static_cast<std::uint64_t>(lo) + 1;
    return static_cast<Int>(static_cast<std::uint64_t>(lo) + random_below(g, span));
}

template <class Gen>
inline double random_unit(Gen& g) {
    return static_cast<double>(random_bits64(g) >> 11) * (1.0 / 9007199254740992.0);
}

template <class Gen>
inline double random_real(Gen& g, double lo, double hi) {
    return lo + (hi - lo) * random_unit(g);
}

template <class Gen>
inline float random_float(Gen& g, float lo, float hi) {
    return static_cast<float>(random_real(g, static_cast<double>(lo), static_cast<double>(hi)));
}

template <class Gen>
inline double random_normal(Gen& g, double mean = 0.0, double stddev = 1.0) {
    const double u1 = 1.0 - random_unit(g);
    const double u2 = random_unit(g);
    return mean + stddev * std::sqrt(-2.0 * std::log(u1)) * std::cos(6.283185307179586476925 * u2);
}

template <class Gen>
inline float random_normal_float(Gen& g, float mean = 0.0f, float stddev = 1.0f) {
    return static_cast<float>(random_normal(g, static_cast<double>(mean), static_cast<double>(stddev)));
}

template <class Gen>
inline bool random_chance(Gen& g, double probability) {
    return random_unit(g) < probability;
}

} // namespace fizmo_test

#endif // FIZMO_TESTING_PORTABLE_RANDOM_HPP
