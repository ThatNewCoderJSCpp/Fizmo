#ifndef FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP

#include <cstring>
#include <tuple>
#include <utility>
#include "float.hpp"
#include "decimal_conversion.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t TotalBits, std::size_t MantissaBits>
class floatmp<TotalBits, MantissaBits, sign::is_signed> {
    static_assert(MantissaBits >= 1 && MantissaBits + 2 <= TotalBits, "need at least 1 mantissa bit and at least 2 exponent bits");

    template <std::size_t, std::size_t, sign> friend class floatmp;

public:
    using store_t  = integer<TotalBits,     sign::is_unsigned>; 
    using sstore_t = integer<TotalBits,     sign::is_signed>;   
    using wide_t   = integer<TotalBits * 2, sign::is_unsigned>; 
    using guard_t  = floatmp<TotalBits * 2, TotalBits + MantissaBits - 8, sign::is_signed>;

    using exponent_type = sstore_t;

    using bit_index = long long;

    static constexpr std::size_t math_bits     = TotalBits;
    static constexpr std::size_t mantissa_bits = MantissaBits;
    static constexpr std::size_t exponent_bits = TotalBits - MantissaBits;
    static constexpr unsigned    radix         = 2;

    static constexpr bit_index mantissa_index = static_cast<bit_index>(MantissaBits);
    static constexpr bit_index total_index    = static_cast<bit_index>(TotalBits);
    static constexpr bit_index wide_index     = total_index * 2;

private:
    bool    m_is_negative;
    store_t m_data;

    struct raw_tag {};

    constexpr floatmp(raw_tag, bool neg, const store_t& bits) noexcept
        : m_is_negative(neg), m_data(bits) {}

    static OPTIONAL_CPP14_CONSTEXPR sstore_t to_signed(const store_t& u) noexcept { return sstore_t(u); }      
    static OPTIONAL_CPP14_CONSTEXPR store_t  to_unsigned(const sstore_t& s) noexcept { return store_t(s); }    

    static constexpr store_t one() noexcept { return store_t(std::uint64_t(1)); }

    static constexpr bit_index sv(const sstore_t& s) noexcept {
        return s.is_negative() ? -static_cast<bit_index>(s.get_lowest_bits()) : static_cast<bit_index>(s.get_lowest_bits());
    }

    static constexpr std::size_t lo(const sstore_t& s) noexcept {
        return (sv(s) <= 0) ? std::size_t(0) : static_cast<std::size_t>(sv(s));
    }

    static constexpr std::size_t lo(const store_t& s) noexcept {
        return static_cast<std::size_t>(s.get_lowest_bits());
    }

    static OPTIONAL_CPP14_CONSTEXPR bit_index top_bit(const store_t& v) noexcept {
        return static_cast<bit_index>(v.highest_bit());
    }

    static OPTIONAL_CPP14_CONSTEXPR bit_index top_bit_wide(const wide_t& v) noexcept {
        return static_cast<bit_index>(v.highest_bit());
    }

public:
    static constexpr sstore_t exponent_bias = sstore_t((store_t(std::uint64_t(1)) << (TotalBits - MantissaBits - 1)) - store_t(std::uint64_t(1)));
    static constexpr store_t  max_biased_exponent = (store_t(std::uint64_t(1)) << (TotalBits - MantissaBits)) - store_t(std::uint64_t(1));
    static constexpr store_t  mantissa_mask = (store_t(std::uint64_t(1)) << MantissaBits) - store_t(std::uint64_t(1));
    static constexpr store_t  max_exact_int_value = (store_t(std::uint64_t(1)) << (MantissaBits + 1)) - store_t(std::uint64_t(1));
    static constexpr sstore_t max_exponent = sstore_t(max_biased_exponent) - exponent_bias - sstore_t(1);
    static constexpr sstore_t min_exponent = sstore_t(1) - exponent_bias;

    constexpr floatmp() noexcept : m_is_negative(false), m_data() {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR floatmp(T i) noexcept : m_is_negative(false), m_data() {
        if (i == T(0)) return;
        *this = from_unsigned_int(store_t(fdetail::abs_u64(i)));
        set_sign(i);   
    }

    explicit OPTIONAL_CPP14_CONSTEXPR floatmp(const store_t& i, bool negative = false) noexcept : m_is_negative(negative), m_data() {
        *this = from_unsigned_int(i);
        m_is_negative = negative && !is_zero();
    }

    explicit OPTIONAL_CPP14_CONSTEXPR floatmp(const sstore_t& i) noexcept : floatmp(store_t(i.abs().magnitude()), i.is_negative()) {}

    template <typename T, typename std::enable_if<std::is_same<T, float>::value, int>::type = 0>
    floatmp(T value) noexcept : m_is_negative(false), m_data() {
        if (value != value)                               { *this = nan(); return; }
        if (value ==  std::numeric_limits<T>::infinity()) { *this = positive_infinity(); return; }
        if (value == -std::numeric_limits<T>::infinity()) { *this = negative_infinity(); return; }
        if (value == T(0)) { m_is_negative = std::signbit(value); return; }
        m_is_negative = value < T(0);
        float a = m_is_negative ? -value : value;
        construct_from_double(static_cast<double>(a));
    }

    template <typename T, typename std::enable_if<std::is_same<T, double>::value, int>::type = 0>
    floatmp(T value) noexcept : m_is_negative(false), m_data() {
        if (value != value)                               { *this = nan(); return; }
        if (value ==  std::numeric_limits<T>::infinity()) { *this = positive_infinity(); return; }
        if (value == -std::numeric_limits<T>::infinity()) { *this = negative_infinity(); return; }
        if (value == T(0)) { m_is_negative = std::signbit(value); return; }
        m_is_negative = value < T(0);
        double a = m_is_negative ? -value : value;
        construct_from_double(a);
    }

    template <typename T, typename std::enable_if<std::is_same<T, long double>::value, int>::type = 0>
    floatmp(T value) noexcept : m_is_negative(false), m_data() {
        if (value != value)                               { *this = nan(); return; }
        if (value ==  std::numeric_limits<T>::infinity()) { *this = positive_infinity(); return; }
        if (value == -std::numeric_limits<T>::infinity()) { *this = negative_infinity(); return; }
        if (value == T(0)) { m_is_negative = std::signbit(value); return; }
        m_is_negative = value < T(0);
        long double a = m_is_negative ? -value : value;
        construct_from_long_double(a);
    }

    floatmp(const std::string& str) noexcept : m_is_negative(false), m_data() { if (!parse_string(str.c_str())) *this = undefined(); }

    floatmp(const char* str) noexcept : m_is_negative(false), m_data() { if (!parse_string(str)) *this = undefined(); }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR floatmp(const floatmp<TB2, MB2, S2>& other) noexcept : m_is_negative(false), m_data() {
        if (other.is_zero())      { *this = zero(other.is_negative()); return; }
        if (other.is_infinite())  { *this = infinity(other.is_negative()); return; }
        if (other.is_nan())       { *this = nan(); return; }
        if (other.is_undefined()) { *this = undefined(); return; }
        const auto fx   = other.frexp_signed();
        const auto srcM = std::get<0>(fx);
        const auto srcE = std::get<1>(fx);
        const bool sgn  = std::get<2>(fx);
        const bit_index leading = srcM.highest_bit();
        if (leading < 0) { *this = zero(sgn); return; }
        const sstore_t e = sstore_t(srcE) - sstore_t(static_cast<bit_index>(MB2));

        if (leading < wide_index) {
            *this = pack_wide(sgn, wide_t(resize_mag<TotalBits * 2>(srcM.magnitude())), e, false);
            return;
        }

        const bit_index shift = leading - (wide_index - 1);
        const bool sticky = srcM.any_bit_below(static_cast<std::size_t>(shift));

        *this = pack_wide(
            sgn,
            wide_t(resize_mag<TotalBits * 2>((srcM >> shift).magnitude())),
            e + sstore_t(shift),
            sticky
        );
    }

public:
    OPTIONAL_CPP14_CONSTEXPR store_t get_biased_exponent() const noexcept { return m_data >> static_cast<std::size_t>(MantissaBits); }
    OPTIONAL_CPP14_CONSTEXPR store_t get_mantissa()        const noexcept { return m_data & mantissa_mask; }
    constexpr bool                   is_negative()         const noexcept { return m_is_negative; }
    constexpr const store_t&         get_bits()            const noexcept { return m_data; }

    OPTIONAL_CPP14_CONSTEXPR void set_biased_exponent(store_t e) noexcept { m_data = (m_data & mantissa_mask) | (e << static_cast<std::size_t>(MantissaBits)); }
    OPTIONAL_CPP14_CONSTEXPR void set_mantissa(store_t m)        noexcept { m_data = (get_biased_exponent() << static_cast<std::size_t>(MantissaBits)) | (m & mantissa_mask); }
    OPTIONAL_CPP14_CONSTEXPR void set_sign(bool n)               noexcept { m_is_negative = n; }

    OPTIONAL_CPP14_CONSTEXPR sstore_t get_unbiased_exponent() const noexcept {
        if (is_zero() || is_nan() || is_undefined()) return sstore_t::undefined();
        if (is_infinite()) return m_is_negative ? sstore_t::min() : sstore_t::max();

        if (is_subnormal()) {
            const bit_index lead = top_bit(get_mantissa());
            if (lead < 0) return sstore_t::undefined();
            return sstore_t(1) - exponent_bias - sstore_t(mantissa_index - 1 - lead);
        }

        return to_signed(get_biased_exponent()) - exponent_bias;
    }

    OPTIONAL_CPP14_CONSTEXPR sstore_t exponent_base2() const noexcept { return get_unbiased_exponent(); }

    static constexpr bit_index series_cap() noexcept { return static_cast<bit_index>(guard_t::mantissa_bits) + 16; }

public:
    OPTIONAL_CPP14_CONSTEXPR bool is_zero()              const noexcept { return get_biased_exponent().is_zero() && get_mantissa().is_zero(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_subnormal()         const noexcept { return get_biased_exponent().is_zero() && !get_mantissa().is_zero(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_infinite()          const noexcept { return get_biased_exponent() == max_biased_exponent && get_mantissa().is_zero(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_positive_infinity() const noexcept { return is_infinite() && !m_is_negative; }
    OPTIONAL_CPP14_CONSTEXPR bool is_negative_infinity() const noexcept { return is_infinite() &&  m_is_negative; }

    OPTIONAL_CPP14_CONSTEXPR bool is_nan() const noexcept {
        return get_biased_exponent() == max_biased_exponent && get_mantissa() == ((one() << (MantissaBits - 1)) | one()) && !m_is_negative;
    }

    OPTIONAL_CPP14_CONSTEXPR bool is_undefined() const noexcept {
        return get_biased_exponent() == max_biased_exponent && get_mantissa() == ((one() << (MantissaBits - 1)) | store_t(std::uint64_t(2))) && !m_is_negative;
    }

    OPTIONAL_CPP14_CONSTEXPR bool is_normalized() const noexcept {
        return !(get_biased_exponent().is_zero() || get_biased_exponent() == max_biased_exponent);
    }

    OPTIONAL_CPP14_CONSTEXPR bool is_finite() const noexcept { return !(is_undefined() || is_infinite() || is_nan()); }

public:
    static OPTIONAL_CPP14_CONSTEXPR floatmp zero(bool neg = false) noexcept {
        return floatmp(raw_tag{}, neg, store_t());
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp positive_zero() noexcept { return zero(false); }
    static OPTIONAL_CPP14_CONSTEXPR floatmp negative_zero() noexcept { return zero(true); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp infinity(bool neg = false) noexcept {
        return floatmp(raw_tag{}, neg, max_biased_exponent << static_cast<std::size_t>(MantissaBits));
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp positive_infinity() noexcept { return infinity(false); }
    static OPTIONAL_CPP14_CONSTEXPR floatmp negative_infinity() noexcept { return infinity(true); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp nan() noexcept {
        return floatmp(raw_tag{}, false, (max_biased_exponent << static_cast<std::size_t>(MantissaBits)) | ((one() << (MantissaBits - 1)) | one()));
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp undefined() noexcept {
        return floatmp(raw_tag{}, false, (max_biased_exponent << static_cast<std::size_t>(MantissaBits)) | ((one() << (MantissaBits - 1)) | store_t(std::uint64_t(2))));
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp min() noexcept {
        return floatmp(raw_tag{}, false, one() << static_cast<std::size_t>(MantissaBits));
    }
    
    static OPTIONAL_CPP14_CONSTEXPR floatmp max() noexcept {
        return floatmp(raw_tag{}, false, ((max_biased_exponent - one()) << static_cast<std::size_t>(MantissaBits)) | mantissa_mask);
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp lowest() noexcept {
        return floatmp(raw_tag{}, true, max().m_data);
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp subnormal_min() noexcept {
        return floatmp(raw_tag{}, false, one());
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp epsilon() noexcept {
        return ldexp(one() << static_cast<std::size_t>(MantissaBits), sstore_t(-mantissa_index));
    }

public:
    OPTIONAL_CPP14_CONSTEXPR bool operator==(const floatmp& o) const noexcept {
        return (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) ? false
             : (is_zero() && o.is_zero())                                     ? true
             : (m_is_negative == o.m_is_negative && m_data == o.m_data);
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator!=(const floatmp& o) const noexcept { return !(*this == o); }

    OPTIONAL_CPP14_CONSTEXPR bool operator<(const floatmp& o) const noexcept {
        return (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) ? false
             : is_negative_infinity()                                         ? !o.is_negative_infinity()
             : o.is_positive_infinity()                                       ? !is_positive_infinity()
             : (is_positive_infinity() || o.is_negative_infinity())           ? false
             : (m_is_negative != o.m_is_negative)
                   ? (!(is_zero() && o.is_zero()) && m_is_negative)
                   : (m_is_negative ? (m_data > o.m_data) : (m_data < o.m_data));
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator<=(const floatmp& o) const noexcept {
        return (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) ? false : (*this < o || *this == o);
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator>(const floatmp& o)  const noexcept {
        return (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) ? false : !(*this <= o);
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator>=(const floatmp& o) const noexcept {
        return (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) ? false : !(*this < o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR bool operator==(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type;
        return C(*this) == C(o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR bool operator<(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type;
        return C(*this) < C(o);
    }

public:
    OPTIONAL_CPP14_CONSTEXPR floatmp operator-() const noexcept {
        return (is_nan() || is_undefined()) ? *this : floatmp(raw_tag{}, !m_is_negative, m_data);
    }

    constexpr floatmp operator+() const noexcept { return *this; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator++() noexcept { *this = *this + floatmp(1); return *this; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator--() noexcept { *this = *this - floatmp(1); return *this; }
    OPTIONAL_CPP14_CONSTEXPR floatmp operator++(int) noexcept { floatmp r(*this); ++(*this); return r; }
    OPTIONAL_CPP14_CONSTEXPR floatmp operator--(int) noexcept { floatmp r(*this); --(*this); return r; }

    OPTIONAL_CPP14_CONSTEXPR floatmp& negate() noexcept { 
        if (is_nan() || is_undefined()) return *this;
        m_is_negative = !m_is_negative; 
        return *this; 
    }

public:
    OPTIONAL_CPP14_CONSTEXPR floatmp operator+(const floatmp& o) const noexcept { return add_impl(*this, o); }
    OPTIONAL_CPP14_CONSTEXPR floatmp operator-(const floatmp& o) const noexcept { return add_impl(*this, -o); }

    OPTIONAL_CPP14_CONSTEXPR floatmp operator*(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan()) return nan();
        if (is_undefined() || o.is_undefined()) return undefined();
        if (is_infinite())   { if (o.is_zero()) return undefined(); return infinity(m_is_negative != o.m_is_negative); }
        if (o.is_infinite()) { if (is_zero())  return undefined(); return infinity(m_is_negative != o.m_is_negative); }
        if (is_zero() || o.is_zero()) return zero(m_is_negative != o.m_is_negative);
        return multiply_finite(o);
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp operator/(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan()) return nan();
        if (is_undefined() || o.is_undefined()) return undefined();
        if (o.is_zero())   { if (is_zero()) return undefined(); return infinity(m_is_negative != o.m_is_negative); }
        if (is_infinite()) { if (o.is_infinite()) return undefined(); return infinity(m_is_negative != o.m_is_negative); }
        if (o.is_infinite() || is_zero()) return zero(m_is_negative != o.m_is_negative);
        return divide_finite(o);
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp& operator+=(const floatmp& o) noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator-=(const floatmp& o) noexcept { return *this = *this - o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator*=(const floatmp& o) noexcept { return *this = *this * o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator/=(const floatmp& o) noexcept { return *this = *this / o; }

    OPTIONAL_CPP14_CONSTEXPR floatmp  reciprocal()         const noexcept { return floatmp(1) / *this; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& reciprocal_mutable()       noexcept { return *this = reciprocal(); }

public:
    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator+(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; 
        return C(*this) + C(o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator-(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; 
        return C(*this) - C(o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator*(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; 
        return C(*this) * C(o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator/(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; 
        return C(*this) / C(o);
    }

public:
    OPTIONAL_CPP14_CONSTEXPR std::pair<store_t, sstore_t> frexp() const noexcept {
        if (is_zero())      return { store_t(), sstore_t(0) };
        if (is_nan())       return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), sstore_t::max() };
        if (is_undefined()) return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), sstore_t::min() };
        if (is_infinite())  return { store_t(), m_is_negative ? sstore_t::min() : sstore_t::max() };
        if (is_subnormal()) return { get_mantissa(), min_exponent };
        return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), get_unbiased_exponent() };
    }

    OPTIONAL_CPP14_CONSTEXPR std::tuple<store_t, sstore_t, bool> frexp_signed() const noexcept {
        const auto p = frexp(); return { p.first, p.second, m_is_negative };
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(store_t mantissa, sstore_t exponent, bool neg = false) noexcept {
        if (mantissa.is_zero()) return zero(neg);
        return pack_wide(neg, wide_t(mantissa), exponent - sstore_t(mantissa_index), false);
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(std::pair<store_t, sstore_t> me, bool neg = false) noexcept { return ldexp(me.first, me.second, neg); }

public:
    OPTIONAL_CPP14_CONSTEXPR floatmp get_integer_part() const noexcept {
        if (is_nan() || is_undefined() || is_infinite() || is_zero()) return *this;
        const sstore_t exp = get_unbiased_exponent();
        if (exp < sstore_t(0)) return zero(m_is_negative);                      
        if (exp >= sstore_t(mantissa_index)) return *this;     
        const bit_index ev = sv(exp);
        const store_t fmask = (one() << static_cast<std::size_t>(mantissa_index - ev)) - one();
        floatmp r(*this);
        r.set_mantissa(get_mantissa() & ~fmask);
        return r;
    }

    OPTIONAL_CPP14_CONSTEXPR store_t get_integer_part_as_int() const noexcept {
        if (is_nan() || is_undefined() || is_zero()) return store_t();
        if (is_infinite()) return store_t::max();
        const sstore_t exp = get_unbiased_exponent();
        if (exp < sstore_t(0)) return store_t();
        if (exp >= sstore_t(total_index)) return store_t::max();
        const store_t full = is_subnormal() ? get_mantissa() : (get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)));
        const bit_index e = sv(exp);
        return (e >= mantissa_index) ? (full << static_cast<std::size_t>(e - mantissa_index)) : (full >> static_cast<std::size_t>(mantissa_index - e));
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp get_fractional_part() const noexcept {
        if (is_nan() || is_undefined()) return *this;
        if (is_infinite()) return zero(m_is_negative);
        if (is_zero())     return *this;
        const sstore_t exp = get_unbiased_exponent();
        if (exp < sstore_t(0)) return *this;
        if (exp >= sstore_t(mantissa_index)) return zero(m_is_negative);
        const bit_index ev = sv(exp);
        const bit_index fb = mantissa_index - ev;
        const store_t fmask = (one() << static_cast<std::size_t>(fb)) - one();
        const store_t fmant = get_mantissa() & fmask;
        if (fmant.is_zero()) return zero(m_is_negative);
        bit_index lead = -1;
        for (bit_index i = fb - 1; i >= 0; --i) if (fmant.get_bit(static_cast<std::size_t>(i))) { lead = i; break; }
        if (lead < 0) return zero(m_is_negative);
        return ldexp(fmant << static_cast<std::size_t>(mantissa_index - lead), -sstore_t(fb - lead), m_is_negative);
    }

    OPTIONAL_CPP14_CONSTEXPR store_t get_fractional_part_as_int() const noexcept {
        if (is_nan() || is_undefined() || is_infinite() || is_zero()) return store_t();
        bool s = false; sstore_t e2; store_t mfull;
        decompose(*this, s, e2, mfull);                      
        const sstore_t E = e2 + sstore_t(mantissa_index);
        if (E >= sstore_t(mantissa_index)) return store_t();   

        if (E < sstore_t(0)) {
            const sstore_t sh = -E;
            if (sh >= sstore_t(total_index)) return store_t();
            return mfull >> lo(sh);                         
        }

        const bit_index ev = sv(E);
        const store_t fmask = (one() << static_cast<std::size_t>(mantissa_index - ev)) - one();
        return (mfull & fmask) << static_cast<std::size_t>(ev);
    }

public:
    OPTIONAL_CPP14_CONSTEXPR sstore_t approximate_exponent_base10() const noexcept {
        if (is_zero() || is_nan() || is_undefined()) return sstore_t::undefined();
        if (is_infinite()) return m_is_negative ? sstore_t::min() : sstore_t::max();
        const sstore_t NUM(std::uint64_t(30102999566ull));
        const sstore_t DEN(std::uint64_t(100000000000ull));
        const sstore_t be = get_unbiased_exponent();

        if (be < sstore_t(0)) {
            const sstore_t a = -be;
            const sstore_t r = (a * NUM) / DEN;
            return -(((a * NUM) % DEN != sstore_t(0)) ? r + sstore_t(1) : r);
        }

        return (be * NUM) / DEN;
    }

public:
    std::string to_string(long long sig = 0) const {
        if (is_nan()) return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite()) return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero()) return m_is_negative ? "-0" : "0";
        const long long limit = sig > 0 ? sig : default_significant_digits();
        const fdetail::decimal::digits_result dr = decimal_digits(sig);
        const long long e10 = dr.exponent10 - 1;
        const std::string body = (e10 >= limit || e10 < -4) ? fdetail::decimal::format_scientific(dr.digits, dr.exponent10) : fdetail::decimal::format_fixed(dr.digits, dr.exponent10);
        return m_is_negative ? "-" + body : body;
    }

    std::string to_scientific_string(long long sig = 0) const {
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite())  return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero())      return m_is_negative ? "-0e+0" : "0e+0";
        const fdetail::decimal::digits_result dr = decimal_digits(sig);
        const std::string body = fdetail::decimal::format_scientific(dr.digits, dr.exponent10);
        return m_is_negative ? "-" + body : body;
    }

    friend std::ostream& operator<<(std::ostream& os, const floatmp& f) { return os << f.to_string(); }

private:
    static OPTIONAL_CPP14_CONSTEXPR floatmp pack_wide(bool neg, wide_t m, sstore_t e, bool sticky) noexcept {
        const bit_index lead = top_bit_wide(m);
        if (lead < 0) return zero(neg);
        sstore_t E = e + sstore_t(lead);
        if (E < min_exponent) E = min_exponent;
        const sstore_t drop = E - sstore_t(mantissa_index) - e;

        if (drop > sstore_t(0)) {
            if (drop > sstore_t(wide_index)) return zero(neg);
            const std::size_t d = static_cast<std::size_t>(sv(drop));
            const bool rb = (d >= 1) && m.get_bit(d - 1);
            if (!sticky && d >= 2) sticky = m.any_bit_below(d - 1);
            m = m >> d;
            if (rb && (sticky || m.get_bit(0))) m = m + wide_t(std::uint64_t(1));
        } else if (drop < sstore_t(0)) {
            m = m << static_cast<std::size_t>(-sv(drop));
        }

        if (m.get_bit(static_cast<std::size_t>(mantissa_index + 1))) { m = m >> 1; E = E + sstore_t(1); }
        if (m.is_zero()) return zero(neg);
        const store_t sig = store_t(resize_mag<TotalBits>(m.magnitude()));
        if (!m.get_bit(static_cast<std::size_t>(mantissa_index))) return floatmp(raw_tag{}, neg, sig & mantissa_mask);
        const sstore_t biased = E + exponent_bias;
        if (biased >= to_signed(max_biased_exponent)) return infinity(neg);
        return floatmp(raw_tag{}, neg, (to_unsigned(biased) << static_cast<std::size_t>(MantissaBits)) | (sig & mantissa_mask));
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp from_unsigned_int(store_t v) noexcept {
        return ldexp(v, sstore_t(mantissa_index)); 
    }

    static OPTIONAL_CPP14_CONSTEXPR void decompose(const floatmp& x, bool& s, sstore_t& e2, store_t& m) noexcept {
        s = x.m_is_negative;
        if (x.is_zero()) { e2 = sstore_t(0); m = store_t(); return; }
        if (x.is_subnormal()) { e2 = min_exponent - sstore_t(mantissa_index); m = x.get_mantissa(); return; }
        e2 = x.get_unbiased_exponent() - sstore_t(mantissa_index);
        m  = (one() << static_cast<std::size_t>(MantissaBits)) | x.get_mantissa();
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp from_mant_exp(bool s, sstore_t e2, store_t m) noexcept {
        if (m.is_zero()) return zero(s);
        return pack_wide(s, wide_t(m), e2, false);
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp add_impl(const floatmp& a, const floatmp& b) noexcept {
        if (a.is_undefined() || b.is_undefined()) return undefined();
        if (a.is_nan() || b.is_nan()) return nan();

        if (a.is_infinite() || b.is_infinite()) {
            if (a.is_infinite() && b.is_infinite() && (a.m_is_negative != b.m_is_negative)) return nan();
            return a.is_infinite() ? a : b;
        }

        if (a.is_zero() && b.is_zero()) return zero(a.m_is_negative && b.m_is_negative);
        if (a.is_zero()) return b;
        if (b.is_zero()) return a;
        bool sa = false, sb = false; 
        sstore_t ea, eb; 
        store_t ma, mb;
        decompose(a, sa, ea, ma); 
        decompose(b, sb, eb, mb);
        wide_t wa = wide_t(ma), wb = wide_t(mb);
        sstore_t er;
        bool sticky = false;

        if (ea > eb) {
            const sstore_t d = ea - eb;
            if (d >= sstore_t(total_index + 4)) return from_mant_exp(sa, ea, ma);
            wa = wa << static_cast<std::size_t>(sv(d));
            er = eb;
        } else if (eb > ea) {
            const sstore_t d = eb - ea;
            if (d >= sstore_t(total_index + 4)) return from_mant_exp(sb, eb, mb);
            wb = wb << static_cast<std::size_t>(sv(d));
            er = ea;
        } else er = ea;

        if (sa == sb) return pack_wide(sa, wa + wb, er, sticky);
        if (wa == wb) return zero(false);
        return (wa > wb) ? pack_wide(sa, wa - wb, er, sticky) : pack_wide(sb, wb - wa, er, sticky);
    }

private:
    OPTIONAL_CPP14_CONSTEXPR floatmp multiply_finite(const floatmp& o) const noexcept {
        bool sa = false, sb = false;
        sstore_t ea, eb;
        store_t ma, mb;
        decompose(*this, sa, ea, ma);
        decompose(o, sb, eb, mb);
        umag<TotalBits> phi, plo;
        wide_mul<TotalBits>::mul(ma.magnitude(), mb.magnitude(), phi, plo);
        return pack_wide(sa != sb, wide_t(combine_product<TotalBits>(plo, phi)), ea + eb, false);
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp newton_recip_unit(const store_t& d) noexcept {
        const store_t nm = d & mantissa_mask;
        const floatmp f(raw_tag{}, false, (to_unsigned(exponent_bias) << static_cast<std::size_t>(MantissaBits)) | nm);
        const std::uint64_t frac52 = extract_top_bits<MantissaBits>(nm);
        const std::uint64_t f32    = ((std::uint64_t(1) << 52) | frac52) >> 21;
        const std::uint64_t q      = (std::uint64_t(1) << 62) / f32;
        floatmp r = ldexp(store_t(q), sstore_t(mantissa_index - 31));
        const floatmp one_v(1);
        bit_index steps = 1;
        for (bit_index have = 30; have < mantissa_index + 8; have *= 2) ++steps;
        for (bit_index i = 0; i < steps; ++i) r = r + r * (one_v - f * r);
        return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR wide_t reciprocal_scaled(const store_t& d) noexcept {
        const floatmp r = newton_recip_unit(d);
        const auto t = r.frexp();                      
        return wide_t(t.first) << static_cast<std::size_t>(sv(t.second) + 3);
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp divide_finite(const floatmp& o) const noexcept {
        bool sa = false, sb = false;
        sstore_t ea, eb;
        store_t ma, mb;
        decompose(*this, sa, ea, ma);
        decompose(o, sb, eb, mb);
        const bool sr = sa != sb;
        const bit_index la = top_bit(ma), lb = top_bit(mb);
        if (la < 0) return zero(sr);
        if (lb < 0) return infinity(sr);
        ma = ma << static_cast<std::size_t>(mantissa_index - la);
        mb = mb << static_cast<std::size_t>(mantissa_index - lb);
        ea = ea - sstore_t(mantissa_index - la);
        eb = eb - sstore_t(mantissa_index - lb);
        const wide_t one_w(std::uint64_t(1));
        const wide_t D = wide_t(mb);
        const wide_t N = wide_t(ma) << static_cast<std::size_t>(mantissa_index + 2);
        wide_t q = (wide_t(ma) * reciprocal_scaled(mb)) >> static_cast<std::size_t>(mantissa_index + 1);
        wide_t p = q * D;
        while (p > N)      { p = p - D; q = q - one_w; }
        while (N - p >= D) { p = p + D; q = q + one_w; }
        return pack_wide(sr, q, ea - eb - sstore_t(mantissa_index + 2), N != p);
    }

private:
    void construct_from_significand(std::uint64_t sig, long long exp2) noexcept {
        if (sig == 0) { *this = zero(m_is_negative); return; }
        bool sticky = false;
        long long bits = 0;
        for (std::uint64_t t = sig; t; t >>= 1) ++bits;

        while (bits > wide_index - 1) {
            sticky = sticky || (sig & 1ull);
            sig >>= 1;
            ++exp2;
            --bits;
        }

        *this = pack_wide(m_is_negative, wide_t(sig), sstore_t(exp2), sticky);
    }

    void construct_from_double(double a) noexcept {
        std::uint64_t bits;
        std::memcpy(&bits, &a, sizeof(bits));
        const std::uint64_t ie = (bits >> 52) & 0x7FF;
        const std::uint64_t im = bits & 0xFFFFFFFFFFFFFull;
        if (ie == 0) construct_from_significand(im, -1074);
        else construct_from_significand(im | (1ull << 52), static_cast<long long>(ie) - 1075);
    }

    void construct_from_long_double(long double a) noexcept {
        int e = 0;
        const long double m = std::frexp(a, &e);
        const std::uint64_t sig = static_cast<std::uint64_t>(std::ldexp(m, 64));
        construct_from_significand(sig, static_cast<long long>(e) - 64);
    }

private:
    static long long default_significant_digits() noexcept {
        return static_cast<long long>(static_cast<double>(mantissa_index + 1) * 0.30102999566 + 1);
    }

    static fdetail::decimal::bignum to_big(const store_t& v) {
        fdetail::decimal::bignum b;
        const bit_index top = top_bit(v);
        for (bit_index i = 0; i <= top; ++i) if (v.get_bit(static_cast<std::size_t>(i))) b.set_bit(i);
        return b;
    }

    static wide_t wide_from_big(const fdetail::decimal::bignum& b) {
        wide_t w;
        const long long n = b.bit_length();
        for (long long i = 0; i < n; ++i) if (b.get_bit(i)) w.set_bit(static_cast<std::size_t>(i));
        return w;
    }

    fdetail::decimal::digits_result decimal_digits(long long sig) const {
        bool s = false;
        sstore_t e2;
        store_t m;
        decompose(*this, s, e2, m);
        const fdetail::decimal::bignum f = to_big(m);
        const long long e = sv(e2);
        if (sig > 0) return fdetail::decimal::fixed_digits(f, e, sig);
        const bool lower_gap_half = is_normalized() && get_mantissa().is_zero() && get_biased_exponent() > one();
        return fdetail::decimal::shortest(f, e, lower_gap_half, !m.get_bit(0));
    }

    bool parse_string(const char* text) noexcept {
        namespace dd = fdetail::decimal;
        const dd::parsed_decimal p = dd::parse(text);
        if (!p.ok) return false;
        m_is_negative = p.negative;
        if (p.is_nan) { *this = nan(); return true; }
        if (p.is_inf) { *this = infinity(p.negative); return true; }
        if (p.digits.is_zero()) { *this = zero(p.negative); return true; }
        const long long dexp = p.exponent10;
        const double approx2 = (static_cast<double>(p.digit_count) + static_cast<double>(dexp)) * 3.3219280948873623;
        const double top_e = static_cast<double>(sv(max_exponent));
        const double low_e = static_cast<double>(sv(min_exponent)) - static_cast<double>(mantissa_index);
        if (approx2 > top_e + 8.0) { *this = infinity(p.negative); return true; }
        if (approx2 < low_e - 8.0) { *this = zero(p.negative); return true; }
        dd::bignum num = p.digits, den(1);
        if (dexp >= 0) num = dd::bignum::mul(num, dd::bignum::pow10(dexp));
        else den = dd::bignum::pow10(-dexp);
        const long long precision = mantissa_index + 3;
        const long long shift = precision - (num.bit_length() - den.bit_length());
        if (shift >= 0) num.shl(shift); else den.shl(-shift);
        bool sticky = false;
        const dd::bignum q = dd::bignum::div_bits(num, den, precision + 2, sticky);
        *this = pack_wide(p.negative, wide_from_big(q), sstore_t(-shift), sticky);
        return true;
    }

private:
    template <typename T>
    OPTIONAL_CPP14_CONSTEXPR typename std::enable_if<std::is_signed<T>::value>::type
    set_sign(T i) noexcept {
        m_is_negative = (i < T(0));
    }

    template <typename T>
    OPTIONAL_CPP14_CONSTEXPR typename std::enable_if<!std::is_signed<T>::value>::type
    set_sign(T) noexcept {
        m_is_negative = false;
    }

    template <std::size_t MB>
    static constexpr typename std::enable_if<(MB >= 52), std::uint64_t>::type
    extract_top_bits(const store_t& nm) noexcept {
        return (nm >> (MB - 52)).get_lowest_bits();
    }

    template <std::size_t MB>
    static constexpr typename std::enable_if<(MB < 52), std::uint64_t>::type
    extract_top_bits(const store_t& nm) noexcept {
        return nm.get_lowest_bits() << (52 - MB);
    }
};

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::sstore_t floatmp<TB, MB, sign::is_signed>::exponent_bias;

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::store_t  floatmp<TB, MB, sign::is_signed>::max_biased_exponent;

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::store_t  floatmp<TB, MB, sign::is_signed>::mantissa_mask;

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::store_t  floatmp<TB, MB, sign::is_signed>::max_exact_int_value;

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::sstore_t floatmp<TB, MB, sign::is_signed>::max_exponent;

template <std::size_t TB, std::size_t MB>
constexpr typename floatmp<TB, MB, sign::is_signed>::sstore_t floatmp<TB, MB, sign::is_signed>::min_exponent;

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP