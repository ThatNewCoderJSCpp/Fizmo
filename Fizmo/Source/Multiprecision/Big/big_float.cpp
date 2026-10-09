#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {

ScopedContext::ScopedContext(std::size_t prec) noexcept : m_saved(BigFloatContext::current()) {
        BigFloatContext::current() = BigFloatContext(prec, m_saved.rounding_mode);
    }

ScopedContext::ScopedContext(std::size_t prec, RoundingMode rnd) noexcept : m_saved(BigFloatContext::current()) {
        BigFloatContext::current() = BigFloatContext(prec, rnd);
    }

auto BigFloat::add_sat(std::int64_t e, std::int64_t d) noexcept -> std::int64_t {
        if (d > 0 && e > std::numeric_limits<std::int64_t>::max() - d) return std::numeric_limits<std::int64_t>::max();
        if (d < 0 && e < std::numeric_limits<std::int64_t>::min() - d) return std::numeric_limits<std::int64_t>::min();
        return e + d;
    }

auto BigFloat::set_finite(BigUInt mag, bool neg, std::int64_t exp) -> BigFloat& {
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

auto BigFloat::set_signed(const BigInt& v, std::int64_t exp) -> BigFloat& {
        if (v.is_nan())       return *this = nan();
        if (v.is_undefined()) return *this = undefined();
        return set_finite(v.magnitude(), v.is_negative(), exp);
    }

auto BigFloat::round_significand(BigUInt& mag, std::int64_t& exp, bool neg, std::size_t prec, bool sticky, RoundingMode mode) -> void {
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

auto BigFloat::pow5(std::uint64_t n) -> BigUInt {
        BigUInt r = BigUInt::one(), b(std::uint64_t(5));
        while (n != 0) { if (n & 1u) r = r * b; n >>= 1; if (n != 0) b = b * b; }
        return r;
    }

auto BigFloat::assign_decimal(BigUInt digits, bool neg, long long e10) -> void {
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

auto BigFloat::construct_from_long_double(long double value) -> void {
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

auto BigFloat::word_is(const char* s, const char* w) noexcept -> bool {
        std::size_t i = 0;

        for (; w[i] != '\0'; ++i) {
            const char c = s[i];
            const char l = (c >= 'A' && c <= 'Z') ? char(c - 'A' + 'a') : c;
            if (l != w[i]) return false;
        }

        while (s[i] == ' ' || s[i] == '\t') ++i;
        return s[i] == '\0';
    }

auto BigFloat::parse_string(const char* s) -> bool {
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

auto BigFloat::compare_abs_pow10(std::int64_t k, bool& ok) const -> int {
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

auto BigFloat::reciprocal_finite(const BigUInt& mag, bool neg, std::int64_t exp, const BigFloatContext& ctx) -> BigFloat {
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

auto BigFloat::align_to(const BigUInt& m, std::int64_t e, std::int64_t E, bool& sticky) -> BigUInt {
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

auto BigFloat::add_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
) -> BigFloat {
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

auto BigFloat::mul_finite(
        const BigUInt& am, bool aneg, std::int64_t ae,
        const BigUInt& bm, bool bneg, std::int64_t be,
        const BigFloatContext& ctx
) -> BigFloat {
        const bool   neg = (aneg != bneg);
        BigUInt      p   = am * bm;          
        std::int64_t e   = add_sat(ae, be);
        round_significand(p, e, neg, ctx.precision, false, ctx.rounding_mode);
        BigFloat r;
        r.set_finite(std::move(p), neg, e);
        return r;
    }

auto BigFloat::add_signed_impl(const BigFloat& a, const BigFloat& b, bool flip_b, const BigFloatContext& ctx) -> BigFloat {
        if (a.is_nan()       || b.is_nan())       return nan();
        if (a.is_undefined() || b.is_undefined()) return undefined();
        const bool bneg = flip_b ? !b.m_neg : b.m_neg;

        if (a.is_infinite() || b.is_infinite()) {
            if (a.is_infinite() && b.is_infinite()) return (a.m_neg == bneg) ? infinity(a.m_neg) : undefined();
            return a.is_infinite() ? infinity(a.m_neg) : infinity(bneg);
        }

        return add_finite(a.m_mag, a.m_neg, a.m_exp, b.m_mag, bneg, b.m_exp, ctx);
    }

auto BigFloat::divide_finite(
        const BigUInt& xm, bool xneg, std::int64_t xe,
        const BigUInt& ym, bool yneg, std::int64_t ye,
        const BigFloatContext& ctx
) -> BigFloat {
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

auto BigFloat::compare_aligned(const BigUInt& a, std::size_t La, const BigUInt& b, std::size_t Lb) noexcept -> int {
        if (La == Lb) return a.compare(b);
        const std::size_t n = (La < Lb) ? La : Lb;

        for (std::size_t i = 1; i <= n; ++i) {
            const bool x = a.get_bit(La - i);
            const bool y = b.get_bit(Lb - i);
            if (x != y) return x ? 1 : -1;
        }

        return (La > Lb) ? 1 : -1;
    }

auto BigFloat::digits_of(BigUInt v) -> std::string {
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

auto BigFloat::exponent_string(std::int64_t k) -> std::string {
        const bool neg = (k < 0);
        std::uint64_t v = neg ? (~static_cast<std::uint64_t>(k) + 1u) : static_cast<std::uint64_t>(k);
        std::string t;
        if (v == 0) t.push_back('0');
        while (v != 0) { t.push_back(char('0' + v % 10)); v /= 10; }
        std::string s(1, neg ? '-' : '+');
        for (std::size_t i = t.size(); i > 0; --i) s.push_back(t[i - 1]);
        return s;
    }

auto BigFloat::decimal_digits(std::size_t digits, bool truncate, std::string& out, std::int64_t& k, bool& inexact) const -> bool {
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

auto BigFloat::render(const std::string& d, std::int64_t k, bool scientific, bool ellipsis) const -> std::string {
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

auto BigFloat::approximate_string() const -> std::string {
        std::string s;
        if (m_neg) s.push_back('-');
        s += "~1e";
        s += exponent_string(approximate_exp_base10());
        return s;
    }

auto BigFloat::mantissa() const -> BigInt {
        if (m_cls == fpclass::nan)       return BigInt::nan();
        if (m_cls == fpclass::undefined) return BigInt::undefined();
        return BigInt::from_magnitude(m_mag, m_neg);
    }

auto BigFloat::sign() const noexcept -> int {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return 0;
        if (is_zero()) return 0;
        return m_neg ? -1 : 1;
    }

auto BigFloat::abs() const -> BigFloat {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return *this;
        return BigFloat(raw_tag{}, m_mag, false, m_exp, m_cls);
    }

auto BigFloat::with_sign(bool neg) const -> BigFloat {
        if (m_cls == fpclass::nan || m_cls == fpclass::undefined) return *this;
        return BigFloat(raw_tag{}, m_mag, neg, m_exp, m_cls);
    }

auto BigFloat::compare(const BigFloat& a, const BigFloat& b) noexcept -> ordering {
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

auto BigFloat::total_order(const BigFloat& a, const BigFloat& b) noexcept -> int {
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

auto BigFloat::signbit_order(const BigFloat& a, const BigFloat& b) noexcept -> ordering {
        if (!(a.is_comparable() && b.is_comparable())) return ordering::unordered;
        if (a.m_neg == b.m_neg) return ordering::equal;
        return a.m_neg ? ordering::less : ordering::greater;
    }

auto BigFloat::signbit_compare(const BigFloat& other) const noexcept -> int {
        const ordering o = signbit_order(other);
        return (o == ordering::less) ? -1 : ((o == ordering::greater) ? 1 : 0);
    }

auto BigFloat::frexp_normalized() const -> std::pair<BigFloat, std::int64_t> {
        if (is_special() || is_zero()) return std::pair<BigFloat, std::int64_t>(*this, std::int64_t(0));
        const std::int64_t len = static_cast<std::int64_t>(m_mag.bit_length());
        return std::pair<BigFloat, std::int64_t>(BigFloat(raw_tag{}, m_mag, m_neg, -len, fpclass::finite), add_sat(m_exp, len));
    }

auto BigFloat::frexp_signed_normalized() const -> std::tuple<BigFloat, std::int64_t, bool> {
        if (is_special() || is_zero()) return std::tuple<BigFloat, std::int64_t, bool>(*this, std::int64_t(0), m_neg);
        const std::int64_t len = static_cast<std::int64_t>(m_mag.bit_length());
        return std::tuple<BigFloat, std::int64_t, bool>(BigFloat(raw_tag{}, m_mag, m_neg, -len, fpclass::finite), add_sat(m_exp, len), m_neg);
    }

auto BigFloat::get_significand() const -> BigFloat {
        if (is_special() || is_zero()) return *this;
        return BigFloat(raw_tag{}, m_mag, m_neg, -(static_cast<std::int64_t>(m_mag.bit_length()) - 1), fpclass::finite);
    }

auto BigFloat::get_exp_base2() const noexcept -> std::int64_t {
        if (is_infinite()) return exp_inf;
        if (is_special() || is_zero()) return exp_none;
        return add_sat(m_exp, static_cast<std::int64_t>(m_mag.bit_length()) - 1);
    }

auto BigFloat::approximate_exp_base10() const noexcept -> std::int64_t {
        const std::int64_t e2 = get_exp_base2();
        if (e2 == exp_none || e2 == exp_inf) return e2;
        return static_cast<std::int64_t>(std::floor(static_cast<long double>(e2) * BigFloatContext::log10_2));
    }

auto BigFloat::get_exp_base10() const -> std::int64_t {
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

auto BigFloat::scaled2(std::int64_t n) const -> BigFloat {
        if (is_special() || is_zero()) return *this;
        BigFloat r;
        r.set_finite(m_mag, m_neg, add_sat(m_exp, n));
        return r;
    }

auto BigFloat::scale2_mutable(std::int64_t n) -> BigFloat& {
        if (is_finite() && !is_zero()) set_finite(std::move(m_mag), m_neg, add_sat(m_exp, n));
        return *this;
    }

auto BigFloat::get_integer_part() const -> BigInt {
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

auto BigFloat::get_fractional_part() const -> BigFloat {
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

auto BigFloat::rounded(const BigFloatContext& ctx) const -> BigFloat {
        if (m_cls != fpclass::finite || m_mag.is_zero()) return *this;
        if (m_mag.bit_length() <= ctx.precision)         return *this;
        BigUInt      m = m_mag;
        std::int64_t e = m_exp;
        round_significand(m, e, m_neg, ctx.precision, false, ctx.rounding_mode);
        BigFloat r;
        r.set_finite(std::move(m), m_neg, e);
        return r;
    }

auto BigFloat::scaled_pow2(std::int64_t k) const -> BigFloat {          
        if (m_cls != fpclass::finite || m_mag.is_zero()) return *this;
        BigFloat r;
        r.set_finite(m_mag, m_neg, add_sat(m_exp, k));
        return r;
    }

auto BigFloat::reciprocal(const BigFloatContext& ctx) const -> BigFloat {
        if (m_cls == fpclass::nan)       return nan();
        if (m_cls == fpclass::undefined) return undefined();
        if (m_cls == fpclass::infinite)  return zero(m_neg);     
        if (m_mag.is_undefined())        return undefined();
        if (m_mag.is_zero())             return infinity(m_neg); 
        return reciprocal_finite(m_mag, m_neg, m_exp, ctx);
    }

auto BigFloat::mul(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) -> BigFloat {
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

auto BigFloat::div(const BigFloat& a, const BigFloat& b, const BigFloatContext& ctx) -> BigFloat {
        if (a.is_nan()       || b.is_nan())       return nan();
        if (a.is_undefined() || b.is_undefined()) return undefined();
        const bool neg = (a.m_neg != b.m_neg);
        if (a.is_infinite()) return b.is_infinite() ? undefined() : infinity(neg);
        if (b.is_infinite()) return zero(neg);
        if (b.is_zero())     return a.is_zero() ? undefined() : infinity(neg);
        if (a.is_zero())     return zero(neg);
        return divide_finite(a.m_mag, a.m_neg, a.m_exp, b.m_mag, b.m_neg, b.m_exp, ctx);
    }

auto BigFloat::to_string_scientific(std::size_t max_digits, std::size_t digits) const -> std::string {
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

auto BigFloat::to_string(std::size_t max_digits, std::int64_t scientific_notation_exp, std::size_t digits) const -> std::string {
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

bool operator<=(const BigFloat& a, const BigFloat& b) noexcept {
    const BigFloat::ordering o = BigFloat::compare(a, b);
    return o == BigFloat::ordering::less || o == BigFloat::ordering::equal;
}

bool operator>=(const BigFloat& a, const BigFloat& b) noexcept {
    const BigFloat::ordering o = BigFloat::compare(a, b);
    return o == BigFloat::ordering::greater || o == BigFloat::ordering::equal;
}

} // namespace multiprecision
} // namespace fizmo
