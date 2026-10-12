#ifndef FIZMO_MULTIPRECISION_FIXED_SPECIAL_CONSTANTS_HPP
#define FIZMO_MULTIPRECISION_FIXED_SPECIAL_CONSTANTS_HPP

#include "basic_constants.hpp"
#include "Details/bigfloat_bridge.hpp"

namespace fizmo {
namespace multiprecision {
namespace constants {

#define FIZMO_FW_CONSTANT(NAME, EXPR)                                                                         \
    template <class F>                                                                                        \
    detail::require_float_t<F>                                                                                \
    calculate_##NAME() {                                                                                      \
        return math::bdetail::eval<F>([](const BigFloatContext& c) { return EXPR; });                        \
    }                                                                                                         \
                                                                                                              \
    template <class F>                                                                                        \
    detail::require_float_t<F, const F&>                                                                      \
    NAME() {                                                                                                  \
        static const F value = calculate_##NAME<F>();                                                         \
        return value;                                                                                         \
    }

#define FIZMO_FW_INDEXED_CONSTANT(NAME, IT, EXPR)                                                             \
    template <class F>                                                                                        \
    detail::require_float_t<F>                                                                                \
    NAME(IT n) {                                                                                              \
        return math::bdetail::eval<F>([n](const BigFloatContext& c) { return EXPR; });                       \
    }

#define FIZMO_FW_EXACT_CONSTANT(NAME, IT, EXPR)                                                               \
    template <class F>                                                                                        \
    detail::require_float_t<F>                                                                                \
    NAME(IT n) {                                                                                              \
        return math::bdetail::exact<F>(BigFloat(EXPR));                                                       \
    }

FIZMO_FW_CONSTANT(euler_mascheroni, euler_mascheroni(c))
FIZMO_FW_CONSTANT(catalan, math::dirichlet_beta(BigFloat(2.0), c))
FIZMO_FW_CONSTANT(silver_ratio, silver_ratio(c))
FIZMO_FW_CONSTANT(bronze_ratio, bronze_ratio(c))
FIZMO_FW_CONSTANT(silver_constant, silver_constant(c))

FIZMO_FW_INDEXED_CONSTANT(stieltjes, std::size_t, stieltjes(n, c))
FIZMO_FW_INDEXED_CONSTANT(bernoulli, std::size_t, bernoulli(n, c))
FIZMO_FW_INDEXED_CONSTANT(metallic_ratio, std::uint64_t, metallic_ratio(n, c))
FIZMO_FW_INDEXED_CONSTANT(inv_metallic_ratio, std::uint64_t, inv_metallic_ratio(n, c))
FIZMO_FW_INDEXED_CONSTANT(metallic_mean, std::uint64_t, metallic_mean(n, c))
FIZMO_FW_INDEXED_CONSTANT(inv_metallic_mean, std::uint64_t, inv_metallic_mean(n, c))
FIZMO_FW_INDEXED_CONSTANT(beraha, std::uint64_t, beraha(n, c))

FIZMO_FW_EXACT_CONSTANT(tangent_number, std::size_t, tangent_number(n))
FIZMO_FW_EXACT_CONSTANT(secant_number, std::size_t, secant_number(n))
FIZMO_FW_EXACT_CONSTANT(euler_number, std::size_t, euler_number(n))

template <class F>
detail::require_float_t<F>
eulerian_number(std::size_t n, std::size_t k) {
    return math::bdetail::exact<F>(BigFloat(eulerian_number(n, k)));
}

#undef FIZMO_FW_CONSTANT
#undef FIZMO_FW_INDEXED_CONSTANT
#undef FIZMO_FW_EXACT_CONSTANT

} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FIXED_SPECIAL_CONSTANTS_HPP
