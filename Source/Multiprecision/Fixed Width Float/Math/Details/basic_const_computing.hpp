#ifndef FIZMO_MULTIPRECISION_CONSTANT_COMPUTATIONS_HPP
#define FIZMO_MULTIPRECISION_CONSTANT_COMPUTATIONS_HPP

#include <tuple>
#include "../../unsigned_float.hpp"
#include "../../traits.hpp"

namespace fizmo {
namespace multiprecision {
namespace fmath {

template <class G>
long long top_bit(const typename G::store_t& m) noexcept { return m.highest_bit(); }

template <class G>
G scalb(const G& x, typename G::sstore_t n) noexcept {
    if (x.is_zero() || !x.is_finite()) return x;
    const auto t = x.frexp_signed();
    return G::ldexp(std::get<0>(t), std::get<1>(t) + n, std::get<2>(t));
}

template <class G>
G scalb_i(const G& x, long long n) noexcept { return scalb(x, typename G::sstore_t(n)); }

template <class G>
void split_pow2(const G& x, G& f, typename G::sstore_t& e) noexcept {
    using S = typename G::sstore_t;
    const auto t = x.frexp_signed();
    const typename G::store_t mant = std::get<0>(t);
    const long long lead = top_bit<G>(mant);
    if (lead < 0) { f = G::zero(std::get<2>(t)); e = S(0); return; }
    const S shift = S(static_cast<long long>(G::mantissa_bits)) - S(lead);
    e = std::get<1>(t) - shift;
    f = G::ldexp(mant, shift, std::get<2>(t));
}

template <class G>
double head_as_double(const G& v) noexcept {
    const auto t = v.frexp();
    const long long lead = top_bit<G>(t.first);
    if (lead < 0) return 0.0;
    const long long drop = (lead > 52) ? (lead - 52) : 0;
    long long ev = static_cast<long long>(t.second.get_lowest_bits());
    if (t.second.is_negative()) ev = -ev;
    const long long sh = ev - static_cast<long long>(G::mantissa_bits) + drop;
    if (sh >  1000) return std::numeric_limits<double>::infinity();
    if (sh < -1000) return 0.0;
    const double head = static_cast<double>((t.first >> drop).get_lowest_bits());
    return std::ldexp(head, static_cast<int>(sh));
}

template <class G>
G ipow_u(const G& b, std::uint64_t n) noexcept {
    G r(1), p(b);
    while (n) { if (n & 1u) r = r * p; n >>= 1u; if (n) p = p * p; }
    return r;
}

constexpr long long newton_steps(std::size_t target, std::size_t have = 48, long long k = 0) noexcept {
    return have >= target ? k : newton_steps(target, have * 2, k + 1);
}

template <class G>
constexpr long long newton_root_cap() noexcept {
    return (G::exponent_bits >= 10) ? 64u : ((1u << (G::exponent_bits - 1)) >> 2);
}

template <class F, bool Signed = is_fizmo_signed_v<F>>
struct float_cast_impl {
    template <class G>
    static F from(const G& g) noexcept { return F(g); }
};

template <class F>
struct float_cast_impl<F, false> {
    template <class G>
    static F from(const G& g) noexcept {
        using sibling = floatmp<F::math_bits, F::mantissa_bits, sign::is_signed>;
        const sibling s(g);                      
        const auto t = s.frexp_signed();         
        return F::ldexp(std::get<0>(t), std::get<1>(t), std::get<2>(t));
    }
};

template <class F, class G>
F float_cast(const G& g) noexcept { return float_cast_impl<F>::template from<G>(g); }

template <class G>
G div_u32(const G& x, std::uint32_t d) noexcept {
    using U = typename G::store_t;
    using S = typename G::sstore_t;
    if (d == 0) return G::undefined();
    if (d == 1) return x;
    if (x.is_zero() || !x.is_finite()) return x;
    const auto t   = x.frexp_signed();
    U          m   = std::get<0>(t);
    S          e   = std::get<1>(t);
    const bool neg = std::get<2>(t);
    const long long lead = m.highest_bit();
    if (lead < 0) return G::zero(neg);
    const long long room = static_cast<long long>(G::math_bits) - 1 - lead;
    if (room > 0) { m = m << static_cast<std::size_t>(room); e = e - S(room); }
    U q;
    const std::uint32_t r = m.divmod_small(d, q);
    if (r != 0 && (std::uint64_t(r) << 1) >= std::uint64_t(d)) q = q + U(std::uint64_t(1));                 
    return G::ldexp(q, e, neg);
}

} // namespace fmath

namespace constants {
namespace detail {

template <class F>
struct is_constant_source : std::integral_constant<bool, is_fizmo_float_v<F>> {};

template <class F>
constexpr bool is_constant_source_v = is_constant_source<F>::value;

template <class F, class R = F>
using require_float_t = typename std::enable_if<is_constant_source_v<F>, R>::type;

template <class F, class R = F>
using require_signed_float_t = typename std::enable_if<is_constant_source_v<F> && is_fizmo_signed_v<F>, R>::type;

} // namespace detail
} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_CONSTANT_COMPUTATIONS_HPP