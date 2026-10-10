#ifndef FIZMO_PRIME_NUMBERS_HPP
#define FIZMO_PRIME_NUMBERS_HPP

#include <type_traits>

namespace fizmo {

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, bool>::type 
is_prime(const T n) noexcept {
    if (n <= 1) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;
    for (T i = 5; i * i <= n; i += 6) { if (n % i == 0 || n % (i + 2) == 0) return false; }
    return true;
}

template <typename T>
constexpr typename std::enable_if<std::is_integral<T>::value, T>::type
nth_prime(T n) noexcept {
    if (n < 1) return T(0); 
    if (n == 1) return T(2);
    T count = 1;  
    T candidate = 3;

    while (true) {
        if (is_prime(candidate)) {
            ++count;
            if (count == n) return candidate;
        }
        candidate += 2; 
    }
}

} // namespace fizmo

#endif // FIZMO_PRIME_NUMBERS_HPP