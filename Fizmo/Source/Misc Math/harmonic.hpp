#ifndef HARMONIC_NUMBERS_HPP
#define HARMONIC_NUMBERS_HPP

#include "alternating_power.hpp"
#include "../Standard Overloads/pow.hpp"

namespace fizmo {
namespace math {
namespace harmonic {

template <typename RET = double, typename N>
constexpr typename std::enable_if<std::is_integral<N>::value && std::is_floating_point<RET>::value, RET>::type 
harmonic(const N order = 1) noexcept {
    constexpr RET p2 = constants::PI<RET> * constants::PI<RET>;

    switch (order) {
        case 2: return p2 / RET(6);
        case 4: return p2 * p2 / RET(90);
        case 6: return p2 * p2 * p2 / RET(945);
        case 8: return p2 * p2 * p2 * p2 / RET(9450);
        case 10: return p2 * p2 * p2 * p2 * p2 / RET(93555);
    }

    RET sum = RET(0);
    RET term = RET(0);
    RET prev_sum = RET(0);
    RET compensation = RET(0);  

    for (std::uint64_t i = 1; i < std::numeric_limits<std::uint64_t>::max(); ++i) { 
        prev_sum = sum;
        term = RET(1) / fizmo::math::pow_constexpr<RET>(i, order);
        RET y = term - compensation;
        RET t = sum + y;
        compensation = (t - sum) - y;
        sum = t;
        if (abs_constexpr(term) <= constants::TYPE_EPSILON<RET> * abs_constexpr(sum) || abs_constexpr(sum - prev_sum) <= constants::TYPE_EPSILON<RET>) { break; }
    }

    return sum;
}

template <typename RET = double, typename T, typename K>
constexpr typename std::enable_if<std::is_integral<T>::value && std::is_integral<K>::value && std::is_floating_point<RET>::value, RET>::type 
harmonic_derivative(const T num_terms, const K order) noexcept { 
    const RET n = static_cast<RET>(num_terms) + static_cast<RET>(order) - 1;
    const RET r = static_cast<RET>(order) - 1;
    const RET nf = std::tgamma(n);
    const RET rf = std::tgamma(r);
    const RET mult = nf / (rf * std::tgamma(n - r));
    return mult * (harmonic<RET>(num_terms + order - 1) - harmonic<RET>(order - 1)); 
}

} // namespace harmonic
} // namespace math
} // namespace fizmo

#endif // HARMONIC_NUMBERS_HPP