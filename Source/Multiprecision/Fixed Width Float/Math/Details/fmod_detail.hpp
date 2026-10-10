#ifndef FIZMO_MULTIPRECISION_FMOD_DETAIL_HPP
#define FIZMO_MULTIPRECISION_FMOD_DETAIL_HPP

#include "log_pow_roots_detail.hpp"

namespace fizmo {
namespace multiprecision {
namespace math {
namespace edetail {

// b == 2^eb exactly: the remainder is just the scaled fractional part. O(1).
template <class G>
G fmod_pow2(const G& a, const typename G::sstore_t& eb, bool& q_odd) noexcept {
    using S = typename G::sstore_t;
    const G t = fmath::scalb(a, S(0) - eb);            // a / 2^eb
    q_odd = odd_integer(t);                            // false when ulp(t) > 1, which is correct
    const G f = t.get_fractional_part();
    return f.is_zero() ? G::zero() : fmath::scalb(f, eb);
}

// a, b finite and strictly positive. Returns a mod b in [0, b), exactly.
// q_odd receives the parity of trunc(a / b), needed for round-to-even in remainder().
template <class G>
G fmod_core(const G& a, const G& b, bool& q_odd) noexcept {
    using S = typename G::sstore_t;
    q_odd = false;
    if (a < b) return a;
    S ea, eb; G fa, fb;
    fmath::split_pow2(a, fa, ea);                      // fa, fb in [1, 2)
    fmath::split_pow2(b, fb, eb);
    if (fb == G(1)) return fmod_pow2(a, eb, q_odd);
    const long long d = root_exp_ll<G>(ea) - root_exp_ll<G>(eb);
    G r = fa;

    for (long long i = d; i >= 0; --i) {
        if (!(r < fb)) {                               // both in [1, 2): subtraction is exact
            r = r - fb;
            if (i == 0) q_odd = true;
        }
        if (r.is_zero()) break;                        // remaining quotient bits are all zero
        if (i > 0) r = fmath::scalb_i(r, 1);
    }

    return fmath::scalb(r, eb);
}

// ---- shared special-case handling ---------------------------------------

template <class F>
struct rem_parts {
    typename F::guard_t mag;                           // |x| mod |y|, in [0, |y|)
    bool xneg, yneg, q_odd, done;
    F    early;
};

template <class F>
rem_parts<F> rem_split(const F& x, const F& y) noexcept {
    using G = typename F::guard_t;
    rem_parts<F> p;
    p.mag = G::zero(); p.xneg = x.is_negative(); p.yneg = y.is_negative();
    p.q_odd = false; p.done = true; p.early = F::nan();
    if (x.is_undefined() || y.is_undefined()) { p.early = F::undefined(); return p; }
    if (x.is_nan() || y.is_nan())             { return p; }
    if (y.is_zero() || x.is_infinite())       { return p; }
    if (x.is_zero() || y.is_infinite())       { p.early = x; return p; }
    p.done = false;
    p.mag  = fmod_core(fmath::float_cast<G>(math::abs(x)),
                       fmath::float_cast<G>(math::abs(y)), p.q_odd);
    return p;
}

// ---- the four variants ---------------------------------------------------

template <class F>
F fmod_run(const F& x, const F& y) noexcept {
    const rem_parts<F> p = rem_split(x, y);
    if (p.done) return p.early;
    return signed_root(fmath::float_cast<F>(p.mag), p.xneg);
}

template <class F>
F remainder_run(const F& x, const F& y) noexcept {
    using G = typename F::guard_t;
    const rem_parts<F> p = rem_split(x, y);
    if (p.done) return p.early;
    const G gy    = fmath::float_cast<G>(math::abs(y));
    const G twice = fmath::scalb_i(p.mag, 1);
    const bool up = (twice > gy) || (twice == gy && p.q_odd);      // ties to even
    const G r     = up ? (gy - p.mag) : p.mag;
    return signed_root(fmath::float_cast<F>(r), p.xneg != up);
}

// Result carries the sign of y and lies in [0, |y|).
template <class F>
F mod_floor_run(const F& x, const F& y) noexcept {
    using G = typename F::guard_t;
    const rem_parts<F> p = rem_split(x, y);
    if (p.done) return p.early;
    if (p.mag.is_zero() || p.xneg == p.yneg)
        return signed_root(fmath::float_cast<F>(p.mag), p.yneg && !p.mag.is_zero());
    const G gy = fmath::float_cast<G>(math::abs(y));
    const F r  = fmath::float_cast<F>(gy - p.mag);                 // complement, at guard precision
    if (!(r < math::abs(y))) return F::zero(p.yneg);               // rounded up to |y|: clamp
    return signed_root(r, p.yneg);
}

// Always non-negative. Differs from mod_floor only when y < 0.
template <class F>
F mod_euclid_run(const F& x, const F& y) noexcept {
    using G = typename F::guard_t;
    const rem_parts<F> p = rem_split(x, y);
    if (p.done) return p.done && p.early.is_negative() ? math::abs(p.early) : p.early;
    if (p.mag.is_zero() || !p.xneg) return fmath::float_cast<F>(p.mag);
    const G gy = fmath::float_cast<G>(math::abs(y));
    const F r  = fmath::float_cast<F>(gy - p.mag);
    return (!(r < math::abs(y))) ? F::zero() : r;
}

} // namespace edetail

namespace fdetail {

using ldetail::is_operand;
using ldetail::common_result;
using ldetail::widen;
using ldetail::is_mp_float;

template <class F, bool Mp = is_mp_float<F>::value>
struct apply {
    template <class A, class B> static F fmod_   (const A& a, const B& b) noexcept { return edetail::fmod_run      (widen<F>(a), widen<F>(b)); }
    template <class A, class B> static F rem_    (const A& a, const B& b) noexcept { return edetail::remainder_run (widen<F>(a), widen<F>(b)); }
    template <class A, class B> static F floor_  (const A& a, const B& b) noexcept { return edetail::mod_floor_run (widen<F>(a), widen<F>(b)); }
    template <class A, class B> static F euclid_ (const A& a, const B& b) noexcept { return edetail::mod_euclid_run(widen<F>(a), widen<F>(b)); }
};

template <class F>
struct apply<F, false> {
    template <class A, class B> static F fmod_(const A& a, const B& b) noexcept {
        return std::fmod(static_cast<F>(a), static_cast<F>(b));
    }

    template <class A, class B> static F rem_(const A& a, const B& b) noexcept {
        return std::remainder(static_cast<F>(a), static_cast<F>(b));
    }

    template <class A, class B> static F floor_(const A& a, const B& b) noexcept {
        const F x = static_cast<F>(a), y = static_cast<F>(b);
        const F r = std::fmod(x, y);
        if (r == F(0) || (r < F(0)) == (y < F(0))) return r;
        const F s = r + y;
        return (std::fabs(s) < std::fabs(y)) ? s : ((y < F(0)) ? -F(0) : F(0));
    }

    template <class A, class B> static F euclid_(const A& a, const B& b) noexcept {
        const F x = static_cast<F>(a), y = static_cast<F>(b);
        const F r = std::fmod(x, y);
        if (r >= F(0)) return r;
        const F s = r + std::fabs(y);
        return (s < std::fabs(y)) ? s : F(0);
    }
};

} // namespace fdetail
} // namespace math
} // namespace multiprecision
} // namespace fizmo

#endif // FIZMO_MULTIPRECISION_FMOD_DETAIL_HPP