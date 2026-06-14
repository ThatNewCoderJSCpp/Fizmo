#ifndef FIZMO_MULTIPRECISION_FLOAT_HPP
#define FIZMO_MULTIPRECISION_FLOAT_HPP

#include "../Fixed Width int/type_traits.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t TotalBits, std::size_t MantissaBits, sign S = sign::is_signed>
class floatmp;

namespace fdetail {

template <typename T>
constexpr std::uint64_t abs_u64(T v) noexcept {
    return v < 0 ? (static_cast<std::uint64_t>(-(v + 1)) + 1ull) : static_cast<std::uint64_t>(v);
}

template <std::size_t TBa, std::size_t MBa, sign Sa, std::size_t TBb, std::size_t MBb, sign Sb>
struct common {
    static constexpr std::size_t TB = (TBa >= TBb) ? TBa : TBb;
    static constexpr std::size_t MB = (TBa == TBb) ? ((MBa >= MBb) ? MBa : MBb) : ((TBa > TBb) ? MBa : MBb);
    static constexpr sign S = (Sa == sign::is_signed || Sb == sign::is_signed) ? sign::is_signed : sign::is_unsigned;
    using type = floatmp<TB, MB, S>;
};

} // namespace fdetail

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FLOAT_HPP