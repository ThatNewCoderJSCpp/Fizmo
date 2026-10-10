#ifndef FIZMO_FIXED_FLOAT_TYPES_HPP
#define FIZMO_FIXED_FLOAT_TYPES_HPP

#include "signed_float.hpp"

namespace fizmo {

namespace multiprecision {

template <std::size_t TotalBits, std::size_t MantissaBits>
using Float = floatmp<TotalBits, MantissaBits, sign::is_signed>;

template <std::size_t TotalBits, std::size_t MantissaBits>
using UFloat = floatmp<TotalBits, MantissaBits, sign::is_unsigned>;

template <std::size_t MantissaBits>
using Float8 = Float<8, MantissaBits>;

template <std::size_t MantissaBits>
using Float16 = Float<16, MantissaBits>;

template <std::size_t MantissaBits>
using Float32 = Float<32, MantissaBits>;

template <std::size_t MantissaBits>
using Float64 = Float<64, MantissaBits>;

template <std::size_t MantissaBits>
using Float128 = Float<128, MantissaBits>;

template <std::size_t MantissaBits>
using Float256 = Float<256, MantissaBits>;

template <std::size_t MantissaBits>
using Float512 = Float<512, MantissaBits>;

template <std::size_t MantissaBits>
using Float1024 = Float<1024, MantissaBits>;

template <std::size_t MantissaBits>
using Float2048 = Float<2048, MantissaBits>;

template <std::size_t MantissaBits>
using Float4096 = Float<4096, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat8 = UFloat<8, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat16 = UFloat<16, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat32 = UFloat<32, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat64 = UFloat<64, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat128 = UFloat<128, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat256 = UFloat<256, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat512 = UFloat<512, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat1024 = UFloat<1024, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat2048 = UFloat<2048, MantissaBits>;

template <std::size_t MantissaBits>
using UFloat4096 = UFloat<4096, MantissaBits>;

} // namespace multiprecision

namespace fdetail {
    constexpr std::size_t ilog2(std::size_t n) noexcept { std::size_t r = 0; while (n > 1) { n >>= 1; ++r; } return r; }
} // namespace fdetail

template <std::size_t TotalBits>
struct fizmo_standard_exp_for_bits {
private:
    static constexpr std::size_t ieee_like(std::size_t bits) {
        return bits <= 8  ? 3  : 
               bits <= 16 ? 5  : 
               bits <= 32 ? 8  :
               bits <= 64 ? 11 :
               0; // unreachable for <=64 branch
    }

public:
    static constexpr std::size_t value = (TotalBits <= 64) ? ieee_like(TotalBits) : 16 + 2 * (fdetail::ilog2(TotalBits) - 7);
};

template <std::size_t TotalBits>
constexpr std::size_t fizmo_standard_exp_for_bits_v = fizmo_standard_exp_for_bits<TotalBits>::value;

template <typename T>
struct fizmo_standard_exp;

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_standard_exp<multiprecision::floatmp<TB, MB, S>> {
    static constexpr std::size_t value = fizmo_standard_exp_for_bits<TB>::value;
};

template <typename T>
inline constexpr std::size_t fizmo_standard_exp_v = fizmo_standard_exp<T>::value;

namespace multiprecision {

template <std::size_t TotalBits>
using StandardFloat = Float<TotalBits, TotalBits - fizmo_standard_exp_for_bits_v<TotalBits>>;

template <std::size_t TotalBits>
using StandardUFloat = UFloat<TotalBits, TotalBits - fizmo_standard_exp_for_bits_v<TotalBits>>;

using float8    = Float8<8       - fizmo_standard_exp_for_bits_v<8>>;
using float16   = Float16<16     - fizmo_standard_exp_for_bits_v<16>>;
using float32   = Float32<32     - fizmo_standard_exp_for_bits_v<32>>;
using float64   = Float64<64     - fizmo_standard_exp_for_bits_v<64>>;
using float128  = Float128<128   - fizmo_standard_exp_for_bits_v<128>>;
using float256  = Float256<256   - fizmo_standard_exp_for_bits_v<256>>;
using float512  = Float512<512   - fizmo_standard_exp_for_bits_v<512>>;
using float1024 = Float1024<1024 - fizmo_standard_exp_for_bits_v<1024>>;
using float2048 = Float2048<2048 - fizmo_standard_exp_for_bits_v<2048>>;
using float4096 = Float4096<4096 - fizmo_standard_exp_for_bits_v<4096>>;

using ufloat8    = UFloat8<8       - fizmo_standard_exp_for_bits_v<8>>;
using ufloat16   = UFloat16<16     - fizmo_standard_exp_for_bits_v<16>>;
using ufloat32   = UFloat32<32     - fizmo_standard_exp_for_bits_v<32>>;
using ufloat64   = UFloat64<64     - fizmo_standard_exp_for_bits_v<64>>;
using ufloat128  = UFloat128<128   - fizmo_standard_exp_for_bits_v<128>>;
using ufloat256  = UFloat256<256   - fizmo_standard_exp_for_bits_v<256>>;
using ufloat512  = UFloat512<512   - fizmo_standard_exp_for_bits_v<512>>;
using ufloat1024 = UFloat1024<1024 - fizmo_standard_exp_for_bits_v<1024>>;
using ufloat2048 = UFloat2048<2048 - fizmo_standard_exp_for_bits_v<2048>>;
using ufloat4096 = UFloat4096<4096 - fizmo_standard_exp_for_bits_v<4096>>;

} // namespace multiprecision

template <typename>
struct is_fizmo_float : std::false_type {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct is_fizmo_float<multiprecision::floatmp<TB, MB, S>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_float_v = is_fizmo_float<T>::value;

template <typename>
struct is_fizmo_fixed_float : std::false_type {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct is_fizmo_fixed_float<multiprecision::floatmp<TB, MB, S>> : std::true_type {};

template <typename T>
constexpr bool is_fizmo_fixed_float_v = is_fizmo_fixed_float<T>::value; 

template <std::size_t TB, std::size_t MB>
struct is_fizmo_signed<multiprecision::floatmp<TB, MB, multiprecision::sign::is_signed>> : std::true_type {};

template <std::size_t TB, std::size_t MB>
struct is_fizmo_unsigned<multiprecision::floatmp<TB, MB, multiprecision::sign::is_unsigned>> : std::true_type {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_make_signed<multiprecision::floatmp<TB, MB, S>> {
    using type = multiprecision::floatmp<TB, MB, multiprecision::sign::is_signed>;
};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_make_unsigned<multiprecision::floatmp<TB, MB, S>> {
    using type = multiprecision::floatmp<TB, MB, multiprecision::sign::is_unsigned>;
};

namespace fdetail {
    template <class F, multiprecision::sign S> struct float_lowest;

    template <class F>
    struct float_lowest<F, multiprecision::sign::is_signed> {
        static F get() noexcept { return F::lowest(); }
    };

    template <class F>
    struct float_lowest<F, multiprecision::sign::is_unsigned> {
        static F get() noexcept { return F::zero(); }   
    };
} // namespace fdetail

template <typename T>
struct fizmo_float_traits;   

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_float_traits<multiprecision::floatmp<TB, MB, S>> {
private:
    static constexpr long double LOG10_2 = 0.301029995663981195L;
    static constexpr std::uint64_t U1 = 1;

public:
    using value_type = multiprecision::floatmp<TB, MB, S>;
    using int_type   = typename value_type::exponent_type;             
    using uint_type  = fizmo_make_unsigned_t<int_type>; 
    
    using store_type    = typename value_type::store_t;
    using sstore_type   = typename value_type::sstore_t;
    using guard_type    = typename value_type::guard_t;
    using signed_type   = fizmo_make_signed_t<value_type>;
    using unsigned_type = fizmo_make_unsigned_t<value_type>;

    static constexpr multiprecision::sign signedness = S;
    static constexpr bool is_signed_float   = (S == multiprecision::sign::is_signed);
    static constexpr bool is_unsigned_float = (S == multiprecision::sign::is_unsigned);

    static constexpr std::size_t total_bits    = TB;
    static constexpr std::size_t mantissa_bits = MB;
    static constexpr std::size_t exp_bits      = TB - MB;

    static constexpr std::size_t standard_exp      = fizmo_standard_exp_for_bits<TB>::value;
    static constexpr std::size_t standard_mantissa = total_bits - standard_exp;
    static constexpr bool        is_standard       = (mantissa_bits == standard_mantissa);

    static constexpr std::size_t digits               = MB + 1;
    static constexpr std::size_t digits10             = static_cast<std::size_t>(digits * LOG10_2);
    static constexpr std::size_t max_digits10         = static_cast<std::size_t>(1 + digits * LOG10_2 + 0.9999999999999999L);
    static constexpr std::size_t decimal_digits_exact = static_cast<std::size_t>(digits * LOG10_2 + 0.9999999999999999L);

    static constexpr uint_type exp_bias       = (uint_type(U1) << (exp_bits - 1)) - uint_type(U1);
    static constexpr uint_type max_biased_exp = (uint_type(U1) << exp_bits) - uint_type(U1);
    static constexpr uint_type mantissa_mask  = (uint_type(U1) << mantissa_bits) - uint_type(U1);
    static constexpr uint_type max_exact_int  = (uint_type(U1) << digits) - uint_type(U1);
    static constexpr uint_type max_exp        = max_biased_exp - exp_bias - uint_type(U1);

    static constexpr int_type  min_exact_int  = -int_type(max_exact_int);           
    static constexpr int_type  min_exp        = int_type(U1) - int_type(exp_bias);

    static constexpr unsigned int radix           = 2;
    static constexpr bool         has_infinity    = true;
    static constexpr bool         has_quiet_nan   = true;
    static constexpr bool         has_undefined   = true;
    static constexpr bool         has_denorm      = true;
    static constexpr bool         has_signed_zero = is_signed_float;
    static constexpr bool         is_exact        = false;
    static constexpr bool         is_integer      = false;
    static constexpr bool         is_bounded      = true;
    static constexpr bool         is_iec559       = false;

    static value_type zero()      noexcept { return value_type::zero();      }
    static value_type one()       noexcept { return value_type(1);           }
    static value_type min()       noexcept { return value_type::min();       }  
    static value_type max()       noexcept { return value_type::max();       }
    static value_type epsilon()   noexcept { return value_type::epsilon();   }
    static value_type infinity()  noexcept { return value_type::infinity();  }
    static value_type nan()       noexcept { return value_type::nan();       }
    static value_type undefined() noexcept { return value_type::undefined(); }

    static value_type lowest()      noexcept { return fdetail::float_lowest<value_type, S>::get(); }
    static value_type round_error() noexcept { return value_type(0.5); }
    static value_type denorm_min()  noexcept { return value_type::ldexp(uint_type(U1), min_exp); }

    static value_type negative_zero() noexcept {
        static_assert(is_signed_float, "negative_zero() requires a signed floatmp");
        return value_type::zero(true);
    }

    static value_type negative_infinity() noexcept {
        static_assert(is_signed_float, "negative_infinity() requires a signed floatmp");
        return value_type::infinity(true);
    }
};

template <typename Int, typename Enable = void>
struct fizmo_float_from_int;

template <typename T>
struct fizmo_float_from_int<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    using type = double;
};

template <std::size_t Bits, multiprecision::sign S>
struct fizmo_float_from_int<multiprecision::integer<Bits, S>, void> {
    static constexpr std::size_t exp_bits = fizmo_standard_exp_for_bits_v<Bits>;
    static constexpr std::size_t mantissa_bits = Bits - exp_bits;

    using type = multiprecision::floatmp<
        Bits,
        mantissa_bits,
        S
    >;
};

template <typename T>
using fizmo_float_from_int_t = typename fizmo_float_from_int<T>::type;

template <typename Float, typename Enable = void>
struct fizmo_int_from_float;

template <typename T>
struct fizmo_int_from_float<
    T,
    typename std::enable_if<std::is_floating_point<T>::value>::type
> {
    using type = multiprecision::int128;
};

template <
    std::size_t Bits,
    std::size_t MantissaBits,
    multiprecision::sign S
>
struct fizmo_int_from_float<
    multiprecision::floatmp<Bits, MantissaBits, S>,
    void
> {
    static constexpr std::size_t int_bits = Bits; 

    using type = multiprecision::integer<
        int_bits,
        S
    >;
};

template <typename T>
using fizmo_int_from_float_t = typename fizmo_int_from_float<T>::type;

template <class T, class = void>
struct fizmo_float_rank : std::integral_constant<std::size_t, 0> {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_float_rank<
    multiprecision::floatmp<TB, MB, S>,
    void
> : std::integral_constant<std::size_t, TB * 2 + 1> {};

template <class T, class = void>
struct fizmo_type_rank : std::integral_constant<std::size_t, integer_rank<T>::value> {};

template <class T>
struct fizmo_type_rank<T, typename std::enable_if<is_fizmo_float_v<T>>::type> : std::integral_constant<std::size_t, fizmo_float_rank<T>::value> {};

template <class T, class = void>
struct fizmo_type_bits;

template <std::size_t B, multiprecision::sign S>
struct fizmo_type_bits<multiprecision::integer<B, S>, void> : std::integral_constant<std::size_t, B> {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_type_bits<multiprecision::floatmp<TB, MB, S>, void> : std::integral_constant<std::size_t, TB> {};

template <class T>
constexpr std::size_t fizmo_type_bits_v = fizmo_type_bits<T>::value;

template <class T, class = void>
struct fizmo_float_mantissa_bits : std::integral_constant<std::size_t, 0> {};

template <std::size_t TB, std::size_t MB, multiprecision::sign S>
struct fizmo_float_mantissa_bits<multiprecision::floatmp<TB, MB, S>, void> : std::integral_constant<std::size_t, MB> {};

template <class T>
constexpr std::size_t fizmo_float_mantissa_bits_v = fizmo_float_mantissa_bits<T>::value;

template <class T>
struct fizmo_type_is_float : std::integral_constant<bool, is_fizmo_float_v<T>> {};

template <class T>
struct fizmo_type_is_signed : std::integral_constant<bool, is_fizmo_signed_v<T>> {};

template <std::size_t Bits, bool Signed>
struct fizmo_make_int_from_bits {
    using type = multiprecision::integer<
        Bits,
        Signed ? multiprecision::sign::is_signed : multiprecision::sign::is_unsigned
    >;
};


template <std::size_t Bits, std::size_t MantissaBits, bool Signed>
struct fizmo_make_float_from_bits_and_mantissa {
    using type = multiprecision::floatmp<
        Bits,
        MantissaBits,
        Signed ? multiprecision::sign::is_signed : multiprecision::sign::is_unsigned
    >;
};

template <std::size_t Bits>
struct fizmo_standard_mantissa_for_bits {
    static constexpr std::size_t exp_bits = fizmo_standard_exp_for_bits_v<Bits>;
    static constexpr std::size_t value = Bits - exp_bits;
};

template <std::size_t ResultBits, class A, class B>
struct fizmo_select_mantissa_for_bits {
private:
    static constexpr bool a_is_float = fizmo_type_is_float<A>::value && (fizmo_type_bits_v<A> == ResultBits);
    static constexpr bool b_is_float = fizmo_type_is_float<B>::value && (fizmo_type_bits_v<B> == ResultBits);
    static constexpr std::size_t a_mb = fizmo_float_mantissa_bits_v<A>;
    static constexpr std::size_t b_mb = fizmo_float_mantissa_bits_v<B>;

public:
    static constexpr std::size_t value =
        a_is_float && b_is_float ? (a_mb >= b_mb ? a_mb : b_mb) :
        a_is_float               ? a_mb :
        b_is_float               ? b_mb :
        fizmo_standard_mantissa_for_bits<ResultBits>::value;
};

template <class A, class B, class = void>
struct fizmo_common_type {
private:
    static constexpr std::size_t bitsA = fizmo_type_bits_v<A>;
    static constexpr std::size_t bitsB = fizmo_type_bits_v<B>;
    static constexpr std::size_t bits  = bitsA >= bitsB ? bitsA : bitsB;
    static constexpr bool any_float = fizmo_type_is_float<A>::value || fizmo_type_is_float<B>::value;
    static constexpr bool any_signed = fizmo_type_is_signed<A>::value || fizmo_type_is_signed<B>::value;
    static constexpr std::size_t mantissa = fizmo_select_mantissa_for_bits<bits, A, B>::value;

public:
    using type = typename std::conditional<
        any_float,
        typename fizmo_make_float_from_bits_and_mantissa<bits, mantissa, any_signed>::type,
        typename fizmo_make_int_from_bits<bits, any_signed>::type
    >::type;
};

template <class A, class B>
using fizmo_common_type_t = typename fizmo_common_type<A, B>::type;

template <typename T, typename Enable = void>
struct fizmo_promote;

template <typename T>
struct fizmo_promote<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    using type = T; 
};

template <std::size_t Bits, fizmo::multiprecision::sign S>
struct fizmo_promote<
    fizmo::multiprecision::integer<Bits, S>,
    void
> {
    using type = fizmo::multiprecision::integer<Bits * 2, S>;
};

template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
struct fizmo_promote<
    fizmo::multiprecision::floatmp<TB, MB, S>,
    void
> {
    using type = fizmo::multiprecision::floatmp<TB * 2, MB * 2, S>;
};

template <typename T>
using fizmo_promote_t = typename fizmo_promote<T>::type;

template <typename T, typename Enable = void>
struct fizmo_demote;

template <typename T>
struct fizmo_demote<T, typename std::enable_if<std::is_integral<T>::value>::type> {
    using type = T; 
};

template <std::size_t Bits, fizmo::multiprecision::sign S>
struct fizmo_demote<
    fizmo::multiprecision::integer<Bits, S>,
    void
> {
    using type = fizmo::multiprecision::integer<Bits / 2, S>;
};

template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
struct fizmo_demote<
    fizmo::multiprecision::floatmp<TB, MB, S>,
    void
> {
    using type = fizmo::multiprecision::floatmp<TB / 2, MB / 2, S>;
};

template <typename T>
using fizmo_demote_t = typename fizmo_demote<T>::type;

template <typename T, typename Enable = void>
struct fizmo_numeric_bit_width;

template <std::size_t Bits, fizmo::multiprecision::sign S>
struct fizmo_numeric_bit_width<
    fizmo::multiprecision::integer<Bits, S>,
    void
> {
    static constexpr std::size_t value = Bits;
};

template <std::size_t TB, std::size_t MB, fizmo::multiprecision::sign S>
struct fizmo_numeric_bit_width<
    fizmo::multiprecision::floatmp<TB, MB, S>,
    void
> {
    static constexpr std::size_t value = TB;
};

template <typename T>
constexpr std::size_t fizmo_numeric_bit_width_v = fizmo_numeric_bit_width<T>::value;

} // namespace fizmo

#endif // FIZMO_FIXED_FLOAT_TYPES_HPP