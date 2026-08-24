#ifndef FIZMO_MULTIPRECISION_GCD_LCM_HPP
#define FIZMO_MULTIPRECISION_GCD_LCM_HPP

#include <initializer_list>
#include "basic_constants.hpp"
#include "Details/gcd_lcm_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {

template <class A>
typename std::enable_if<gdetail::is_int_operand<A>::value, A>::type
gcd(const A& a) noexcept { return math::abs(a); }

template <class A>
typename std::enable_if<gdetail::is_int_operand<A>::value, A>::type
lcm(const A& a) noexcept { return math::abs(a); }

template <class A, class B>
typename std::enable_if<gdetail::is_int_operand<A>::value && gdetail::is_int_operand<B>::value, typename gdetail::common2<A, B>::type>::type
gcd(const A& a, const B& b) noexcept {
    using C = typename gdetail::common2<A, B>::type;
    return gdetail::fold2<C>::gcd(C(a), C(b));
}

template <class A, class B>
typename std::enable_if<gdetail::is_int_operand<A>::value && gdetail::is_int_operand<B>::value, typename gdetail::common2<A, B>::type>::type
lcm(const A& a, const B& b) noexcept {
    using C = typename gdetail::common2<A, B>::type;
    return gdetail::fold2<C>::lcm(C(a), C(b));
}

template <class A, class B, class C0, class... R>
typename gdetail::common_n<A, B, C0, R...>::type
gcd(const A& a, const B& b, const C0& c, const R&... rest) noexcept {
    return math::gcd(math::gcd(a, b), c, rest...);
}

template <class A, class B, class C0, class... R>
typename gdetail::common_n<A, B, C0, R...>::type
lcm(const A& a, const B& b, const C0& c, const R&... rest) noexcept {
    return math::lcm(math::lcm(a, b), c, rest...);
}

template <class T>
typename std::enable_if<gdetail::is_int_operand<T>::value, T>::type
gcd(std::initializer_list<T> xs) noexcept {
    T r = T(0);                                        
    
    for (typename std::initializer_list<T>::const_iterator it = xs.begin(); it != xs.end(); ++it) {
        r = gdetail::fold2<T>::gcd(r, *it);
        if (r == T(1)) break;                          
    }

    return r;
}

template <class T>
typename std::enable_if<gdetail::is_int_operand<T>::value, T>::type
lcm(std::initializer_list<T> xs) noexcept {
    T r = T(1);                                        
    
    for (typename std::initializer_list<T>::const_iterator it = xs.begin(); it != xs.end(); ++it) {
        r = gdetail::fold2<T>::lcm(r, *it);
        if (r == T(0)) break;                          
    }

    return r;
}

} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_GCD_LCM_HPP