#ifndef FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP
#define FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP

#include "htrig.hpp"
#include "inv_htrig.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace gddetail {

inline bool gd_guard_exhausted(std::size_t prec, std::size_t guard) {
    return guard >= 4096 || prec + guard >= BigFloatContext::max_prec / 4;
}

 bool gd_safe(const BigFloat& v, std::size_t want, std::size_t lost, std::size_t prec);

 bool gd_in_domain(const BigFloat& ax);

} // namespace gddetail

 BigFloat gudermannian(const BigFloat& x, const BigFloatContext& ctx);

 BigFloat inv_gudermannian(const BigFloat& x, const BigFloatContext& ctx);

FIZMO_MP_TRIG_FORWARD(gudermannian)
FIZMO_MP_TRIG_FORWARD(inv_gudermannian)

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_GUDERMANNIAN_HPP