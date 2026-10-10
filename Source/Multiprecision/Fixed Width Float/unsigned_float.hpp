#ifndef FIZMO_MULTIPRECISION_UNSIGNED_FLOAT_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_UNSIGNED_FLOAT_CLASS_SPECIALIZATION_HPP

#include "signed_float.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t TotalBits, std::size_t MantissaBits>
class floatmp<TotalBits, MantissaBits, sign::is_unsigned> {
    using core = floatmp<TotalBits, MantissaBits, sign::is_signed>;
    core v;

    static OPTIONAL_CPP14_CONSTEXPR floatmp wrap(core c) noexcept {
        floatmp r; r.v = (c.is_negative() && !c.is_nan() && !c.is_undefined()) ? core::zero(false) : c;
        if (r.v.is_negative()) r.v.set_sign(false);
        return r;
    }

    template <std::size_t, std::size_t, sign> friend class floatmp;

public:
    using store_t  = typename core::store_t;
    using sstore_t = typename core::sstore_t;
    using exponent_type = sstore_t;
    using guard_t = typename core::guard_t;
    static constexpr std::size_t math_bits     = TotalBits;
    static constexpr std::size_t mantissa_bits = MantissaBits;
    static constexpr std::size_t exponent_bits = TotalBits - MantissaBits;

    constexpr floatmp() noexcept : v() {}
    
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR floatmp(T x) noexcept : v(x) { if (v.is_negative()) v = core::zero(false); }
    
    floatmp(const std::string& s) noexcept : v(s) { if (v.is_negative()) v = core::zero(false); }

    OPTIONAL_CPP14_CONSTEXPR floatmp(const char* s) noexcept : v(s) { if (v.is_negative()) v = core::zero(false); }
    
    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR floatmp(const floatmp<TB2, MB2, S2>& o) noexcept : v(o.as_signed_core()) { if (v.is_negative()) v = core::zero(false); }

    constexpr const core& as_signed_core() const noexcept { return v; } 

public:
    constexpr bool                is_negative()  const noexcept { return false; }
    OPTIONAL_CPP14_CONSTEXPR bool is_zero()      const noexcept { return v.is_zero(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_subnormal() const noexcept { return v.is_subnormal(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_infinite()  const noexcept { return v.is_infinite(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_nan()       const noexcept { return v.is_nan(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_undefined() const noexcept { return v.is_undefined(); }
    OPTIONAL_CPP14_CONSTEXPR bool is_finite()    const noexcept { return v.is_finite(); }

public:
    OPTIONAL_CPP14_CONSTEXPR store_t  get_mantissa()          const noexcept { return v.get_mantissa();          }
    OPTIONAL_CPP14_CONSTEXPR store_t  get_biased_exponent()   const noexcept { return v.get_biased_exponent();   }
    OPTIONAL_CPP14_CONSTEXPR sstore_t get_unbiased_exponent() const noexcept { return v.get_unbiased_exponent(); }

public:
    static OPTIONAL_CPP14_CONSTEXPR floatmp nan()       noexcept { return wrap(core::nan());           }
    static OPTIONAL_CPP14_CONSTEXPR floatmp undefined() noexcept { return wrap(core::undefined());     }
    static OPTIONAL_CPP14_CONSTEXPR floatmp min()       noexcept { return wrap(core::min());           }
    static OPTIONAL_CPP14_CONSTEXPR floatmp max()       noexcept { return wrap(core::max());           }
    static OPTIONAL_CPP14_CONSTEXPR floatmp epsilon()   noexcept { return wrap(core::epsilon());       }

    static OPTIONAL_CPP14_CONSTEXPR floatmp zero(bool = false)     noexcept { return wrap(core::zero(false));     }
    static OPTIONAL_CPP14_CONSTEXPR floatmp infinity(bool = false) noexcept { return wrap(core::infinity(false)); }
    static OPTIONAL_CPP14_CONSTEXPR floatmp positive_infinity()    noexcept { return infinity(); }
    
    static constexpr typename core::bit_index series_cap() noexcept { return core::series_cap(); }

public:
    constexpr floatmp                operator+() const noexcept { return *this; }
    OPTIONAL_CPP14_CONSTEXPR floatmp operator-() const noexcept { return wrap(-v); }

    OPTIONAL_CPP14_CONSTEXPR floatmp  operator+ (const floatmp& o) const noexcept { return wrap(v + o.v);     }
    OPTIONAL_CPP14_CONSTEXPR floatmp  operator- (const floatmp& o) const noexcept { return wrap(v - o.v);     } 
    OPTIONAL_CPP14_CONSTEXPR floatmp  operator* (const floatmp& o) const noexcept { return wrap(v * o.v);     }
    OPTIONAL_CPP14_CONSTEXPR floatmp  operator/ (const floatmp& o) const noexcept { return wrap(v / o.v);     }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator+=(const floatmp& o)       noexcept { return *this = *this + o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator-=(const floatmp& o)       noexcept { return *this = *this - o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator*=(const floatmp& o)       noexcept { return *this = *this * o; }
    OPTIONAL_CPP14_CONSTEXPR floatmp& operator/=(const floatmp& o)       noexcept { return *this = *this / o; }

public:
    OPTIONAL_CPP14_CONSTEXPR bool operator==(const floatmp& o) const noexcept { return v == o.v; }
    OPTIONAL_CPP14_CONSTEXPR bool operator!=(const floatmp& o) const noexcept { return v != o.v; }
    OPTIONAL_CPP14_CONSTEXPR bool operator< (const floatmp& o) const noexcept { return v <  o.v; }
    OPTIONAL_CPP14_CONSTEXPR bool operator<=(const floatmp& o) const noexcept { return v <= o.v; }
    OPTIONAL_CPP14_CONSTEXPR bool operator> (const floatmp& o) const noexcept { return v >  o.v; }
    OPTIONAL_CPP14_CONSTEXPR bool operator>=(const floatmp& o) const noexcept { return v >= o.v; }

public:
    OPTIONAL_CPP14_CONSTEXPR floatmp get_integer_part()           const noexcept { return wrap(v.get_integer_part());    }
    OPTIONAL_CPP14_CONSTEXPR floatmp get_fractional_part()        const noexcept { return wrap(v.get_fractional_part()); }
    OPTIONAL_CPP14_CONSTEXPR store_t get_integer_part_as_int()    const noexcept { return v.get_integer_part_as_int();    }
    OPTIONAL_CPP14_CONSTEXPR store_t get_fractional_part_as_int() const noexcept { return v.get_fractional_part_as_int(); }

    OPTIONAL_CPP14_CONSTEXPR std::pair<store_t, sstore_t>        frexp()        const noexcept { return v.frexp();        }
    OPTIONAL_CPP14_CONSTEXPR std::tuple<store_t, sstore_t, bool> frexp_signed() const noexcept { return v.frexp_signed(); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(store_t mantissa, sstore_t exponent, bool neg = false) noexcept {
        return wrap(core::ldexp(mantissa, exponent, neg));
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(std::pair<store_t, sstore_t> me, bool neg = false) noexcept {
        return wrap(core::ldexp(me, neg));
    }

    OPTIONAL_CPP14_CONSTEXPR sstore_t exponent_base2() const noexcept { return v.exponent_base2(); }
    OPTIONAL_CPP14_CONSTEXPR sstore_t approximate_exponent_base10() const noexcept { return v.approximate_exponent_base10(); }

    OPTIONAL_CPP14_CONSTEXPR floatmp  reciprocal()         const noexcept { return wrap(v.reciprocal()); }
    OPTIONAL_CPP14_CONSTEXPR floatmp& reciprocal_mutable()       noexcept { return *this = reciprocal(); }

public:
    std::string to_string(long long sig = 0) const { return v.to_string(sig); }
    std::string to_scientific_string(long long sig = 0) const { return v.to_scientific_string(sig); }
    friend std::ostream& operator<<(std::ostream& os, const floatmp& f) { return os << f.v; }
};

template <std::size_t TB, std::size_t MB>
constexpr std::size_t floatmp<TB, MB, sign::is_unsigned>::math_bits;

template <std::size_t TB, std::size_t MB>
constexpr std::size_t floatmp<TB, MB, sign::is_unsigned>::mantissa_bits;

template <std::size_t TB, std::size_t MB>
constexpr std::size_t floatmp<TB, MB, sign::is_unsigned>::exponent_bits;

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_UNSIGNED_FLOAT_CLASS_SPECIALIZATION_HPP