#include "fizmo_library.hpp"

#include <cstdio>
#include <cstdlib>

namespace fizmo {
namespace math {
namespace cas {
namespace symbols {

namespace {

constexpr std::int64_t simple_denominator_limit = 65536;
constexpr double       simple_search_limit      = 1099511627776.0;

BigInt pow10(std::uint64_t k) { return Rational::int_pow(BigInt(10), k); }

std::size_t mix_hash(std::size_t h, std::size_t v) noexcept {
    return h ^ (v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
}

std::size_t hash_bigint(std::size_t h, const BigInt& v) noexcept {
    const BigUInt& m = v.magnitude();
    const std::uint64_t* p = m.limbs();
    for (std::size_t i = 0; i < m.limb_count(); ++i) h = mix_hash(h, std::hash<std::uint64_t>{}(p[i]));
    return mix_hash(h, v.is_negative() ? 1u : 0u);
}

bool from_decimal_text(const std::string& text, Rational& out) {
    std::size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    bool neg = false;
    if (i < text.size() && (text[i] == '+' || text[i] == '-')) { neg = text[i] == '-'; ++i; }
    std::string digits;
    long long   frac_digits = 0;
    bool        seen_point  = false;
    bool        any_digit   = false;

    for (; i < text.size(); ++i) {
        const char c = text[i];
        if (c >= '0' && c <= '9') {
            digits.push_back(c);
            any_digit = true;
            if (seen_point) ++frac_digits;
        } else if ((c == '.' || c == ',') && !seen_point) {
            seen_point = true;
        } else {
            break;
        }
    }

    if (!any_digit) return false;
    long long exp10 = 0;

    if (i < text.size() && (text[i] == 'e' || text[i] == 'E')) {
        ++i;
        bool eneg = false;
        if (i < text.size() && (text[i] == '+' || text[i] == '-')) { eneg = text[i] == '-'; ++i; }
        bool any_exp = false;
        long long e = 0;

        for (; i < text.size() && text[i] >= '0' && text[i] <= '9'; ++i) {
            any_exp = true;
            if (e < 100000000) e = e * 10 + (text[i] - '0');
        }

        if (!any_exp) return false;
        exp10 = eneg ? -e : e;
    }

    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) ++i;
    if (i != text.size()) return false;

    const long long scale = exp10 - frac_digits;
    if (scale > 100000 || scale < -100000) return false;
    BigInt num(digits, 10);
    if (neg) num.negate_mutable();

    if (scale >= 0) {
        out = Rational(num * pow10(static_cast<std::uint64_t>(scale)));
    } else {
        out = Rational(num, pow10(static_cast<std::uint64_t>(-scale)));
    }

    return true;
}

} // namespace

Rational::Rational(const BigInt& n, const BigInt& d) : m_num(n), m_den(d) { normalize(); }

void Rational::normalize() {
    if (m_den.is_zero()) {
        m_num = BigInt::zero();
        m_den = BigInt::one();
        return;
    }

    if (m_den.is_negative()) {
        m_num.negate_mutable();
        m_den.negate_mutable();
    }

    if (m_num.is_zero()) {
        m_den = BigInt::one();
        return;
    }

    if (m_den.is_one()) return;
    const BigInt g = gcd(m_num, m_den);

    if (!g.is_one()) {
        m_num /= g;
        m_den /= g;
    }
}

BigInt Rational::gcd(const BigInt& a, const BigInt& b) {
    if (a.is_zero()) return BigInt(b.magnitude(), false);
    if (b.is_zero()) return BigInt(a.magnitude(), false);
    return BigInt(::fizmo::math::gcd(a.magnitude(), b.magnitude()), false);
}

BigInt Rational::int_pow(const BigInt& base, std::uint64_t k) {
    BigInt result = BigInt::one();
    BigInt b      = base;

    while (k > 0) {
        if (k & 1u) result *= b;
        k >>= 1;
        if (k > 0) b *= b;
    }

    return result;
}

bool Rational::int_root(const BigInt& n, std::uint64_t k, BigInt& out) {
    if (k == 0 || n.is_negative()) return false;
    if (k == 1 || n.is_zero() || n.is_one()) { out = n; return true; }
    const std::size_t bits = n.bit_length();
    if (k >= bits) return false;
    BigInt x = BigInt::one() << ((bits + k - 1) / k);
    const BigInt kk(k);
    const BigInt km1(k - 1);

    for (;;) {
        BigInt y = (km1 * x + n / int_pow(x, k - 1)) / kk;
        if (!(y < x)) break;
        x = std::move(y);
    }

    if (int_pow(x, k) != n) return false;
    out = std::move(x);
    return true;
}

Rational Rational::from_double_exact(double v) {
    if (!std::isfinite(v) || v == 0.0) return Rational();
    int e = 0;
    const double m = std::frexp(std::fabs(v), &e);
    const double scaled = std::ldexp(m, 53);
    BigInt mant(scaled);
    if (v < 0.0) mant.negate_mutable();
    const int shift = e - 53;
    if (shift >= 0) return Rational(mant << static_cast<std::size_t>(shift));
    return Rational(mant, BigInt::one() << static_cast<std::size_t>(-shift));
}

Rational Rational::from_double(double v) {
    if (!std::isfinite(v) || v == 0.0) return Rational();
    if (v == std::floor(v)) return Rational(BigInt(v));
    const double a = std::fabs(v);

    if (a < simple_search_limit) {
        std::int64_t h1 = 1, h2 = 0, k1 = 0, k2 = 1;
        double x = a;

        for (int it = 0; it < 64; ++it) {
            const double fl = std::floor(x);
            if (fl > 9.0e15) break;
            const std::int64_t q = static_cast<std::int64_t>(fl);
            const std::int64_t h = q * h1 + h2;
            const std::int64_t k = q * k1 + k2;
            if (k > simple_denominator_limit || k <= 0) break;

            if (static_cast<double>(h) / static_cast<double>(k) == a) {
                return Rational(BigInt(v < 0.0 ? -h : h), BigInt(k));
            }

            h2 = h1; h1 = h;
            k2 = k1; k1 = k;
            const double frac = x - fl;
            if (frac <= 0.0) break;
            x = 1.0 / frac;
        }
    }

    char buf[64];

    for (int prec = 1; prec <= 17; ++prec) {
        std::snprintf(buf, sizeof(buf), "%.*e", prec - 1, v);
        if (std::strtod(buf, nullptr) != v) continue;
        Rational r;
        if (from_decimal_text(buf, r)) return r;
        break;
    }

    return from_double_exact(v);
}

bool Rational::parse(const std::string& text, Rational& out) {
    const std::size_t slash = text.find('/');

    if (slash != std::string::npos) {
        Rational n, d;
        if (!from_decimal_text(text.substr(0, slash), n)) return false;
        if (!from_decimal_text(text.substr(slash + 1), d)) return false;
        if (d.is_zero()) return false;
        out = n / d;
        return true;
    }

    return from_decimal_text(text, out);
}

bool Rational::is_minus_one() const {
    return m_den.is_one() && m_num.is_negative() && m_num.magnitude().is_one();
}

bool Rational::is_half_integer() const {
    return m_den == BigInt(2);
}

bool Rational::to_int64(std::int64_t& out) const {
    if (!m_den.is_one()) return false;
    if (m_num.bit_length() > 62) return false;
    const std::uint64_t mag = m_num.get_lowest_bits();
    out = m_num.is_negative() ? -static_cast<std::int64_t>(mag) : static_cast<std::int64_t>(mag);
    return true;
}

double Rational::to_double() const {
    if (m_num.is_zero()) return 0.0;
    if (m_num.bit_length() <= 53 && m_den.bit_length() <= 53) return m_num.to_double() / m_den.to_double();
    const BigFloatContext c(53, RoundingMode::nearest_even);
    return BigFloat::div(BigFloat(m_num), BigFloat(m_den), c).to_double();
}

Rational Rational::abs() const {
    return Rational(raw_tag{}, BigInt(m_num.magnitude(), false), m_den);
}

Rational Rational::reciprocal() const {
    if (m_num.is_zero()) return Rational();
    return Rational(m_den, m_num);
}

BigInt Rational::trunc() const {
    if (m_den.is_one()) return m_num;
    return m_num / m_den;
}

BigInt Rational::floor() const {
    if (m_den.is_one()) return m_num;
    BigInt t = m_num / m_den;
    if (m_num.is_negative()) t -= BigInt(1);
    return t;
}

BigInt Rational::ceil() const {
    if (m_den.is_one()) return m_num;
    BigInt t = m_num / m_den;
    if (m_num.is_positive()) t += BigInt(1);
    return t;
}

BigInt Rational::round() const {
    if (m_den.is_one()) return m_num;
    const Rational half(raw_tag{}, BigInt(1), BigInt(2));
    const Rational shifted = is_negative() ? (*this - half) : (*this + half);
    return shifted.trunc();
}

Rational Rational::fractional_part() const {
    if (m_den.is_one()) return Rational();
    return *this - Rational(trunc());
}

bool Rational::pow(std::int64_t k, Rational& out) const {
    if (k == 0) { out = Rational(1); return true; }
    if (m_num.is_zero()) {
        if (k < 0) return false;
        out = Rational();
        return true;
    }

    const std::uint64_t ak = k < 0 ? static_cast<std::uint64_t>(-(k + 1)) + 1u : static_cast<std::uint64_t>(k);

    if (m_den.is_one() && m_num.magnitude().is_one()) {
        out = (m_num.is_negative() && (ak & 1u)) ? Rational(-1) : Rational(1);
        return true;
    }

    const std::size_t   nb = m_num.bit_length() > m_den.bit_length() ? m_num.bit_length() : m_den.bit_length();
    if (ak > max_power_bits || (nb > 1 && static_cast<double>(nb) * static_cast<double>(ak) > static_cast<double>(max_power_bits))) return false;
    BigInt n = int_pow(m_num, ak);
    BigInt d = int_pow(m_den, ak);

    if (k < 0) {
        out = Rational(d, n);
    } else {
        out = Rational(raw_tag{}, std::move(n), std::move(d));
    }

    return true;
}

bool Rational::exact_root(std::uint64_t k, Rational& out) const {
    if (k == 0) return false;
    if (k == 1) { out = *this; return true; }

    if (m_num.is_negative()) {
        if ((k & 1u) == 0) return false;
        Rational pos;
        if (!abs().exact_root(k, pos)) return false;
        out = -pos;
        return true;
    }

    BigInt rn, rd;
    if (!int_root(m_num, k, rn)) return false;
    if (!int_root(m_den, k, rd)) return false;
    out = Rational(raw_tag{}, std::move(rn), std::move(rd));
    return true;
}

std::size_t Rational::hash() const noexcept {
    std::size_t h = 0x51ed27f3a4c1b5d7ULL;
    h = hash_bigint(h, m_num);
    h = hash_bigint(h, m_den);
    return h;
}

std::string Rational::to_string() const {
    if (m_den.is_one()) return m_num.to_string();
    return m_num.to_string() + "/" + m_den.to_string();
}

Rational operator-(const Rational& a) {
    BigInt n = a.m_num;
    n.negate_mutable();
    return Rational(Rational::raw_tag{}, std::move(n), a.m_den);
}

Rational operator+(const Rational& a, const Rational& b) {
    if (a.m_den.is_one() && b.m_den.is_one()) return Rational(Rational::raw_tag{}, a.m_num + b.m_num, BigInt::one());
    return Rational(a.m_num * b.m_den + b.m_num * a.m_den, a.m_den * b.m_den);
}

Rational operator-(const Rational& a, const Rational& b) {
    if (a.m_den.is_one() && b.m_den.is_one()) return Rational(Rational::raw_tag{}, a.m_num - b.m_num, BigInt::one());
    return Rational(a.m_num * b.m_den - b.m_num * a.m_den, a.m_den * b.m_den);
}

Rational operator*(const Rational& a, const Rational& b) {
    if (a.m_den.is_one() && b.m_den.is_one()) return Rational(Rational::raw_tag{}, a.m_num * b.m_num, BigInt::one());
    return Rational(a.m_num * b.m_num, a.m_den * b.m_den);
}

Rational operator/(const Rational& a, const Rational& b) {
    if (b.m_num.is_zero()) return Rational();
    return Rational(a.m_num * b.m_den, a.m_den * b.m_num);
}

bool operator<(const Rational& a, const Rational& b) {
    if (a.m_den == b.m_den) return a.m_num < b.m_num;
    return a.m_num * b.m_den < b.m_num * a.m_den;
}

} // namespace symbols
} // namespace cas
} // namespace math
} // namespace fizmo
