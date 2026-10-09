#define ALL_FIZMO
#include <fizmo/includes.hpp>

namespace fizmo {
namespace multiprecision {

auto BigComplexContext::extended(std::size_t guard_bits) const noexcept -> BigComplexContext {
        return BigComplexContext(real_precision + guard_bits, imaginary_precision + guard_bits, real_rounding, imaginary_rounding);
    }

BigComplexContext BigComplexContext::current() noexcept {
    const bcdetail::context_state& s = bcdetail::context_tls();
    return s.overridden ? s.ctx : BigComplexContext(BigFloatContext::current());
}

ScopedComplexContext::ScopedComplexContext(std::size_t prec) noexcept : ScopedComplexContext(BigComplexContext(prec, BigComplexContext::current().real_rounding)) {}

ScopedComplexContext::ScopedComplexContext(std::size_t p_re, std::size_t p_im) noexcept : ScopedComplexContext(BigComplexContext(p_re, p_im, BigComplexContext::current().real_rounding, BigComplexContext::current().imaginary_rounding)) {}

} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {
namespace bcdetail {

dyadic dy_of(const BF& x) {
    if (x.is_zero()) return dyadic(BigUInt::zero(), 0, x.signbit());
    return dyadic(x.significand(), x.exponent(), x.signbit());
}

dyadic dy_mul(const BF& x, const BF& y) {
    const bool neg = (x.signbit() != y.signbit());
    if (x.is_zero() || y.is_zero()) return dyadic(BigUInt::zero(), 0, neg);
    return dyadic(x.significand() * y.significand(), x.exponent() + y.exponent(), neg);
}

dyadic dy_square(const BF& x) {
    if (x.is_zero()) return dyadic();
    return dyadic(x.significand() * x.significand(), 2 * x.exponent(), false);
}

BF dy_round(const dyadic& d, const BFC& ctx) {
    if (dy_bad(d))  return BF::undefined();
    if (dy_zero(d)) return BF::zero(d.neg);
    return dy_unit(d).rounded(ctx).scaled2(dy_top(d));
}

BF dy_exact(const dyadic& d) {
    if (dy_bad(d))  return BF::undefined();
    if (dy_zero(d)) return BF::zero(d.neg);
    return BF(d.mag, d.neg, d.exp);
}

dyadic dy_add_exact(const dyadic& a, const dyadic& b) {
    const std::int64_t lo = (a.exp < b.exp) ? a.exp : b.exp;
    BigUInt A = a.mag;
    BigUInt B = b.mag;
    A.shift_left_mutable(static_cast<std::size_t>(a.exp - lo));
    B.shift_left_mutable(static_cast<std::size_t>(b.exp - lo));
    if (A.is_undefined() || B.is_undefined()) return dy_bad_value();
    if (a.neg == b.neg) { A.add_mutable(B); return dyadic(std::move(A), lo, a.neg); }
    const int c = A.compare(B);
    if (c == 0) return dyadic(BigUInt::zero(), 0, false);
    if (c > 0) { A.sub_mutable(B); return dyadic(std::move(A), lo, a.neg); }
    B.sub_mutable(A);
    return dyadic(std::move(B), lo, b.neg);
}

dyadic dy_sum(const dyadic& a, const dyadic& b, std::size_t K, RoundingMode mode, bool& exact) {
    exact = true;
    if (dy_bad(a) || dy_bad(b)) return dy_bad_value();
    const bool az = dy_zero(a), bz = dy_zero(b);
    const bool zero_neg_on_cancel = (mode == RoundingMode::toward_neg_inf);
    if (az && bz) return dyadic(BigUInt::zero(), 0, (a.neg == b.neg) ? a.neg : zero_neg_on_cancel);
    if (az) return b;
    if (bz) return a;
    const std::int64_t ta = dy_top(a), tb = dy_top(b);
    const bool          a_big = (ta >= tb);
    const dyadic&       L     = a_big ? a : b;
    const dyadic&       S     = a_big ? b : a;
    const std::int64_t  tL    = a_big ? ta : tb;
    const std::int64_t  tS    = a_big ? tb : ta;
    const bool          opp   = (a.neg != b.neg);
    const std::int64_t  lo    = (L.exp < S.exp) ? L.exp : S.exp;
    const std::uint64_t width  = static_cast<std::uint64_t>(tL - lo);
    const std::uint64_t budget = static_cast<std::uint64_t>(K) + L.mag.bit_length() + S.mag.bit_length() + 64;

    if ((opp && tL - tS < 2) || width <= budget) {
        dyadic r = dy_add_exact(a, b);
        if (dy_zero(r)) r.neg = zero_neg_on_cancel;
        return r;
    }

    exact = false;
    const std::int64_t tK = tL - static_cast<std::int64_t>(K);
    const std::int64_t F  = (tK < L.exp) ? tK : L.exp;
    BigUInt m = L.mag;
    m.shift_left_mutable(static_cast<std::size_t>(L.exp - F) + 1);
    if (opp) m.sub_small_mutable(1); else m.add_small_mutable(1);
    return dyadic(std::move(m), F - 1, L.neg);
}

dyadic dy_truncate(const dyadic& d, std::size_t K, bool& exact) {
    if (dy_bad(d) || dy_zero(d)) return d;
    const std::size_t len = d.mag.bit_length();
    if (len <= K + 1) return d;
    const std::size_t drop = len - K;
    const long long   ctz  = d.mag.count_trailing_zeros();
    BigUInt m = d.mag;
    m.shift_right_mutable(drop);
    if (ctz >= 0 && static_cast<std::size_t>(ctz) >= drop) return dyadic(std::move(m), d.exp + static_cast<std::int64_t>(drop), d.neg);
    exact = false;
    m.shift_left_mutable(1);
    m.add_small_mutable(1);
    return dyadic(std::move(m), d.exp + static_cast<std::int64_t>(drop) - 1, d.neg);
}

BF dy_divide(const dyadic& n, const dyadic& d, const BFC& ctx) {
    if (dy_bad(n) || dy_bad(d) || dy_zero(d)) return BF::undefined();
    if (dy_zero(n)) return BF::zero(n.neg != d.neg);
    return BF::div(dy_unit(n), dy_unit(d), ctx).scaled2(dy_top(n) - dy_top(d));
}

int dy_cmp_abs(const dyadic& x, const dyadic& y) {
    const bool xz = dy_zero(x), yz = dy_zero(y);
    if (xz || yz) return xz ? (yz ? 0 : -1) : 1;
    const std::int64_t tx = dy_top(x), ty = dy_top(y);
    if (tx != ty) return (tx < ty) ? -1 : 1;
    if (x.exp == y.exp) return x.mag.compare(y.mag);

    if (x.exp > y.exp) {
        BigUInt t = x.mag;
        t.shift_left_mutable(static_cast<std::size_t>(x.exp - y.exp));
        return t.compare(y.mag);
    }

    BigUInt t = y.mag;
    t.shift_left_mutable(static_cast<std::size_t>(y.exp - x.exp));
    return x.mag.compare(t);
}

bool dy_order(const dyadic& a, bool a_exact, const dyadic& b, bool b_exact, int& out) {
    if (a_exact && b_exact) { out = dy_cmp_abs(a, b); return true; }
    dyadic a_lo = a, a_hi = a, b_lo = b, b_hi = b;
    if (!a_exact) { a_lo.mag.sub_small_mutable(1); a_hi.mag.add_small_mutable(1); }
    if (!b_exact) { b_lo.mag.sub_small_mutable(1); b_hi.mag.add_small_mutable(1); }
    if (dy_cmp_abs(a_hi, b_lo) <= 0) { out = -1; return true; }
    if (dy_cmp_abs(a_lo, b_hi) >= 0) { out =  1; return true; }
    return false;
}

int dy_sign_of_difference(const dyadic& p, const dyadic& q) {
    const int sp = dy_sign(p), sq = dy_sign(q);
    if (sp != sq) return (sp > sq) ? 1 : -1;
    if (sp == 0) return 0;
    const int c = dy_cmp_abs(p, q);
    return (sp > 0) ? c : -c;
}

bool symmetric(RoundingMode r) noexcept {
    return r == RoundingMode::nearest_even || r == RoundingMode::nearest_away || r == RoundingMode::toward_zero;
}

BigFloat::ordering to_ordering(int c) noexcept {
    return (c < 0) ? BigFloat::ordering::less : ((c > 0) ? BigFloat::ordering::greater : BigFloat::ordering::equal);
}

} // namespace bcdetail
} // namespace multiprecision
} // namespace fizmo

namespace fizmo {
namespace multiprecision {

BigComplex::cache_t::cache_t() : real_sq(), imaginary_sq(), norm(), norm_k(0), have_sq(false), have_norm(false), norm_exact(false), branch_k(0) {}

auto BigComplex::normalize() -> void {
        const bool bad = m_real.is_nan() || m_real.is_undefined() || m_imaginary.is_nan() || m_imaginary.is_undefined();
        if (!bad) return;
        if (!m_real.is_undefined())      m_real      = BigFloat::undefined();
        if (!m_imaginary.is_undefined()) m_imaginary = BigFloat::undefined();
    }

auto BigComplex::ensure_squares() const -> void {
        cache_t& c = cache();
        if (c.have_sq) return;
        c.real_sq   = bcdetail::dy_square(m_real);
        c.imaginary_sq   = bcdetail::dy_square(m_imaginary);
        c.have_sq = true;
    }

auto BigComplex::norm_cell(std::size_t K, bool& exact) const -> const bcdetail::dyadic& {
        cache_t& c = cache();

        if (!c.have_norm || (!c.norm_exact && c.norm_k < K)) {
            ensure_squares();
            bool ex = true;
            c.norm       = bcdetail::dy_sum(c.real_sq, c.imaginary_sq, K, RoundingMode::nearest_even, ex);
            c.norm_exact = ex;
            c.norm_k     = K;
            c.have_norm  = true;
        }

        exact = c.norm_exact;
        return c.norm;
    }

auto BigComplex::exact_norm() const -> const bcdetail::dyadic* {
        bool ex = true;
        const bcdetail::dyadic& N = norm_cell(64, ex);
        if (ex) return bcdetail::dy_bad(N) ? nullptr : &N;
        cache_t& c = cache();
        bcdetail::dyadic X = bcdetail::dy_sum(c.real_sq, c.imaginary_sq, bcdetail::k_exact, RoundingMode::nearest_even, ex);
        if (bcdetail::dy_bad(X) || !ex) return nullptr;
        c.norm       = std::move(X);
        c.norm_exact = true;
        c.norm_k     = bcdetail::k_exact;
        return &c.norm;
    }

auto BigComplex::boxed() const -> BigComplex {
        if (!is_infinite()) return BigComplex(m_real, m_imaginary);
        const BigFloat r = m_real.is_infinite() ? BigFloat::one(m_real.signbit()) : BigFloat::zero(m_real.signbit());
        const BigFloat i = m_imaginary.is_infinite() ? BigFloat::one(m_imaginary.signbit()) : BigFloat::zero(m_imaginary.signbit());
        return BigComplex(r, i);
    }

auto BigComplex::principal_at(const BigFloatContext& wc) const -> const BigFloat& {
        cache_t& c = cache();
        if (c.arg_w.hit(wc)) return c.arg_w.value;
        return c.arg_w.put(math::atan2(m_imaginary, m_real, wc), wc);
    }

auto BigComplex::mul_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        const bcdetail::dyadic ac = bcdetail::dy_mul(z.m_real, w.m_real);
        const bcdetail::dyadic bd = bcdetail::dy_neg(bcdetail::dy_mul(z.m_imaginary, w.m_imaginary));
        const bcdetail::dyadic ad = bcdetail::dy_mul(z.m_real, w.m_imaginary);
        const bcdetail::dyadic bc = bcdetail::dy_mul(z.m_imaginary, w.m_real);
        bool ex = true;
        BigFloat re = bcdetail::dy_round(bcdetail::dy_sum(ac, bd, cr.precision + 8, cr.rounding_mode, ex), cr);
        BigFloat im = bcdetail::dy_round(bcdetail::dy_sum(ad, bc, ci.precision + 8, ci.rounding_mode, ex), ci);
        return BigComplex(std::move(re), std::move(im));
    }

auto BigComplex::div_finite(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        if (w.m_imaginary.is_zero()) return BigComplex(BigFloat::div(z.m_real, w.m_real, cr), BigFloat::div(z.m_imaginary, w.m_real, ci));
        if (w.m_real.is_zero()) return BigComplex(BigFloat::div(z.m_imaginary, w.m_imaginary, cr), BigFloat::div(z.m_real, -w.m_imaginary, ci));
        const bcdetail::dyadic ac = bcdetail::dy_mul(z.m_real, w.m_real);
        const bcdetail::dyadic bd = bcdetail::dy_mul(z.m_imaginary, w.m_imaginary);
        const bcdetail::dyadic bc = bcdetail::dy_mul(z.m_imaginary, w.m_real);
        const bcdetail::dyadic ad = bcdetail::dy_neg(bcdetail::dy_mul(z.m_real, w.m_imaginary));
        const RoundingMode rr = cr.rounding_mode, ri = ci.rounding_mode;
        auto den = [&w](std::size_t K, bool& ex) -> bcdetail::dyadic { return w.norm_cell(K, ex); };
        auto nre = [&](std::size_t K, bool& ex) { return bcdetail::dy_sum(ac, bd, K, rr, ex); };
        auto nim = [&](std::size_t K, bool& ex) { return bcdetail::dy_sum(bc, ad, K, ri, ex); };
        BigFloat re = bcdetail::dy_quotient(nre, den, cr);
        BigFloat im = bcdetail::dy_quotient(nim, den, ci);
        return BigComplex(std::move(re), std::move(im));
    }

auto BigComplex::reciprocal_finite(const BigComplexContext& ctx) const -> BigComplex {
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        if (m_imaginary.is_zero()) return BigComplex(m_real.reciprocal(cr), BigFloat::zero(!m_imaginary.signbit()));
        if (m_real.is_zero()) return BigComplex(BigFloat::zero(m_real.signbit()), (-m_imaginary).reciprocal(ci));
        const bcdetail::dyadic c = bcdetail::dy_of(m_real);
        const bcdetail::dyadic d = bcdetail::dy_neg(bcdetail::dy_of(m_imaginary));
        auto den = [this](std::size_t K, bool& ex) -> bcdetail::dyadic { return norm_cell(K, ex); };
        auto nre = [&c](std::size_t, bool& ex) { ex = true; return c; };
        auto nim = [&d](std::size_t, bool& ex) { ex = true; return d; };
        BigFloat re = bcdetail::dy_quotient(nre, den, cr);
        BigFloat im = bcdetail::dy_quotient(nim, den, ci);
        return BigComplex(std::move(re), std::move(im));
    }

auto BigComplex::scale_slot(slot& s, std::int64_t n) -> void {
        if (!s.valid) return;
        if (!s.value.is_finite() || s.value.is_zero()) { s.valid = false; return; }
        s.value.scale2_mutable(n);
    }

BigComplex::BigComplex(const BigComplex& o) : m_real(o.m_real), m_imaginary(o.m_imaginary), m_cache(o.m_cache ? new cache_t(*o.m_cache) : nullptr) {}

auto BigComplex::swap(BigComplex& o) noexcept -> void {
        bcdetail::swap_by_move(m_real, o.m_real);
        bcdetail::swap_by_move(m_imaginary, o.m_imaginary);
        m_cache.swap(o.m_cache);
    }

auto BigComplex::classify() const noexcept -> fpclass {
        if (m_real.is_nan() || m_real.is_undefined() || m_imaginary.is_nan() || m_imaginary.is_undefined()) return fpclass::undefined;
        if (m_real.is_infinite()  || m_imaginary.is_infinite()) return fpclass::infinite;
        return fpclass::finite;
    }

auto BigComplex::magnitude_squared(const BigFloatContext& ctx) const -> BigFloat {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        if (is_zero()) return BigFloat::zero();
        cache_t& c = cache();
        if (c.norm_r.hit(ctx)) return c.norm_r.value;
        bool ex = true;
        const bcdetail::dyadic& N = norm_cell(ctx.precision + 8, ex);
        return c.norm_r.put(bcdetail::dy_round(N, ctx), ctx);
    }

auto BigComplex::magnitude_squared_exact() const -> BigFloat {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        const bcdetail::dyadic* N = exact_norm();
        return N ? bcdetail::dy_exact(*N) : BigFloat::undefined();
    }

auto BigComplex::magnitude(const BigFloatContext& ctx) const -> BigFloat {
        switch (classify()) {
            case fpclass::undefined: return BigFloat::undefined();
            case fpclass::infinite:  return BigFloat::infinity();
            default: break;
        }

        if (m_imaginary.is_zero()) return m_real.abs().rounded(ctx);
        if (m_real.is_zero()) return m_imaginary.abs().rounded(ctx);
        cache_t& c = cache();
        if (c.abs_r.hit(ctx)) return c.abs_r.value;
        const std::size_t K  = 2 * ctx.precision + 16;
        bool              ex = true;
        const bcdetail::dyadic& N = norm_cell(K, ex);
        const bcdetail::dyadic  t = bcdetail::dy_truncate(N, K, ex);
        const std::int64_t top = bcdetail::dy_top(t);
        const std::int64_t q   = top + (top & 1);
        const BigFloat r = math::sqrt(BigFloat(t.mag, false, t.exp - q), ctx).scaled2(q / 2);
        return c.abs_r.put(r, ctx);
    }

auto BigComplex::argument(const BigFloatContext& ctx) const -> BigFloat {
        if (is_undefined()) return BigFloat::undefined();
        cache_t& c = cache();
        if (c.arg_r.hit(ctx)) return c.arg_r.value;
        return c.arg_r.put(math::atan2(m_imaginary, m_real, ctx), ctx);
    }

auto BigComplex::argument(std::int64_t branch, const BigFloatContext& ctx) const -> BigFloat {
        if (branch == 0) return argument(ctx);
        if (!argument_defined()) return BigFloat::undefined();
        cache_t& c = cache();
        if (c.branch_r.hit(ctx) && c.branch_k == branch) return c.branch_r.value;
        const std::int64_t    kb    = static_cast<std::int64_t>(bcdetail::bits_u64(bcdetail::abs_u64(branch)));
        const BigFloat        two_k = BigFloat(branch).scaled2(1);
        const BigFloatContext xc(BigFloatContext::max_prec, ctx.rounding_mode);
        std::size_t guard = 32;

        for (;;) {
            const BigFloatContext wc(BigFloatContext::clamp_precision(ctx.precision + guard), RoundingMode::nearest_even);
            const std::int64_t    w  = static_cast<std::int64_t>(wc.precision);
            const BigFloat        A  = principal_at(wc);
            if (!A.is_finite()) return BigFloat::undefined();
            const BigFloat     S     = BigFloat::add(A, BigFloat::mul(two_k, constants::pi(wc), xc), wc);
            const std::int64_t e_in  = kb + 3 - w;
            const std::int64_t e_out = S.get_exp_base2() - w;
            const BigFloat     delta(BigUInt::one(), false, (e_in > e_out ? e_in : e_out) + 1);
            const BigFloat     lo = BigFloat::sub(S, delta, xc).rounded(ctx);
            const BigFloat     hi = BigFloat::add(S, delta, xc).rounded(ctx);

            if (BigFloat::compare(lo, hi) == ordering::equal) {
                c.branch_k = branch;
                return c.branch_r.put(lo, ctx);
            }

            if (guard >= (std::size_t(1) << 16) || ctx.precision + guard >= BigFloatContext::max_prec / 4) return S.rounded(ctx);
            guard *= 2;
        }
    }

auto BigComplex::compare_argument(const BigComplex& a, const BigComplex& b) -> ordering {
        if (!a.argument_defined() || !b.argument_defined()) return ordering::unordered;
        if (&a == &b) return ordering::equal;
        const BigComplex  ba = a.is_infinite() ? a.boxed() : BigComplex();
        const BigComplex  bb = b.is_infinite() ? b.boxed() : BigComplex();
        const BigComplex& da = a.is_infinite() ? ba : a;
        const BigComplex& db = b.is_infinite() ? bb : b;
        const int ca = bcdetail::argument_class(da.m_real, da.m_imaginary);
        const int cb = bcdetail::argument_class(db.m_real, db.m_imaginary);
        if (ca != cb) return (ca < cb) ? ordering::less : ordering::greater;
        if (ca != 1 && ca != 3) return ordering::equal;
        const int s = bcdetail::dy_sign_of_difference(bcdetail::dy_mul(da.m_real, db.m_imaginary), bcdetail::dy_mul(da.m_imaginary, db.m_real));
        return (s > 0) ? ordering::less : ((s < 0) ? ordering::greater : ordering::equal);
    }

auto BigComplex::compare_argument(const BigComplex& a, const BigFloat& theta) -> ordering {
        if (!a.argument_defined() || theta.is_nan() || theta.is_undefined()) return ordering::unordered;
        if (theta.is_infinite()) return theta.signbit() ? ordering::greater : ordering::less;
        const BigComplex  ba = a.is_infinite() ? a.boxed() : BigComplex();
        const BigComplex& da = a.is_infinite() ? ba : a;
        if (bcdetail::argument_class(da.m_real, da.m_imaginary) == 2) return BigFloat::compare(BigFloat::zero(), theta);
        const BigFloat four(4);
        if (theta > four)  return ordering::less;
        if (theta < -four) return ordering::greater;
        const BigFloatContext xc(BigFloatContext::max_prec);
        std::size_t w = 64;

        for (;;) {
            const BigFloatContext wc(BigFloatContext::clamp_precision(w), RoundingMode::nearest_even);
            const BigFloat A = a.principal_at(wc);
            if (!A.is_finite() || A.is_zero()) return ordering::unordered;
            const BigFloat err(BigUInt::one(), false, A.get_exp_base2() + 1 - static_cast<std::int64_t>(wc.precision));
            if (theta < BigFloat::sub(A, err, xc)) return ordering::greater;
            if (theta > BigFloat::add(A, err, xc)) return ordering::less;
            if (w >= (std::size_t(1) << 20) || w >= BigFloatContext::max_prec / 4) return ordering::unordered;
            w *= 2;
        }
    }

auto BigComplex::compare_magnitude(const BigComplex& a, const BigComplex& b) -> ordering {
        if (a.is_undefined() || b.is_undefined()) return ordering::unordered;
        if (&a == &b) return ordering::equal;
        const bool ai = a.is_infinite(), bi = b.is_infinite();
        if (ai || bi) return (ai && bi) ? ordering::equal : (ai ? ordering::greater : ordering::less);
        bool ea = true, eb = true;
        int  c  = 0;
        const bcdetail::dyadic& na = a.norm_cell(64, ea);
        const bcdetail::dyadic& nb = b.norm_cell(64, eb);
        if (bcdetail::dy_order(na, ea, nb, eb, c)) return bcdetail::to_ordering(c);
        const bcdetail::dyadic* xa = a.exact_norm();
        const bcdetail::dyadic* xb = b.exact_norm();
        if (!xa || !xb) return ordering::unordered;
        return bcdetail::to_ordering(bcdetail::dy_cmp_abs(*xa, *xb));
    }

auto BigComplex::compare_magnitude(const BigComplex& a, const BigFloat& r) -> ordering {
        if (a.is_undefined() || r.is_nan() || r.is_undefined()) return ordering::unordered;
        if (r.is_negative() && !r.is_zero()) return ordering::greater;
        if (r.is_infinite()) return a.is_infinite() ? ordering::equal : ordering::less;
        if (a.is_infinite()) return ordering::greater;
        const bcdetail::dyadic rr = bcdetail::dy_square(r);
        bool ea = true;
        int  c  = 0;
        const bcdetail::dyadic& na = a.norm_cell(64, ea);
        if (bcdetail::dy_order(na, ea, rr, true, c)) return bcdetail::to_ordering(c);
        const bcdetail::dyadic* xa = a.exact_norm();
        if (!xa) return ordering::unordered;
        return bcdetail::to_ordering(bcdetail::dy_cmp_abs(*xa, rr));
    }

auto BigComplex::conjugate_mutable() -> BigComplex& {
        m_imaginary.negate_mutable();

        if (m_cache) {
            cache_t& c = *m_cache;
            if (c.arg_r.valid) { if (bcdetail::symmetric(c.arg_r.mode)) c.arg_r.value.negate_mutable(); else c.arg_r.valid = false; }
            if (c.arg_w.valid) c.arg_w.value.negate_mutable();
            c.branch_r.valid = false;
            if (c.recip.valid) { if (bcdetail::symmetric(c.recip.ctx.imaginary_rounding)) c.recip.im.negate_mutable(); else c.recip.valid = false; }
        }

        return *this;
    }

auto BigComplex::negate_mutable() -> BigComplex& {
        m_real.negate_mutable();
        m_imaginary.negate_mutable();

        if (m_cache) {
            cache_t& c = *m_cache;
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;

            if (c.recip.valid) {
                if (bcdetail::symmetric(c.recip.ctx.real_rounding) && bcdetail::symmetric(c.recip.ctx.imaginary_rounding)) {
                    c.recip.re.negate_mutable();
                    c.recip.im.negate_mutable();
                } else {
                    c.recip.valid = false;
                }
            }
        }

        return *this;
    }

auto BigComplex::mul_i_mutable() -> BigComplex& {
        BigFloat t = std::move(m_real);
        m_real = std::move(m_imaginary);
        m_real.negate_mutable();
        m_imaginary = std::move(t);

        if (m_cache) {
            cache_t& c = *m_cache;
            bcdetail::swap_by_move(c.real_sq, c.imaginary_sq);
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;
            c.recip.valid = false;
        }

        return *this;
    }

auto BigComplex::div_i_mutable() -> BigComplex& {
        BigFloat t = std::move(m_imaginary);
        m_imaginary = std::move(m_real);
        m_imaginary.negate_mutable();
        m_real = std::move(t);

        if (m_cache) {
            cache_t& c = *m_cache;
            bcdetail::swap_by_move(c.real_sq, c.imaginary_sq);
            c.arg_r.valid    = false;
            c.arg_w.valid    = false;
            c.branch_r.valid = false;
            c.recip.valid = false;
        }

        return *this;
    }

auto BigComplex::scale2_mutable(std::int64_t n) -> BigComplex& {
        if (n == 0) return *this;
        const bool fin = is_finite();
        const bool rz  = m_real.is_zero(), iz = m_imaginary.is_zero();
        m_real.scale2_mutable(n);
        m_imaginary.scale2_mutable(n);
        if (!m_cache) return *this;

        if (!fin || !is_finite() || m_real.is_zero() != rz || m_imaginary.is_zero() != iz) {
            invalidate();
            return *this;
        }

        cache_t& c = *m_cache;

        if (c.have_sq) {
            if (!bcdetail::dy_zero(c.real_sq)) c.real_sq.exp += 2 * n;
            if (!bcdetail::dy_zero(c.imaginary_sq)) c.imaginary_sq.exp += 2 * n;
        }

        if (c.have_norm && !bcdetail::dy_zero(c.norm)) c.norm.exp += 2 * n;
        scale_slot(c.norm_r, 2 * n);
        scale_slot(c.abs_r, n);

        if (c.recip.valid) {
            bool ok = true;
            scale_part(c.recip.re, ok, -n);
            scale_part(c.recip.im, ok, -n);
            if (!ok) c.recip.valid = false;
        }

        return *this;
    }

auto BigComplex::rounded(const BigComplexContext& ctx) const -> BigComplex {
        if (m_real.significand_bits() <= ctx.real_precision && m_imaginary.significand_bits() <= ctx.imaginary_precision) return *this;
        return BigComplex(m_real.rounded(ctx.real()), m_imaginary.rounded(ctx.imaginary()));
    }

auto BigComplex::square(const BigComplexContext& ctx) const -> BigComplex {
        if (is_undefined()) return undefined();
        if (is_infinite())  { const BigComplex t(*this); return mul(*this, t, ctx); }
        const BigFloatContext cr = ctx.real(), ci = ctx.imaginary();
        ensure_squares();
        const cache_t& c = *m_cache;
        bool ex = true;
        BigFloat re = bcdetail::dy_round(bcdetail::dy_sum(c.real_sq, bcdetail::dy_neg(c.imaginary_sq), cr.precision + 8, cr.rounding_mode, ex), cr);
        BigFloat im = bcdetail::dy_round(bcdetail::dy_twice(bcdetail::dy_mul(m_real, m_imaginary)), ci);
        return BigComplex(std::move(re), std::move(im));
    }

auto BigComplex::reciprocal(const BigComplexContext& ctx) const -> BigComplex {
        if (!is_finite() || is_zero()) return div(one(), *this, ctx);
        cache_t& c = cache();
        if (c.recip.valid && c.recip.ctx == ctx) return BigComplex(c.recip.re, c.recip.im);
        BigComplex r = reciprocal_finite(ctx);
        c.recip.re    = r.m_real;
        c.recip.im    = r.m_imaginary;
        c.recip.ctx   = ctx;
        c.recip.valid = true;
        return r;
    }

auto BigComplex::add(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        return BigComplex(BigFloat::add(z.m_real, w.m_real, ctx.real()), BigFloat::add(z.m_imaginary, w.m_imaginary, ctx.imaginary()));
    }

auto BigComplex::sub(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        return BigComplex(BigFloat::sub(z.m_real, w.m_real, ctx.real()), BigFloat::sub(z.m_imaginary, w.m_imaginary, ctx.imaginary()));
    }

auto BigComplex::mul(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        if (z.is_undefined() || w.is_undefined()) return undefined();

        if (z.is_infinite() || w.is_infinite()) {
            if (z.is_zero() || w.is_zero()) return undefined();
            const BigComplex s = mul_finite(z.boxed(), w.boxed(), sign_ctx());
            return BigComplex(bcdetail::to_infinity(s.m_real), bcdetail::to_infinity(s.m_imaginary));
        }

        if (&z == &w) return z.square(ctx);
        return mul_finite(z, w, ctx);
    }

auto BigComplex::div(const BigComplex& z, const BigComplex& w, const BigComplexContext& ctx) -> BigComplex {
        if (z.is_undefined() || w.is_undefined()) return undefined();
        const bool zi = z.is_infinite(), wi = w.is_infinite();
        if (zi && wi) return undefined();

        if (w.is_zero()) {
            if (z.is_zero()) return undefined();
            return BigComplex(bcdetail::to_infinity(z.m_real), bcdetail::to_infinity(z.m_imaginary));
        }

        if (zi) {
            const BigComplex s = div_finite(z.boxed(), w, sign_ctx());
            return BigComplex(bcdetail::to_infinity(s.m_real), bcdetail::to_infinity(s.m_imaginary));
        }

        if (wi) {
            const BigComplex s = div_finite(z, w.boxed(), sign_ctx());
            return BigComplex(bcdetail::to_zero(s.m_real), bcdetail::to_zero(s.m_imaginary));
        }

        if (&z == &w) return BigComplex(BigFloat::one(), BigFloat::zero(ctx.imaginary_rounding == RoundingMode::toward_neg_inf));
        return div_finite(z, w, ctx);
    }

auto BigComplex::to_string(std::size_t max_digits, std::int64_t scientific_notation_exp, std::size_t digits) const -> std::string {
        const BigComplexContext cc = BigComplexContext::current();
        const std::size_t dr = digits ? digits : BigFloatContext::dps_from_precision(cc.real_precision);
        const std::size_t di = digits ? digits : BigFloatContext::dps_from_precision(cc.imaginary_precision);
        return "(" + m_real.to_string(max_digits, scientific_notation_exp, dr) + ", " + m_imaginary.to_string(max_digits, scientific_notation_exp, di) + ")";
    }

} // namespace multiprecision
} // namespace fizmo
