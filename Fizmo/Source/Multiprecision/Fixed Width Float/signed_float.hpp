#ifndef FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP

#include <cstring>
#include <tuple>
#include <utility>
#include "float.hpp"

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

    OPTIONAL_CPP14_CONSTEXPR floatmp(const char* str) noexcept : m_is_negative(false), m_data() { if (!parse_string(str)) *this = undefined(); }

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
        if (sig <= 0) sig = static_cast<long long>(static_cast<double>(mantissa_index + 1) * 0.30102999566 + 1);
        if (is_nan()) return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite()) return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero()) return m_is_negative ? "-0" : "0";
        const long long ev = sv(approximate_exponent_base10());
        if (ev >= sig || ev < -4) return to_scientific_string(sig);
        std::string out;
        if (m_is_negative) out += '-';
        const store_t ip = get_integer_part_as_int();
        const std::string is = ip.to_string();
        const long long isize = static_cast<long long>(is.size());

        if (isize >= sig) {
            out += is.substr(0, static_cast<std::size_t>(sig));
            out.append(static_cast<std::size_t>(isize - sig), '0');
            return out;
        }

        out += is;
        const long long rem = ip.is_zero() ? sig : sig - isize;
        if (rem == 0) return out;
        out += '.';
        floatmp frac = get_fractional_part(); if (frac.is_negative()) frac = -frac;
        if (frac.is_zero()) { out.pop_back(); return out; }
        const floatmp ten(10);
        long long written = 0;
        bool sigseen = !ip.is_zero();

        for (long long i = 0; i < sig + 20 && written < rem; ++i) {
            frac = frac * ten;
            store_t d = frac.get_integer_part_as_int();
            if (d > store_t(std::uint64_t(9))) d = store_t(std::uint64_t(9));
            const char c = static_cast<char>('0' + static_cast<long long>(d.get_lowest_bits()));
            out += c; if (c != '0') sigseen = true; if (sigseen) ++written;
            frac = frac.get_fractional_part(); if (frac.is_negative()) frac = -frac;
            if (frac.is_zero() && sigseen) break;
        }

        while (out.size() > 1 && out.back() == '0' && out[out.size() - 2] != '.') out.pop_back();
        if (out.back() == '.') out.pop_back();
        return out;
    }

    std::string to_scientific_string(long long sig = 0) const {
        if (sig <= 0) sig = static_cast<long long>(static_cast<double>(mantissa_index + 1) * 0.30102999566 + 1);
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite())  return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero())      return m_is_negative ? "-0e+0" : "0e+0";
        using G = guard_t;
        static const G g_ten(10), g_one(1);
        std::string out; if (m_is_negative) out += '-';
        G val(*this); if (val.is_negative()) val = -val;
        long long e10 = sv(approximate_exponent_base10());
        G p; long long kp = 0;
        gpow10(e10 < 0 ? -e10 : e10, p, kp);
        const auto vf = val.frexp();                       
        const G    mv = G::ldexp(vf.first, typename G::sstore_t(0));   
        const long long kv = static_cast<long long>(vf.second.get_lowest_bits()) * (vf.second.is_negative() ? -1 : 1);
        G         rm = (e10 >= 0) ? (mv / p) : (mv * p);
        long long rk = (e10 >= 0) ? (kv - kp) : (kv + kp);
        gnorm(rm, rk);
        const auto rf = rm.frexp();
        G r = G::ldexp(rf.first, rf.second + typename G::sstore_t(rk));
        while (r >= g_ten) { r = r / g_ten; ++e10; }           
        while (r <  g_one) { r = r * g_ten; --e10; }
        std::string digits;

        for (long long i = 0; i < sig; ++i) {
            typename G::store_t d = r.get_integer_part_as_int();
            if (d > typename G::store_t(std::uint64_t(9))) d = typename G::store_t(std::uint64_t(9));
            digits += static_cast<char>('0' + static_cast<long long>(d.get_lowest_bits()));
            r = r.get_fractional_part() * g_ten; if (r.is_negative()) r = -r;
        }

        const typename G::store_t nd = r.get_integer_part_as_int();

        if (nd >= typename G::store_t(std::uint64_t(5))) {
            long long i = static_cast<long long>(digits.size()) - 1;
            while (i >= 0) { if (digits[static_cast<std::size_t>(i)] < '9') { ++digits[static_cast<std::size_t>(i)]; break; } digits[static_cast<std::size_t>(i)] = '0'; --i; }
            if (i < 0) { digits = "1" + std::string(static_cast<std::size_t>(sig - 1), '0'); ++e10; }
        }

        out += digits[0];

        if (digits.size() > 1) {
            out += '.'; out += digits.substr(1);
            while (out.size() > 1 && out.back() == '0') out.pop_back();
            if (out.back() == '.') out.pop_back();
        }

        out += 'e'; if (e10 >= 0) out += '+'; out += std::to_string(e10);
        return out;
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
    void construct_from_double(double a) noexcept {
        std::uint64_t bits; 
        std::memcpy(&bits, &a, sizeof(bits));
        const std::uint64_t ie = (bits >> 52) & 0x7FF;
        const std::uint64_t im = bits & 0xFFFFFFFFFFFFFull;
        sstore_t uexp;
        store_t full;

        if (ie == 0) {
            if (im == 0) return;
            bit_index lb = -1;
            for (bit_index i = 51; i >= 0; --i) if (im & (1ull << i)) { lb = i; break; }
            uexp = sstore_t(-1022) - sstore_t(52 - lb);
            const bit_index sh = mantissa_index - lb;
            full = (sh >= 0) ? (store_t(im) << static_cast<std::size_t>(sh)) : (store_t(im) >> static_cast<std::size_t>(-sh));
        } else {
            uexp = sstore_t(static_cast<bit_index>(ie)) - sstore_t(1023);
            full = shift_mantissa<MantissaBits>(im);
        }

        *this = ldexp(
            (full & mantissa_mask) | (one() << MantissaBits),
            uexp,
            m_is_negative
        );
    }

    void construct_from_long_double(long double a) noexcept {
        int e;                                      
        long double m = std::frexp(a, &e); 
        m *= 2.0L; 
        e -= 1;
        const sstore_t uexp(static_cast<bit_index>(e)); 
        m -= 1.0L;
        store_t full; 
        long double scale = 1.0L;
        
        for (bit_index i = 0; i < mantissa_index && m > 0.0L; ++i) {
            scale *= 2.0L; const long double bv = m * scale;
            if (bv >= 1.0L) { full.set_bit(static_cast<std::size_t>(mantissa_index - 1 - i)); m = bv - 1.0L; m /= scale; scale = 1.0L; }
        }
        
        *this = ldexp(full | (one() << static_cast<std::size_t>(MantissaBits)), uexp, m_is_negative);
    }

private:
    static OPTIONAL_CPP14_CONSTEXPR bit_index hsb(const wide_t& v) noexcept { return top_bit_wide(v); }

    static constexpr bool is_ws(char c)    noexcept { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; }
    static constexpr bool is_digit(char c) noexcept { return c >= '0' && c <= '9'; }
    static constexpr char lower(char c)    noexcept { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; }

    static OPTIONAL_CPP14_CONSTEXPR bool starts_with(const char* p, const char* pat) noexcept {
        while (*pat) { if (lower(*p) != *pat) return false; ++p; ++pat; }
        return true;
    }

    OPTIONAL_CPP14_CONSTEXPR bool parse_string(const char* s) noexcept {
        if (!s) return false;
        while (is_ws(*s)) ++s;
        m_is_negative = false;
        if (*s == '-') { m_is_negative = true; ++s; } else if (*s == '+') ++s;
        if (starts_with(s, "nan")) { *this = nan(); return true; }
        if (starts_with(s, "inf")) { *this = infinity(m_is_negative); return true; }
        const char* istart = s; while (is_digit(*s)) ++s;
        const long long ilen = static_cast<long long>(s - istart);
        const char* fstart = nullptr; long long flen = 0;
        if (*s == '.') { ++s; fstart = s; while (is_digit(*s)) ++s; flen = static_cast<long long>(s - fstart); }
        if (ilen == 0 && flen == 0) return false;
        long long dexp = 0;

        if (*s=='e'||*s=='E') {
            ++s;
            bool en=false;
            if(*s=='-'){en=true;++s;} else if(*s=='+')++s;
            if(!is_digit(*s)) return false;

            while(is_digit(*s)){
                dexp=dexp*10+(*s-'0');
                ++s;
                if(dexp>1000000) { *this = en?zero(m_is_negative):infinity(m_is_negative); return true; }
            }

            if(en) dexp=-dexp;
        }

        while (*s) { if (!is_ws(*s)) return false; ++s; }
        wide_t mant; long long parsed = 0;
        const long long cap = wide_t::max_digits_base10() - 1;
        for (long long i = 0; i < ilen && parsed < cap; ++i) { mant = mant * wide_t(std::uint64_t(10)) + wide_t(std::uint64_t(istart[i]-'0')); ++parsed; }
        if (ilen > parsed) dexp += ilen - parsed;
        long long fparsed = 0;      

        if (fstart) for (long long i = 0; i < flen && parsed < cap; ++i) {
            mant = mant * wide_t(std::uint64_t(10)) + wide_t(std::uint64_t(fstart[i]-'0'));
            ++parsed; ++fparsed;                                                            
        }

        dexp -= fparsed;                                         
        if (mant.is_zero()) { *this = zero(m_is_negative); return true; }
        long long bexp = 0;

        if (dexp >= 0) {
            for (long long i = 0; i < dexp; ++i) {
                if (mant > wide_t::max() / wide_t(std::uint64_t(5))) {
                    const bit_index sh = hsb(mant) - wide_index + 64;
                    if (sh > 0) { mant = mant >> static_cast<std::size_t>(sh); bexp += sh; }
                }

                mant = mant * wide_t(std::uint64_t(5));
            }

            bexp += dexp;
        } else {
            const long long ae = -dexp;
            const bit_index target = mantissa_index + 10 + total_index;
            const bit_index cur = hsb(mant) + 1;
            bit_index shl = target - cur; if (shl < 0) shl = 0;
            mant = mant << static_cast<std::size_t>(shl); bexp -= shl; bexp += dexp;

            for (long long i = 0; i < ae; ++i) {
                const bit_index msb = hsb(mant);
                if (msb < target - 10) { const bit_index ex = target - msb; mant = mant << static_cast<std::size_t>(ex); bexp -= ex; }
                mant = mant / wide_t(std::uint64_t(5));
            }
        }

        if (mant.is_zero()) { *this = zero(m_is_negative); return true; }
        *this = pack_wide(m_is_negative, mant, sstore_t(bexp), false);
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

    template <std::size_t MB>
    static constexpr typename std::enable_if<(MB >= 52), store_t>::type
    shift_mantissa(std::uint64_t im) noexcept {
        return store_t(im) << (MB - 52);
    }

    template <std::size_t MB>
    static constexpr typename std::enable_if<(MB < 52), store_t>::type
    shift_mantissa(std::uint64_t im) noexcept {
        return store_t(im) >> (52 - MB);
    }

private:
    static void gnorm(guard_t& m, long long& e) noexcept {
        static const guard_t two(2), half(0.5);
        while (m >= two) { m = m * half; ++e; }
    }

    static void gpow10(long long n, guard_t& p, long long& k) noexcept {
        p = guard_t(1); k = 0;
        guard_t b(10); long long kb = 0; gnorm(b, kb);

        while (n > 0) {
            if (n & 1) { p = p * b; k += kb; gnorm(p, k); }
            n >>= 1;
            if (n)     { b = b * b; kb += kb; gnorm(b, kb); }
        }
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