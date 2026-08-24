#ifndef FIZMO_MULTIPRECISION_FMOD_HPP
#define FIZMO_MULTIPRECISION_FMOD_HPP

#include "Details/fmod_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

#define FIZMO_DEFINE_MOD(name, impl)                                                     \
    template <class A, class B>                                                          \
    typename std::enable_if<fdetail::is_operand<A>::value && fdetail::is_operand<B>::value, \
                            typename fdetail::common_result<A, B>::type>::type            \
    name(const A& a, const B& b) noexcept {                                              \
        using F = typename fdetail::common_result<A, B>::type;                           \
        return fdetail::apply<F>::impl(a, b);                                            \
    }

FIZMO_DEFINE_MOD(fmod,       fmod_)
FIZMO_DEFINE_MOD(remainder,  rem_)
FIZMO_DEFINE_MOD(mod_floor,  floor_)
FIZMO_DEFINE_MOD(mod_euclid, euclid_)

#undef FIZMO_DEFINE_MOD

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FMOD_HPP