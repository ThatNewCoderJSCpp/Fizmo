#ifndef FIZMO_CORE_CONVERT_HPP
#define FIZMO_CORE_CONVERT_HPP

#include "units.hpp"
#include "../Basic/constants.hpp"

namespace fizmo {
namespace units {

template <class FromU, class ToU>
constexpr typename std::enable_if<is_fizmo_unit_v<FromU> && is_fizmo_unit_v<ToU>, long double>::type
convert(long double value, FromU from, ToU to) noexcept {
    static_assert(FromU::dimension() == ToU::dimension(), "convert: source and target dimensions differ");
    return to.from_base(from.to_base(value));
}

template <class U>
constexpr typename std::enable_if<is_fizmo_unit_v<U>, long double>::type
to_base(long double value, U from) noexcept {
    return from.to_base(value);
}

template <class U>
constexpr typename std::enable_if<is_fizmo_unit_v<U>, long double>::type
from_base(long double base, U to) noexcept {
    return to.from_base(base);
}

template <class A, class B>
constexpr typename std::enable_if<is_fizmo_unit_v<A> && is_fizmo_unit_v<B>, bool>::type
same_dimension(A, B) noexcept {
    return A::dimension() == B::dimension();
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr long double scale_ratio(Unit<L,M,T,I,K,N,J,A> from, Unit<L,M,T,I,K,N,J,A> to) noexcept {
    return from.scale / to.scale;
}

} // namespace units
} // namespace fizmo

#endif // FIZMO_CORE_CONVERT_HPP