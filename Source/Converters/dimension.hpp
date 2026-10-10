#ifndef FIZMO_CORE_DIMENSION_HPP
#define FIZMO_CORE_DIMENSION_HPP

#include <type_traits>

namespace fizmo {
namespace units {

struct Dimension {
    int L; // length
    int M; // mass
    int T; // time
    int I; // electric current
    int K; // thermodynamic temperature
    int N; // amount of substance
    int J; // luminous intensity
    int A; // plane angle 

    constexpr Dimension(int l = 0, int m = 0, int t = 0, int i = 0, int k = 0, int n = 0, int j = 0, int a = 0) noexcept : L(l), M(m), T(t), I(i), K(k), N(n), J(j), A(a) {}

    constexpr Dimension operator*(Dimension o) const noexcept {
        return Dimension(L + o.L, M + o.M, T + o.T, I + o.I, K + o.K, N + o.N, J + o.J, A + o.A);
    }
    constexpr Dimension operator/(Dimension o) const noexcept {
        return Dimension(L - o.L, M - o.M, T - o.T, I - o.I, K - o.K, N - o.N, J - o.J, A - o.A);
    }
    constexpr Dimension pow(int n) const noexcept {
        return Dimension(L * n, M * n, T * n, I * n, K * n, N * n, J * n, A * n);
    }
    constexpr Dimension inverse() const noexcept {
        return Dimension(-L, -M, -T, -I, -K, -N, -J, -A);
    }
    constexpr bool operator==(Dimension o) const noexcept {
        return L == o.L && M == o.M && T == o.T && I == o.I && K == o.K && N == o.N && J == o.J && A == o.A;
    }
    constexpr bool operator!=(Dimension o) const noexcept { return !(*this == o); }
};

constexpr Dimension DIMENSION_DIMENSIONLESS = Dimension();
constexpr Dimension DIMENSION_LENGTH        = Dimension(1,0,0,0,0,0,0,0);
constexpr Dimension DIMENSION_MASS          = Dimension(0,1,0,0,0,0,0,0);
constexpr Dimension DIMENSION_TIME          = Dimension(0,0,1,0,0,0,0,0);
constexpr Dimension DIMENSION_CURRENT       = Dimension(0,0,0,1,0,0,0,0);
constexpr Dimension DIMENSION_TEMPERATURE   = Dimension(0,0,0,0,1,0,0,0);
constexpr Dimension DIMENSION_AMOUNT        = Dimension(0,0,0,0,0,1,0,0);
constexpr Dimension DIMENSION_LUMINOUS      = Dimension(0,0,0,0,0,0,1,0);
constexpr Dimension DIMENSION_ANGLE         = Dimension(0,0,0,0,0,0,0,1);

constexpr Dimension DIMENSION_FREQUENCY        = DIMENSION_DIMENSIONLESS / DIMENSION_TIME;        
constexpr Dimension DIMENSION_VELOCITY         = DIMENSION_LENGTH / DIMENSION_TIME;
constexpr Dimension DIMENSION_ACCELERATION     = DIMENSION_VELOCITY / DIMENSION_TIME;
constexpr Dimension DIMENSION_ANGULAR_VELOCITY = DIMENSION_ANGLE / DIMENSION_TIME;
constexpr Dimension DIMENSION_AREA             = DIMENSION_LENGTH * DIMENSION_LENGTH;
constexpr Dimension DIMENSION_VOLUME           = DIMENSION_LENGTH * DIMENSION_LENGTH * DIMENSION_LENGTH;
constexpr Dimension DIMENSION_DENSITY_2D       = DIMENSION_MASS / DIMENSION_AREA;
constexpr Dimension DIMENSION_DENSITY_3D       = DIMENSION_MASS / DIMENSION_VOLUME;
constexpr Dimension DIMENSION_MOMENTUM         = DIMENSION_MASS * DIMENSION_VELOCITY;
constexpr Dimension DIMENSION_FORCE            = DIMENSION_MASS * DIMENSION_ACCELERATION;
constexpr Dimension DIMENSION_IMPULSE          = DIMENSION_FORCE * DIMENSION_TIME;
constexpr Dimension DIMENSION_ENERGY           = DIMENSION_FORCE * DIMENSION_LENGTH;             
constexpr Dimension DIMENSION_POWER            = DIMENSION_ENERGY / DIMENSION_TIME;
constexpr Dimension DIMENSION_PRESSURE         = DIMENSION_FORCE / DIMENSION_AREA;
constexpr Dimension DIMENSION_ABSORBED_DOSE    = DIMENSION_ENERGY / DIMENSION_MASS;             
constexpr Dimension DIMENSION_CHARGE           = DIMENSION_CURRENT * DIMENSION_TIME;
constexpr Dimension DIMENSION_VOLTAGE          = DIMENSION_POWER / DIMENSION_CURRENT;
constexpr Dimension DIMENSION_RESISTANCE       = DIMENSION_VOLTAGE / DIMENSION_CURRENT;
constexpr Dimension DIMENSION_CONDUCTANCE      = DIMENSION_CURRENT / DIMENSION_VOLTAGE;
constexpr Dimension DIMENSION_CAPACITANCE      = DIMENSION_CHARGE / DIMENSION_VOLTAGE;
constexpr Dimension DIMENSION_INDUCTANCE       = DIMENSION_VOLTAGE * DIMENSION_TIME / DIMENSION_CURRENT;
constexpr Dimension DIMENSION_MAGNETIC_FLUX    = DIMENSION_VOLTAGE * DIMENSION_TIME;
constexpr Dimension DIMENSION_TORQUE           = DIMENSION_FORCE * DIMENSION_LENGTH;  
constexpr Dimension DIMENSION_MASS_FLOW_RATE   = DIMENSION_MASS   / DIMENSION_TIME;
constexpr Dimension DIMENSION_VOLUME_FLOW_RATE = DIMENSION_VOLUME / DIMENSION_TIME;
constexpr Dimension DIMENSION_AREA_FLOW_RATE   = DIMENSION_AREA / DIMENSION_TIME;
constexpr Dimension DIMENSION_MOLAR_FLOW_RATE  = DIMENSION_AMOUNT / DIMENSION_TIME;
constexpr Dimension DIMENSION_MOLARITY_VOLUME  = DIMENSION_AMOUNT / DIMENSION_VOLUME;  
constexpr Dimension DIMENSION_MOLARITY_MASS    = DIMENSION_AMOUNT / DIMENSION_MASS;   
constexpr Dimension DIMENSION_SPECIFIC_AREA    = DIMENSION_AREA / DIMENSION_MASS;  
constexpr Dimension DIMENSION_SPECIFIC_VOLUME  = DIMENSION_VOLUME / DIMENSION_MASS;    

} // namespace units

template <typename T> struct is_fizmo_dimension : std::false_type {};
template <> struct is_fizmo_dimension<units::Dimension> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_dimension_v = is_fizmo_dimension<T>::value;

} // namespace fizmo

#endif // FIZMO_CORE_DIMENSION_HPP