#ifndef FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP
#define FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace fizmo {
namespace multiprecision {
namespace fdetail {
namespace decimal {

class bignum {
public:
    std::vector<std::uint32_t> limbs;

    bignum() = default;

    explicit bignum(std::uint64_t v) {
        while (v) { limbs.push_back(static_cast<std::uint32_t>(v & 0xFFFFFFFFull)); v >>= 32; }
    }

    bool is_zero() const noexcept { return limbs.empty(); }

    void trim() noexcept { while (!limbs.empty() && limbs.back() == 0) limbs.pop_back(); }

    long long bit_length() const noexcept {
        if (limbs.empty()) return 0;
        std::uint32_t top = limbs.back();
        long long n = 0;
        while (top) { ++n; top >>= 1; }
        return static_cast<long long>(limbs.size() - 1) * 32 + n;
    }

    bool get_bit(long long i) const noexcept {
        const std::size_t li = static_cast<std::size_t>(i / 32);
        if (i < 0 || li >= limbs.size()) return false;
        return ((limbs[li] >> (i % 32)) & 1u) != 0;
    }

    void set_bit(long long i) {
        const std::size_t li = static_cast<std::size_t>(i / 32);
        if (limbs.size() <= li) limbs.resize(li + 1, 0);
        limbs[li] |= (1u << (i % 32));
    }

    void mul_small(std::uint32_t k) {
        if (k == 0) { limbs.clear(); return; }
        std::uint64_t carry = 0;
        for (std::uint32_t& l : limbs) {
            const std::uint64_t t = static_cast<std::uint64_t>(l) * k + carry;
            l = static_cast<std::uint32_t>(t & 0xFFFFFFFFull);
            carry = t >> 32;
        }
        if (carry) limbs.push_back(static_cast<std::uint32_t>(carry));
    }

    void add_small(std::uint32_t k) {
        std::uint64_t carry = k;
        for (std::size_t i = 0; carry && i < limbs.size(); ++i) {
            const std::uint64_t t = static_cast<std::uint64_t>(limbs[i]) + carry;
            limbs[i] = static_cast<std::uint32_t>(t & 0xFFFFFFFFull);
            carry = t >> 32;
        }
        if (carry) limbs.push_back(static_cast<std::uint32_t>(carry));
    }

    void shl(long long bits) {
        if (limbs.empty() || bits <= 0) return;
        const std::size_t whole = static_cast<std::size_t>(bits / 32);
        const unsigned part = static_cast<unsigned>(bits % 32);

        if (part) {
            std::uint32_t carry = 0;
            for (std::uint32_t& l : limbs) {
                const std::uint32_t next = l >> (32 - part);
                l = (l << part) | carry;
                carry = next;
            }
            if (carry) limbs.push_back(carry);
        }

        if (whole) limbs.insert(limbs.begin(), whole, 0u);
    }

    static int compare(const bignum& a, const bignum& b) noexcept {
        if (a.limbs.size() != b.limbs.size()) return a.limbs.size() < b.limbs.size() ? -1 : 1;
        for (std::size_t i = a.limbs.size(); i-- > 0;) {
            if (a.limbs[i] != b.limbs[i]) return a.limbs[i] < b.limbs[i] ? -1 : 1;
        }
        return 0;
    }

    void add(const bignum& b) {
        if (limbs.size() < b.limbs.size()) limbs.resize(b.limbs.size(), 0);
        std::uint64_t carry = 0;
        for (std::size_t i = 0; i < limbs.size(); ++i) {
            const std::uint64_t t = static_cast<std::uint64_t>(limbs[i]) + (i < b.limbs.size() ? b.limbs[i] : 0u) + carry;
            limbs[i] = static_cast<std::uint32_t>(t & 0xFFFFFFFFull);
            carry = t >> 32;
        }
        if (carry) limbs.push_back(static_cast<std::uint32_t>(carry));
    }

    void sub(const bignum& b) {
        std::int64_t borrow = 0;
        for (std::size_t i = 0; i < limbs.size(); ++i) {
            std::int64_t t = static_cast<std::int64_t>(limbs[i]) - (i < b.limbs.size() ? static_cast<std::int64_t>(b.limbs[i]) : 0) - borrow;
            borrow = t < 0 ? 1 : 0;
            if (t < 0) t += (std::int64_t(1) << 32);
            limbs[i] = static_cast<std::uint32_t>(t);
        }
        trim();
    }

    static bignum mul(const bignum& a, const bignum& b) {
        bignum r;
        if (a.is_zero() || b.is_zero()) return r;
        r.limbs.assign(a.limbs.size() + b.limbs.size(), 0);

        for (std::size_t i = 0; i < a.limbs.size(); ++i) {
            std::uint64_t carry = 0;
            const std::uint64_t ai = a.limbs[i];
            for (std::size_t j = 0; j < b.limbs.size(); ++j) {
                const std::uint64_t t = ai * b.limbs[j] + r.limbs[i + j] + carry;
                r.limbs[i + j] = static_cast<std::uint32_t>(t & 0xFFFFFFFFull);
                carry = t >> 32;
            }
            std::size_t k = i + b.limbs.size();
            while (carry) {
                const std::uint64_t t = static_cast<std::uint64_t>(r.limbs[k]) + carry;
                r.limbs[k] = static_cast<std::uint32_t>(t & 0xFFFFFFFFull);
                carry = t >> 32;
                ++k;
            }
        }

        r.trim();
        return r;
    }

    static bignum pow5(long long n) {
        bignum result(1), base(5);
        while (n > 0) {
            if (n & 1) result = mul(result, base);
            n >>= 1;
            if (n) base = mul(base, base);
        }
        return result;
    }

    static bignum pow10(long long n) {
        bignum r = pow5(n);
        r.shl(n);
        return r;
    }

    static bignum div_bits(bignum a, const bignum& b, long long qbits, bool& remainder_nonzero) {
        bignum q;

        for (long long i = qbits - 1; i >= 0; --i) {
            bignum t = b;
            t.shl(i);
            if (compare(a, t) >= 0) { a.sub(t); q.set_bit(i); }
        }

        remainder_nonzero = !a.is_zero();
        return q;
    }

    std::uint32_t div_digit(const bignum& s) {
        std::uint32_t d = 0;
        while (compare(*this, s) >= 0) { sub(s); ++d; }
        return d;
    }
};

struct digits_result {
    std::string digits;
    long long   exponent10 = 0;
};

inline digits_result shortest(const bignum& f, long long e, bool lower_gap_half, bool even) {
    bignum r, s, mplus, mminus;

    if (e >= 0) {
        bignum be(1); be.shl(e);
        if (!lower_gap_half) {
            r = f; r.shl(e + 1);
            s = bignum(2);
            mplus = be; mminus = be;
        } else {
            r = f; r.shl(e + 2);
            s = bignum(4);
            mplus = be; mplus.shl(1);
            mminus = be;
        }
    } else {
        if (!lower_gap_half) {
            r = f; r.shl(1);
            s = bignum(1); s.shl(1 - e);
            mplus = bignum(1); mminus = bignum(1);
        } else {
            r = f; r.shl(2);
            s = bignum(1); s.shl(2 - e);
            mplus = bignum(2); mminus = bignum(1);
        }
    }

    const long long bits = f.bit_length() + e - 1;
    long long k = static_cast<long long>(static_cast<double>(bits) * 0.30102999566398119521) - 1;

    if (k >= 0) {
        s = bignum::mul(s, bignum::pow10(k));
    } else {
        const bignum p = bignum::pow10(-k);
        r = bignum::mul(r, p);
        mplus = bignum::mul(mplus, p);
        mminus = bignum::mul(mminus, p);
    }

    for (;;) {
        bignum high = r; high.add(mplus);
        const int c = bignum::compare(high, s);
        if (even ? c >= 0 : c > 0) { s.mul_small(10); ++k; }
        else break;
    }

    digits_result out;
    out.exponent10 = k;

    for (;;) {
        r.mul_small(10); mplus.mul_small(10); mminus.mul_small(10);
        std::uint32_t d = r.div_digit(s);
        const int lowc = bignum::compare(r, mminus);
        bignum high = r; high.add(mplus);
        const int highc = bignum::compare(high, s);
        const bool tc1 = even ? lowc <= 0 : lowc < 0;
        const bool tc2 = even ? highc >= 0 : highc > 0;

        if (!tc1 && !tc2) { out.digits += static_cast<char>('0' + d); continue; }

        if (tc1 && tc2) {
            bignum twice = r; twice.shl(1);
            const int half = bignum::compare(twice, s);
            if (half > 0 || (half == 0 && (d & 1u))) ++d;
        } else if (tc2) {
            ++d;
        }

        out.digits += static_cast<char>('0' + d);
        break;
    }

    return out;
}

inline digits_result fixed_digits(const bignum& f, long long e, long long count) {
    bignum r = f, s(1);
    if (e >= 0) r.shl(e); else s.shl(-e);
    const long long bits = f.bit_length() + e - 1;
    long long k = static_cast<long long>(static_cast<double>(bits) * 0.30102999566398119521) - 1;
    if (k >= 0) s = bignum::mul(s, bignum::pow10(k)); else r = bignum::mul(r, bignum::pow10(-k));
    while (bignum::compare(r, s) >= 0) { s.mul_small(10); ++k; }

    digits_result out;
    out.exponent10 = k;

    for (long long i = 0; i < count; ++i) {
        r.mul_small(10);
        out.digits += static_cast<char>('0' + r.div_digit(s));
    }

    bignum twice = r; twice.shl(1);
    const int half = bignum::compare(twice, s);
    const bool up = half > 0 || (half == 0 && !out.digits.empty() && ((out.digits.back() - '0') & 1));

    if (up) {
        long long i = static_cast<long long>(out.digits.size()) - 1;
        while (i >= 0 && out.digits[static_cast<std::size_t>(i)] == '9') { out.digits[static_cast<std::size_t>(i)] = '0'; --i; }
        if (i >= 0) ++out.digits[static_cast<std::size_t>(i)];
        else { out.digits.insert(out.digits.begin(), '1'); out.digits.pop_back(); ++out.exponent10; }
    }

    while (out.digits.size() > 1 && out.digits.back() == '0') out.digits.pop_back();
    return out;
}

inline std::string format_fixed(const std::string& digits, long long k) {
    const long long n = static_cast<long long>(digits.size());
    std::string out;

    if (k <= 0) {
        out = "0.";
        out.append(static_cast<std::size_t>(-k), '0');
        out += digits;
    } else if (k >= n) {
        out = digits;
        out.append(static_cast<std::size_t>(k - n), '0');
    } else {
        out = digits.substr(0, static_cast<std::size_t>(k));
        out += '.';
        out += digits.substr(static_cast<std::size_t>(k));
    }

    return out;
}

inline std::string format_scientific(const std::string& digits, long long k) {
    std::string out(1, digits[0]);
    if (digits.size() > 1) { out += '.'; out += digits.substr(1); }
    const long long e10 = k - 1;
    out += 'e';
    if (e10 >= 0) out += '+';
    out += std::to_string(e10);
    return out;
}

struct parsed_decimal {
    bool      ok = false;
    bool      negative = false;
    bool      is_nan = false;
    bool      is_inf = false;
    bignum    digits;
    long long digit_count = 0;
    long long exponent10 = 0;
};

inline parsed_decimal parse(const char* s) {
    parsed_decimal p;
    if (!s) return p;
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\n' || c == '\r'; };
    auto digit = [](char c) { return c >= '0' && c <= '9'; };
    auto lower = [](char c) { return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : c; };
    while (ws(*s)) ++s;
    if (*s == '-') { p.negative = true; ++s; } else if (*s == '+') ++s;

    if (lower(s[0]) == 'n' && lower(s[1]) == 'a' && lower(s[2]) == 'n') { p.ok = true; p.is_nan = true; return p; }
    if (lower(s[0]) == 'i' && lower(s[1]) == 'n' && lower(s[2]) == 'f') { p.ok = true; p.is_inf = true; return p; }
    if (s[0] == '\xE2' && s[1] == '\x88' && s[2] == '\x9E') { p.ok = true; p.is_inf = true; return p; }

    long long int_len = 0, frac_len = 0;
    bool seen_nonzero = false;

    auto take = [&](char c) {
        if (c != '0') seen_nonzero = true;
        if (!seen_nonzero) return;
        p.digits.mul_small(10);
        p.digits.add_small(static_cast<std::uint32_t>(c - '0'));
        ++p.digit_count;
    };

    while (digit(*s)) { take(*s); ++s; ++int_len; }

    if (*s == '.') {
        ++s;
        while (digit(*s)) { take(*s); ++s; ++frac_len; }
    }

    if (int_len == 0 && frac_len == 0) return p;
    long long dexp = 0;

    if (*s == 'e' || *s == 'E') {
        ++s;
        bool neg = false;
        if (*s == '-') { neg = true; ++s; } else if (*s == '+') ++s;
        if (!digit(*s)) return p;
        bool huge = false;

        while (digit(*s)) {
            if (dexp < 100000000000000LL) dexp = dexp * 10 + (*s - '0');
            else huge = true;
            ++s;
        }

        if (huge) dexp = 100000000000000LL;
        if (neg) dexp = -dexp;
    }

    while (*s) { if (!ws(*s)) return p; ++s; }
    p.exponent10 = dexp - frac_len;
    p.ok = true;
    return p;
}

} // namespace decimal
} // namespace fdetail
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_DECIMAL_CONVERSION_HPP
