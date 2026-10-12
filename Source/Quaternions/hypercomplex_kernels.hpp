#ifndef FIZMO_HYPERCOMPLEX_KERNELS_HPP
#define FIZMO_HYPERCOMPLEX_KERNELS_HPP

#include "../Vectors/kvectors.hpp"

namespace fizmo {
namespace linalg {
namespace hypercomplex_detail {

using scalar::BigFloat;
using scalar::BigFloatContext;
using Vec = scalar::kernel::Vec;

inline int product_sign(std::size_t k, std::size_t i) {
    static const int signs[8][8] = {
        { 1, -1, -1, -1, -1, -1, -1, -1 },
        { 1,  1,  1, -1,  1, -1, -1,  1 },
        { 1, -1,  1,  1,  1,  1, -1, -1 },
        { 1,  1, -1,  1,  1, -1,  1, -1 },
        { 1, -1, -1, -1,  1,  1,  1,  1 },
        { 1,  1, -1,  1, -1,  1, -1,  1 },
        { 1,  1,  1, -1, -1,  1,  1, -1 },
        { 1, -1,  1,  1, -1, -1,  1,  1 }
    };

    return signs[k][i];
}

template <class S, std::size_t M>
std::array<S, M> product(const std::array<S, M>& a, const std::array<S, M>& b, const typename scalar::field<S>::context_type& c) {
    using F = scalar::field<S>;
    std::array<S, M> out;

    for (std::size_t k = 0; k < M; ++k) {
        std::array<S, M> x;
        std::array<S, M> y;

        for (std::size_t i = 0; i < M; ++i) {
            x[i] = product_sign(k, i) > 0 ? a[i] : F::neg(a[i]);
            y[i] = b[i ^ k];
        }

        out[k] = F::dot(x.data(), y.data(), M, c);
    }

    return out;
}

inline Vec exact_product(const Vec& a, const Vec& b) {
    const std::size_t m = a.size();
    Vec out(m);

    for (std::size_t k = 0; k < m; ++k) {
        BigFloat s = BigFloat::zero();

        for (std::size_t i = 0; i < m; ++i) {
            const BigFloat p = scalar::exact_mul(a[i], b[i ^ k]);
            s = product_sign(k, i) > 0 ? scalar::exact_add(s, p) : scalar::exact_sub(s, p);
        }

        out[k] = s;
    }

    return out;
}

inline Vec working_product(const Vec& a, const Vec& b, const BigFloatContext& c) {
    Vec out = exact_product(a, b);
    for (auto& v : out) v = v.rounded(c);
    return out;
}

inline Vec conjugate(const Vec& a) {
    Vec out(a);
    for (std::size_t i = 1; i < out.size(); ++i) out[i] = -out[i];
    return out;
}

inline BigFloat vector_norm_squared(const Vec& a) {
    BigFloat s = BigFloat::zero();
    for (std::size_t i = 1; i < a.size(); ++i) s = scalar::exact_add(s, scalar::exact_mul(a[i], a[i]));
    return s;
}

inline Vec ln(const Vec& q, const BigFloatContext& c) {
    const std::size_t m = q.size();
    const BigFloatContext w = scalar::kernel::widen(c, 16);
    const BigFloat nq = scalar::kernel::exact_dot(q, q);
    Vec out(m, BigFloat::zero());

    if (nq.is_zero()) {
        out[0] = BigFloat::infinity(true);
        return out;
    }

    out[0] = BigFloat::ldexp(math::ln(nq, w), -1);
    const BigFloat nv = vector_norm_squared(q);

    if (nv.is_zero()) {
        if (q[0].signbit()) out[1] = multiprecision::constants::pi(w);
        return out;
    }

    const BigFloat lv = math::sqrt(nv, w);
    const BigFloat theta = math::atan2(lv, q[0], w);
    const BigFloat scale = BigFloat::div(theta, lv, w);
    for (std::size_t i = 1; i < m; ++i) out[i] = q[i].is_zero() ? BigFloat::zero() : BigFloat::mul(q[i], scale, w);
    return out;
}

inline Vec exp(const Vec& q, const BigFloatContext& c) {
    const std::size_t m = q.size();
    const BigFloatContext w = scalar::kernel::widen(c, 16);
    const BigFloat ew = math::exp(q[0], w);
    const BigFloat nv = vector_norm_squared(q);
    Vec out(m, BigFloat::zero());

    if (nv.is_zero()) {
        out[0] = ew;
        return out;
    }

    const BigFloat lv = math::sqrt(nv, w);
    out[0] = BigFloat::mul(ew, math::cos(lv, w), w);
    const BigFloat scale = BigFloat::div(BigFloat::mul(ew, math::sin(lv, w), w), lv, w);
    for (std::size_t i = 1; i < m; ++i) out[i] = q[i].is_zero() ? BigFloat::zero() : BigFloat::mul(q[i], scale, w);
    return out;
}

inline Vec scale(const Vec& q, const BigFloat& s, const BigFloatContext& c) {
    Vec out(q.size());
    for (std::size_t i = 0; i < q.size(); ++i) out[i] = BigFloat::mul(q[i], s, c);
    return out;
}

inline Vec power(const Vec& q, const BigFloat& t, const BigFloatContext& c) {
    const BigFloatContext w = scalar::kernel::widen(c, 32);
    return exp(scale(ln(q, w), t, w), c);
}

inline Vec power(const Vec& q, const Vec& p, const BigFloatContext& c) {
    const BigFloatContext w = scalar::kernel::widen(c, 32);
    return exp(working_product(ln(q, w), p, w), c);
}

inline Vec nlerp(const Vec& a, const Vec& b, const BigFloat& t, const BigFloatContext& c) {
    const BigFloat d = scalar::kernel::exact_dot(a, b);
    const bool flip = d.signbit() && !d.is_zero();
    const BigFloat omt = scalar::exact_sub(BigFloat::one(), t);
    Vec mix(a.size());

    for (std::size_t i = 0; i < a.size(); ++i) {
        const BigFloat bi = flip ? -b[i] : b[i];
        mix[i] = scalar::exact_add(scalar::exact_mul(a[i], omt), scalar::exact_mul(bi, t));
    }

    return scalar::kernel::unit(mix, c);
}

inline Vec slerp(const Vec& a, const Vec& b, const BigFloat& t, const BigFloatContext& c) {
    const BigFloat d = scalar::kernel::exact_dot(a, b);
    const bool flip = d.signbit() && !d.is_zero();
    const BigFloat w2 = scalar::kernel::wedge_norm_squared(a, b);
    if (w2.is_zero()) return nlerp(a, b, t, c);

    const BigFloatContext w = scalar::kernel::widen(c, 32);
    const BigFloat theta = math::atan2(math::sqrt(w2, w), d.abs(), w);
    const BigFloat inv_sin = BigFloat::div(BigFloat::one(), math::sin(theta, w), w);
    const BigFloat omt = scalar::exact_sub(BigFloat::one(), t);
    const BigFloat ka = BigFloat::mul(math::sin(BigFloat::mul(omt, theta, w), w), inv_sin, w);
    const BigFloat kb = BigFloat::mul(math::sin(BigFloat::mul(t, theta, w), w), inv_sin, w);
    Vec out(a.size());

    for (std::size_t i = 0; i < a.size(); ++i) {
        const BigFloat bi = flip ? -b[i] : b[i];
        out[i] = BigFloat::add(BigFloat::mul(a[i], ka, w), BigFloat::mul(bi, kb, w), c);
    }

    return out;
}

inline BigFloat angle_between(const Vec& a, const Vec& b, const BigFloatContext& c) {
    const BigFloat aa = scalar::kernel::exact_dot(a, a);
    const BigFloat bb = scalar::kernel::exact_dot(b, b);
    if (aa.is_zero() || bb.is_zero()) return BigFloat::nan();
    const BigFloat d = scalar::kernel::exact_dot(a, b).abs();
    const BigFloat w2 = scalar::kernel::wedge_norm_squared(a, b);
    const BigFloatContext w = scalar::kernel::widen(c, 8);
    return BigFloat::ldexp(math::atan2(math::sqrt(w2, w), d, c), 1);
}

} // namespace hypercomplex_detail
} // namespace linalg
} // namespace fizmo

#endif // FIZMO_HYPERCOMPLEX_KERNELS_HPP
