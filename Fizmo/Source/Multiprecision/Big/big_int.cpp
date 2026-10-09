#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {

auto BigInt::add_signed(const BigUInt& omag, bool oneg) -> BigInt& {
        if (m_neg == oneg) {
            m_mag.add_mutable(omag);
            normalize();
            return *this;
        }

        const int c = m_mag.compare(omag);
        if (c == 0) { m_mag = BigUInt::zero(); m_neg = false; return *this; }

        if (c > 0) {
            m_mag.sub_mutable(omag);
            normalize();
            return *this;
        }

        BigUInt t(omag);
        t.sub_mutable(m_mag);
        return assign_from(std::move(t), oneg);
    }

auto BigInt::add_mutable(const BigInt& o) -> BigInt& {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        return add_signed(o.m_mag, o.m_neg);
    }

auto BigInt::sub_mutable(const BigInt& o) -> BigInt& {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        if (this == &o) { m_mag = BigUInt::zero(); m_neg = false; return *this; }
        return add_signed(o.m_mag, !o.m_neg);
    }

auto BigInt::mul_mutable(const BigInt& o) -> BigInt& {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite()) return *this = undefined();
        return assign_from(m_mag * o.m_mag, m_neg != o.m_neg);
    }

auto BigInt::div_mutable(const BigInt& o) -> BigInt& {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return *this = undefined();
        return assign_from(m_mag / o.m_mag, m_neg != o.m_neg);
    }

auto BigInt::mod_mutable(const BigInt& o) -> BigInt& {
        if (is_nan() || o.is_nan()) return *this = nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return *this = undefined();
        return assign_from(m_mag % o.m_mag, m_neg);
    }

auto BigInt::is_exact_division(const BigInt& o) const -> bool {
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return false;
        return (m_mag % o.m_mag).is_zero();
    }

auto BigInt::floor_div(const BigInt& o) const -> BigInt {
        if (is_nan() || o.is_nan()) return nan();
        if (!is_finite() || !o.is_finite() || o.m_mag.is_zero()) return undefined();
        BigInt q = make(m_mag / o.m_mag, m_neg != o.m_neg);
        if ((m_neg != o.m_neg) && !is_exact_division(o)) q.add_signed(BigUInt::one(), true);
        return q;
    }

auto BigInt::to_double() const noexcept -> double {
        if (!is_finite()) return std::numeric_limits<double>::quiet_NaN();
        double result = 0.0;
        for (std::size_t i = m_mag.limb_count(); i > 0; --i) { result = result * 18446744073709551616.0 + static_cast<double>(m_mag.limb(i - 1)); }
        return m_neg ? -result : result;
    }

auto BigInt::to_string(long long base) const -> std::string {
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (base < 2 || base > 36) return "";
        if (m_mag.is_zero()) return "0";
        BigUInt t = m_mag;
        const std::uint64_t b = static_cast<std::uint64_t>(base);
        std::string s;

        while (!t.is_zero()) {
            BigUInt r = t;
            r.mod_small_mutable(b);
            const std::uint64_t d = r.limb(0);
            s.push_back(d < 10 ? char('0' + d) : char('a' + (d - 10)));
            t.div_small_mutable(b);
        }

        if (m_neg) s.push_back('-');
        for (std::size_t i = 0, j = s.size(); i < j; ++i, --j) { char c = s[i]; s[i] = s[j - 1]; s[j - 1] = c; }
        return s;
    }

auto BigInt::parse_string(const std::string& str, long long base, bool validate_only) -> bool {
        if (str.empty() || base < 2 || base > 36) return false;
        std::size_t start = 0;
        bool negative = false;
        if (str[0] == '-') { negative = true; start = 1; }
        else if (str[0] == '+') start = 1;
        if (start >= str.size()) return false;
        if (base == 16 && str.size() > start + 2 && str[start] == '0' && (str[start + 1] == 'x' || str[start + 1] == 'X')) start += 2;
        BigUInt acc = BigUInt::zero();
        const std::uint64_t b = static_cast<std::uint64_t>(base);
        bool found = false;

        for (std::size_t i = start; i < str.size(); ++i) {
            const char c = str[i];
            long long d = -1;
            if (c >= '0' && c <= '9') d = c - '0';
            else if (c >= 'a' && c <= 'z') d = c - 'a' + 10;
            else if (c >= 'A' && c <= 'Z') d = c - 'A' + 10;
            else if (c == '_' || c == ',' || c == '\'') continue;
            else return false;
            if (d < 0 || d >= base) return false;
            found = true;
            if (!validate_only) { acc.mul_small_mutable(b); acc.add_small_mutable(static_cast<std::uint64_t>(d)); }
        }

        if (found && !validate_only) assign_from(std::move(acc), negative);
        return found;
    }

bool operator==(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return a.is_negative() == b.is_negative() && a.magnitude() == b.magnitude();
}

bool operator!=(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    return !(a.is_negative() == b.is_negative() && a.magnitude() == b.magnitude());
}

bool operator<(const BigInt& a, const BigInt& b) noexcept {
    if (!a.is_finite() || !b.is_finite()) return false;
    if (a.is_negative() != b.is_negative()) return a.is_negative();
    return a.is_negative() ? (b.magnitude() < a.magnitude()) : (a.magnitude() < b.magnitude());
}

BigInt operator-(const BigInt& a, BigInt&& b) {
    if (&a == &b) { b.sub_mutable(a); return std::move(b); }
    b.negate_mutable();
    b.add_mutable(a);
    return std::move(b);
}

} // namespace multiprecision
} // namespace fizmo
