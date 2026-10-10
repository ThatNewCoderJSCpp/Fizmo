#ifndef FIZMO_MULTIPRECISION_GCD_LCM_DETAIL_HPP
#define FIZMO_MULTIPRECISION_GCD_LCM_DETAIL_HPP

#include <initializer_list>
#include "min_max_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace gdetail {

using mmdetail::common2;
using mmdetail::common_n;
using mmdetail::empty_fold;

template <class T>
struct is_int_operand : std::integral_constant<bool, is_fizmo_int_v<T> || (std::is_integral<T>::value && !std::is_same<T, bool>::value)> {};

template <class T>
typename std::enable_if<std::is_signed<T>::value, typename std::make_unsigned<T>::type>::type
abs_u(T v) noexcept {
    using U = typename std::make_unsigned<T>::type;
    const U u = static_cast<U>(v);
    return (v < T(0)) ? static_cast<U>(U(0) - u) : u;
}

template <class T>
constexpr typename std::enable_if<std::is_unsigned<T>::value, T>::type
abs_u(T v) noexcept { return v; }

template <class U>
U gcd_u(U a, U b) noexcept {                          
    if (a == U(0)) return b;
    if (b == U(0)) return a;
    unsigned sa = 0, sb = 0;
    while ((a & U(1)) == U(0)) { a >>= 1; ++sa; }
    while ((b & U(1)) == U(0)) { b >>= 1; ++sb; }
    const unsigned sh = (sa < sb) ? sa : sb;

    for (;;) {
        if (b < a) { const U t = a; a = b; b = t; }   
        b = static_cast<U>(b - a);                     
        if (b == U(0)) break;
        while ((b & U(1)) == U(0)) b >>= 1;
    }

    return static_cast<U>(a << sh);
}

template <std::size_t B, sign S>
long long ctz(const integer<B, S>& x) noexcept {       
    const umag<B> m = x.magnitude();
    return (m & (umag<B>() - m)).highest_bit();        
}

template <std::size_t B, sign S>
integer<B, S> gcd_mp(integer<B, S> a, integer<B, S> b) noexcept {
    using I = integer<B, S>;
    if (a.is_undefined() || b.is_undefined()) return empty_fold<I>::get();
    a = math::abs(a);
    b = math::abs(b);
    if (a.is_zero()) return b;
    if (b.is_zero()) return a;
    const long long sa = ctz(a), sb = ctz(b);
    const long long sh = (sa < sb) ? sa : sb;
    a = a >> sa;
    b = b >> sb;

    for (;;) {
        if (b < a) { const I t = a; a = b; b = t; }
        b = b - a;
        if (b.is_zero()) break;
        b = b >> ctz(b);
    }

    return a << sh;
}

template <class C, bool Mp = is_fizmo_int_v<C>>
struct fold2 {
    static C gcd(const C& a, const C& b) noexcept { return gcd_mp(a, b); }

    static C lcm(const C& a, const C& b) noexcept {
        const C x = math::abs(a), y = math::abs(b);
        if (x.is_undefined() || y.is_undefined()) return empty_fold<C>::get();
        if (x.is_zero() || y.is_zero())           return C();       
        const C g = gcd_mp(x, y);
        const C q = x / g;                                          
        const C r = q * y;
        if ((r / y) != q) return empty_fold<C>::get();              
        return r;
    }
};

template <class C>
struct fold2<C, false> {
    using U = typename std::make_unsigned<C>::type;

    static C gcd(C a, C b) noexcept { return static_cast<C>(gcd_u<U>(abs_u(a), abs_u(b))); }

    static C lcm(C a, C b) noexcept {
        const U x = abs_u(a), y = abs_u(b);
        if (x == U(0) || y == U(0)) return C(0);
        const U q = static_cast<U>(x / gcd_u<U>(x, y));
        if (q > static_cast<U>(std::numeric_limits<U>::max() / y)) return C(0); 
        return static_cast<C>(q * y);
    }
};

} // namespace gdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_GCD_LCM_DETAIL_HPP