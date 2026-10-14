#include "fizmo_library.hpp"
#include "special_functions.hpp"

namespace fizmo {
namespace math {
namespace cxsf {
namespace {

bool real_axis(const BC& z) { return z.imaginary().is_zero(); }
bool imag_axis(const BC& z) { return z.real().is_zero() && !z.imaginary().is_zero(); }

BC real_result(const BF& v) { return BC(v, BF::zero()); }

CX one_plus_sq(const CX& t, bool minus) {
    const std::size_t w = 64;
    const CX t2 = sqr(t, w);
    return minus ? sub(CX(BF::one()), t2, w) : add(CX(BF::one()), t2, w);
}

} // namespace

BC sinc(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return real_result(math::sinc(z.real(), ctx.real()));
    if (!z.is_finite()) return BC::undefined();
    if (imag_axis(z)) {
        const BF y = z.imaginary();
        return drive(ctx, [&](std::size_t w) {
            const BFC wc = wctx(w + 8);
            const BF  v  = BF::div(math::sinh(y, wc), y, wc);
            Approx a;
            a.v  = CX(v);
            a.er = l2(v) - static_cast<double>(w) + 1;
            return a;
        });
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) {
        const CX q = div(sin(Z, w + 8), Z, w + 8);
        return approx(q, l2(q) - static_cast<double>(w) + 1);
    });
}

BC normalized_sinc(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return real_result(math::normalized_sinc(z.real(), ctx.real()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    if (imag_axis(z)) {
        const BF y = z.imaginary();
        return drive(ctx, [&](std::size_t w) {
            const BFC wc = wctx(w + extra_bits(y) + 8);
            const BF  a  = BF::mul(y, multiprecision::constants::pi(wc), wc);
            const BF  v  = BF::div(math::sinh(a, wctx(w + 8)), a, wctx(w + 8));
            Approx r;
            r.v  = CX(v);
            r.er = l2(v) - static_cast<double>(w) + 1;
            return r;
        });
    }
    return drive(ctx, [&](std::size_t w) {
        const CV s  = sinpi(Z, w + 8);
        const CX pz = mul(Z, pi(w + 12), w + 12);
        const CX q  = div(s.v, pz, w + 8);
        return approx(q, l2(q) - static_cast<double>(w) + 1);
    });
}

BC gudermannian(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return real_result(math::gudermannian(z.real(), ctx.real()));
    if (!z.is_finite()) return BC::undefined();
    if (imag_axis(z)) {
        const BF r = math::inv_gudermannian(z.imaginary(), wctx(ctx.imaginary_precision + 2));
        if (r.is_finite()) return BC(BF::zero(), math::inv_gudermannian(z.imaginary(), ctx.imaginary()));
    }
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const CX t   = cx(math::tanh(bc(scale2(Z, -1)), BCC(BFC::clamp_precision(wi))));
        const CX a   = cx(math::arctan(bc(t), BCC(BFC::clamp_precision(wi))));
        const CX g   = scale2(a, 1);
        const double den = l2(one_plus_sq(t, false));
        if (den == ninf) return approx_undefined();
        const double e = lsum(l2(t) - static_cast<double>(wi) - den, l2(a) - static_cast<double>(wi)) + 2;
        return approx(g, e);
    });
}

BC inv_gudermannian(const BC& z, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) {
        const BF r = math::inv_gudermannian(z.real(), wctx(ctx.real_precision + 2));
        if (r.is_finite()) return real_result(math::inv_gudermannian(z.real(), ctx.real()));
    }
    if (!z.is_finite()) return BC::undefined();
    if (imag_axis(z)) return BC(BF::zero(), math::gudermannian(z.imaginary(), ctx.imaginary()));
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wi = w + 8;
        const CX t   = cx(math::tan(bc(scale2(Z, -1)), BCC(BFC::clamp_precision(wi))));
        const CX a   = cx(math::arctanh(bc(t), BCC(BFC::clamp_precision(wi))));
        const CX g   = scale2(a, 1);
        const double den = l2(one_plus_sq(t, true));
        if (den == ninf) return approx_undefined();
        const double e = lsum(l2(t) - static_cast<double>(wi) - den, l2(a) - static_cast<double>(wi)) + 2;
        return approx(g, e);
    });
}

BC log(const BC& z, const BC& base, const BCC& ctx) {
    if (!(z.is_undefined() || base.is_undefined()) && (z.is_indeterminate() || base.is_indeterminate())) return BC::indeterminate();
    if (z.is_undefined() || base.is_undefined() || !z.is_finite() || !base.is_finite()) return BC::undefined();
    if (base.is_zero() || z.is_zero() || base == BC::one()) return BC::undefined();
    if (z == base) return BC::one();
    if (z == BC::one()) return BC::zero();
    if (real_axis(z) && real_axis(base) && !z.real().signbit() && !base.real().signbit()) {
        return real_result(math::log(z.real(), base.real(), ctx.real()));
    }
    const CX Z = cx(z), B = cx(base);
    return drive(ctx, [&](std::size_t w) {
        const CX lz = ln(Z, w + 8);
        const CX lb = ln(B, w + 8);
        if (is_zero(lb)) return approx_undefined();
        const CX q = div(lz, lb, w + 8);
        return approx(q, l2(q) - static_cast<double>(w) + 1);
    });
}

namespace {

BC pow_base(const BC& z, bool ten, const BCC& ctx) {
    if (z.is_undefined()) return BC::undefined();
    if (z.is_indeterminate()) return BC::indeterminate();
    if (real_axis(z)) return real_result(ten ? math::pow10(z.real(), ctx.real()) : math::pow2(z.real(), ctx.real()));
    if (!z.is_finite()) return BC::undefined();
    const CX Z = cx(z);
    return drive(ctx, [&](std::size_t w) {
        const std::size_t wu = w + extra_bits(Z) + 8;
        const BFC uc = wctx(wu);
        const BF  L  = ten ? multiprecision::constants::ln10(uc) : multiprecision::constants::ln2(uc);
        const CX  u  = mul(Z, L, wu);
        const CX  r  = exp(u, w + 8);
        const double e = l2(r) + lsum(l2(Z) - static_cast<double>(wu) + 2, -static_cast<double>(w + 8)) + 1;
        return approx(r, e);
    });
}

} // namespace

BC pow2(const BC& z, const BCC& ctx) { return pow_base(z, false, ctx); }
BC pow10(const BC& z, const BCC& ctx) { return pow_base(z, true, ctx); }

} // namespace cxsf
} // namespace math
} // namespace fizmo
