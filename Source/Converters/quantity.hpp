#ifndef FIZMO_UNITS_QUANTITY_HPP
#define FIZMO_UNITS_QUANTITY_HPP

#include "Units/include.hpp"

namespace fizmo {
namespace units {

template <int L,int M,int T,int I,int K,int N,int J,int A>
struct Quantity {
    long double base;
    constexpr explicit Quantity(long double b = 0.0L) noexcept : base(b) {}
    static constexpr Dimension dimension() noexcept { return Dimension{L,M,T,I,K,N,J,A}; }
};

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> quantity(long double value, Unit<L,M,T,I,K,N,J,A> u) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(u.to_base(value));
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr long double in(Quantity<L,M,T,I,K,N,J,A> q, Unit<L,M,T,I,K,N,J,A> u) noexcept {
    return u.from_base(q.base);   
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>
constexpr Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>
operator*(Quantity<L1,M1,T1,I1,K1,N1,J1,A1> a, Quantity<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return Quantity<L1+L2,M1+M2,T1+T2,I1+I2,K1+K2,N1+N2,J1+J2,A1+A2>(a.base * b.base);
}

template <
    int L1,int M1,int T1,int I1,int K1,int N1,int J1,int A1,
    int L2,int M2,int T2,int I2,int K2,int N2,int J2,int A2
>

constexpr Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>
operator/(Quantity<L1,M1,T1,I1,K1,N1,J1,A1> a, Quantity<L2,M2,T2,I2,K2,N2,J2,A2> b) noexcept {
    return Quantity<L1-L2,M1-M2,T1-T2,I1-I2,K1-K2,N1-N2,J1-J2,A1-A2>(a.base / b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> operator+(Quantity<L,M,T,I,K,N,J,A> a, Quantity<L,M,T,I,K,N,J,A> b) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(a.base + b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> operator-(Quantity<L,M,T,I,K,N,J,A> a, Quantity<L,M,T,I,K,N,J,A> b) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(a.base - b.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> operator*(Quantity<L,M,T,I,K,N,J,A> a, long double k) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> operator*(long double k, Quantity<L,M,T,I,K,N,J,A> a) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(a.base * k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<L,M,T,I,K,N,J,A> operator/(Quantity<L,M,T,I,K,N,J,A> a, long double k) noexcept {
    return Quantity<L,M,T,I,K,N,J,A>(a.base / k);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A> reciprocal(Quantity<L,M,T,I,K,N,J,A> q) noexcept {
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A>(1.0L / q.base);
}

template <int L,int M,int T,int I,int K,int N,int J,int A>
constexpr Quantity<-L,-M,-T,-I,-K,-N,-J,-A> operator/(long double k, Quantity<L,M,T,I,K,N,J,A> q) noexcept {
    return Quantity<-L,-M,-T,-I,-K,-N,-J,-A>(k / q.base);
}

} // namespace units
} // namespace fizmo

#endif // FIZMO_UNITS_QUANTITY_HPP