#ifndef FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP
#define FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP

#include <cstdint>
#include <cstddef>
#include <type_traits>
#include <limits>
#include <string>
#include <ostream>
#include <cmath>
#include <utility>

#include "big_int.hpp"
#include "../Fixed Width Float/traits.hpp"

namespace fizmo {
namespace multiprecision {

enum class RoundingMode {
    nearest_even = 0, // ties to even significand 
    nearest_away,     // ties away from zero
    toward_zero,      // truncate
    toward_pos_inf,   // ceiling
    toward_neg_inf    // floor
};

struct BigFloatContext {
    static constexpr long double log2_10      = 3.321928094887362347870319429489L;
    static constexpr long double log10_2      = 0.301029995663981195213738894724L;
    static constexpr std::size_t min_prec     = 2;
    static constexpr std::size_t default_prec = 53; // double's significant precision
    static constexpr std::size_t max_prec     = BigUInt::max_bits;
    static constexpr std::size_t max_dps      = std::size_t(static_cast<long double>(max_prec) * log10_2);

    std::size_t  precision;
    RoundingMode rounding_mode;

    constexpr BigFloatContext() noexcept : precision(default_prec), rounding_mode(RoundingMode::nearest_even) {}
    explicit constexpr BigFloatContext(std::size_t p, RoundingMode r = RoundingMode::nearest_even) noexcept : precision(clamp_precision(p)), rounding_mode(r) {}

    static constexpr std::size_t clamp_precision(std::size_t p) noexcept { return p < min_prec ? min_prec : (p > max_prec ? max_prec : p); }
    static constexpr std::size_t precision_from_dps(std::size_t dps) noexcept { return clamp_precision(static_cast<std::size_t>(static_cast<long double>(dps) * log2_10) + 2); }

    static constexpr std::size_t dps_from_precision(std::size_t prec) noexcept {
        const std::size_t d = static_cast<std::size_t>(static_cast<long double>(prec) * log10_2);
        return d == 0 ? 1 : d;
    }

    std::size_t dps() const noexcept { return dps_from_precision(precision); }

    static constexpr BigFloatContext with_dps(std::size_t dps, RoundingMode r = RoundingMode::nearest_even) noexcept {
        return BigFloatContext(precision_from_dps(dps), r);
    }

    BigFloatContext extended(std::size_t guard_bits) const noexcept {
        return BigFloatContext(precision + guard_bits, rounding_mode);
    }

    static BigFloatContext& current() noexcept {
        static thread_local BigFloatContext ctx;
        return ctx;
    }

    static void set_current(const BigFloatContext& c) noexcept { current() = c; }
};

class ScopedContext {
private:
    BigFloatContext m_saved;

public:
    explicit ScopedContext(const BigFloatContext& c) noexcept : m_saved(BigFloatContext::current()) { BigFloatContext::current() = c; }

    explicit ScopedContext(std::size_t prec) noexcept : m_saved(BigFloatContext::current()) {
        BigFloatContext::current() = BigFloatContext(prec, m_saved.rounding_mode);
    }

    ScopedContext(std::size_t prec, RoundingMode rnd) noexcept : m_saved(BigFloatContext::current()) {
        BigFloatContext::current() = BigFloatContext(prec, rnd);
    }

    static ScopedContext extend(std::size_t guard_bits) noexcept {
        return ScopedContext(BigFloatContext::current().extended(guard_bits));
    }

    ~ScopedContext() noexcept { BigFloatContext::current() = m_saved; }

    ScopedContext(const ScopedContext&)            = delete;
    ScopedContext& operator=(const ScopedContext&) = delete;

    const BigFloatContext& saved() const noexcept { return m_saved; }
};

class BigFloat {
public:
    enum class fpclass : std::uint8_t { finite = 0, infinite, nan, undefined };
    enum class ordering : std::int8_t { less = -1, equal = 0, greater = 1, unordered = 2 };

    static constexpr std::int64_t exp_max         = std::int64_t(1) << 48;
    static constexpr std::int64_t exp_min         = -exp_max;
    static constexpr std::int64_t exp_inf         = std::numeric_limits<std::int64_t>::max(); // returned by get_exp_*, never stored
    static constexpr std::int64_t exp_none        = std::numeric_limits<std::int64_t>::min(); // returned by get_exp_*, never stored
    static constexpr std::size_t  pow10_cmp_cap   = std::size_t(1) << 20;
    static constexpr std::size_t  no_digit_limit  = ~std::size_t(0);
    static constexpr std::size_t  decimal_exp_cap = std::size_t(1) << 22;

private:
    BigUInt      m_mag;  // significand magnitude, always odd when finite and non-zero
    std::int64_t m_exp;  // value == (-1)^m_neg * m_mag * 2^m_exp
    bool         m_neg;  // sign bit; meaningful for zero, ignored for nan/undefined
    fpclass      m_cls;

    struct raw_tag {};

    BigFloat(raw_tag, BigUInt mag, bool neg, std::int64_t exp, fpclass cls) : m_mag(std::move(mag)), m_exp(exp), m_neg(neg), m_cls(cls) {}

    static std::int64_t add_sat(std::int64_t e, std::int64_t d) noexcept {
        if (d > 0 && e > std::numeric_limits<std::int64_t>::max() - d) return std::numeric_limits<std::int64_t>::max();
        if (d < 0 && e < std::numeric_limits<std::int64_t>::min() - d) return std::numeric_limits<std::int64_t>::min();
        return e + d;
    }

    BigFloat& set_finite(BigUInt mag, bool neg, std::int64_t exp) {
        if (mag.is_undefined()) return *this = undefined();
        if (mag.is_zero())      return *this = zero(neg);
        const long long tz = mag.count_trailing_zeros();

        if (tz > 0) {
            mag.shift_right_mutable(static_cast<std::size_t>(tz));
            exp = add_sat(exp, static_cast<std::int64_t>(tz));
        }

        if (exp > exp_max) return *this = infinity(neg);
        if (exp < exp_min) return *this = zero(neg);
        m_mag = std::move(mag);
        m_exp = exp;
        m_neg = neg;
        m_cls = fpclass::finite;
        return *this;
    }

    BigFloat& set_signed(const BigInt& v, std::int64_t exp) {
        if (v.is_nan())       return *this = nan();
        if (v.is_undefined()) return *this = undefined();
        return set_finite(v.magnitude(), v.is_negative(), exp);
    }

    static void round_significand(BigUInt& mag, std::int64_t& exp, bool neg, std::size_t prec, bool sticky, RoundingMode mode) {
        if (mag.is_undefined() || mag.is_zero()) return;
        const std::size_t len = mag.bit_length();
        if (len <= prec && !sticky) return;
        const std::size_t drop = (len > prec) ? (len - prec) : 0;
        bool round_bit = false;

        if (drop > 0) {
            round_bit = mag.get_bit(drop - 1);

            if (!sticky && drop > 1) {
                const long long ctz = mag.count_trailing_zeros();
                sticky = (ctz >= 0) && (ctz < static_cast<long long>(drop) - 1);
            }

            mag.shift_right_mutable(drop);
            exp = add_sat(exp, static_cast<std::int64_t>(drop));
        }

        bool up = false;

        switch (mode) {
            case RoundingMode::nearest_even:   up = round_bit && (sticky || mag.get_bit(0)); break;
            case RoundingMode::nearest_away:   up = round_bit;                               break;
            case RoundingMode::toward_zero:    up = false;                                   break;
            case RoundingMode::toward_pos_inf: up = !neg && (round_bit || sticky);           break;
            case RoundingMode::toward_neg_inf: up =  neg && (round_bit || sticky);           break;
        }

        if (up) mag.add_small_mutable(1);
    }

    static BigUInt pow5(std::uint64_t n) {
        BigUInt r = BigUInt::one(), b(std::uint64_t(5));
        while (n != 0) { if (n & 1u) r = r * b; n >>= 1; if (n != 0) b = b * b; }
        return r;
    }

    void assign_decimal(BigUInt digits, bool neg, long long e10) {
        if (digits.is_undefined()) { *this = undefined(); return; }
        if (digits.is_zero())      { *this = zero(neg);   return; }
        const long double be = static_cast<long double>(e10) * BigFloatContext::log2_10;
        if (be >  static_cast<long double>(exp_max)) { *this = infinity(neg); return; }
        if (be <  static_cast<long double>(exp_min)) { *this = zero(neg);     return; }
        const long long cap = static_cast<long long>(BigUInt::max_bits / 3);
        if (e10 >  cap) { *this = infinity(neg); return; }
        if (e10 < -cap) { *this = zero(neg);     return; }

        if (e10 >= 0) {
            set_finite(digits * pow5(static_cast<std::uint64_t>(e10)), neg, static_cast<std::int64_t>(e10));
            return;
        }

        const std::uint64_t m    = static_cast<std::uint64_t>(-e10);
        const BigUInt       d    = pow5(m);
        const BigFloatContext& c = BigFloatContext::current();
        const std::size_t   want = c.precision + 2;
        const std::size_t   have = digits.bit_length();
        const std::size_t   need = d.bit_length() + want;
        const std::size_t   sh   = (have >= need) ? 0 : (need - have);
        digits.shift_left_mutable(sh);
        BigUInt      q      = digits / d;
        const bool   sticky = !(digits % d).is_zero();
        std::int64_t exp    = -(static_cast<std::int64_t>(sh) + static_cast<std::int64_t>(m));
        round_significand(q, exp, neg, c.precision, sticky, c.rounding_mode);
        set_finite(std::move(q), neg, exp);
    }

    void construct_from_long_double(long double value) {
        if (!(value == value)) { *this = nan(); return; }
        const bool neg = std::signbit(value);
        if (value == 0.0L)     { *this = zero(neg); return; }
        long double a = neg ? -value : value;
        if (!(a < std::numeric_limits<long double>::infinity())) { *this = infinity(neg); return; }
        int e2 = 0;
        a = std::frexp(a, &e2);
        BigUInt   mag  = BigUInt::zero();
        long long bits = 0;

        while (a != 0.0L) {
            a = std::ldexp(a, 64);
            const std::uint64_t part = static_cast<std::uint64_t>(a);
            a -= static_cast<long double>(part);
            mag.shift_left_mutable(64);
            mag.add_small_mutable(part);
            bits += 64;
        }

        set_finite(std::move(mag), neg, static_cast<std::int64_t>(e2) - bits);
    }

    static bool word_is(const char* s, const char* w) noexcept {
        std::size_t i = 0;

        for (; w[i] != '\0'; ++i) {
            const char c = s[i];
            const char l = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
            if (l != w[i]) return false;
        }

        while (s[i] == ' ' || s[i] == '\t') ++i;
        return s[i] == '\0';
    }

    bool parse_string(const char* s) {
        if (s == nullptr) return false;
        std::size_t i = 0;
        while (s[i] == ' ' || s[i] == '\t') ++i;
        bool neg = false;
        if (s[i] == '+' || s[i] == '-') { neg = (s[i] == '-'); ++i; }
        if (word_is(s + i, "nan"))       { *this = nan();         return true; }
        if (word_is(s + i, "undefined")) { *this = undefined();   return true; }
        if (word_is(s + i, "inf") || word_is(s + i, "infinity")) { *this = infinity(neg); return true; }
        BigUInt   mag  = BigUInt::zero();
        long long frac = 0;
        bool      any = false, dot = false;

        for (; s[i] != '\0'; ++i) {
            const char c = s[i];
            if (c == '.') { if (dot) return false; dot = true; continue; }
            if (c < '0' || c > '9') break;
            mag.mul_small_mutable(10);
            mag.add_small_mutable(static_cast<std::uint64_t>(c - '0'));
            if (dot) ++frac;
            any = true;
        }

        if (!any) return false;
        long long e10 = 0;

        if (s[i] == 'e' || s[i] == 'E') {
            ++i;
            bool eneg = false;
            if (s[i] == '+' || s[i] == '-') { eneg = (s[i] == '-'); ++i; }
            if (s[i] < '0' || s[i] > '9') return false;
            const long long ecap = 1000000000000000LL;
            for (; s[i] >= '0' && s[i] <= '9'; ++i) if (e10 < ecap) e10 = e10 * 10 + (s[i] - '0');
            if (eneg) e10 = -e10;
        }

        while (s[i] == ' ' || s[i] == '\t') ++i;
        if (s[i] != '\0') return false;
        assign_decimal(std::move(mag), neg, e10 - frac);
        return true;
    }

    int compare_abs_pow10(std::int64_t k, bool& ok) const {
        ok = true;
        const std::int64_t cap = static_cast<std::int64_t>(pow10_cmp_cap);
        const std::int64_t E   = m_exp;

        if (k >= 0) {
            if (k > cap) { ok = false; return 0; }
            BigUInt lhs = m_mag;
            BigUInt rhs = pow5(static_cast<std::uint64_t>(k));
            const std::int64_t d = E - k;
            if (d >= 0) { if ( d > cap) { ok = false; return 0; } lhs.shift_left_mutable(static_cast<std::size_t>(d)); }
            else        { if (-d > cap) { ok = false; return 0; } rhs.shift_left_mutable(static_cast<std::size_t>(-d)); }
            return lhs.compare(rhs);
        }

        if (-k > cap) { ok = false; return 0; }
        const std::uint64_t m = static_cast<std::uint64_t>(-k);
        BigUInt lhs = m_mag * pow5(m);
        BigUInt rhs = BigUInt::one();
        const std::int64_t s = E + static_cast<std::int64_t>(m);
        if (s >= 0) { if ( s > cap) { ok = false; return 0; } lhs.shift_left_mutable(static_cast<std::size_t>(s)); }
        else        { if (-s > cap) { ok = false; return 0; } rhs.shift_left_mutable(static_cast<std::size_t>(-s)); }
        return lhs.compare(rhs);
    }

    template <std::size_t Bits, sign S>
    typename std::enable_if<S == sign::is_unsigned, BigFloat&>::type
    assign_integer(const integer<Bits, S>& v, std::int64_t exp) {
        if (v.is_undefined()) return *this = undefined();
        return set_finite(BigUInt(v), false, exp);
    }

    template <std::size_t Bits, sign S>
    typename std::enable_if<S == sign::is_signed, BigFloat&>::type
    assign_integer(const integer<Bits, S>& v, std::int64_t exp) {
        if (v.is_undefined()) return *this = undefined();
        return set_finite(BigUInt(v.abs()), v.is_negative(), exp);
    }

    static BigFloat reciprocal_finite(const BigUInt& mag, bool neg, std::int64_t exp, const BigFloatContext& ctx) {
        BigFloat r;

        if (mag.is_one()) {
            r.set_finite(BigUInt::one(), neg, add_sat(0, -exp));
            return r;
        }

        const std::size_t L = mag.bit_length();
        if (L + 1 >= BigUInt::max_bits) return undefined();
        if (ctx.precision > BigUInt::max_bits - L - 1) return undefined();
        const std::size_t k = L + ctx.precision + 1;
        BigUInt num = BigUInt::one();
        num.shift_left_mutable(k);
        BigUInt q = num / mag;
        std::int64_t e = add_sat(-static_cast<std::int64_t>(k), -exp);
        round_significand(q, e, neg, ctx.precision, true, ctx.rounding_mode);
        r.set_finite(std::move(q), neg, e);
        return r;
    }

    static BigUInt align_to(const BigUInt& m, std::int64_t e, std::int64_t E, bool& sticky) {
        if (e >= E) {
            BigUInt r(m);
            r.shift_left_mutable(static_cast<std::size_t>(e - E));
            return r;
        }

        const std::uint64_t d   = static_cast<std::uint64_t>(E - e);
        const std::uint64_t len = static_cast<std::uint64_t>(m.bit_length());
        if (d >= len) { if (!m.is_zero()) sticky = true; return BigUInt::zero(); }
        const long long ctz = m.count_trailing_zeros();
        if (ctz >= 0 && static_cast<std::uint64_t>(ctz) < d) sticky = true;
        BigUInt r(m);
        r.shift_right_mutable(static_cast<std::size_t>(d));
        return r;
    }

    static BigFloat add_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
    ) {
        const std::int64_t prec = static_cast<std::int64_t>(ctx.precision);
        const std::int64_t a2   = add_sat(ae, static_cast<std::int64_t>(am.bit_length()));
        const std::int64_t b2   = add_sat(be, static_cast<std::int64_t>(bm.bit_length()));
        const std::int64_t top  = (a2 > b2) ? a2 : b2;
        const std::int64_t hi_e = (ae > be) ? ae : be;
        const std::int64_t lo_e = (ae < be) ? ae : be;
        const std::int64_t d2   = (a2 > b2) ? (a2 - b2) : (b2 - a2);
        bool exact_align = (aneg != bneg) && (d2 <= 1);

        for (;;) {
            std::int64_t E;

            if (exact_align) {
                E = lo_e;
            } else {
                const std::int64_t F = add_sat(top, -(prec + 3));
                E = (F <= lo_e) ? lo_e : ((F < hi_e) ? F : hi_e);
            }

            bool sticky = false;
            BigUInt A = align_to(am, ae, E, sticky);
            BigUInt B = align_to(bm, be, E, sticky);
            const bool   trunc_a = (ae < E);   
            std::int64_t e       = E;

            if (aneg == bneg) {
                A.add_mutable(B);
                round_significand(A, e, aneg, ctx.precision, sticky, ctx.rounding_mode);
                BigFloat r;
                r.set_finite(std::move(A), aneg, e);
                return r;
            }

            const int c = A.compare(B);
            if (c == 0 && !sticky) return zero(ctx.rounding_mode == RoundingMode::toward_neg_inf);
            const bool a_bigger = (c > 0) || (c == 0 && trunc_a);
            const bool neg      = a_bigger ? aneg : bneg;
            BigUInt D;

            if (a_bigger) { D = std::move(A); D.sub_mutable(B); }
            else          { D = std::move(B); D.sub_mutable(A); }

            if (sticky && D.bit_length() < ctx.precision + 2 && !exact_align) {
                exact_align = true;
                continue;
            }

            if (sticky && (a_bigger != trunc_a)) D.sub_small_mutable(1);
            round_significand(D, e, neg, ctx.precision, sticky, ctx.rounding_mode);
            BigFloat r;
            r.set_finite(std::move(D), neg, e);
            return r;
        }
    }

    static BigFloat mul_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
    ) {
        const bool   neg = (aneg != bneg);
        BigUInt      p   = am * bm;          
        std::int64_t e   = add_sat(ae, be);
        round_significand(p, e, neg, ctx.precision, false, ctx.rounding_mode);
        BigFloat r;
        r.set_finite(std::move(p), neg, e);
        return r;
    }

    static BigFloat add_signed_impl(const BigFloat& a, const BigFloat& b, bool flip_b, const BigFloatContext& ctx) {
        if (a.is_nan()       || b.is_nan())       return nan();
        if (a.is_undefined() || b.is_undefined()) return undefined();
        const bool bneg = flip_b ? !b.m_neg : b.m_neg;

        if (a.is_infinite() || b.is_infinite()) {
            if (a.is_infinite() && b.is_infinite()) return (a.m_neg == bneg) ? infinity(a.m_neg) : undefined();
            return a.is_infinite() ? infinity(a.m_neg) : infinity(bneg);
        }

        return add_finite(a.m_mag, a.m_neg, a.m_exp, b.m_mag, bneg, b.m_exp, ctx);
    }

    static BigFloat divide_finite(
        const BigUInt& xm, bool xneg, std::int64_t xe,
        const BigUInt& ym, bool yneg, std::int64_t ye,
        const BigFloatContext& ctx
    ) {
        const bool        neg  = (xneg != yneg);
        const std::size_t Lx   = xm.bit_length();
        const std::size_t Ly   = ym.bit_length();
        const std::size_t want = ctx.precision + 1;  
        const std::size_t s    = (Lx < Ly + want) ? (Ly + want - Lx) : 0;
        if (s > BigUInt::max_bits - Lx) return undefined();
        BigUInt n = xm;
        n.shift_left_mutable(s);
        BigUInt    q      = n / ym;
        const bool sticky = !(n % ym).is_zero();
        std::int64_t e    = add_sat(add_sat(xe, -ye), -static_cast<std::int64_t>(s));
        round_significand(q, e, neg, ctx.precision, sticky, ctx.rounding_mode);
        BigFloat r;
        r.set_finite(std::move(q), neg, e);
        return r;
    }

    static int compare_aligned(const BigUInt& a, std::size_t La, const BigUInt& b, std::size_t Lb) noexcept {
        if (La == Lb) return a.compare(b);
        const std::size_t n = (La < Lb) ? La : Lb;

        for (std::size_t i = 1; i <= n; ++i) {
            const bool x = a.get_bit(La - i);
            const bool y = b.get_bit(Lb - i);
            if (x != y) return x ? 1 : -1;
        }

        return (La > Lb) ? 1 : -1;
    }

    static std::string digits_of(BigUInt v) {
        if (v.is_undefined()) return std::string();
        if (v.is_zero())      return std::string("0");
        const std::uint64_t chunk = 10000000000000000000ull;   
        std::string out;

        while (!v.is_zero()) {
            std::uint64_t r = v.divmod_small_mutable(chunk);
            if (v.is_zero()) { while (r != 0) { out.push_back(char('0' + r % 10)); r /= 10; } }
            else             { for (int i = 0; i < 19; ++i) { out.push_back(char('0' + r % 10)); r /= 10; } }
        }

        for (std::size_t i = 0, j = out.size(); i < j; ++i, --j) {
            const char c = out[i]; 
            out[i] = out[j - 1]; 
            out[j - 1] = c;
        }

        return out;
    }

    static std::string exponent_string(std::int64_t k) {
        const bool neg = (k < 0);
        std::uint64_t v = neg ? (~static_cast<std::uint64_t>(k) + 1u) : static_cast<std::uint64_t>(k);
        std::string t;
        if (v == 0) t.push_back('0');
        while (v != 0) { t.push_back(char('0' + v % 10)); v /= 10; }
        std::string s(1, neg ? '-' : '+');
        for (std::size_t i = t.size(); i > 0; --i) s.push_back(t[i - 1]);
        return s;
    }

    bool decimal_digits(std::size_t digits, bool truncate, std::string& out, std::int64_t& k, bool& inexact) const {
        k = get_exp_base10();

        for (int attempt = 0; attempt < 4; ++attempt) {
            const long long p = static_cast<long long>(digits) - 1 - static_cast<long long>(k);
            if (p > static_cast<long long>(decimal_exp_cap) || p < -static_cast<long long>(decimal_exp_cap)) return false;
            const std::int64_t s = add_sat(m_exp, static_cast<std::int64_t>(p));
            if (s >  static_cast<std::int64_t>(decimal_exp_cap) * 4 || s < -static_cast<std::int64_t>(decimal_exp_cap) * 4) return false;
            BigUInt num = m_mag;
            BigUInt den = BigUInt::one();

            if (p > 0)      num = num * pow5(static_cast<std::uint64_t>(p));
            else if (p < 0) den = pow5(static_cast<std::uint64_t>(-p));

            if (s > 0)      num.shift_left_mutable(static_cast<std::size_t>(s));
            else if (s < 0) den.shift_left_mutable(static_cast<std::size_t>(-s));

            BigUInt N, rem;
            if (den.is_one()) { N = std::move(num); rem = BigUInt::zero(); }
            else              { N = num / den;      rem = num % den;       }
            if (N.is_undefined()) return false;
            inexact = !rem.is_zero();

            if (!truncate && inexact) {
                BigUInt twice = rem;
                twice.shift_left_mutable(1);
                const int c = twice.compare(den);
                if (c > 0 || (c == 0 && N.is_odd())) N.add_small_mutable(1);
            }

            out = digits_of(N);
            if (out.empty()) return false;
            if (out.size() == digits) return true;
            k += static_cast<std::int64_t>(out.size()) - static_cast<std::int64_t>(digits);
        }

        return false;
    }

    std::string render(const std::string& d, std::int64_t k, bool scientific, bool ellipsis) const {
        std::string s;
        if (m_neg) s.push_back('-');

        if (scientific) {
            s.push_back(d[0]);
            if (d.size() > 1) { s.push_back('.'); s.append(d, 1, std::string::npos); }
            if (ellipsis) s += "...";
            s.push_back('e');
            s += exponent_string(k);
            return s;
        }

        std::string frac;

        if (k >= 0) {
            const std::size_t ip = static_cast<std::size_t>(k) + 1;  
            s.append(d, 0, ip);
            if (d.size() > ip) frac.assign(d, ip, std::string::npos);
        } else {
            s += "0.";
            s.append(static_cast<std::size_t>(-k) - 1, '0');
            frac = d;
        }

        if (!ellipsis) { while (!frac.empty() && frac[frac.size() - 1] == '0') frac.erase(frac.size() - 1); }
        if (!frac.empty()) { if (k >= 0) s.push_back('.'); s += frac; }
        else if (k < 0)    { s.erase(s.size() - 1); }  
        if (ellipsis) s += "...";
        return s;
    }

    std::string approximate_string() const {
        std::string s;
        if (m_neg) s.push_back('-');
        s += "~1e";
        s += exponent_string(approximate_exp_base10());
        return s;
    }

public:
    BigFloat() : m_mag(BigUInt::zero()), m_exp(0), m_neg(false), m_cls(fpclass::finite) {}

    BigFloat(const BigFloat&)            = default;
    BigFloat(BigFloat&&)                 = default;
    BigFloat& operator=(const BigFloat&) = default;
    BigFloat& operator=(BigFloat&&)      = default;

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    BigFloat(T v) : BigFloat() { set_signed(BigInt(v), 0); }

    template <typename T, typename = typename std::enable_if<std::is_floating_point<T>::value>::type, typename = void>
    BigFloat(T v) : BigFloat() { construct_from_long_double(static_cast<long double>(v)); }

    BigFloat(const BigInt& mant, std::int64_t exp = 0) : BigFloat() { set_signed(mant, exp); }

    BigFloat(const BigUInt& mag, bool neg = false, std::int64_t exp = 0) : BigFloat() { set_finite(mag, neg, exp); }
    BigFloat(BigUInt&& mag, bool neg = false, std::int64_t exp = 0)      : BigFloat() { set_finite(std::move(mag), neg, exp); }

    template <std::size_t Bits, sign S>
    BigFloat(const integer<Bits, S>& v, std::int64_t exp = 0) : BigFloat() { assign_integer(v, exp); }

    template <std::size_t TB, std::size_t MB, sign S>
    BigFloat(const floatmp<TB, MB, S>& v) : BigFloat() {
        if (v.is_nan())       { *this = nan();                     return; }
        if (v.is_undefined()) { *this = undefined();               return; }
        if (v.is_infinite())  { *this = infinity(v.is_negative()); return; }
        if (v.is_zero())      { *this = zero(v.is_negative());     return; }
        const auto fx = v.frexp_signed();
        const auto  m = std::get<0>(fx);
        const auto  e = std::get<1>(fx);
        const bool  n = std::get<2>(fx);
        const std::int64_t ev = e.is_negative() ? -static_cast<std::int64_t>(e.get_lowest_bits()) : static_cast<std::int64_t>(e.get_lowest_bits());
        set_signed(BigInt(m, n), ev - static_cast<std::int64_t>(MB));
    }

    explicit BigFloat(const std::string& str) : BigFloat() { if (!parse_string(str.c_str())) *this = undefined(); }
    explicit BigFloat(const char* str)        : BigFloat() { if (!parse_string(str))         *this = undefined(); }

public:
    static BigFloat zero(bool neg = false)     { return BigFloat(raw_tag{}, BigUInt::zero(), neg,   0, fpclass::finite); }
    static BigFloat positive_zero()            { return zero(false); }
    static BigFloat negative_zero()            { return zero(true);  }
    static BigFloat one(bool neg = false)      { return BigFloat(raw_tag{}, BigUInt::one(),  neg,   0, fpclass::finite); }
    static BigFloat infinity(bool neg = false) { return BigFloat(raw_tag{}, BigUInt::zero(), neg,   0, fpclass::infinite); }
    static BigFloat positive_infinity()        { return infinity(false); }
    static BigFloat negative_infinity()        { return infinity(true);  }
    static BigFloat nan()                      { return BigFloat(raw_tag{}, BigUInt::zero(), false, 0, fpclass::nan); }
    static BigFloat undefined()                { return BigFloat(raw_tag{}, BigUInt::zero(), false, 0, fpclass::undefined); }

    static BigFloat ldexp(const BigUInt& mag, bool neg, std::int64_t exp) { return BigFloat(mag, neg, exp); }
    static BigFloat ldexp(BigUInt&& mag, bool neg, std::int64_t exp)      { return BigFloat(std::move(mag), neg, exp); }
    static BigFloat ldexp(const BigInt& mant, std::int64_t exp)           { return BigFloat(mant, exp); }
    static BigFloat ldexp(const BigFloat& x, std::int64_t n)              { return x.scaled2(n); }

    template <typename T, typename = typename std::enable_if<std::is_integral<T>::value>::type>
    static BigFloat ldexp(T v, std::int64_t n) { return BigFloat(v).scaled2(n); }

    static BigFloat from_magnitude(const BigUInt& mag, bool neg = false, std::int64_t exp = 0) { return BigFloat(mag, neg, exp); }
    static BigFloat from_magnitude(BigUInt&& mag, bool neg = false, std::int64_t exp = 0)      { return BigFloat(std::move(mag), neg, exp); }

    static BigFloat from_string(const std::string& s) { return BigFloat(s); }

public:
    const BigUInt& significand()      const noexcept { return m_mag; }
    std::size_t    significand_bits() const noexcept { return m_mag.bit_length(); }
    std::int64_t   exponent()         const noexcept { return m_exp; }
    bool           signbit()          const noexcept { return m_neg; }
    fpclass        classify()         const noexcept { return m_cls; }

    BigInt mantissa() const {
        if (m_cls == fpclass::nan)       return BigInt::nan();
        if (m_cls == fpclass::undefined) return BigInt::undefined();
        return BigInt::from_magnitude(m_mag, m_neg);
    }

    BigInt get_significand_int() const { return mantissa(); }

    bool is_nan()           const noexcept { return m_cls == fpclass::nan; }
    bool is_undefined()     const noexcept { return m_cls == fpclass::undefined; }
    bool is_infinite()      const noexcept { return m_cls == fpclass::infinite; }
    bool is_finite()        const noexcept { return m_cls == fpclass::finite; }
    bool is_special()       const noexcept { return m_cls != fpclass::finite; }
    bool is_zero()          const noexcept { return m_cls == fpclass::finite && m_mag.is_zero(); }
    bool is_negative_zero() const noexcept { return is_zero() &&  m_neg; }
    bool is_positive_zero() const noexcept { return is_zero() && !m_neg; }
    bool is_negative()      const noexcept { return m_neg && (m_cls == fpclass::finite || m_cls == fpclass::infinite); }
    bool is_positive()      const noexcept { return !m_neg && (m_cls == fpclass::finite || m_cls == fpclass::infinite); }
    bool is_integer()       const noexcept { return m_cls == fpclass::finite && (m_mag.is_zero() || m_exp >= 0); }
    bool is_comparable()    const noexcept { return m_cls == fpclass::finite || m_cls == fpclass::infinite; }

    int sign() const noexcept {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return 0;
        if (is_zero()) return 0;
        return m_neg ? -1 : 1;
    }

public:
    template <typename T, typename = typename std::enable_if<std::is_arithmetic<T>::value>::type>
    BigFloat& operator=(T v) { return *this = BigFloat(v); }

    BigFloat& operator=(const BigInt& v)  { return set_signed(v, 0); }
    BigFloat& operator=(const BigUInt& v) { return set_finite(v, false, 0); }
    BigFloat& operator=(BigUInt&& v)      { return set_finite(std::move(v), false, 0); }

    template <std::size_t Bits, multiprecision::sign S>
    BigFloat& operator=(const integer<Bits, S>& v) { return assign_integer(v, 0); }

    template <std::size_t TB, std::size_t MB, multiprecision::sign S>
    BigFloat& operator=(const floatmp<TB, MB, S>& v) { return *this = BigFloat(v); }

    BigFloat& operator=(const std::string& s) { if (!parse_string(s.c_str())) *this = undefined(); return *this; }
    BigFloat& operator=(const char* s)        { if (!parse_string(s))         *this = undefined(); return *this; }

public:
    BigFloat abs() const {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return *this;
        return BigFloat(raw_tag{}, m_mag, false, m_exp, m_cls);
    }

    BigFloat with_sign(bool neg) const {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return *this;
        return BigFloat(raw_tag{}, m_mag, neg, m_exp, m_cls);
    }

    BigFloat& negate_mutable()          noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = !m_neg; return *this; }
    BigFloat& abs_mutable()             noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = false;  return *this; }
    BigFloat& set_sign_mutable(bool n)  noexcept { if (m_cls != fpclass::nan && m_cls != fpclass::undefined) m_neg = n;      return *this; }

    static BigFloat copysign(const BigFloat& x, const BigFloat& y) { return x.with_sign(y.signbit()); }
    
public:
    static ordering compare(const BigFloat& a, const BigFloat& b) noexcept {
        if (!a.is_comparable() || !b.is_comparable()) return ordering::unordered;
        const bool az = a.is_zero();
        const bool bz = b.is_zero();
        if (az && bz) return ordering::equal;              

        if (a.m_neg != b.m_neg) {
            if (az) return b.m_neg ? ordering::greater : ordering::less;
            if (bz) return a.m_neg ? ordering::less    : ordering::greater;
            return a.m_neg ? ordering::less : ordering::greater;
        }

        if (az) return a.m_neg ? ordering::greater : ordering::less;
        if (bz) return b.m_neg ? ordering::less    : ordering::greater;

        if (a.is_infinite() || b.is_infinite()) {
            if (a.is_infinite() && b.is_infinite()) return ordering::equal;
            if (a.is_infinite()) return a.m_neg ? ordering::less    : ordering::greater;
            return                      b.m_neg ? ordering::greater : ordering::less;
        }

        const std::int64_t ea = a.get_exp_base2();
        const std::int64_t eb = b.get_exp_base2();
        int c;

        if (ea != eb) c = (ea < eb) ? -1 : 1;
        else          c = compare_aligned(a.m_mag, a.m_mag.bit_length(), b.m_mag, b.m_mag.bit_length());

        if (a.m_neg) c = -c;
        return (c < 0) ? ordering::less : ((c > 0) ? ordering::greater : ordering::equal);
    }

    ordering compare(const BigFloat& o) const noexcept { return compare(*this, o); }

    static bool unordered(const BigFloat& a, const BigFloat& b) noexcept {
        return compare(a, b) == ordering::unordered;
    }

public:
    static int total_order(const BigFloat& a, const BigFloat& b) noexcept {
        const int ra = a.is_undefined() ? 0 : (a.is_nan() ? 1 : 2);
        const int rb = b.is_undefined() ? 0 : (b.is_nan() ? 1 : 2);
        if (ra != rb) return (ra < rb) ? -1 : 1;
        if (ra != 2)  return 0;
        const ordering o = compare(a, b);
        if (o == ordering::less)    return -1;
        if (o == ordering::greater) return 1;
        const ordering so = signbit_order(a, b);
        return (so == ordering::less) ? -1 : ((so == ordering::greater) ? 1 : 0);
    }

    struct total_less {
        bool operator()(const BigFloat& a, const BigFloat& b) const noexcept {
            return total_order(a, b) < 0;
        }
    };

public:
    static ordering signbit_order(const BigFloat& a, const BigFloat& b) noexcept {
        if (!(a.is_comparable() && b.is_comparable())) return ordering::unordered;
        if (a.m_neg == b.m_neg) return ordering::equal;
        return a.m_neg ? ordering::less : ordering::greater;
    }

    ordering signbit_order(const BigFloat& o) const noexcept { return signbit_order(*this, o); }

    bool same_signbit(const BigFloat& o) const noexcept {
        return signbit_order(*this, o) == ordering::equal;
    }

    int signbit_compare(const BigFloat& other) const noexcept {
        const ordering o = signbit_order(other);
        return (o == ordering::less) ? -1 : ((o == ordering::greater) ? 1 : 0);
    }

public:
    std::pair<BigInt, std::int64_t> frexp() const {
        return std::pair<BigInt, std::int64_t>(mantissa(), m_exp);
    }

    std::tuple<BigUInt, std::int64_t, bool> frexp_signed() const {
        return std::tuple<BigUInt, std::int64_t, bool>(m_mag, m_exp, m_neg);
    }

    std::pair<BigFloat, std::int64_t> frexp_normalized() const {
        if (is_special() || is_zero()) return std::pair<BigFloat, std::int64_t>(*this, std::int64_t(0));
        const std::int64_t len = static_cast<std::int64_t>(m_mag.bit_length());
        return std::pair<BigFloat, std::int64_t>(BigFloat(raw_tag{}, m_mag, m_neg, -len, fpclass::finite), add_sat(m_exp, len));
    }

    std::tuple<BigFloat, std::int64_t, bool> frexp_signed_normalized() const {
        if (is_special() || is_zero()) return std::tuple<BigFloat, std::int64_t, bool>(*this, std::int64_t(0), m_neg);
        const std::int64_t len = static_cast<std::int64_t>(m_mag.bit_length());
        return std::tuple<BigFloat, std::int64_t, bool>(BigFloat(raw_tag{}, m_mag, m_neg, -len, fpclass::finite), add_sat(m_exp, len), m_neg);
    }

    BigFloat get_significand() const {
        if (is_special() || is_zero()) return *this;
        return BigFloat(raw_tag{}, m_mag, m_neg, -(static_cast<std::int64_t>(m_mag.bit_length()) - 1), fpclass::finite);
    }

    std::int64_t get_exp_base2() const noexcept {
        if (is_infinite()) return exp_inf;
        if (is_special() || is_zero()) return exp_none;
        return add_sat(m_exp, static_cast<std::int64_t>(m_mag.bit_length()) - 1);
    }

    std::int64_t get_exp_low_bit() const noexcept { return (is_finite() && !is_zero()) ? m_exp : exp_none; }

    std::int64_t approximate_exp_base10() const noexcept {
        const std::int64_t e2 = get_exp_base2();
        if (e2 == exp_none || e2 == exp_inf) return e2;
        return static_cast<std::int64_t>(std::floor(static_cast<long double>(e2) * BigFloatContext::log10_2));
    }

    std::int64_t get_exp_base10() const {
        std::int64_t k = approximate_exp_base10();
        if (k == exp_none || k == exp_inf) return k;
        bool ok = false;
        const int c = compare_abs_pow10(k, ok);
        if (!ok) return k;

        if (c < 0) {
            --k;
            const int c2 = compare_abs_pow10(k, ok);
            if (ok && c2 < 0) --k;
            return k;
        }

        const int c2 = compare_abs_pow10(k + 1, ok);
        if (ok && c2 >= 0) ++k;
        return k;
    }

    // value * 2^n
    BigFloat scaled2(std::int64_t n) const {
        if (is_special() || is_zero()) return *this;
        BigFloat r;
        r.set_finite(m_mag, m_neg, add_sat(m_exp, n));
        return r;
    }

    BigFloat& scale2_mutable(std::int64_t n) {
        if (is_finite() && !is_zero()) set_finite(std::move(m_mag), m_neg, add_sat(m_exp, n));
        return *this;
    }

    BigInt get_integer_part() const {
        if (m_cls == fpclass::nan)       return BigInt::nan();
        if (m_cls == fpclass::undefined) return BigInt::undefined();
        if (m_cls == fpclass::infinite)  return BigInt::undefined();
        if (m_mag.is_zero() || m_exp == 0) return BigInt::from_magnitude(m_mag, m_neg);

        if (m_exp > 0) {
            if (m_exp > static_cast<std::int64_t>(BigUInt::max_bits)) return BigInt::undefined();
            BigUInt r(m_mag);
            r.shift_left_mutable(static_cast<std::size_t>(m_exp));
            return BigInt::from_magnitude(std::move(r), m_neg);
        }

        const std::int64_t d = -m_exp;
        if (d >= static_cast<std::int64_t>(m_mag.bit_length())) return BigInt::zero();
        BigUInt r(m_mag);
        r.shift_right_mutable(static_cast<std::size_t>(d));  
        return BigInt::from_magnitude(std::move(r), m_neg);
    }

    BigFloat get_fractional_part() const {
        if (is_special())            return is_infinite() ? zero(m_neg) : *this;
        if (is_zero() || m_exp >= 0) return zero(m_neg);
        const std::int64_t d   = -m_exp;
        const std::size_t  len = m_mag.bit_length();
        if (static_cast<std::int64_t>(len) <= d) return *this;
        BigUInt mask = BigUInt::one();
        mask.shift_left_mutable(static_cast<std::size_t>(d));
        mask.sub_small_mutable(1);
        BigUInt mag = m_mag;
        mag.and_mutable(mask);
        if (mag.is_zero()) return zero(m_neg);
        BigFloat r;
        r.set_finite(std::move(mag), m_neg, m_exp);
        return r;
    }

    BigFloat rounded(const BigFloatContext& ctx) const {
        if (m_cls != fpclass::finite || m_mag.is_zero()) return *this;
        if (m_mag.bit_length() <= ctx.precision)         return *this;
        BigUInt      m = m_mag;
        std::int64_t e = m_exp;
        round_significand(m, e, m_neg, ctx.precision, false, ctx.rounding_mode);
        BigFloat r;
        r.set_finite(std::move(m), m_neg, e);
        return r;
    }

    BigFloat rounded() const { return rounded(BigFloatContext::current()); }

    BigFloat scaled_pow2(std::int64_t k) const {          
        if (m_cls != fpclass::finite || m_mag.is_zero()) return *this;
        BigFloat r;
        r.set_finite(m_mag, m_neg, add_sat(m_exp, k));
        return r;
    }

    BigFloat& scale_pow2_mutable(std::int64_t k) {
        if (m_cls == fpclass::finite && !m_mag.is_zero()) m_exp = add_sat(m_exp, k);
        return *this;
    }

public:
    BigFloat reciprocal(const BigFloatContext& ctx) const {
        if (m_cls == fpclass::nan)       return nan();
        if (m_cls == fpclass::undefined) return undefined();
        if (m_cls == fpclass::infinite)  return zero(m_neg);     
        if (m_mag.is_undefined())        return undefined();
        if (m_mag.is_zero())             return infinity(m_neg); 
        return reciprocal_finite(m_mag, m_neg, m_exp, ctx);
    }

    BigFloat reciprocal() const { return reciprocal(BigFloatContext::current()); }

    BigFloat& reciprocal_inplace(const BigFloatContext& ctx) { return *this = reciprocal(ctx); }
    BigFloat& reciprocal_inplace() { return reciprocal_inplace(BigFloatContext::current()); }

public:
    static BigFloat add(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        return add_signed_impl(a, b, false, ctx);
    }

    static BigFloat sub(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        return add_signed_impl(a, b, true, ctx);
    }

    static BigFloat mul(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        if (a.is_nan()       || b.is_nan())       return nan();
        if (a.is_undefined() || b.is_undefined()) return undefined();
        const bool neg = (a.m_neg != b.m_neg);

        if (a.is_infinite() || b.is_infinite()) {
            if (a.is_zero() || b.is_zero()) return undefined();   
            return infinity(neg);
        }

        if (a.is_zero() || b.is_zero()) return zero(neg);
        return mul_finite(a.m_mag, a.m_neg, a.m_exp, b.m_mag, b.m_neg, b.m_exp, ctx);
    }

    static BigFloat div(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) {
        if (a.is_nan()       || b.is_nan())       return nan();
        if (a.is_undefined() || b.is_undefined()) return undefined();
        const bool neg = (a.m_neg != b.m_neg);
        if (a.is_infinite()) return b.is_infinite() ? undefined() : infinity(neg);
        if (b.is_infinite()) return zero(neg);
        if (b.is_zero())     return a.is_zero() ? undefined() : infinity(neg);
        if (a.is_zero())     return zero(neg);
        return divide_finite(a.m_mag, a.m_neg, a.m_exp, b.m_mag, b.m_neg, b.m_exp, ctx);
    }

    static BigFloat add(const BigFloat& a, const BigFloat& b) { return add(a, b, BigFloatContext::current()); }
    static BigFloat sub(const BigFloat& a, const BigFloat& b) { return sub(a, b, BigFloatContext::current()); }
    static BigFloat mul(const BigFloat& a, const BigFloat& b) { return mul(a, b, BigFloatContext::current()); }
    static BigFloat div(const BigFloat& a, const BigFloat& b) { return div(a, b, BigFloatContext::current()); }

    BigFloat& add_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = add(*this, o, c); }
    BigFloat& sub_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = sub(*this, o, c); }
    BigFloat& mul_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = mul(*this, o, c); }
    BigFloat& div_mutable(const BigFloat& o, const BigFloatContext& c) { return *this = div(*this, o, c); }

    BigFloat& add_mutable(const BigFloat& o) { return add_mutable(o, BigFloatContext::current()); }
    BigFloat& sub_mutable(const BigFloat& o) { return sub_mutable(o, BigFloatContext::current()); }
    BigFloat& mul_mutable(const BigFloat& o) { return mul_mutable(o, BigFloatContext::current()); }
    BigFloat& div_mutable(const BigFloat& o) { return div_mutable(o, BigFloatContext::current()); }

public:
    std::string to_string_scientific(std::size_t max_digits = no_digit_limit, std::size_t digits = 0) const {
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite())  return m_neg ? "-\u221e" : "\u221e";
        if (digits == 0) digits = BigFloatContext::current().dps();
        if (digits == 0) digits = 1;
        bool elide = false;
        if (max_digits < digits) { digits = (max_digits == 0) ? 1 : max_digits; elide = true; }

        if (is_zero()) {
            std::string s = m_neg ? "-0" : "0";
            if (digits > 1) { s.push_back('.'); s.append(digits - 1, '0'); }
            return s + "e+0";
        }

        std::string d;
        std::int64_t k = 0;
        bool inexact = false;
        if (!decimal_digits(digits, elide, d, k, inexact)) return approximate_string();
        return render(d, k, true, elide && inexact);
    }

    std::string to_string(std::size_t max_digits = no_digit_limit, std::int64_t scientific_notation_exp = -5, std::size_t digits = 0) const {
        if (is_nan())       return "nan";
        if (is_undefined()) return "undefined";
        if (is_infinite())  return m_neg ? "-\u221e" : "\u221e";
        if (is_zero())      return m_neg ? "-0" : "0";
        if (digits == 0) digits = BigFloatContext::current().dps();
        if (digits == 0) digits = 1;
        bool elide = false;
        if (max_digits < digits) { digits = (max_digits == 0) ? 1 : max_digits; elide = true; }
        if (scientific_notation_exp >= 0) scientific_notation_exp = -5;
        std::string d;
        std::int64_t k = 0;
        bool inexact = false;
        if (!decimal_digits(digits, elide, d, k, inexact)) return approximate_string();
        const bool sci = (k < scientific_notation_exp) || (k >= static_cast<std::int64_t>(digits));
        return render(d, k, sci, elide && inexact);
    }

    inline friend std::ostream& operator<<(std::ostream& os, const BigFloat& x) { return os << x.to_string(); }
};

inline bool operator==(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::equal;
}

inline bool operator!=(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) != BigFloat::ordering::equal;   // unordered => true
}

inline bool operator<(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::less;
}

inline bool operator>(const BigFloat& a, const BigFloat& b) noexcept {
    return BigFloat::compare(a, b) == BigFloat::ordering::greater;
}

inline bool operator<=(const BigFloat& a, const BigFloat& b) noexcept {
    const BigFloat::ordering o = BigFloat::compare(a, b);
    return o == BigFloat::ordering::less || o == BigFloat::ordering::equal;
}

inline bool operator>=(const BigFloat& a, const BigFloat& b) noexcept {
    const BigFloat::ordering o = BigFloat::compare(a, b);
    return o == BigFloat::ordering::greater || o == BigFloat::ordering::equal;
}

inline BigFloat operator+(const BigFloat& a) { return a; }
inline BigFloat operator+(BigFloat&& a)      { return std::move(a); }

inline BigFloat operator-(const BigFloat& a) { BigFloat r(a); r.negate_mutable(); return r; }
inline BigFloat operator-(BigFloat&& a)      { a.negate_mutable(); return std::move(a); }

inline BigFloat operator+(const BigFloat& a, const BigFloat& b) { return BigFloat::add(a, b); }
inline BigFloat operator+(BigFloat&& a,      const BigFloat& b) { a.add_mutable(b); return std::move(a); }
inline BigFloat operator+(const BigFloat& a, BigFloat&& b)      { b.add_mutable(a); return std::move(b); }
inline BigFloat operator+(BigFloat&& a,      BigFloat&& b)      { a.add_mutable(b); return std::move(a); }

inline BigFloat operator-(const BigFloat& a, const BigFloat& b) { return BigFloat::sub(a, b); }
inline BigFloat operator-(BigFloat&& a,      const BigFloat& b) { a.sub_mutable(b); return std::move(a); }
inline BigFloat operator-(BigFloat&& a,      BigFloat&& b)      { a.sub_mutable(b); return std::move(a); }

inline BigFloat operator-(const BigFloat& a, BigFloat&& b) {
    b = BigFloat::sub(a, b);   
    return std::move(b);
}

inline BigFloat operator*(const BigFloat& a, const BigFloat& b) { return BigFloat::mul(a, b); }
inline BigFloat operator*(BigFloat&& a,      const BigFloat& b) { a.mul_mutable(b); return std::move(a); }
inline BigFloat operator*(const BigFloat& a, BigFloat&& b)      { b.mul_mutable(a); return std::move(b); }
inline BigFloat operator*(BigFloat&& a,      BigFloat&& b)      { a.mul_mutable(b); return std::move(a); }

inline BigFloat operator/(const BigFloat& a, const BigFloat& b) { return BigFloat::div(a, b); }
inline BigFloat operator/(BigFloat&& a,      const BigFloat& b) { a.div_mutable(b); return std::move(a); }
inline BigFloat operator/(BigFloat&& a,      BigFloat&& b)      { a.div_mutable(b); return std::move(a); }

inline BigFloat operator/(const BigFloat& a, BigFloat&& b) {
    b = BigFloat::div(a, b);
    return std::move(b);
}

inline BigFloat& operator+=(BigFloat& a, const BigFloat& b) { return a.add_mutable(b); }
inline BigFloat& operator-=(BigFloat& a, const BigFloat& b) { return a.sub_mutable(b); }
inline BigFloat& operator*=(BigFloat& a, const BigFloat& b) { return a.mul_mutable(b); }
inline BigFloat& operator/=(BigFloat& a, const BigFloat& b) { return a.div_mutable(b); }

inline BigFloat& operator++(BigFloat& a) { return a.add_mutable(BigFloat::one()); }
inline BigFloat& operator--(BigFloat& a) { return a.sub_mutable(BigFloat::one()); }
inline BigFloat  operator++(BigFloat& a, int) { BigFloat t(a); ++a; return t; }
inline BigFloat  operator--(BigFloat& a, int) { BigFloat t(a); --a; return t; }

} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_FLOAT_CLASS_HPP