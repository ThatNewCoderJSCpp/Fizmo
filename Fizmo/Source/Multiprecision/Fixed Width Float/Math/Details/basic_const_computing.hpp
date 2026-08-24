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

constexpr double INV_SQRT2_D = 0.70710678118654752440;
constexpr double INV_SQRT3_D = 0.57735026918962576451;
constexpr double INV_SQRT5_D = 0.44721359549995793928;

template <class F>
struct is_constant_source : std::integral_constant<bool, is_fizmo_float_v<F>> {};

template <class F>
constexpr bool is_constant_source_v = is_constant_source<F>::value;

template <class F, class R = F>
using require_float_t = typename std::enable_if<is_constant_source_v<F>, R>::type;

template <class F, class R = F>
using require_signed_float_t = typename std::enable_if<is_constant_source_v<F> && is_fizmo_signed_v<F>, R>::type;

template <class F, bool Signed = is_fizmo_signed_v<F>>
struct negate_value {
    static F from(const F& v) noexcept { return -v; }
};

template <class F>
struct negate_value<F, false> {
    static F from(const F&) noexcept { return F::undefined(); }     
};

template <class F>
using guard_of = typename F::guard_t;

template <class F>
struct materialize {
    static_assert(is_constant_source_v<F>, "materialize<F> requires a fizmo multiprecision float type");

    using target = F;

    template <class G>
    static F from(const G& g) noexcept {
        static_assert(is_constant_source_v<G>, "materialize<F>::from requires a fizmo multiprecision float source");
        return fmath::float_cast<F>(g);
    }

    static F from(const F& v) noexcept { return v; }
};

template <class G>
G atanh_inv(unsigned q) noexcept {
    using U = typename G::store_t;    
    using S = typename G::sstore_t;
    const std::size_t   N  = G::math_bits - 8;               
    const std::uint32_t qq = static_cast<std::uint32_t>(q) * q;
    U term = (U(std::uint64_t(1)) << N).div_small(q);         
    U sum  = term;

    for (std::uint32_t k = 3; ; k += 2) {
        term = term.div_small(qq);
        if (term.is_zero()) break;
        sum = sum + term.div_small(k);
    }
    
    return G::ldexp(sum, S(static_cast<long long>(G::mantissa_bits)) - S(static_cast<long long>(N)));
}

template <class G, unsigned Q>
const G& atanh_inv_c() noexcept { static const G v = atanh_inv<G>(Q); return v; }

template <class G>
G atan_inv(unsigned q) noexcept {
    using U = typename G::store_t;
    using S = typename G::sstore_t;
    const std::size_t   N  = G::math_bits - 8;
    const std::uint32_t qq = static_cast<std::uint32_t>(q) * q;
    U term = (U(std::uint64_t(1)) << N).div_small(q);
    U pos  = term;
    U neg  = U();
    bool sub = true;

    for (std::uint32_t k = 3; ; k += 2, sub = !sub) {
        term = term.div_small(qq);
        if (term.is_zero()) break;
        const U t = term.div_small(k);
        if (t.is_zero()) break;
        if (sub) neg = neg + t; else pos = pos + t;
    }

    return G::ldexp(pos - neg, S(static_cast<long long>(G::mantissa_bits)) - S(static_cast<long long>(N)));
}

template <class G, unsigned Q>
const G& atan_inv_c() noexcept { static const G v = atan_inv<G>(Q); return v; }

template <class G>
G ln2_guard() noexcept {
    return G(14) * atanh_inv_c<G, 31>() + G(10) * atanh_inv_c<G, 49>() + G(6) * atanh_inv_c<G, 161>();
}

template <class G>
G ln10_guard() noexcept {
    return G(46) * atanh_inv_c<G, 31>() + G(34) * atanh_inv_c<G, 49>() + G(20) * atanh_inv_c<G, 161>();
}

template <class G> const G& ln2_c()   noexcept { static const G v = ln2_guard<G>();  return v; }
template <class G> const G& ln10_c()  noexcept { static const G v = ln10_guard<G>(); return v; }

template <class G> const G& inv_ln2_c()    noexcept { static const G v = ln2_c<G>().reciprocal();  return v; }
template <class G> const G& inv_ln10_c()   noexcept { static const G v = ln10_c<G>().reciprocal(); return v; }

template <class G>
G e_guard() noexcept {
    using U = typename G::store_t;
    using S = typename G::sstore_t;
    const std::size_t N = G::math_bits - 8;    
    U term = U(std::uint64_t(1)) << N;           
    U sum  = term;

    for (std::uint32_t k = 1; ; ++k) {
        term = (term + U(std::uint64_t(k / 2))).div_small(k);  
        if (term.is_zero()) break;
        sum = sum + term;
    }

    return G::ldexp(sum, S(static_cast<long long>(G::mantissa_bits)) - S(static_cast<long long>(N)));
}

template <class G>
const G& e_guard_c() noexcept { static const G v = e_guard<G>(); return v; }

template <class G>
G inv_sqrt_int_guard(std::uint32_t n, double inv_seed) noexcept {
    const G v(n), three(3);
    G y(inv_seed);
    const long long want = fmath::newton_steps(G::mantissa_bits + 16);
    const long long cap  = fmath::newton_root_cap<G>();
    const long long s    = (want > cap) ? cap : want;
    for (long long i = 0; i <= s; ++i) y = fmath::scalb_i(y * (three - v * y * y), -1);
    return y;
}

template <class G>
G sqrt_int_guard(std::uint32_t n, double inv_seed) noexcept {
    const G v(n), r = v * inv_sqrt_int_guard<G>(n, inv_seed);
    return fmath::scalb_i(r + v / r, -1);
}

template <class G>
G inv_sqrt_guard(const G& v) noexcept {
    const G three(3);
    G y(1.0 / std::sqrt(fmath::head_as_double(v)));
    const long long want = fmath::newton_steps(G::mantissa_bits + 16);
    const long long cap  = fmath::newton_root_cap<G>();
    const long long s    = (want > cap) ? cap : want;
    for (long long i = 0; i <= s; ++i) y = fmath::scalb_i(y * (three - v * y * y), -1);
    return y;
}

template <class G>
G sqrt_guard(const G& v) noexcept {
    const G r = v * inv_sqrt_guard<G>(v);
    return fmath::scalb_i(r + v / r, -1);
}

template <class G> G sqrt2_guard() noexcept { return sqrt_int_guard<G>(2, INV_SQRT2_D); }
template <class G> G sqrt3_guard() noexcept { return sqrt_int_guard<G>(3, INV_SQRT3_D); }
template <class G> G sqrt5_guard() noexcept { return sqrt_int_guard<G>(5, INV_SQRT5_D); }

template <class G> const G& sqrt2_guard_c() noexcept { static const G v = sqrt2_guard<G>(); return v; }
template <class G> const G& sqrt3_guard_c() noexcept { static const G v = sqrt3_guard<G>(); return v; }
template <class G> const G& sqrt5_guard_c() noexcept { static const G v = sqrt5_guard<G>(); return v; }

template <class G> G inv_sqrt2_guard() noexcept { return inv_sqrt_int_guard<G>(2, INV_SQRT2_D); }
template <class G> G inv_sqrt3_guard() noexcept { return inv_sqrt_int_guard<G>(3, INV_SQRT3_D); }
template <class G> G inv_sqrt5_guard() noexcept { return inv_sqrt_int_guard<G>(5, INV_SQRT5_D); }

template <class G> const G& inv_sqrt2_guard_c() noexcept { static const G v = inv_sqrt2_guard<G>(); return v; }
template <class G> const G& inv_sqrt3_guard_c() noexcept { static const G v = inv_sqrt3_guard<G>(); return v; }
template <class G> const G& inv_sqrt5_guard_c() noexcept { static const G v = inv_sqrt5_guard<G>(); return v; }

template <class G>
G phi_guard() noexcept { return fmath::scalb_i(G(1) + sqrt5_guard_c<G>(), -1); }

template <class G>
G inv_phi_guard() noexcept { return fmath::scalb_i(sqrt5_guard_c<G>() - G(1), -1); }

template <class G> const G& phi_guard_c()     noexcept { static const G v = phi_guard<G>();     return v; }
template <class G> const G& inv_phi_guard_c() noexcept { static const G v = inv_phi_guard<G>(); return v; }

template <class G>
G pi_guard() noexcept { return G(16) * atan_inv_c<G, 5>() - G(4) * atan_inv_c<G, 239>(); }

template <class G> const G& pi_guard_c() noexcept { static const G v = pi_guard<G>(); return v; }

template <class G> const G& two_pi_c()     noexcept { static const G v = fmath::scalb_i(pi_guard_c<G>(),  1); return v; }
template <class G> const G& half_pi_c()    noexcept { static const G v = fmath::scalb_i(pi_guard_c<G>(), -1); return v; }
template <class G> const G& quarter_pi_c() noexcept { static const G v = fmath::scalb_i(pi_guard_c<G>(), -2); return v; }
template <class G> const G& inv_pi_c()     noexcept { static const G v = pi_guard_c<G>().reciprocal();        return v; }

template <class G> const G& third_pi_c()   noexcept { static const G v = fmath::div_u32(pi_guard_c<G>(), 3u);  return v; }
template <class G> const G& pi_6_c()       noexcept { static const G v = fmath::div_u32(pi_guard_c<G>(), 6);   return v; }
template <class G> const G& pi_180_c()     noexcept { static const G v = fmath::div_u32(pi_guard_c<G>(), 180); return v; }
template <class G> const G& inv_pi_180_c() noexcept { static const G v = G(180) * inv_pi_c<G>();               return v; }
template <class G> const G& two_inv_pi_c() noexcept { static const G v = fmath::scalb_i(inv_pi_c<G>(),  1);    return v; }
template <class G> const G& inv_two_pi_c() noexcept { static const G v = fmath::scalb_i(inv_pi_c<G>(), -1);    return v; }

template <class U>
U mul_small(const U& x, std::uint64_t m) noexcept {
    U r = U(), p = x;
    while (m) { if (m & 1ull) r = r + p; m >>= 1; if (m) p = p << 1; }
    return r;
}

template <class G>
G zeta3_guard() noexcept {
    using U = typename G::store_t;
    using S = typename G::sstore_t;
    const std::size_t N = G::math_bits - 8;
    U term = (U(std::uint64_t(1)) << N).div_small(2u);        
    U pos  = term;
    U neg  = U();

    for (std::uint32_t k = 1; ; ++k) {
        term = mul_small(term, std::uint64_t(k) * k * k);
        term = term.div_small(2u);
        term = term.div_small(k + 1u);
        term = term.div_small(k + 1u);
        term = term.div_small(2u * k + 1u);
        if (term.is_zero()) break;
        if (k & 1u) neg = neg + term; else pos = pos + term;
    }

    const U s = mul_small(pos - neg, 5ull);                   
    return G::ldexp(s, S(static_cast<long long>(G::mantissa_bits)) - S(static_cast<long long>(N)) - S(1));
}

template <class G> const G& zeta2_c() noexcept { static const G v = fmath::div_u32(pi_guard_c<G>() * pi_guard_c<G>(), 6u); return v; }
template <class G> const G& apery_c() noexcept { static const G v = zeta3_guard<G>(); return v; }

template <class G> const G& sqrt_pi_c()         noexcept { static const G v = sqrt_guard<G>(pi_guard_c<G>());        return v; }
template <class G> const G& inv_sqrt_pi_c()     noexcept { static const G v = inv_sqrt_guard<G>(pi_guard_c<G>());    return v; }
template <class G> const G& two_inv_sqrt_pi_c() noexcept { static const G v = fmath::scalb_i(inv_sqrt_pi_c<G>(), 1); return v; }

} // namespace detail
} // namespace constants
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_CONSTANT_COMPUTATIONS_HPP