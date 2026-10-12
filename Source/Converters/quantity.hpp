#ifndef FIZMO_UNITS_QUANTITY_HPP
#define FIZMO_UNITS_QUANTITY_HPP

#include "Units/include.hpp"
#include "convert.hpp"

namespace fizmo {
namespace units {

template <int L,int M,int T,int I,int K,int N,int J,int A, class V = long double>
struct Quantity {
    using value_type = V;

    V base;

    constexpr explicit Quantity(V b = V(0)) : base(b) {}

    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }
};

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> quantity(long double value, Unit<L,M,T,I,K,N,J,A> u) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(u.to_base(value));
}

template <class V, int L,int M,int T,int I,int K,int N,int J,int A, typename std::enable_if<detail::is_mp_value<V>::value, int>::type = 0>
Quantity<L,M,T,I,K,N,J,A, detail::value_result_t<V>> quantity(const V& value, Unit<L,M,T,I,K,N,J,A> u) {
    return Quantity<L,M,T,I,K,N,J,A, detail::value_result_t<V>>(to_base(value, u));
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat> quantity(const multiprecision::BigFloat& value, Unit<L,M,T,I,K,N,J,A> u, const multiprecision::BigFloatContext& ctx) {
    return Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat>(to_base(value, u, ctx));
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr long double in(Quantity<L,M,T,I,K,N,J,A> q, Unit<L,M,T,I,K,N,J,A> u) noexcept {
    return u.from_base(q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V, typename std::enable_if<detail::is_mp_value<V>::value, int>::type = 0>
V in(const Quantity<L,M,T,I,K,N,J,A,V>& q, Unit<L,M,T,I,K,N,J,A> u) {
    return from_base(q.base, u);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
multiprecision::BigFloat in(const Quantity<L,M,T,I,K,N,J,A, multiprecision::BigFloat>& q, Unit<L,M,T,I,K,N,J,A> u, const multiprecision::BigFloatContext& ctx) {
    return from_base(q.base, u, ctx);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class V
>
constexpr Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2,V>
operator*(const Quantity<L1,M1,T1,I1,K1,N1,J1,A1,V>& a, const Quantity<L2,M2,T2,I2,K2,N2,J2,A2,V>& b) {
    return Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2,V>(a.base * b.base);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2, class V
>
constexpr Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2,V>
operator/(const Quantity<L1,M1,T1,I1,K1,N1,J1,A1,V>& a, const Quantity<L2,M2,T2,I2,K2,N2,J2,A2,V>& b) {
    return Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2,V>(a.base / b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator+(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) {
    return Quantity<L,M,T,I,K,N,J,A,V>(a.base + b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator-(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) {
    return Quantity<L,M,T,I,K,N,J,A,V>(a.base - b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator-(const Quantity<L,M,T,I,K,N,J,A,V>& a) {
    return Quantity<L,M,T,I,K,N,J,A,V>(V(0) - a.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator*(const Quantity<L,M,T,I,K,N,J,A,V>& a, const typename Quantity<L,M,T,I,K,N,J,A,V>::value_type& k) {
    return Quantity<L,M,T,I,K,N,J,A,V>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator*(const typename Quantity<L,M,T,I,K,N,J,A,V>::value_type& k, const Quantity<L,M,T,I,K,N,J,A,V>& a) {
    return Quantity<L,M,T,I,K,N,J,A,V>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<L,M,T,I,K,N,J,A,V> operator/(const Quantity<L,M,T,I,K,N,J,A,V>& a, const typename Quantity<L,M,T,I,K,N,J,A,V>::value_type& k) {
    return Quantity<L,M,T,I,K,N,J,A,V>(a.base / k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V> reciprocal(const Quantity<L,M,T,I,K,N,J,A,V>& q) {
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V>(V(1) / q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V> operator/(const typename Quantity<L,M,T,I,K,N,J,A,V>::value_type& k, const Quantity<L,M,T,I,K,N,J,A,V>& q) {
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A,V>(k / q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator==(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return a.base == b.base; }

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator!=(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return !(a.base == b.base); }

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator<(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return a.base < b.base; }

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator>(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return b.base < a.base; }

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator<=(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return !(b.base < a.base); }

template <int L,int M,int T,int I,int K,int N,int J,int A, class V>
constexpr bool operator>=(const Quantity<L,M,T,I,K,N,J,A,V>& a, const Quantity<L,M,T,I,K,N,J,A,V>& b) { return !(a.base < b.base); }

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_QUANTITY_HPP
