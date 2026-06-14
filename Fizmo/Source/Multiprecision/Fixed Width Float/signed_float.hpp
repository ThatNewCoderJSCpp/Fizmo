#ifndef FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP
#define FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP

#include "float.hpp"

namespace fizmo {
namespace multiprecision {

template <std::size_t TotalBits, std::size_t MantissaBits>
class floatmp<TotalBits, MantissaBits, sign::is_signed> {
    static_assert(MantissaBits >= 1 && MantissaBits + 1 <= TotalBits, "need at least 1 mantissa bit and room for the exponent");

    template <std::size_t, std::size_t, sign> friend class floatmp;

public:
    using store_t  = integer<TotalBits,     sign::is_unsigned>; 
    using sstore_t = integer<TotalBits,     sign::is_signed>;   
    using wide_t   = integer<TotalBits * 2, sign::is_unsigned>; 

    using exponent_type = sstore_t;
    static constexpr std::size_t math_bits     = TotalBits;
    static constexpr std::size_t mantissa_bits = MantissaBits;
    static constexpr std::size_t exponent_bits = TotalBits - MantissaBits;
    static constexpr unsigned    radix         = 2;

private:
    bool    m_is_negative;
    store_t m_data;

    static OPTIONAL_CPP14_CONSTEXPR sstore_t to_signed(const store_t& u) noexcept { return sstore_t(u); }      
    static OPTIONAL_CPP14_CONSTEXPR store_t  to_unsigned(const sstore_t& s) noexcept { return store_t(s); }    

    static OPTIONAL_CPP14_CONSTEXPR store_t one()  noexcept { return store_t(std::uint64_t(1)); }
    static OPTIONAL_CPP14_CONSTEXPR std::size_t lo(const sstore_t& s) noexcept { return static_cast<std::size_t>(s.get_lowest_bits()); }
    static OPTIONAL_CPP14_CONSTEXPR std::size_t lo(const store_t& s)  noexcept { return static_cast<std::size_t>(s.get_lowest_bits()); }

public:
    static const sstore_t exponent_bias;
    static const store_t  max_biased_exponent;
    static const store_t  mantissa_mask;
    static const store_t  max_exact_int_value;
    static const sstore_t max_exponent;
    static const sstore_t min_exponent;

    OPTIONAL_CPP14_CONSTEXPR floatmp() noexcept : m_is_negative(false), m_data() {}

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    OPTIONAL_CPP14_CONSTEXPR floatmp(T i) noexcept : m_is_negative(i < 0), m_data() {
        if (i == 0) { m_is_negative = false; return; }
        *this = from_unsigned_int(store_t(fdetail::abs_u64(i)));
        m_is_negative = (i < 0);
    }

    explicit OPTIONAL_CPP14_CONSTEXPR floatmp(const store_t& i, bool negative = false) noexcept : m_is_negative(negative), m_data() {
        *this = from_unsigned_int(i);
        m_is_negative = negative && !is_zero();
    }

    explicit OPTIONAL_CPP14_CONSTEXPR floatmp(const sstore_t& i) noexcept : floatmp(store_t(i.abs().magnitude()), i.is_negative()) {}

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void>
    floatmp(T value) noexcept : m_is_negative(false), m_data() {
        if (value != value)                               { *this = nan(); return; }
        if (value ==  std::numeric_limits<T>::infinity()) { *this = positive_infinity(); return; }
        if (value == -std::numeric_limits<T>::infinity()) { *this = negative_infinity(); return; }
        if (value == T(0)) { m_is_negative = std::signbit(value); return; }
        m_is_negative = value < T(0);
        T a = m_is_negative ? -value : value;
        if constexpr (std::is_same<T, float>::value)       construct_from_double(static_cast<double>(a));
        else if constexpr (std::is_same<T, double>::value) construct_from_double(a);
        else                                               construct_from_long_double(a);
    }

    floatmp(const std::string& str) noexcept : m_is_negative(false), m_data() { if (!parse_string(str.c_str())) *this = undefined(); }
    floatmp(const char* str) noexcept : m_is_negative(false), m_data() { if (!parse_string(str)) *this = undefined(); }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR floatmp(const floatmp<TB2, MB2, S2>& other) noexcept : m_is_negative(false), m_data() {
        if (other.is_zero())      { *this = zero(other.is_negative()); return; }
        if (other.is_infinite())  { *this = infinity(other.is_negative()); return; }
        if (other.is_nan())       { *this = nan(); return; }
        if (other.is_undefined()) { *this = undefined(); return; }
        auto fx   = other.frexp_signed();                 
        auto srcM = std::get<0>(fx);                     
        auto srcE = std::get<1>(fx);                      
        bool sgn  = std::get<2>(fx);
        const std::int64_t e64 = static_cast<std::int64_t>(srcE.get_lowest_bits());
        const bool e_fits = (srcE.abs().magnitude().compare(typename floatmp<TB2, MB2, S2>::store_t(std::uint64_t(1) << 62)) < 0);

        if (e_fits) {
            if (e64 > static_cast<std::int64_t>(MantissaBits) + (1ll << 40)) { *this = infinity(sgn); return; }
            if (e64 < -((1ll << 40)))                                        { *this = zero(sgn);     return; }
        }

        int leading = -1;
        for (long i = static_cast<long>(TB2) - 1; i >= 0; --i) if (srcM.get_bit(static_cast<std::size_t>(i))) { leading = static_cast<int>(i); break; }
        if (leading < 0) { *this = zero(sgn); return; }
        store_t  tgtMant;
        sstore_t tgtExp = sstore_t(srcE);                 

        if (leading < static_cast<int>(TotalBits)) {
            tgtMant = resize_mag<TotalBits>(srcM.magnitude());
        } else {
            int shift = leading - (static_cast<int>(TotalBits) - 1);
            tgtMant = resize_mag<TotalBits>((srcM >> shift).magnitude());

            if (srcM.get_bit(static_cast<std::size_t>(shift - 1))) {
                bool sticky = false;
                for (int i = 0; i < shift - 1; ++i) if (srcM.get_bit(static_cast<std::size_t>(i))) { sticky = true; break; }
                if (sticky || tgtMant.get_bit(0)) tgtMant = tgtMant + one();
            }

            tgtExp = tgtExp - sstore_t(static_cast<long>(MB2)) + sstore_t(static_cast<long>(MantissaBits)) + sstore_t(static_cast<long>(shift));
            tgtExp = sstore_t(srcE) + sstore_t(static_cast<long>(shift)) - sstore_t(static_cast<long>(MB2)) + sstore_t(static_cast<long>(0)); 
        }

        *this = ldexp(tgtMant, sstore_t(srcE), sgn);      
        (void)tgtExp;
    }

    OPTIONAL_CPP14_CONSTEXPR store_t get_biased_exponent() const noexcept { return m_data >> static_cast<std::size_t>(MantissaBits); }
    OPTIONAL_CPP14_CONSTEXPR store_t get_mantissa()        const noexcept { return m_data & mantissa_mask; }
    OPTIONAL_CPP14_CONSTEXPR bool    is_negative()         const noexcept { return m_is_negative; }
    OPTIONAL_CPP14_CONSTEXPR const store_t& get_bits()     const noexcept { return m_data; }

    OPTIONAL_CPP14_CONSTEXPR void set_biased_exponent(store_t e) noexcept { m_data = (m_data & mantissa_mask) | (e << static_cast<std::size_t>(MantissaBits)); }
    OPTIONAL_CPP14_CONSTEXPR void set_mantissa(store_t m)        noexcept { m_data = (get_biased_exponent() << static_cast<std::size_t>(MantissaBits)) | (m & mantissa_mask); }
    OPTIONAL_CPP14_CONSTEXPR void set_sign(bool n)               noexcept { m_is_negative = n; }

    OPTIONAL_CPP14_CONSTEXPR sstore_t get_unbiased_exponent() const noexcept {
        if (is_zero() || is_nan() || is_undefined()) return sstore_t::undefined();
        if (is_infinite()) return m_is_negative ? sstore_t::min() : sstore_t::max();

        if (is_subnormal()) {
            store_t mant = get_mantissa(); int lead = -1;
            for (long i = static_cast<long>(MantissaBits) - 1; i >= 0; --i) if (mant.get_bit(static_cast<std::size_t>(i))) { lead = static_cast<int>(i); break; }
            if (lead < 0) return sstore_t::undefined();
            int shift = static_cast<int>(MantissaBits) - 1 - lead;
            return sstore_t(1) - exponent_bias - sstore_t(static_cast<long>(shift));
        }

        return to_signed(get_biased_exponent()) - exponent_bias;
    }

    OPTIONAL_CPP14_CONSTEXPR sstore_t exponent_base2() const noexcept { return get_unbiased_exponent(); }

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
        store_t e = get_biased_exponent(); return !e.is_zero() && e != max_biased_exponent;
    }

    OPTIONAL_CPP14_CONSTEXPR bool is_finite() const noexcept { return !is_undefined() && !is_infinite() && !is_nan(); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp zero(bool neg = false) noexcept { floatmp r; r.m_is_negative = neg; return r; }
    static OPTIONAL_CPP14_CONSTEXPR floatmp positive_zero() noexcept { return zero(false); }
    static OPTIONAL_CPP14_CONSTEXPR floatmp negative_zero() noexcept { return zero(true); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp infinity(bool neg = false) noexcept {
        floatmp r; r.m_is_negative = neg; r.m_data = max_biased_exponent << static_cast<std::size_t>(MantissaBits); return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp positive_infinity() noexcept { return infinity(false); }
    static OPTIONAL_CPP14_CONSTEXPR floatmp negative_infinity() noexcept { return infinity(true); }

    static OPTIONAL_CPP14_CONSTEXPR floatmp nan() noexcept {
        floatmp r; r.m_data = (max_biased_exponent << static_cast<std::size_t>(MantissaBits)) | ((one() << (MantissaBits - 1)) | one()); return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp undefined() noexcept {
        floatmp r; r.m_data = (max_biased_exponent << static_cast<std::size_t>(MantissaBits)) | ((one() << (MantissaBits - 1)) | store_t(std::uint64_t(2))); return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp min() noexcept { floatmp r; r.m_data = one() << static_cast<std::size_t>(MantissaBits); return r; }
    
    static OPTIONAL_CPP14_CONSTEXPR floatmp max() noexcept {
        floatmp r; store_t e = max_biased_exponent - one();
        r.m_data = (e << static_cast<std::size_t>(MantissaBits)) | mantissa_mask; return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp lowest() noexcept { floatmp r = max(); r.m_is_negative = true; return r; }
    static OPTIONAL_CPP14_CONSTEXPR floatmp subnormal_min() noexcept { floatmp r; r.m_data = one(); return r; }

    OPTIONAL_CPP14_CONSTEXPR bool operator==(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false;
        if (is_zero() && o.is_zero()) return true;
        return m_is_negative == o.m_is_negative && m_data == o.m_data;
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator!=(const floatmp& o) const noexcept { return !(*this == o); }

    OPTIONAL_CPP14_CONSTEXPR bool operator<(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false;
        if (is_negative_infinity())    return !o.is_negative_infinity();
        if (o.is_positive_infinity())  return !is_positive_infinity();
        if (is_positive_infinity() || o.is_negative_infinity()) return false;
        if (m_is_negative != o.m_is_negative) { if (is_zero() && o.is_zero()) return false; return m_is_negative; }
        return m_is_negative ? (m_data > o.m_data) : (m_data < o.m_data);
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator<=(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false; return *this < o || *this == o; 
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator>(const floatmp& o)  const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false; return !(*this <= o); 
    }

    OPTIONAL_CPP14_CONSTEXPR bool operator>=(const floatmp& o) const noexcept {
        if (is_nan() || o.is_nan() || is_undefined() || o.is_undefined()) return false; return !(*this < o); 
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

    OPTIONAL_CPP14_CONSTEXPR floatmp operator-() const noexcept {
        if (is_nan() || is_undefined()) return *this;
        floatmp r(*this); r.m_is_negative = !r.m_is_negative; return r;
    }

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

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator+(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; return C(*this) + C(o);
    }

    template <std::size_t TB2, std::size_t MB2, sign S2>
    OPTIONAL_CPP14_CONSTEXPR typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type
    operator*(const floatmp<TB2, MB2, S2>& o) const noexcept {
        using C = typename fdetail::common<TotalBits, MantissaBits, sign::is_signed, TB2, MB2, S2>::type; return C(*this) * C(o);
    }

    OPTIONAL_CPP14_CONSTEXPR std::pair<store_t, sstore_t> frexp() const noexcept {
        if (is_zero())      return { store_t(), sstore_t(0) };
        if (is_nan())       return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), sstore_t::max() };
        if (is_undefined()) return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), sstore_t::min() };
        if (is_infinite())  return { store_t(), m_is_negative ? sstore_t::min() : sstore_t::max() };
        if (is_subnormal()) return { get_mantissa(), min_exponent };
        return { get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)), get_unbiased_exponent() };
    }

    OPTIONAL_CPP14_CONSTEXPR std::tuple<store_t, sstore_t, bool> frexp_signed() const noexcept {
        auto p = frexp(); return { p.first, p.second, m_is_negative };
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(store_t mantissa, sstore_t exponent, bool neg = false) noexcept {
        if (mantissa.is_zero()) return zero(neg);
        int lead = -1;
        for (long i = static_cast<long>(TotalBits) - 1; i >= 0; --i) if (mantissa.get_bit(static_cast<std::size_t>(i))) { lead = static_cast<int>(i); break; }
        if (lead < 0) return zero(neg);
        int bit_diff = lead - static_cast<int>(MantissaBits);
        sstore_t adj = exponent + sstore_t(static_cast<long>(bit_diff));
        store_t norm; bool round_bit = false, sticky = false;

        if (bit_diff > 0) {
            norm = mantissa >> static_cast<std::size_t>(bit_diff);
            round_bit = mantissa.get_bit(static_cast<std::size_t>(bit_diff - 1));
            for (int i = 0; i < bit_diff - 1; ++i) if (mantissa.get_bit(static_cast<std::size_t>(i))) { sticky = true; break; }
        } else if (bit_diff < 0) { 
            norm = mantissa << static_cast<std::size_t>(-bit_diff); 
        } else { 
            norm = mantissa; 
        }

        if (round_bit && (sticky || norm.get_bit(0))) {
            norm = norm + one();
            if (norm.get_bit(MantissaBits + 1)) { norm = norm >> 1; adj = adj + sstore_t(1); }
        }

        store_t stored = norm & mantissa_mask;
        if (adj > max_exponent) return infinity(neg);

        if (adj < min_exponent) {
            sstore_t sh = min_exponent - adj;
            if (sh >= sstore_t(static_cast<long>(MantissaBits + 1))) return zero(neg);
            store_t full = stored | (one() << static_cast<std::size_t>(MantissaBits));
            floatmp r; r.m_is_negative = neg; r.m_data = (full >> lo(sh)) & mantissa_mask; return r;
        }

        floatmp r; r.m_is_negative = neg;
        r.m_data = (to_unsigned(adj + exponent_bias) << static_cast<std::size_t>(MantissaBits)) | stored;
        return r;
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp ldexp(std::pair<store_t, sstore_t> me, bool neg = false) noexcept { return ldexp(me.first, me.second, neg); }

    OPTIONAL_CPP14_CONSTEXPR store_t get_integer_part_as_int() const noexcept {
        if (is_nan() || is_undefined() || is_zero()) return store_t();
        if (is_infinite()) return store_t::max();
        sstore_t exp = get_unbiased_exponent();
        if (exp < sstore_t(0)) return store_t();
        if (exp >= sstore_t(static_cast<long>(TotalBits))) return store_t::max();
        store_t full = is_subnormal() ? get_mantissa() : (get_mantissa() | (one() << static_cast<std::size_t>(MantissaBits)));
        std::size_t e = lo(exp);
        return (e >= MantissaBits) ? (full << (e - MantissaBits)) : (full >> (MantissaBits - e));
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp get_fractional_part() const noexcept {
        if (is_nan() || is_undefined()) return *this;
        if (is_infinite()) return zero(m_is_negative);
        if (is_zero())     return *this;
        sstore_t exp = get_unbiased_exponent();
        if (exp < sstore_t(0)) return *this;
        if (exp >= sstore_t(static_cast<long>(MantissaBits))) return zero(m_is_negative);
        std::size_t ev = lo(exp);
        std::size_t fb = MantissaBits - ev;
        store_t fmask = (one() << fb) - one();
        store_t fmant = get_mantissa() & fmask;
        if (fmant.is_zero()) return zero(m_is_negative);
        int lead = -1;
        for (long i = static_cast<long>(fb) - 1; i >= 0; --i) if (fmant.get_bit(static_cast<std::size_t>(i))) { lead = static_cast<int>(i); break; }
        if (lead < 0) return zero(m_is_negative);
        sstore_t nexp = -sstore_t(static_cast<long>(fb - static_cast<std::size_t>(lead)));
        return ldexp(fmant << (MantissaBits - static_cast<std::size_t>(lead)), nexp, m_is_negative);
    }

    sstore_t approximate_exponent_base10() const noexcept {
        static const sstore_t NUM(std::uint64_t(30102999566ull));
        static const sstore_t DEN(std::uint64_t(100000000000ull));
        if (is_zero() || is_nan() || is_undefined()) return sstore_t::undefined();
        if (is_infinite()) return m_is_negative ? sstore_t::min() : sstore_t::max();
        sstore_t be = get_unbiased_exponent();

        if (be < sstore_t(0)) {
            sstore_t a = -be, r = (a * NUM) / DEN, rem = (a * NUM) % DEN;
            if (rem != sstore_t(0)) r = r + sstore_t(1);
            return -r;
        }

        return (be * NUM) / DEN;
    }

    std::string to_string(unsigned sig = 0) const {
        if (sig == 0) sig = static_cast<unsigned>(static_cast<double>(MantissaBits + 1) * 0.30103 + 1);
        if (is_nan()) return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite()) return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero()) return m_is_negative ? "-0" : "0";
        sstore_t e10 = approximate_exponent_base10();
        std::int64_t ev = static_cast<std::int64_t>(e10.get_lowest_bits());
        if (e10 < sstore_t(0)) ev = -ev;
        if (ev >= static_cast<std::int64_t>(sig) || ev < -4) return to_scientific_string(sig);
        std::string out;
        if (m_is_negative) out += '-';
        store_t ip = get_integer_part_as_int();
        std::string is = ip.to_string();
        if (is.size() >= sig) { out += is.substr(0, sig); out.append(is.size() - sig, '0'); return out; }
        out += is;
        unsigned rem = ip.is_zero() ? sig : sig - static_cast<unsigned>(is.size());
        if (rem == 0) return out;
        out += '.';
        floatmp frac = get_fractional_part(); if (frac.is_negative()) frac = -frac;
        if (frac.is_zero()) { out.pop_back(); return out; }
        const floatmp ten(10);
        unsigned written = 0; bool sigseen = !ip.is_zero();

        for (unsigned i = 0; i < sig + 20 && written < rem; ++i) {
            frac = frac * ten;
            store_t d = frac.get_integer_part_as_int();
            if (d > store_t(std::uint64_t(9))) d = store_t(std::uint64_t(9));
            char c = static_cast<char>('0' + static_cast<int>(d.get_lowest_bits()));
            out += c; if (c != '0') sigseen = true; if (sigseen) ++written;
            frac = frac.get_fractional_part(); if (frac.is_negative()) frac = -frac;
            if (frac.is_zero() && sigseen) break;
        }

        while (out.size() > 1 && out.back() == '0' && out[out.size() - 2] != '.') out.pop_back();
        if (out.back() == '.') out.pop_back();
        return out;
    }

    std::string to_scientific_string(unsigned sig = 0) const {
        if (sig == 0) sig = static_cast<unsigned>(static_cast<double>(MantissaBits + 1) * 0.30103 + 1);
        if (is_nan()) return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite()) return m_is_negative ? "-\u221e" : "\u221e";
        if (is_zero()) return m_is_negative ? "-0e+0" : "0e+0";
        std::string out; if (m_is_negative) out += '-';
        floatmp val = m_is_negative ? -(*this) : *this;
        const floatmp ten(10), one_v(1), tenth = one_v / ten;
        std::int64_t e10 = 0;
        while (val >= ten)                    { val = val * tenth; if (++e10 > 1000000) break;  }
        while (val < one_v && !val.is_zero()) { val = val * ten;   if (--e10 < -1000000) break; }
        std::string digits;

        for (unsigned i = 0; i < sig; ++i) {
            store_t d = val.get_integer_part_as_int();
            if (d > store_t(std::uint64_t(9))) d = store_t(std::uint64_t(9));
            digits += static_cast<char>('0' + static_cast<int>(d.get_lowest_bits()));
            val = val.get_fractional_part() * ten; if (val.is_negative()) val = -val;
        }

        store_t nd = val.get_integer_part_as_int();
        
        if (nd >= store_t(std::uint64_t(5))) {
            int i = static_cast<int>(digits.size()) - 1;
            while (i >= 0) { if (digits[i] < '9') { ++digits[i]; break; } digits[i] = '0'; --i; }
            if (i < 0) { digits = "1" + std::string(sig - 1, '0'); ++e10; }
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
    static OPTIONAL_CPP14_CONSTEXPR floatmp from_unsigned_int(store_t v) noexcept {
        if (v.is_zero()) return zero();
        int hb = -1;
        for (long i = static_cast<long>(TotalBits) - 1; i >= 0; --i) if (v.get_bit(static_cast<std::size_t>(i))) { hb = static_cast<int>(i); break; }
        return ldexp(v, sstore_t(static_cast<long>(hb))); 
    }

    static OPTIONAL_CPP14_CONSTEXPR void decompose(const floatmp& x, bool& s, sstore_t& e2, store_t& m) noexcept {
        s = x.m_is_negative;
        if (x.is_zero()) { e2 = sstore_t(0); m = store_t(); return; }
        if (x.is_subnormal()) { e2 = min_exponent - sstore_t(static_cast<long>(MantissaBits)); m = x.get_mantissa(); return; }
        e2 = x.get_unbiased_exponent() - sstore_t(static_cast<long>(MantissaBits));
        m  = (one() << static_cast<std::size_t>(MantissaBits)) | x.get_mantissa();
    }

    static OPTIONAL_CPP14_CONSTEXPR floatmp from_mant_exp(bool s, sstore_t e2, store_t m) noexcept {
        if (m.is_zero()) return zero(s);
        return ldexp(m, e2 + sstore_t(static_cast<long>(MantissaBits)), s);
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
        bool sa = false;
        bool sb = false; 
        sstore_t ea, eb; 
        store_t ma, mb;
        decompose(a, sa, ea, ma); 
        decompose(b, sb, eb, mb);
        sstore_t er;
        
        if (ea > eb) {
            sstore_t d = ea - eb;
            if (d >= sstore_t(static_cast<long>(TotalBits + 4))) return from_mant_exp(sa, ea, ma);
            std::size_t sh = lo(d);
            if (sh > 0) { store_t lost = (one() << sh) - one(); bool st = !(mb & lost).is_zero(); mb = mb >> sh; if (st) mb = mb | one(); }
            er = ea;
        } else if (eb > ea) {
            sstore_t d = eb - ea;
            if (d >= sstore_t(static_cast<long>(TotalBits + 4))) return from_mant_exp(sb, eb, mb);
            std::size_t sh = lo(d);
            if (sh > 0) { store_t lost = (one() << sh) - one(); bool st = !(ma & lost).is_zero(); ma = ma >> sh; if (st) ma = ma | one(); }
            er = eb;
        } else er = ea;

        if (sa == sb) {
            store_t mr = ma + mb;
            if (mr.get_bit(MantissaBits + 1)) { mr = mr >> 1; er = er + sstore_t(1); }
            return from_mant_exp(sa, er, mr);
        }

        bool sr = 0; 
        store_t mr;
        if (ma == mb) return zero(false);
        else if (ma > mb) { mr = ma - mb; sr = sa; }
        else              { mr = mb - ma; sr = sb; }
        return from_mant_exp(sr, er, mr);
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp multiply_finite(const floatmp& o) const noexcept {
        store_t e1 = get_biased_exponent(), m1 = get_mantissa();
        store_t e2 = o.get_biased_exponent(), m2 = o.get_mantissa();
        bool sub1 = e1.is_zero(), sub2 = e2.is_zero();
        store_t f1 = sub1 ? m1 : (m1 | (one() << static_cast<std::size_t>(MantissaBits)));
        store_t f2 = sub2 ? m2 : (m2 | (one() << static_cast<std::size_t>(MantissaBits)));
        sstore_t ef1 = sub1 ? sstore_t(1) : to_signed(e1);
        sstore_t ef2 = sub2 ? sstore_t(1) : to_signed(e2);
        bool sr = m_is_negative != o.m_is_negative;
        wide_t prod = wide_t(f1) * wide_t(f2);
        sstore_t rexp = ef1 + ef2 - exponent_bias;
        int lead = -1;
        for (long i = static_cast<long>(2 * TotalBits) - 1; i >= 0; --i) if (prod.get_bit(static_cast<std::size_t>(i))) { lead = static_cast<int>(i); break; }
        if (lead < 0) return zero(sr);
        rexp = rexp + sstore_t(static_cast<long>(lead - 2 * static_cast<int>(MantissaBits)));
        store_t rm; bool rb = false, st = false;

        if (lead >= static_cast<int>(MantissaBits)) {
            int drop = lead - static_cast<int>(MantissaBits);
            rm = resize_mag<TotalBits>((prod >> drop).magnitude());
            
            if (drop > 0) { 
                rb = prod.get_bit(static_cast<std::size_t>(drop - 1)); 
                for (int i = 0; i < drop - 1; ++i) if (prod.get_bit(static_cast<std::size_t>(i))) { st = true; break; } 
            }
        } else rm = resize_mag<TotalBits>((prod << (static_cast<int>(MantissaBits) - lead)).magnitude());
        
        if (rb && (st || rm.get_bit(0))) { rm = rm + one(); if (rm.get_bit(MantissaBits + 1)) { rm = rm >> 1; rexp = rexp + sstore_t(1); } }
        rm = rm & mantissa_mask;
        if (rexp >= to_signed(max_biased_exponent)) return infinity(sr);
        
        if (rexp <= sstore_t(0)) {
            if (rexp < sstore_t(1) - sstore_t(static_cast<long>(MantissaBits))) return zero(sr);
            int ss = 1 - static_cast<int>(rexp.get_lowest_bits());
            rm = (rm | (one() << static_cast<std::size_t>(MantissaBits))) >> static_cast<std::size_t>(ss);
            rexp = sstore_t(0);
        }
        
        floatmp r; r.m_is_negative = sr;
        r.m_data = (to_unsigned(rexp) << static_cast<std::size_t>(MantissaBits)) | rm; return r;
    }

    OPTIONAL_CPP14_CONSTEXPR floatmp divide_finite(const floatmp& o) const noexcept {
        store_t e1 = get_biased_exponent(), m1 = get_mantissa();
        store_t e2 = o.get_biased_exponent(), m2 = o.get_mantissa();
        bool sub1 = e1.is_zero(), sub2 = e2.is_zero();
        store_t f1 = sub1 ? m1 : (m1 | (one() << static_cast<std::size_t>(MantissaBits)));
        store_t f2 = sub2 ? m2 : (m2 | (one() << static_cast<std::size_t>(MantissaBits)));
        sstore_t ef1 = sub1 ? sstore_t(1) : to_signed(e1);
        sstore_t ef2 = sub2 ? sstore_t(1) : to_signed(e2);
        bool sr = m_is_negative != o.m_is_negative;
        sstore_t rexp = ef1 - ef2 + exponent_bias;
        wide_t dividend = wide_t(f1) << (MantissaBits + 2);
        wide_t divisor  = wide_t(f2);
        wide_t q, rem = dividend;
        
        for (int i = static_cast<int>(MantissaBits) + 2; i >= 0; --i) {
            wide_t shifted = divisor << static_cast<std::size_t>(i);
            if (rem >= shifted) { q = q | (wide_t(std::uint64_t(1)) << static_cast<std::size_t>(i)); rem = rem - shifted; }
        }
        
        int lead = -1;
        for (int i = static_cast<int>(MantissaBits) + 2; i >= 0; --i) if (q.get_bit(static_cast<std::size_t>(i))) { lead = i; break; }
        if (lead < 0) return zero(sr);
        rexp = rexp + sstore_t(static_cast<long>(lead - (static_cast<int>(MantissaBits) + 2)));
        store_t rm; bool rb = false, st = false;
        
        if (lead >= static_cast<int>(MantissaBits)) {
            int drop = lead - static_cast<int>(MantissaBits);
            rm = resize_mag<TotalBits>((q >> drop).magnitude());
            
            if (drop > 0) { 
                rb = q.get_bit(static_cast<std::size_t>(drop - 1));
                for (int i = 0; i < drop - 1; ++i) if (q.get_bit(static_cast<std::size_t>(i))) { st = true; break; }
                if (!st && !rem.is_zero()) st = true; 
            }
        } else rm = resize_mag<TotalBits>((q << (static_cast<int>(MantissaBits) - lead)).magnitude());
        
        if (rb && (st || rm.get_bit(0))) { rm = rm + one(); if (rm.get_bit(MantissaBits + 1)) { rm = rm >> 1; rexp = rexp + sstore_t(1); } }
        rm = rm & mantissa_mask;
        if (rexp >= to_signed(max_biased_exponent)) return infinity(sr);
        
        if (rexp <= sstore_t(0)) {
            if (rexp < sstore_t(1) - sstore_t(static_cast<long>(MantissaBits))) return zero(sr);
            int ss = 1 - static_cast<int>(rexp.get_lowest_bits());
            rm = (rm | (one() << static_cast<std::size_t>(MantissaBits))) >> static_cast<std::size_t>(ss);
            rexp = sstore_t(0);
        }
        
        floatmp r; r.m_is_negative = sr;
        r.m_data = (to_unsigned(rexp) << static_cast<std::size_t>(MantissaBits)) | rm; return r;
    }

    void construct_from_double(double a) noexcept {
        std::uint64_t bits; std::memcpy(&bits, &a, sizeof(bits));
        std::uint64_t ie = (bits >> 52) & 0x7FF, im = bits & 0xFFFFFFFFFFFFFull;
        sstore_t uexp; store_t full;

        if (ie == 0) {
            if (im == 0) return;
            int lb = -1; for (int i = 51; i >= 0; --i) if (im & (1ull << i)) { lb = i; break; }
            uexp = sstore_t(-1022) - sstore_t(static_cast<long>(52 - lb));
            int sh = static_cast<int>(MantissaBits) - lb;
            full = (sh >= 0) ? (store_t(im) << static_cast<std::size_t>(sh)) : (store_t(im) >> static_cast<std::size_t>(-sh));
        } else {
            uexp = sstore_t(static_cast<long>(ie)) - sstore_t(1023);
            full = (MantissaBits >= 52) ? (store_t(im) << (MantissaBits - 52)) : (store_t(im) >> (52 - MantissaBits));
        }

        *this = ldexp((full & mantissa_mask) | (one() << static_cast<std::size_t>(MantissaBits)), uexp, m_is_negative);
    }

    void construct_from_long_double(long double a) noexcept {
        int e; long double m = std::frexp(a, &e); m *= 2.0L; e -= 1;
        sstore_t uexp(static_cast<long>(e)); m -= 1.0L;
        store_t full; long double scale = 1.0L;
        
        for (std::size_t i = 0; i < MantissaBits && m > 0.0L; ++i) {
            scale *= 2.0L; long double bv = m * scale;
            if (bv >= 1.0L) { full.set_bit(MantissaBits - 1 - i); m = bv - 1.0L; m /= scale; scale = 1.0L; }
        }
        
        *this = ldexp(full | (one() << static_cast<std::size_t>(MantissaBits)), uexp, m_is_negative);
    }

    bool parse_string(const char* s) noexcept {
        if (!s) return false;
        auto ws = [](char c){ return c==' '||c=='\t'||c=='\n'||c=='\r'; };
        while (ws(*s)) ++s;
        m_is_negative = false;
        if (*s == '-') { m_is_negative = true; ++s; } else if (*s == '+') ++s;
        auto ci = [](char a, char b){ if (a>='A'&&a<='Z') a += 32; return a == b; };
        auto sw = [&](const char* p, const char* pat){ while (*pat){ if(!ci(*p,*pat)) return false; ++p; ++pat;} return true; };
        if (sw(s, "nan")) { *this = nan(); return true; }
        if (sw(s, "inf")) { *this = infinity(m_is_negative); return true; }
        const char* istart = s; while (*s>='0'&&*s<='9') ++s; std::size_t ilen = s - istart;
        const char* fstart = nullptr; std::size_t flen = 0;
        if (*s == '.') { ++s; fstart = s; while (*s>='0'&&*s<='9') ++s; flen = s - fstart; }
        if (ilen == 0 && flen == 0) return false;
        std::int64_t dexp = 0;

        if (*s=='e'||*s=='E') { 
            ++s; 
            bool en=false; 
            if(*s=='-'){en=true;++s;} else if(*s=='+')++s;
            if(!(*s>='0'&&*s<='9')) return false;

            while(*s>='0'&&*s<='9'){ 
                dexp=dexp*10+(*s-'0'); 
                ++s; 
                if(dexp>1000000) { *this = en?zero(m_is_negative):infinity(m_is_negative); return true; } 
            }

            if(en) dexp=-dexp; }
        while (*s) { if (!ws(*s)) return false; ++s; }

        dexp -= static_cast<std::int64_t>(flen);
        wide_t mant; std::size_t parsed = 0; const std::size_t cap = 2 * TotalBits;
        for (std::size_t i = 0; i < ilen && parsed < cap; ++i) { mant = mant * wide_t(std::uint64_t(10)) + wide_t(std::uint64_t(istart[i]-'0')); ++parsed; }
        if (ilen > parsed) dexp += static_cast<std::int64_t>(ilen - parsed);
        if (fstart) for (std::size_t i = 0; i < flen && parsed < cap; ++i) { mant = mant * wide_t(std::uint64_t(10)) + wide_t(std::uint64_t(fstart[i]-'0')); ++parsed; }
        if (mant.is_zero()) { *this = zero(m_is_negative); return true; }
        std::int64_t bexp = 0;
        auto hsb = [](const wide_t& v)->int{ for (long i=static_cast<long>(2*TotalBits)-1;i>=0;--i) if (v.get_bit(static_cast<std::size_t>(i))) return static_cast<int>(i); return -1; };
        
        if (dexp >= 0) {
            for (std::int64_t i = 0; i < dexp; ++i) {
                if (mant > wide_t::max() / wide_t(std::uint64_t(5))) { int sh = hsb(mant) - static_cast<int>(2*TotalBits) + 64; if (sh>0){ mant = mant >> static_cast<std::size_t>(sh); bexp += sh; } }
                mant = mant * wide_t(std::uint64_t(5));
            }

            bexp += dexp;
        } else {
            std::int64_t ae = -dexp;
            int target = static_cast<int>(MantissaBits) + 10 + static_cast<int>(TotalBits);
            int cur = hsb(mant) + 1, shl = target - cur; if (shl < 0) shl = 0;
            mant = mant << static_cast<std::size_t>(shl); bexp -= shl; bexp += dexp;
            
            for (std::int64_t i = 0; i < ae; ++i) {
                int msb = hsb(mant);
                if (msb < target - 10) { int ex = target - msb; mant = mant << static_cast<std::size_t>(ex); bexp -= ex; }
                mant = mant / wide_t(std::uint64_t(5));
            }
        }

        if (mant.is_zero()) { *this = zero(m_is_negative); return true; }
        int lead = hsb(mant);
        sstore_t uexp = sstore_t(static_cast<long>(bexp)) + sstore_t(static_cast<long>(lead));
        store_t rm; bool rb = false, st = false;
        
        if (lead >= static_cast<int>(MantissaBits)) {
            int drop = lead - static_cast<int>(MantissaBits);
            rm = resize_mag<TotalBits>((mant >> drop).magnitude());
            
            if (drop > 0) { 
                rb = mant.get_bit(static_cast<std::size_t>(drop-1));
                for (int i=0;i<drop-1;++i) if (mant.get_bit(static_cast<std::size_t>(i))) { st = true; break; } 
            }
        } else rm = resize_mag<TotalBits>((mant << (static_cast<int>(MantissaBits)-lead)).magnitude());
        
        if (rb && (st || rm.get_bit(0))) { rm = rm + one(); if (rm.get_bit(MantissaBits+1)) { rm = rm >> 1; uexp = uexp + sstore_t(1); } }
        *this = ldexp((rm & mantissa_mask) | (one() << static_cast<std::size_t>(MantissaBits)), uexp, m_is_negative);
        return true;
    }
};

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::sstore_t
floatmp<TB, MB, sign::is_signed>::exponent_bias =
    typename floatmp<TB, MB, sign::is_signed>::sstore_t(
        (typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1)) << (TB - MB - 1))
        - typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1)));

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::store_t
floatmp<TB, MB, sign::is_signed>::max_biased_exponent =
    (typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1)) << (TB - MB))
    - typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1));

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::store_t
floatmp<TB, MB, sign::is_signed>::mantissa_mask =
    (typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1)) << MB)
    - typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1));

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::store_t
floatmp<TB, MB, sign::is_signed>::max_exact_int_value =
    (typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1)) << (MB + 1))
    - typename floatmp<TB, MB, sign::is_signed>::store_t(std::uint64_t(1));

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::sstore_t
floatmp<TB, MB, sign::is_signed>::max_exponent =
    floatmp<TB, MB, sign::is_signed>::sstore_t(floatmp<TB, MB, sign::is_signed>::max_biased_exponent)
    - floatmp<TB, MB, sign::is_signed>::exponent_bias - floatmp<TB, MB, sign::is_signed>::sstore_t(1);

template <std::size_t TB, std::size_t MB>
const typename floatmp<TB, MB, sign::is_signed>::sstore_t
floatmp<TB, MB, sign::is_signed>::min_exponent =
    floatmp<TB, MB, sign::is_signed>::sstore_t(1) - floatmp<TB, MB, sign::is_signed>::exponent_bias;

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_SIGNED_FLOAT_CLASS_SPECIALIZATION_HPP