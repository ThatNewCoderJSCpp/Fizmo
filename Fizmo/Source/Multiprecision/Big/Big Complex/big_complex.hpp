#ifndef FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP
#define FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP

#include "../Big Math/ntrig.hpp"
#include "../Big Math/htrig.hpp"
#include "../Big Math/abs_root.hpp"

namespace fizmo {
namespace multiprecision {

class BigComplex {
private:
    BigFloat m_r;
    BigFloat m_i;

private:
    mutable bool m_arg_valid = false;
    mutable BigFloat m_arg;

    mutable bool m_mag2_valid = false;
    mutable BigFloat m_mag2;

    mutable bool m_mag_valid = false;
    mutable BigFloat m_mag;

    void invalidate_cache() noexcept {
        m_arg_valid = false;
        m_mag2_valid = false;
        m_mag_valid = false;
    }

public:
    BigComplex() : m_r(), m_i() {}
    BigComplex(BigFloat real) : m_r(std::move(real)), m_i() {}
    BigComplex(BigFloat real, BigFloat imag) : m_r(std::move(real)), m_i(std::move(imag)) {}
    BigComplex(const BigComplex&) = default;
    BigComplex(BigComplex&&) noexcept = default;
    BigComplex& operator=(const BigComplex&) = default;
    BigComplex& operator=(BigComplex&&) noexcept = default;
    ~BigComplex() = default;

    static BigComplex make_imaginary(BigFloat imag) { return BigComplex(BigFloat::zero(), std::move(imag)); }

public:
    const BigFloat& real() const noexcept { return m_r; }
    const BigFloat& imaginary() const noexcept { return m_i; }
    
    void set_real(BigFloat f) {
        m_r = std::move(f);
        invalidate_cache();
    }

    void set_imaginary(BigFloat f) {
        m_i = std::move(f);
        invalidate_cache();
    }

public:
    const BigFloat& magnitude_squared() const {
        if (!m_mag2_valid) {
            if (is_error()) {
                m_mag2 = BigFloat::nan();
            } else if (is_undefined()) { 
                m_mag2 = BigFloat::undefined();
            } else {
                BigFloat tmp = m_r;   
                tmp *= m_r;           
                BigFloat imag2 = m_i; 
                imag2 *= m_i;         
                tmp += imag2;         
                m_mag2 = std::move(tmp);
            }
            m_mag2_valid = true;
        }
        return m_mag2;
    }

    const BigFloat& magnitude() const {
        if (!m_mag_valid) {
            if (is_error()) {
                m_mag = BigFloat::nan();
            } else if (is_undefined()) { 
                m_mag = BigFloat::undefined();
            } else {
                m_mag = math::sqrt(magnitude_squared());
            }
            m_mag_valid = true;
        }
        return m_mag;
    }

    const BigFloat& argument() const {
        if (!m_arg_valid) {
            if (is_error())          m_arg = BigFloat::nan();
            else if (is_undefined()) m_arg = BigFloat::undefined();
            else                     m_arg = math::atan2(m_i, m_r);
            m_arg_valid = true;
        }
        return m_arg;
    }

    BigFloat argument(const BigInt& n) const {
        const BigFloat& principal = argument();
        if (n.is_zero() || principal.is_nan() || principal.is_undefined()) return principal;
        std::size_t prec = BigFloat::effective_precision_bits(principal);
        BigFloat offset = constants::pi(prec, false);
        offset *= BigFloat(2);
        offset *= BigFloat(n);
        return principal + std::move(offset);
    }

    BigComplex conjugate() const noexcept { 
        if (is_error())     return BigComplex::error_unit();
        if (is_undefined()) return BigComplex::undefined();
        return BigComplex(m_r, -m_i); 
    }

    BigComplex& conjugate_inplace() noexcept {
        if (is_error()) {
            *this = BigComplex::error_unit();
            return *this;
        }

        if (is_undefined()) {
            *this = BigComplex::undefined();
            return *this;
        }

        m_i.negate();
        invalidate_cache();
        return *this;
    }

    BigComplex& reciprocal_inplace() {
        if (is_error()) {
            *this = BigComplex::error_unit();
            return *this;
        }

        if (is_undefined()) {
            *this = BigComplex::undefined();
            return *this;
        }

        if (is_zero()) {
            *this = BigComplex(BigFloat::undefined());
            return *this;
        }
        
        BigFloat mag2 = magnitude_squared();
        BigFloat new_real = m_r;
        new_real /= mag2;
        BigFloat new_imag = m_i;
        new_imag /= mag2;
        m_r = std::move(new_real);
        m_i = std::move(new_imag);
        invalidate_cache();
        return *this;
    }

    BigComplex reciprocal() const {
        BigComplex temp(*this);
        temp.reciprocal_inplace();
        return temp;
    }

public:
    bool is_zero() const noexcept { return m_r.is_zero() && m_i.is_zero(); }
    bool is_undefined() const noexcept { return (m_r.is_undefined() || m_i.is_undefined()) && !is_error(); }
    bool is_error() const noexcept { return m_r.is_nan() || m_i.is_nan(); }
    bool is_infinite() const noexcept { return (m_r.is_infinite() || m_i.is_infinite()) && !is_error(); } 
    bool is_finite() const noexcept { return m_r.is_finite() && m_i.is_finite(); }
    bool is_purely_real() const noexcept { return !is_error() && !is_undefined() && m_i.is_zero(); }
    bool is_purely_imaginary() const noexcept { return !is_error() && !is_undefined() && m_r.is_zero(); }

public:
    static BigComplex imaginary_unit() noexcept { return BigComplex(BigFloat(0), BigFloat(1)); }
    static BigComplex error_unit() noexcept { return BigComplex(BigFloat::nan(), BigFloat::nan()); }
    static BigComplex undefined() noexcept { return BigComplex(BigFloat::undefined(), BigFloat::undefined()); }
    static BigComplex zero() noexcept { return BigComplex(BigFloat::zero(), BigFloat::zero()); }

public:
    bool operator==(const BigComplex& c) const noexcept { return m_r == c.m_r && m_i == c.m_i; }
    bool operator!=(const BigComplex& c) const noexcept { return !(*this == c); }

    // Returns 1-4 counter-clockwise
    unsigned int quadrant() const noexcept {
        if (m_r.is_positive()) { return m_i.is_positive() ? 1 : 4; }
        return m_i.is_positive() ? 2 : 3;
    }

public:
    BigComplex& negate() noexcept {
        if (is_error())     { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined()) { *this = BigComplex::undefined();  return *this; }
        m_r.negate();
        m_i.negate();
        invalidate_cache();
        return *this;
    }

    BigComplex operator-() const noexcept {
        BigComplex temp(*this);
        temp.negate();
        return temp;
    }

    BigComplex& operator+=(const BigComplex& c) {
        if (is_error() || c.is_error()) { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined() || c.is_undefined()) { *this = BigComplex::undefined(); return *this; }
        if (is_zero()) { *this = c; return *this; }
        if (c.is_zero()) { return *this; }
        m_r += c.m_r;
        m_i += c.m_i;
        invalidate_cache();
        return *this;
    }

    BigComplex operator+(const BigComplex& c) const {
        BigComplex temp(*this);
        temp += c;
        return temp;
    }

    BigComplex operator+=(const BigFloat& c) {
        m_r += c;
        invalidate_cache();
        return *this;
    }

    BigComplex operator+(const BigFloat& c) const {
        BigComplex temp(*this);
        temp += c;
        return temp;
    }

    BigComplex& operator-=(const BigComplex& c) {
        if (is_error() || c.is_error()) { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined() || c.is_undefined()) { *this = BigComplex::undefined(); return *this; }
        if (is_zero()) { *this = c; negate(); return *this; }
        if (c.is_zero()) { return *this; }
        m_r -= c.m_r;
        m_i -= c.m_i;
        invalidate_cache();
        return *this;
    }

    BigComplex operator-(const BigComplex& c) const {
        BigComplex temp(*this);
        temp -= c;
        return temp;
    }

    BigComplex operator-=(const BigFloat& c) {
        m_r -= c;
        invalidate_cache();
        return *this;
    }

    BigComplex operator-(const BigFloat& c) const {
        BigComplex temp(*this);
        temp -= c;
        return temp;
    }

    BigComplex& operator*=(const BigComplex& o) {
        if (is_error() || o.is_error())         { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined() || o.is_undefined()) { *this = BigComplex::undefined();  return *this; }
        BigFloat ac = m_r;
        ac *= o.m_r;
        BigFloat bd = m_i;
        bd *= o.m_i;
        BigFloat ad = m_r;
        ad *= o.m_i;
        BigFloat bc = m_i;
        bc *= o.m_r;
        m_r = std::move(ac);
        m_r -= std::move(bd);
        m_i = std::move(ad);
        m_i += std::move(bc);
        invalidate_cache();
        return *this;
    }

    BigComplex operator*(const BigComplex& o) const {
        BigComplex r(*this);
        r *= o;
        return r;
    }

    BigComplex operator*=(const BigFloat& c) {
        m_r *= c;
        m_i *= c;
        invalidate_cache();
        return *this;
    }

    BigComplex operator*(const BigFloat& c) const {
        BigComplex temp(*this);
        temp *= c;
        return temp;
    }

    BigComplex& operator/=(const BigComplex& o) {
        if (is_error() || o.is_error())     { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined() || o.is_undefined()) { *this = BigComplex::undefined(); return *this; }
        if (o.is_zero()) { *this = BigComplex::undefined(); return *this; }
        BigFloat denom = o.magnitude_squared();
        BigFloat ac = m_r;
        ac *= o.m_r;
        BigFloat bd = m_i;
        bd *= o.m_i;
        BigFloat bc = m_i;
        bc *= o.m_r;
        BigFloat ad = m_r;
        ad *= o.m_i;
        m_r = std::move(ac);
        m_r += std::move(bd);
        m_r /= denom;
        m_i = std::move(bc);
        m_i -= std::move(ad);
        m_i /= denom;
        invalidate_cache();
        return *this;
    }

    BigComplex operator/(const BigComplex& o) const {
        BigComplex r(*this);
        r /= o;
        return r;
    }

    BigComplex operator/=(const BigFloat& c) {
        m_r /= c;
        m_i /= c;
        invalidate_cache();
        return *this;
    }

    BigComplex operator/(const BigFloat& c) const {
        BigComplex temp(*this);
        temp /= c;
        return temp;
    }

    BigComplex& fast_divide_inplace(BigComplex o) {
        if (is_error() || o.is_error())         { *this = BigComplex::error_unit(); return *this; }
        if (is_undefined() || o.is_undefined()) { *this = BigComplex::undefined();  return *this; }
        if (o.is_zero())                        { *this = BigComplex::undefined();  return *this; }
        o.reciprocal_inplace();
        *this *= o;
        return *this;
    }

    BigComplex fast_divide(BigComplex o) const {
        if (is_error() || o.is_error())         return BigComplex::error_unit();
        if (is_undefined() || o.is_undefined()) return BigComplex::undefined();
        if (o.is_zero())                        return BigComplex::undefined();
        o.reciprocal_inplace();
        return *this * o;
    }

public:
    friend BigComplex operator+(BigComplex&& a, const BigComplex& b) { a += b; return a; }
    friend BigComplex operator+(const BigComplex& a, BigComplex&& b) { b += a; return b; }
    friend BigComplex operator+(BigComplex&& a, BigComplex&& b)      { a += b; return a; }

    friend BigComplex operator-(BigComplex&& a, const BigComplex& b) { a -= b; return a; }
    friend BigComplex operator-(const BigComplex& a, BigComplex&& b) { b -= a; b.negate(); return b; }
    friend BigComplex operator-(BigComplex&& a, BigComplex&& b)      { a -= b; return a; }

    friend BigComplex operator*(BigComplex&& a, const BigComplex& b) { a *= b; return a; }
    friend BigComplex operator*(const BigComplex& a, BigComplex&& b) { b *= a; return b; }
    friend BigComplex operator*(BigComplex&& a, BigComplex&& b)      { a *= b; return a; }

    friend BigComplex operator/(BigComplex&& a, const BigComplex& b) { a /= b; return a; }
    friend BigComplex operator/(BigComplex&& a, BigComplex&& b)      { a /= b; return a; }

public:
    friend BigComplex operator+(const BigFloat& a, const BigComplex& c) {
        BigComplex r(a);
        r += c;
        return r;
    }

    friend BigComplex operator+(const BigFloat& a, BigComplex&& c) {
        c += a;                 
        return c;
    }

    friend BigComplex operator-(const BigFloat& a, const BigComplex& c) {
        BigComplex r(a);
        r -= c;
        return r;
    }

    friend BigComplex operator-(const BigFloat& a, BigComplex&& c) {
        c.negate();
        c += a;
        return c;
    }

    friend BigComplex operator/(const BigFloat& a, const BigComplex& c) { return a * c.reciprocal(); }

    friend BigComplex operator/(const BigFloat& a, BigComplex&& c) {
        c.reciprocal_inplace();
        c *= a;
        return c;
    }

public:
    std::string to_string(std::size_t decimals, bool include_seps = true, char sep = ',', std::size_t group = 3) const {
        std::string out = m_r.to_string(decimals, include_seps, sep, group) + "\n";
        const BigFloat imag = m_i.abs();
        const bool imag_neg = m_i.is_negative();
        out += (imag_neg ? "-" : "") + imag.to_string(decimals, include_seps, sep, group) + " i\n";
        return out;
    }

    std::string to_string() const {
        std::string out = m_r.to_string() + "\n";
        const BigFloat imag = m_i.abs();
        const bool imag_neg = m_i.is_negative();
        out += (imag_neg ? "-" : "") + imag.to_string() + " i\n";
        return out;
    }

    std::string to_scientific_string(std::size_t sig_figs) const {
        std::string out = m_r.to_scientific_string(sig_figs) + "\n";
        const BigFloat imag = m_i.abs();
        const bool imag_neg = m_i.is_negative();
        out += (imag_neg ? "-" : "") + imag.to_scientific_string(sig_figs) + " i\n";
        return out;
    }

    std::string to_scientific_string() const {
        std::string out = m_r.to_scientific_string() + "\n";
        const BigFloat imag = m_i.abs();
        const bool imag_neg = m_i.is_negative();
        out += (imag_neg ? "-" : "") + imag.to_scientific_string() + " i\n";
        return out;
    }

    friend std::ostream& operator<<(std::ostream& os, const BigComplex& c) {
        os << c.to_scientific_string();
        return os;
    }
};

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_COMPLEX_HPP