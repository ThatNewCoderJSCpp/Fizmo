#ifndef FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP
#define FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP

#include "../abs_min_max.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

namespace edetail {

constexpr double LN2_D    = 0.69314718055994531;
constexpr double LOG10_2D = 0.30102999566398120;
constexpr double SQRT2_D  = 1.4142135623730951;
constexpr unsigned LOG_TABLE_N = 14;
constexpr long long ROOT_DIRECT_MAX = 1 << 12;   

template <class G>
struct log_table {
    G v[LOG_TABLE_N + 1];

    log_table() noexcept {
        const G two(2);
        for (unsigned i = 1; i <= LOG_TABLE_N; ++i) v[i] = constants::detail::atanh_inv<G>((1u << (i + 1)) + 1u) * two;
    }
};

template <class G>
const log_table<G>& log_tab() noexcept { static const log_table<G> t; return t; }

constexpr long long bias_ll(std::size_t exp_bits) noexcept {
    return (exp_bits >= 63) ? (1LL << 61) : ((1LL << (exp_bits - 1)) - 1LL);
}

inline long long isqrt_ll(long long n) noexcept {
    long long r = 0;
    while ((r + 1) * (r + 1) <= n) ++r;
    return r;
}

template <class F>
F inf_signed(bool neg) noexcept { return (neg && !is_fizmo_signed_v<F>) ? F::zero() : F::infinity(neg); }

template <class F>
F neg_infinity() noexcept { return inf_signed<F>(true); }   

template <class G>
long long round_to_ll(const G& x) noexcept {
    const G i = (x + (x.is_negative() ? G(-0.5) : G(0.5))).get_integer_part();
    const long long v = static_cast<long long>(i.get_integer_part_as_int().get_lowest_bits());
    return i.is_negative() ? -v : v;
}

template <class G>
inline G div_u32(const G& x, std::uint32_t d) noexcept {
    if (d <= 1 || x.is_zero() || !x.is_finite()) return x;
    using S = typename G::sstore_t;
    constexpr std::size_t sh = (G::exponent_bits > 33) ? 32 : (G::exponent_bits - 2);
    const auto t = x.frexp_signed();
    return G::ldexp((std::get<0>(t) << sh).div_small(d), std::get<1>(t) - S(static_cast<long long>(sh)), std::get<2>(t));
}

template <class G>
G exp_core(const G& x) noexcept {
    const G& l2 = constants::detail::ln2_c<G>();
    const long long k = round_to_ll(x / l2);          
    const G r0 = x - G(k) * l2;                       
    const long long P = static_cast<long long>(G::mantissa_bits);
    long long m = isqrt_ll(P);                      
    if (m > 32) m = 32;
    const G r = fmath::scalb_i(r0, -m);
    const typename G::sstore_t tiny(-(P + 8));
    G term(1), sum(1);

    for (long long n = 1; n <= P + 16; ++n) {         
        term = (term * r) / G(n);
        if (term.is_zero()) break;
        sum = sum + term;
        if (term.exponent_base2() < tiny) break;
    }

    for (long long i = 0; i < m; ++i) sum = sum * sum;
    return fmath::scalb_i(sum, k);
}

template <class G>
G log_reduce(const G& x, long long& k) noexcept {
    using S = typename G::sstore_t;
    S e; G f;
    fmath::split_pow2(x, f, e);                 
    k = static_cast<long long>(e.get_lowest_bits());
    if (e.is_negative()) k = -k;
    f = fmath::scalb_i(f, -1); ++k;
    const G one(1);
    G corr = G::zero();                          
    const log_table<G>& T = log_tab<G>();

    for (unsigned i = 1; i <= LOG_TABLE_N; ++i) {
        const G t = f + fmath::scalb_i(f, -static_cast<long long>(i));   
        if (!(t > one)) { f = t; corr = corr + T.v[i]; }
    }

    if (f == one) return G::zero() - corr;
    const G z  = (f - one) / (f + one);        
    const G z2 = z * z;
    const long long P = static_cast<long long>(G::mantissa_bits);
    const S tiny = z.exponent_base2() - S(P + 8);
    G term(z), sum(z);

    for (long long n = 3; n <= 2 * P + 16; n += 2) {
        term = term * z2;
        if (term.is_zero()) break;
        const G t = div_u32(term, static_cast<std::uint32_t>(n));
        if (t.is_zero()) break;
        sum = sum + t;
        if (t.exponent_base2() < tiny) break;
    }

    return (sum + sum) - corr;
}

template <class G>
G log_core(const G& x) noexcept {
    long long k = 0;
    const G lf = log_reduce(x, k);
    return k ? (lf + G(k) * constants::detail::ln2_c<G>()) : lf;
}

template <class G>
long long root_exp_ll(const typename G::sstore_t& e) noexcept {
    const long long v = static_cast<long long>(e.get_lowest_bits());
    return e.is_negative() ? -v : v;
}

template <class G>
long long root_steps() noexcept {
    const long long want = fmath::newton_steps(G::mantissa_bits + 16);
    const long long cap  = fmath::newton_root_cap<G>();
    return (want > cap) ? cap : want;
}

template <class G>
G sqrt_core(const G& x) noexcept {
    typename G::sstore_t e; G f;
    fmath::split_pow2(x, f, e);                       
    long long k = root_exp_ll<G>(e);
    if (k & 1) { f = fmath::scalb_i(f, 1); --k; }     
    const G three(3);
    G y(1.0 / std::sqrt(fmath::head_as_double(f)));  
    const long long n = root_steps<G>();
    for (long long i = 0; i <= n; ++i) y = fmath::scalb_i(y * (three - f * y * y), -1);
    G r = f * y;                                     
    r = fmath::scalb_i(r + f / r, -1);                
    return fmath::scalb_i(r, k / 2);
}

template <class G>
G cbrt_core(const G& x) noexcept {
    typename G::sstore_t e; G f;
    fmath::split_pow2(x, f, e);
    long long k = root_exp_ll<G>(e);
    long long m = k % 3;
    if (m < 0) m += 3;
    f = fmath::scalb_i(f, m); k -= m;                
    const G four(4);
    G y(1.0 / std::cbrt(fmath::head_as_double(f)));   
    const long long n = root_steps<G>();
    for (long long i = 0; i <= n; ++i) y = div_u32(y * (four - f * y * y * y), 3);
    G r = f * y * y;                                  
    r = div_u32(r + r + f / (r * r), 3);            
    return fmath::scalb_i(r, k / 3);
}

template <class F>
F signed_root(const F& mag, bool neg) noexcept {
    if (!neg) return mag;
    if (!is_fizmo_signed_v<F>)  return F::nan();
    if (mag.is_zero())          return F::zero(true);
    if (mag.is_infinite())      return F::infinity(true);
    return F::zero() - mag;
}

template <class G>
bool is_integral_value(const G& x) noexcept {
    return x.is_finite() && x.get_fractional_part().is_zero();
}

template <class G>
bool int_to_ll(const G& x, long long& n) noexcept {     
    n = 0;
    if (x.is_zero())   return true;
    if (!x.is_finite()) return false;
    if (!(x.exponent_base2() < typename G::sstore_t(63))) return false;
    n = static_cast<long long>(x.get_integer_part_as_int().get_lowest_bits());
    if (x.is_negative()) n = -n;
    return true;
}

template <class G>
bool odd_integer(const G& x) noexcept {                 
    if (x.is_zero()) return false;
    if (!(x.exponent_base2() < typename G::sstore_t(static_cast<long long>(G::mantissa_bits)))) return false;                                   
    return x.get_integer_part_as_int().get_bit(0);
}

template <class G>
G pow2_core(const G& t) noexcept {                      
    const G ip = t.get_integer_part();
    long long k = 0;
    int_to_ll(ip, k);
    return fmath::scalb_i(exp_core((t - ip) * constants::detail::ln2_c<G>()), k);
}

template <class F, class G>
F exp2_to(const G& t, bool neg) noexcept {
    const long long bias = bias_ll(F::math_bits - F::mantissa_bits);
    if (!t.is_finite()) return t.is_negative() ? signed_root(F::zero(), neg) : signed_root(F::infinity(), neg);
    const double hd = fmath::head_as_double(t);
    if (hd >  static_cast<double>(bias) + 2.0) return signed_root(F::infinity(), neg);
    if (hd < -static_cast<double>(bias + static_cast<long long>(F::mantissa_bits) + 2)) return signed_root(F::zero(), neg);
    return signed_root(fmath::float_cast<F>(pow2_core(t)), neg);
}

template <class F>
F pow_core(const F& a, const F& b) noexcept {
    using G = typename F::guard_t;
    using S = typename G::sstore_t;
    const F one(1);

    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (b.is_zero())                          return one;
    if (a == one)                             return one;
    if (b == one)                             return a;

    if (b.is_infinite()) {
        const F m = math::abs(a);
        if (m == one) return one;
        return ((m > one) != b.is_negative()) ? F::infinity() : F::zero();
    }

    const bool aneg = a.is_negative();
    const G    gb   = fmath::float_cast<G>(b);
    const bool bint = is_integral_value(gb);
    const bool rneg = aneg && bint && odd_integer(gb);
    long long  n    = 0;
    const bool nfit = bint && int_to_ll(gb, n);
    if (a.is_zero())     return b.is_negative() ? signed_root(F::infinity(), rneg) : signed_root(F::zero(),     rneg);
    if (a.is_infinite()) return b.is_negative() ? signed_root(F::zero(),     rneg) : signed_root(F::infinity(), rneg);
    if (aneg && !bint) return F::nan();                
    const G g = fmath::float_cast<G>(math::abs(a));
    const long long bias = bias_ll(F::math_bits - F::mantissa_bits);
    const double lo = -static_cast<double>(bias + static_cast<long long>(F::mantissa_bits) + 2);
    const double hi =  static_cast<double>(bias) + 2.0;

    if (nfit) {
        S e2; G f2;
        fmath::split_pow2(g, f2, e2);

        if (f2 == G(1)) {                              
            const long long k = root_exp_ll<G>(e2);
            const double t = static_cast<double>(k) * static_cast<double>(n);
            if (t > hi) return signed_root(F::infinity(), rneg);
            if (t < lo) return signed_root(F::zero(),     rneg);
            return signed_root(fmath::float_cast<F>(fmath::scalb_i(G(1), k * n)), rneg);
        }

        const std::uint64_t m = (n < 0) ? (static_cast<std::uint64_t>(-(n + 1)) + 1ull) : static_cast<std::uint64_t>(n);
        G r = fmath::ipow_u(g, m);                      
        if (n < 0) r = r.reciprocal();
        return signed_root(fmath::float_cast<F>(r), rneg);
    }

    {   
        long long m = 0;
        const F b2 = b + b;

        if (is_integral_value(b2) && int_to_ll(fmath::float_cast<G>(b2), m) && m > -64 && m < 64) {
            const std::uint64_t u = static_cast<std::uint64_t>(m < 0 ? -m : m);
            G r = fmath::ipow_u(sqrt_core(g), u);
            if (m < 0) r = r.reciprocal();
            return fmath::float_cast<F>(r);
        }

        const F b3 = b * F(3);

        if (is_integral_value(b3) && int_to_ll(fmath::float_cast<G>(b3), m) && m > -64 && m < 64) {
            const std::uint64_t u = static_cast<std::uint64_t>(m < 0 ? -m : m);
            G r = fmath::ipow_u(cbrt_core(g), u);
            if (m < 0) r = r.reciprocal();
            return fmath::float_cast<F>(r);
        }
    }

    long long k = 0;
    const G lf = log_reduce(g, k);                      
    const G t  = gb * (lf * constants::detail::inv_ln2_c<G>() + G(k));         
    if (!t.is_finite()) return t.is_negative() ? signed_root(F::zero(), rneg) : signed_root(F::infinity(), rneg);
    return exp2_to<F>(t, rneg);
}

template <class G>
G div_ll(const G& x, long long d) noexcept {
    if (d == 1) return x;
    if (d > 0 && d <= static_cast<long long>(0xFFFFFFFFu)) return div_u32(x, static_cast<std::uint32_t>(d));
    return x / G(d);
}

template <class G>
G root_log_path(const G& x, long long n) noexcept {
    long long k = 0;
    const G lf = log_reduce(x, k);
    return pow2_core(div_ll(lf * constants::detail::inv_ln2_c<G>() + G(k), n));
}

template <class G>
G root_core(const G& x, long long n) noexcept {
    if (n == 2) return sqrt_core(x);
    if (n == 3) return cbrt_core(x);
    if (n > ROOT_DIRECT_MAX) return root_log_path(x, n);
    typename G::sstore_t e; G f;
    fmath::split_pow2(x, f, e);                      
    long long k = root_exp_ll<G>(e);
    long long m = k % n;
    if (m < 0) m += n;
    k -= m;                                          
    if (m > 900) return root_log_path(x, n); // 2^m would break double precision, so use log path instead        
    f = fmath::scalb_i(f, m);                       
    const std::uint64_t un = static_cast<std::uint64_t>(n);
    const G np1(n + 1), nm1(n - 1);
    G y(std::pow(fmath::head_as_double(f), -1.0 / static_cast<double>(n)));
    const long long steps = root_steps<G>();
    for (long long i = 0; i <= steps; ++i) y = div_ll(y * (np1 - f * fmath::ipow_u(y, un)), n);
    G r = f * fmath::ipow_u(y, un - 1ull);          
    r = div_ll(nm1 * r + f / fmath::ipow_u(r, un - 1ull), n);   
    return fmath::scalb_i(r, k / n);
}

} // namespace edetail

namespace ldetail {

template <class T> struct is_mp_float : std::false_type {};
template <std::size_t TB, std::size_t MB, sign S>
struct is_mp_float<floatmp<TB, MB, S>> : std::true_type {};

template <class T> struct is_mp_int : std::false_type {};
template <std::size_t B, sign S>
struct is_mp_int<integer<B, S>> : std::true_type {};

template <class T>
struct is_operand : std::integral_constant<bool,
    std::is_arithmetic<T>::value || is_mp_float<T>::value || is_mp_int<T>::value> {};

template <class T, class = void>
struct as_float { using type = T; };                       

template <class T>
struct as_float<T, typename std::enable_if<std::is_integral<T>::value>::type> { using type = double; };

template <std::size_t B, sign S>
struct as_float<integer<B, S>, void> { using type = fizmo_float_from_int_t<integer<B, S>>; };

template <class FA, class FB, bool AMp, bool BMp> struct pick;
template <class FA, class FB> struct pick<FA, FB, true,  true > { using type = fizmo_common_type_t<FA, FB>; };
template <class FA, class FB> struct pick<FA, FB, true,  false> { using type = FA; };
template <class FA, class FB> struct pick<FA, FB, false, true > { using type = FB; };
template <class FA, class FB> struct pick<FA, FB, false, false> { using type = typename std::common_type<FA, FB>::type; };

template <class A, class B>
struct common_result {
    using FA = typename as_float<A>::type;
    using FB = typename as_float<B>::type;
    using type = typename pick<FA, FB, is_mp_float<FA>::value, is_mp_float<FB>::value>::type;
};

template <class F, class T>
F widen(const T& v) noexcept { return F(v); }

template <class F, std::size_t B, sign S>
F widen(const integer<B, S>& n) noexcept {                 
    using GI = typename fizmo_float_from_int_t<integer<B, S>>::guard_t;
    return fmath::float_cast<F>(GI(typename GI::sstore_t(n)));
}

template <class F>
F ratio(const F& a, const F& b) noexcept {
    using G = typename F::guard_t;
    if (a.is_undefined() || b.is_undefined()) return F::undefined();
    if (a.is_nan()       || b.is_nan())       return F::nan();
    if (a.is_negative()  || b.is_negative())  return F::nan();
    const F one(1);
    if (b == one) return (a == one) ? F::undefined() : F::nan();
    const bool a_inf = a.is_zero() || a.is_infinite();    
    const bool b_inf = b.is_zero() || b.is_infinite();
    if (a_inf && b_inf) return F::undefined();

    if (a_inf || b_inf) {
        const bool neg = (a < one) != (b < one);           
        return a_inf ? edetail::inf_signed<F>(neg) : F::zero(neg);
    }

    if (a == b) return one;  
    long long ka = 0, kb = 0;
    const G lfa = edetail::log_reduce(fmath::float_cast<G>(a), ka);
    const G lfb = edetail::log_reduce(fmath::float_cast<G>(b), kb);
    if (lfa.is_zero() && lfb.is_zero()) return fmath::float_cast<F>(G(ka) / G(kb));        
    const G& l2 = constants::detail::ln2_c<G>();
    const G la  = ka ? (lfa + G(ka) * l2) : lfa;
    const G lb  = kb ? (lfb + G(kb) * l2) : lfb;
    return fmath::float_cast<F>(la / lb);
}

template <class F, bool Mp = is_mp_float<F>::value>
struct apply {
    template <class A, class B>
    static F run(const A& a, const B& b) noexcept { return ratio<F>(widen<F>(a), widen<F>(b)); }
};

template <class F>
struct apply<F, false> {
    template <class A, class B>
    static F run(const A& a, const B& b) noexcept {
        return static_cast<F>(std::log(static_cast<F>(a)) / std::log(static_cast<F>(b)));
    }
};

} // namespace ldetail

namespace pdetail {

using ldetail::is_mp_float;
using ldetail::is_mp_int;
using ldetail::is_operand;
using ldetail::widen;

template <class T>
struct is_int_like : std::integral_constant<bool, is_mp_int<T>::value || std::is_integral<T>::value> {};

template <class A, class B>
struct kind : std::integral_constant<int,
    (std::is_arithmetic<A>::value && std::is_arithmetic<B>::value) ? 0
  : (is_mp_int<A>::value && is_int_like<B>::value)                 ? 1 : 2> {};

template <class A, class B, bool BMp = is_mp_int<B>::value>
struct int_result { using type = A; };

template <class A, class B>
struct int_result<A, B, true> { using type = fizmo_common_type_t<A, B>; };

template <class A, class B>
struct native_result {
    using C = typename std::common_type<A, B>::type;
    using type = typename std::conditional<std::is_floating_point<C>::value, C, double>::type;
};

template <class A, class B, int K = kind<A, B>::value> struct result;
template <class A, class B> struct result<A, B, 0> { using type = typename native_result<A, B>::type; };
template <class A, class B> struct result<A, B, 1> { using type = typename int_result<A, B>::type; };
template <class A, class B> struct result<A, B, 2> { using type = typename ldetail::common_result<A, B>::type; };

template <class T>
typename std::enable_if<std::is_integral<T>::value, bool>::type
exp_parts(T v, std::uint64_t& mag, bool& neg, bool& huge) noexcept {
    neg  = (v < T(0));
    huge = false;
    mag  = neg ? (static_cast<std::uint64_t>(-(v + T(1))) + 1ull) : static_cast<std::uint64_t>(v);
    return true;
}

template <std::size_t B, sign S>
bool exp_parts(const integer<B, S>& v, std::uint64_t& mag, bool& neg, bool& huge) noexcept {
    if (v.is_undefined()) return false;
    neg  = v.is_negative();
    huge = (v.magnitude().highest_bit() >= 64);
    mag  = v.get_lowest_bits();
    return true;
}

template <class R, class E>
R int_pow(const R& a, const E& e) noexcept {
    std::uint64_t m = 0;
    bool neg = false, huge = false;
    if (!exp_parts(e, m, neg, huge)) return mmdetail::empty_fold<R>::get();
    if (a.is_undefined())            return mmdetail::empty_fold<R>::get();
    if (!huge && m == 0)             return R(1);
    const bool odd  = (m & 1ull) != 0;
    const bool aneg = a.is_negative();
    const auto mag  = a.magnitude();
    if (mag.is_zero()) return neg ? mmdetail::empty_fold<R>::get() : R();   
    const long long hb = mag.highest_bit();
    if (hb == 0) return odd ? a : R(1);                 
    if (neg)  return R();                              
    if (huge) return R();                                

    if (!mag.any_bit_below(static_cast<std::size_t>(hb))) {       
        const std::uint64_t k = static_cast<std::uint64_t>(hb);
        const bool over = m > (static_cast<std::uint64_t>(R::bits) - 1ull) / k;
        const R r = over ? R() : (R(1) << static_cast<std::size_t>(k * m));
        return (aneg && odd) ? (R() - r) : r;
    }

    R r(1), p(a);

    for (std::uint64_t t = m; t != 0ull; t >>= 1) {
        if (t & 1ull) r = r * p;
        if (t >> 1)   p = p * p;
    }

    return r;
}

template <class A, class B, int K = kind<A, B>::value> struct apply;

template <class A, class B>
struct apply<A, B, 0> {
    static typename result<A, B>::type run(const A& a, const B& b) noexcept {
        using C = typename result<A, B>::type;
        return std::pow(static_cast<C>(a), static_cast<C>(b));
    }
};

template <class A, class B>
struct apply<A, B, 1> {
    static typename result<A, B>::type run(const A& a, const B& b) noexcept {
        using R = typename result<A, B>::type;
        return int_pow(R(a), b);
    }
};

template <class A, class B>
struct apply<A, B, 2> {
    static typename result<A, B>::type run(const A& a, const B& b) noexcept {
        using F = typename result<A, B>::type;
        return edetail::pow_core<F>(widen<F>(a), widen<F>(b));
    }
};

} // namespace pdetail

namespace rdetail {

using ldetail::is_mp_float;
using ldetail::is_mp_int;
using ldetail::is_operand;
using ldetail::widen;

struct degree {
    long long n;
    bool is_int, fits, odd, bad;
    degree() noexcept : n(0), is_int(false), fits(false), odd(false), bad(false) {}
};

template <class T>
typename std::enable_if<std::is_integral<T>::value, degree>::type
read_degree(T v) noexcept {                         
    degree d;
    d.is_int = true;
    d.odd    = ((static_cast<std::uint64_t>(v) & 1ull) != 0);
    const bool big = (sizeof(T) == 8 && std::is_unsigned<T>::value && static_cast<std::uint64_t>(v) > 0x7FFFFFFFFFFFFFFFull);
    
    if (!big) {
        d.n    = static_cast<long long>(v);
        d.fits = (d.n != std::numeric_limits<long long>::min());
    }

    return d;
}

template <std::size_t B, sign S>
degree read_degree(const integer<B, S>& v) noexcept {
    degree d;
    if (v.is_undefined()) { d.bad = true; return d; }
    d.is_int = true;
    d.odd    = ((v.get_lowest_bits() & 1ull) != 0);

    if (v.magnitude().highest_bit() < 63) {
        d.fits = true;
        d.n    = static_cast<long long>(v.get_lowest_bits());
        if (v.is_negative()) d.n = -d.n;
    }

    return d;
}

template <class T>
typename std::enable_if<std::is_floating_point<T>::value, degree>::type
read_degree(T v) noexcept {
    degree d;
    if (!(v == v)) { d.bad = true; return d; }
    if (!(v > -std::numeric_limits<T>::infinity() && v < std::numeric_limits<T>::infinity())) return d;
    if (v != std::floor(v)) return d;
    d.is_int = true;
    if (v >= -9.2e18 && v <= 9.2e18) { d.n = static_cast<long long>(v); d.fits = true; d.odd = ((d.n & 1) != 0); }
    return d;
}

template <std::size_t TB, std::size_t MB, sign S>
degree read_degree(const floatmp<TB, MB, S>& v) noexcept {
    using G = typename floatmp<TB, MB, S>::guard_t;
    degree d;
    if (v.is_undefined() || v.is_nan()) { d.bad = true; return d; }
    if (!v.is_finite()) return d;
    const G g = fmath::float_cast<G>(v);
    if (!edetail::is_integral_value(g)) return d;
    d.is_int = true;
    d.odd    = edetail::odd_integer(g);
    d.fits   = edetail::int_to_ll(g, d.n);
    if (d.n == std::numeric_limits<long long>::min()) d.fits = false;
    return d;
}

template <class F, class N>
F root_run(const F& x, const N& deg) noexcept {
    using G = typename F::guard_t;
    const degree d = read_degree(deg);
    if (x.is_undefined())    return F::undefined();
    if (x.is_nan() || d.bad) return F::nan();
    const bool xneg = x.is_negative();

    if (!d.is_int) {                                  
        const G gd = fmath::float_cast<G>(widen<F>(deg));
        if (!gd.is_finite()) return F(1);
        if (gd.is_zero())    return F::nan();
        if (xneg)            return F::nan();
        if (x.is_zero())     return gd.is_negative() ? F::infinity() : F::zero();
        if (x.is_infinite()) return gd.is_negative() ? F::zero() : F::infinity();
        if (x == F(1))       return F(1);
        long long k = 0;
        const G lf = edetail::log_reduce(fmath::float_cast<G>(x), k);
        return edetail::exp2_to<F>((lf * constants::detail::inv_ln2_c<G>() + G(k)) / gd, false);
    }

    if (xneg && !d.odd) return F::nan();              
    const bool rneg = xneg && d.odd;

    if (!d.fits) {                                  
        if (x.is_zero())     return F::zero(rneg);
        if (x.is_infinite()) return edetail::signed_root(F::infinity(), rneg);
        return edetail::signed_root(F(1), rneg);
    }

    const long long n = d.n;
    if (n == 0) return F::nan();
    if (x.is_zero())     return (n > 0) ? F::zero(rneg) : edetail::signed_root(F::infinity(), rneg);
    if (x.is_infinite()) return (n > 0) ? edetail::signed_root(F::infinity(), rneg) : F::zero(rneg);
    if (n == 1) return x;
    const G g = fmath::float_cast<G>(math::abs(x));
    const long long m = (n < 0) ? -n : n;
    G r = (m == 1) ? g : edetail::root_core(g, m);
    if (n < 0) r = r.reciprocal();
    return edetail::signed_root(fmath::float_cast<F>(r), rneg);
}

template <class X, class N, bool Native = (std::is_arithmetic<X>::value && std::is_arithmetic<N>::value)>
struct result { using type = typename ldetail::common_result<X, N>::type; };

template <class X, class N>
struct result<X, N, true> { using type = typename pdetail::native_result<X, N>::type; };

template <class F, bool Mp = is_mp_float<F>::value>
struct apply {
    template <class X, class N>
    static F run(const X& x, const N& n) noexcept { return root_run<F>(widen<F>(x), n); }
};

template <class F>
struct apply<F, false> {
    template <class X, class N>
    static F run(const X& x, const N& n) noexcept {
        const double dx = static_cast<double>(x), dn = static_cast<double>(n);
        if (dn == 2.0) return static_cast<F>(std::sqrt(dx));
        if (dn == 3.0) return static_cast<F>(std::cbrt(dx));

        if (dx < 0.0) {
            const bool odd_int = (dn == std::floor(dn)) && (std::fmod(dn, 2.0) != 0.0);
            return odd_int ? static_cast<F>(-std::pow(-dx, 1.0 / dn)) : std::numeric_limits<F>::quiet_NaN();
        }

        return static_cast<F>(std::pow(dx, 1.0 / dn));
    }
};

} // namespace rdetail

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_LOG_POW_ROOTS_DETAILS_HPP