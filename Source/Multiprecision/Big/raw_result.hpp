#ifndef FIZMO_MULTIPRECISION_BIG_RAW_RESULT_MATH_HPP
#define FIZMO_MULTIPRECISION_BIG_RAW_RESULT_MATH_HPP

#include "BigFloat Math/big_float_consts.hpp"
#include "BigFloat Math/queries.hpp"
#include "BigFloat Math/exp.hpp"
#include "BigFloat Math/num_theory.hpp"
#include "BigFloat Math/sqrt_cbrt.hpp"
#include "BigFloat Math/inv_htrig.hpp"
#include "BigFloat Math/pow_nth_root.hpp"
#include "BigFloat Math/erf.hpp"
#include "BigFloat Math/stieltjes.hpp"
#include "BigFloat Math/algebraic_consts.hpp"
#include "BigFloat Math/big_int_sequences.hpp"
#include "BigFloat Math/gamma.hpp"
#include "BigFloat Math/polygamma.hpp"
#include "BigFloat Math/polynomials.hpp"
#include "BigFloat Math/harmonic.hpp"
#include "BigFloat Math/gudermannian.hpp"
#include "BigFloat Math/riemann_zeta.hpp"
#include "BigFloat Math/generalized_gaussian.hpp"
#include "BigFloat Math/polylog.hpp"
#include "BigFloat Math/legendre.hpp"
#include "BigFloat Math/dirichlet.hpp"
#include "BigFloat Math/hurwitz_lerch.hpp"
#include "BigFloat Math/airy.hpp"
#include "BigFloat Math/dilog_family.hpp"
#include "BigFloat Math/elliptic.hpp"
#include "BigFloat Math/jacobi_elliptic.hpp"
#include "BigFloat Math/lambert_w.hpp"
#include "BigFloat Math/special_integrals.hpp"
#include "BigFloat Math/combinatorics.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace raw {

enum class RawStatus : std::uint8_t {
    exact = 0,
    approximate,
    special,
    guard_exhausted,
    not_converged
};
 
class RawResult {
public:
    static constexpr double no_error = -std::numeric_limits<double>::infinity();
    static constexpr double no_bound =  std::numeric_limits<double>::infinity();
 
private:
    BigFloat    m_value;
    std::size_t m_precision         = 0;
    std::size_t m_working_precision = 0;
    double      m_log2_error        = no_error;
    RawStatus   m_status            = RawStatus::exact;
    std::size_t m_guard_bits        = 0;
    std::size_t m_iterations        = 0;
 
    RawResult(const BigFloat& v, std::size_t prec, std::size_t wprec, double log2_error, RawStatus st)
        : m_value(v), m_precision(prec), m_working_precision(wprec), m_log2_error(log2_error), m_status(st) {}
 
private:
    static double lsum(double a, double b) noexcept {
        if (a == no_error) return b;
        if (b == no_error) return a;
        if (a == no_bound || b == no_bound) return no_bound;
        if (a < b) std::swap(a, b);
        return a + std::log2(1.0 + std::exp2(b - a));
    }
 
    static double log2_floor(const BigFloat& v) noexcept {
        const std::int64_t e = v.get_exp_base2();
        return static_cast<double>(e);
    }
 
    static BigFloatContext exact_ctx(std::size_t p) noexcept {
        return BigFloatContext(BigFloatContext::clamp_precision(p), RoundingMode::nearest_even);
    }
 
    static bool same_bits(const BigFloat& a, const BigFloat& b) noexcept {
        if (a.is_nan() || b.is_nan())             return a.is_nan() && b.is_nan();
        if (a.is_undefined() || b.is_undefined()) return a.is_undefined() && b.is_undefined();
        return a.signbit() == b.signbit() && BigFloat::compare(a, b) == BigFloat::ordering::equal;
    }
 
    BigFloat endpoint(bool up) const {
        if (m_status == RawStatus::exact || m_status == RawStatus::special) return m_value;
        if (!has_bound() || m_log2_error > static_cast<double>(BigFloat::exp_max)) return BigFloat::infinity(!up);
        if (m_log2_error < static_cast<double>(BigFloat::exp_min)) return m_value;
        const std::int64_t r   = static_cast<std::int64_t>(std::ceil(m_log2_error));
        const BigFloat     rad = BigFloat::one().scaled_pow2(r);
        if (m_value.is_zero()) return up ? rad : -rad;
        const std::int64_t top = (std::max)(m_value.get_exp_base2(), r) + 2;
        const std::int64_t low = (std::min)(m_value.exponent(), r);
        const std::int64_t span = top - low + 1;
        if (span <= 0 || static_cast<std::uint64_t>(span) > BigFloatContext::max_prec) return BigFloat::infinity(!up);
        const BigFloatContext c = exact_ctx(static_cast<std::size_t>(span));
        return up ? BigFloat::add(m_value, rad, c) : BigFloat::sub(m_value, rad, c);
    }
 
public:
    RawResult() : m_value(BigFloat::undefined()), m_status(RawStatus::not_converged) {}
 
    static RawResult exact(const BigFloat& v, std::size_t precision) {
        if (!v.is_finite()) return special(v, precision);
        const std::size_t wp = (std::max)(precision, v.significand_bits());
        return RawResult(v, precision, wp, no_error, RawStatus::exact);
    }
 
    static RawResult special(const BigFloat& v, std::size_t precision) {
        return RawResult(v, precision, precision, no_error, RawStatus::special);
    }
 
    static RawResult failed(std::size_t precision, std::size_t working_precision) {
        return RawResult(BigFloat::undefined(), precision, working_precision, no_bound, RawStatus::not_converged);
    }
 
    static RawResult from_lost_bits(const BigFloat& v, std::size_t precision, std::size_t working_precision, double lost_bits) {
        if (!v.is_finite()) return special(v, precision);
        if (v.is_zero())    return RawResult(v, precision, working_precision, no_bound, RawStatus::approximate);
        const double ulp = log2_floor(v) - static_cast<double>(working_precision) + 1.0;
        return RawResult(v, precision, working_precision, ulp + lost_bits, RawStatus::approximate);
    }
 
    static RawResult from_abs_error(const BigFloat& v, std::size_t precision, std::size_t working_precision, double log2_abs_error) {
        if (!v.is_finite()) return special(v, precision);
        if (log2_abs_error == no_error) return RawResult(v, precision, working_precision, no_error, RawStatus::exact);
        return RawResult(v, precision, working_precision, log2_abs_error, RawStatus::approximate);
    }
 
    const BigFloat& value()             const noexcept { return m_value; }
    std::size_t     precision()         const noexcept { return m_precision; }
    std::size_t     working_precision() const noexcept { return m_working_precision; }
    RawStatus       status()            const noexcept { return m_status; }
    std::size_t     guard_bits()        const noexcept { return m_guard_bits; }
    std::size_t     iterations()        const noexcept { return m_iterations; }
 
    bool is_exact()     const noexcept { return m_status == RawStatus::exact; }
    bool is_special()   const noexcept { return m_status == RawStatus::special; }
    bool is_converged() const noexcept { return m_status != RawStatus::not_converged; }
    bool has_bound()    const noexcept { return is_converged() && m_log2_error != no_bound; }
 
    double log2_abs_error() const noexcept { return m_log2_error; }
 
    double log2_rel_error() const noexcept {
        if (m_log2_error == no_error) return no_error;
        if (!has_bound() || m_value.is_zero() || !m_value.is_finite()) return no_bound;
        return m_log2_error - log2_floor(m_value);
    }
 
    double lost_bits() const noexcept {
        if (m_log2_error == no_error) return 0.0;
        if (!has_bound() || m_value.is_zero() || !m_value.is_finite()) return no_bound;
        return m_log2_error - (log2_floor(m_value) - static_cast<double>(m_working_precision) + 1.0);
    }
 
    double correct_bits() const noexcept {
        if (m_log2_error == no_error) return no_bound;
        const double r = log2_rel_error();
        if (r == no_bound || r >= 0.0) return 0.0;
        return -r;
    }
 
    BigFloat error_bound() const {
        if (m_log2_error == no_error) return BigFloat::zero();
        if (!has_bound() || m_log2_error > static_cast<double>(BigFloat::exp_max)) return BigFloat::infinity();
        if (m_log2_error < static_cast<double>(BigFloat::exp_min)) return BigFloat::zero();
        return BigFloat::one().scaled_pow2(static_cast<std::int64_t>(std::ceil(m_log2_error)));
    }
 
    BigFloat lower() const { return endpoint(false); }
    BigFloat upper() const { return endpoint(true); }
 
    bool contains(const BigFloat& x) const {
        if (!is_converged() || !x.is_comparable()) return false;
        if (m_status == RawStatus::special) return same_bits(m_value, x);
        const BigFloat::ordering a = BigFloat::compare(lower(), x);
        const BigFloat::ordering b = BigFloat::compare(x, upper());
        return a != BigFloat::ordering::greater && a != BigFloat::ordering::unordered && b != BigFloat::ordering::greater && b != BigFloat::ordering::unordered;
    }
 
    bool can_round(const BigFloatContext& ctx) const {
        if (m_status == RawStatus::exact || m_status == RawStatus::special) return true;
        if (!has_bound()) return false;
        const BigFloat lo = lower();
        const BigFloat hi = upper();
        if (!lo.is_finite() || !hi.is_finite()) return false;
        return same_bits(lo.rounded(ctx), hi.rounded(ctx));
    }
 
    bool try_round(const BigFloatContext& ctx, BigFloat& out) const {
        if (!can_round(ctx)) return false;
        out = m_value.rounded(ctx);
        return true;
    }
 
    BigFloat rounded(const BigFloatContext& ctx) const { return m_value.rounded(ctx); }
 
    RawResult& add_abs_error(double log2_abs_error) noexcept {
        if (log2_abs_error == no_error || m_status == RawStatus::special) return *this;
        m_log2_error = lsum(m_log2_error, log2_abs_error);
        if (m_status == RawStatus::exact) m_status = RawStatus::approximate;
        return *this;
    }
 
    RawResult& add_lost_bits(double lost) noexcept {
        if (!m_value.is_finite() || m_value.is_zero()) return add_abs_error(no_bound);
        return add_abs_error(log2_floor(m_value) - static_cast<double>(m_working_precision) + 1.0 + lost);
    }
 
    RawResult& set_precision(std::size_t p)       noexcept { m_precision = p;  return *this; }
    RawResult& set_guard_bits(std::size_t g)      noexcept { m_guard_bits = g; return *this; }
    RawResult& set_iterations(std::size_t n)      noexcept { m_iterations = n; return *this; }
    RawResult& count_iteration()                  noexcept { ++m_iterations;   return *this; }
 
    RawResult& mark_guard_exhausted() noexcept {
        if (m_status == RawStatus::approximate) m_status = RawStatus::guard_exhausted;
        return *this;
    }
 
    std::string to_string(std::size_t digits = 0) const {
        if (m_status == RawStatus::not_converged) return "not_converged";
        std::string s = m_value.to_string(BigFloat::no_digit_limit, -5, digits);
        if (m_log2_error == no_error) return s + " (exact)";
        if (!has_bound()) return s + " +/- unbounded";
        const double e = std::ceil(m_log2_error);
        s += " +/- 2^";
        s += std::to_string(static_cast<long long>(e));
        return s;
    }
 
    friend std::ostream& operator<<(std::ostream& os, const RawResult& r) { return os << r.to_string(); }
};
 
inline const char* to_string(RawStatus s) noexcept {
    switch (s) {
        case RawStatus::exact:           return "exact";
        case RawStatus::approximate:     return "approximate";
        case RawStatus::special:         return "special";
        case RawStatus::guard_exhausted: return "guard_exhausted";
        case RawStatus::not_converged:   return "not_converged";
    }
 
    return "unknown";
}

} // namespace raw
} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_BIG_RAW_RESULT_MATH_HPP