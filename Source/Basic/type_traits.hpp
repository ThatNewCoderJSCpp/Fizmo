#ifndef FIZMO_TYPE_TRAITS_HPP
#define FIZMO_TYPE_TRAITS_HPP

#include <array>
#include <vector>
#include <type_traits>
#include <ostream>
#include <cmath>
#include <utility>
#include <limits>

namespace fizmo {

template <typename T, typename = void>
struct has_ostream_operator : std::false_type{};

template <typename T>
struct has_ostream_operator<T, decltype(std::declval<std::ostream&>() << std::declval<T>(), void())> : std::true_type{};

template <typename T> 
constexpr bool has_ostream_operator_v = has_ostream_operator<T>::value;

template <typename T, typename = void>
struct has_to_string : std::false_type{};

template <typename T>
struct has_to_string<T, 
    std::enable_if_t<
        std::is_same<
            decltype(std::to_string(std::declval<T>())), 
            std::string
        >::value
    >
> : std::true_type{};

template <typename T> 
constexpr bool has_to_string_v = has_to_string<T>::value;

template <typename T> 
struct is_vector : std::false_type {};

template <typename T> 
struct is_vector<std::vector<T>> : std::true_type{};

template <typename T> 
constexpr bool is_vector_v = is_vector<T>::value;

template <typename T> 
struct is_array : std::false_type {};

template <typename T, size_t N> 
struct is_array<std::array<T, N>> : std::true_type{};

template <typename T> 
constexpr bool is_array_v = is_array<T>::value;

template <typename T> 
struct is_c_array : std::false_type{};

template <typename T> 
struct is_c_array<T[]> : std::true_type{};

template <typename T, size_t N> 
struct is_c_array<T[N]> : std::true_type{};

template <typename T> 
constexpr bool is_c_array_v = is_c_array<T>::value;

template <typename T> 
struct is_initializer_list : std::false_type {};

template <typename T> 
struct is_initializer_list<std::initializer_list<T>> : std::true_type{};

template <typename T> 
constexpr bool is_initializer_list_v = is_initializer_list<T>::value;

template <typename T> 
struct is_std_complex : std::false_type {};

template <typename T> 
struct is_std_complex<std::complex<T>> : std::true_type{};

template <typename T> 
constexpr bool is_std_complex_v = is_std_complex<T>::value;

template <typename T, typename Enable = void>
struct safe_make_signed {
    using type = T;  
};

template <typename T>
struct safe_make_signed<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    using type = typename std::make_signed<T>::type;
};

template <typename T>
using safe_make_signed_t = typename safe_make_signed<T>::type;

template <typename T, typename Enable = void>
struct safe_make_unsigned {
    using type = T;  
};

template <typename T>
struct safe_make_unsigned<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    using type = typename std::make_unsigned<T>::type;
};

template <typename T>
using safe_make_unsigned_t = typename safe_make_unsigned<T>::type;

template<class...> struct conjunction : std::true_type {};
template<class B1> struct conjunction<B1> : B1 {};

template<class B1, class... Bn>
struct conjunction<B1, Bn...> : std::conditional<B1::value, conjunction<Bn...>, B1>::type {};

template<typename T>
using enable_if_floating_point = typename std::enable_if<std::is_floating_point<T>::value, T>::type;

template<typename T>
using enable_if_arithmetic = typename std::enable_if<std::is_arithmetic<T>::value, T>::type;

template<typename T>
using enable_if_integer = typename std::enable_if<std::is_integral<T>::value, T>::type;

struct floating_point_traits {
    static constexpr bool is_float_ieee754_binary32 = 
        std::numeric_limits<float>::is_iec559 && 
        sizeof(float) == 4 &&
        std::numeric_limits<float>::digits == 24;
    
    static constexpr bool is_float_valid = sizeof(float) == 4;
    
    static constexpr bool is_double_ieee754_binary64 = 
        std::numeric_limits<double>::is_iec559 && 
        sizeof(double) == 8 &&
        std::numeric_limits<double>::digits == 53;
    
    static constexpr bool is_double_valid = sizeof(double) == 8;
    
    static constexpr bool is_long_double_same_as_double = 
        sizeof(long double) == 8 &&
        std::numeric_limits<long double>::digits == 53;
    
    static constexpr bool is_long_double_x87_80bit = 
        std::numeric_limits<long double>::is_iec559 && 
        std::numeric_limits<long double>::digits == 64 &&
        std::numeric_limits<long double>::max_exponent == 16384;
    
    static constexpr bool is_long_double_x87_96bit = 
        sizeof(long double) == 12 &&
        std::numeric_limits<long double>::digits == 64;
    
    static constexpr bool is_long_double_x87_128bit = 
        sizeof(long double) == 16 &&
        std::numeric_limits<long double>::digits == 64;
    
    static constexpr bool is_long_double_ieee754_binary128 = 
        std::numeric_limits<long double>::is_iec559 && 
        sizeof(long double) == 16 &&
        std::numeric_limits<long double>::digits == 113 &&
        std::numeric_limits<long double>::max_exponent == 16384;
    
    static constexpr bool is_long_double_ibm_double_double = 
        !std::numeric_limits<long double>::is_iec559 &&
        sizeof(long double) == 16 &&
        std::numeric_limits<long double>::digits >= 105 &&
        std::numeric_limits<long double>::digits <= 107; 
    
    static constexpr bool is_long_double_extended = 
        is_long_double_x87_80bit || 
        is_long_double_x87_96bit || 
        is_long_double_x87_128bit;
    
    static constexpr bool all_ieee754_standard = 
        is_float_ieee754_binary32 && 
        is_double_ieee754_binary64 &&
        (is_long_double_ieee754_binary128 || is_long_double_same_as_double);
    
    #ifdef OS_WINDOWS
        static constexpr bool platform_expected_format = 
            is_float_ieee754_binary32 && 
            is_double_ieee754_binary64 &&
            is_long_double_same_as_double;
    #elif defined(OS_LINUX) && (defined(ARCH_X86_64) || defined(ARCH_X86_32))
        static constexpr bool platform_expected_format = 
            is_float_ieee754_binary32 && 
            is_double_ieee754_binary64 &&
            is_long_double_x87_128bit;
    #elif defined(OS_MACOS) && defined(ARCH_X86_64)
        static constexpr bool platform_expected_format = 
            is_float_ieee754_binary32 && 
            is_double_ieee754_binary64 &&
            is_long_double_x87_128bit;
    #elif defined(OS_MACOS) && defined(ARCH_ARM64)
        static constexpr bool platform_expected_format = 
            is_float_ieee754_binary32 && 
            is_double_ieee754_binary64 &&
            is_long_double_same_as_double;
    #elif defined(ARCH_ARM64) || defined(ARCH_ARM32)
        static constexpr bool platform_expected_format = 
            is_float_ieee754_binary32 && 
            is_double_ieee754_binary64 &&
            (is_long_double_same_as_double || is_long_double_ieee754_binary128);
    #else
        static constexpr bool platform_expected_format = false; // unknown platform
    #endif
    
    template<typename T>
    static constexpr bool is_ieee754 = std::numeric_limits<T>::is_iec559;
    
    template<typename T>
    static constexpr bool is_standard_float = std::is_same<T, float>::value && is_float_ieee754_binary32;
    
    template<typename T>
    static constexpr bool is_standard_double = std::is_same<T, double>::value && is_double_ieee754_binary64;
    
    static constexpr unsigned int long_double_bits = static_cast<unsigned int>(sizeof(long double) * 8); 
    static constexpr unsigned int long_double_mantissa_bits = static_cast<unsigned int>(std::numeric_limits<long double>::digits);
};

} // namespace fizmo

#endif // FIZMO_TYPE_TRAITS