#ifndef FIZMO_SCALAR_LINEAR_KERNELS_HPP
#define FIZMO_SCALAR_LINEAR_KERNELS_HPP

#include "field.hpp"
#include "dd.hpp"

#include <algorithm>
#include <utility>

namespace fizmo {
namespace scalar {
namespace kernel {

using multiprecision::BigInt;
using Vec = std::vector<BigFloat>;

inline BigFloatContext widen(const BigFloatContext& c, std::size_t extra) { return context(c.precision + extra); }

inline bool all_finite(const Vec& a) {
    for (const auto& v : a) if (!v.is_finite()) return false;
    return true;
}

inline BigFloat exact_dot(const Vec& a, const Vec& b) { return scalar::exact_dot(a.data(), b.data(), a.size()); }

inline BigFloat exact_product(const BigFloat* a, std::size_t n) {
    BigFloat p = BigFloat::one();
    for (std::size_t i = 0; i < n; ++i) p = exact_mul(p, a[i]);
    return p;
}

inline Vec unit(const Vec& a, const BigFloatContext& c) {
    const BigFloat s = exact_dot(a, a);
    Vec out(a.size());
    if (s.is_zero()) return a;
    const BigFloatContext w = widen(c, 8);
    const BigFloat n = math::sqrt(s, w);
    for (std::size_t i = 0; i < a.size(); ++i) out[i] = a[i].is_zero() ? a[i] : BigFloat::div(a[i], n, c);
    return out;
}

inline BigFloat scalar_project(const Vec& a, const Vec& b, const BigFloatContext& c) {
    const BigFloat bb = exact_dot(b, b);
    if (bb.is_zero()) return BigFloat::zero();
    const BigFloat d = exact_dot(a, b);
    if (d.is_zero()) return d;
    return BigFloat::div(d, math::sqrt(bb, widen(c, 8)), c);
}

inline BigFloat wedge_norm_squared(const Vec& a, const Vec& b) {
    const BigFloat aa = exact_dot(a, a);
    const BigFloat bb = exact_dot(b, b);
    const BigFloat ab = exact_dot(a, b);
    const BigFloat w = exact_sub(exact_mul(aa, bb), exact_mul(ab, ab));
    return w.signbit() && !w.is_zero() ? BigFloat::zero() : w;
}

inline BigFloat radians_to_degrees(const BigFloat& r, const BigFloatContext& c) {
    if (r.is_zero()) return r;
    const BigFloatContext w = widen(c, 8);
    return BigFloat::div(BigFloat::mul(r, BigFloat(180), w), multiprecision::constants::pi(w), c);
}

inline BigFloat angle_between(const Vec& a, const Vec& b, bool degrees, const BigFloatContext& c) {
    const BigFloat aa = exact_dot(a, a);
    const BigFloat bb = exact_dot(b, b);
    if (aa.is_zero() || bb.is_zero()) return BigFloat::zero();
    const BigFloat ab = exact_dot(a, b);
    const BigFloat w = wedge_norm_squared(a, b);
    const BigFloatContext wc = widen(c, 8);
    const BigFloat r = math::atan2(math::sqrt(w, wc), ab, degrees ? wc : c);
    return degrees ? radians_to_degrees(r, c) : r;
}

struct CosSin {
    BigFloat cos;
    BigFloat sin;
};

inline bool reduce_degrees(const BigFloat& d, BigFloat& r, long long& whole) {
    if (!d.is_finite()) return false;
    const BigInt k = d.get_integer_part();
    const BigFloat f = exact_sub(d, BigFloat(k));
    BigInt m = k % BigInt(360);
    if (m.is_negative()) m = m + BigInt(360);
    r = exact_add(BigFloat(m), f);
    whole = f.is_zero() ? static_cast<long long>(BigFloat(m).to_double()) : -1;
    return true;
}

inline CosSin cos_sin(const BigFloat& angle, bool degrees, const BigFloatContext& c) {
    if (!angle.is_finite()) return { BigFloat::nan(), BigFloat::nan() };

    if (!degrees) {
        if (angle.is_zero()) return { BigFloat::one(), angle };
        return { math::cos(angle, c), math::sin(angle, c) };
    }

    BigFloat r;
    long long whole = -1;
    reduce_degrees(angle, r, whole);

    if (whole >= 0 && whole % 30 == 0) {
        const BigFloat half = BigFloat::ldexp(BigFloat::one(), -1);
        const BigFloat root3 = BigFloat::div(math::sqrt(BigFloat(3), widen(c, 8)), BigFloat(2), c);

        switch (whole) {
            case 0:   return { BigFloat::one(), angle.is_zero() ? angle : BigFloat::zero() };
            case 30:  return { root3, half };
            case 60:  return { half, root3 };
            case 90:  return { BigFloat::zero(), BigFloat::one() };
            case 120: return { -half, root3 };
            case 150: return { -root3, half };
            case 180: return { -BigFloat::one(), BigFloat::zero() };
            case 210: return { -root3, -half };
            case 240: return { -half, -root3 };
            case 270: return { BigFloat::zero(), -BigFloat::one() };
            case 300: return { half, -root3 };
            case 330: return { root3, -half };
            default:  break;
        }
    }

    const BigFloatContext w = widen(c, 16);
    const BigFloat rad = BigFloat::div(BigFloat::mul(r, multiprecision::constants::pi(w), w), BigFloat(180), w);
    return { math::cos(rad, c), math::sin(rad, c) };
}

inline BigFloat to_radians(const BigFloat& angle, bool degrees, const BigFloatContext& c) {
    if (!degrees || angle.is_zero() || !angle.is_finite()) return angle;
    const BigFloatContext w = widen(c, 16);
    return BigFloat::div(BigFloat::mul(angle, multiprecision::constants::pi(w), w), BigFloat(180), c);
}

struct IntMatrix {
    std::size_t          rows = 0;
    std::size_t          cols = 0;
    std::vector<BigInt>  a;
    std::int64_t         exponent = 0;

    BigInt&       at(std::size_t r, std::size_t c)       { return a[r * cols + c]; }
    const BigInt& at(std::size_t r, std::size_t c) const { return a[r * cols + c]; }
};

inline IntMatrix to_integers(const Vec& m, std::size_t rows, std::size_t cols) {
    IntMatrix out;
    out.rows = rows;
    out.cols = cols;
    out.a.assign(rows * cols, BigInt::zero());
    bool any = false;
    std::int64_t emin = 0;

    for (const auto& v : m) {
        if (v.is_zero()) continue;
        if (!any || v.exponent() < emin) emin = v.exponent();
        any = true;
    }

    out.exponent = emin;

    for (std::size_t i = 0; i < m.size(); ++i) {
        if (m[i].is_zero()) continue;
        out.a[i] = BigInt(m[i].significand().shifted_left(static_cast<std::size_t>(m[i].exponent() - emin)), m[i].signbit());
    }

    return out;
}

inline BigFloat from_integer(const BigInt& v, std::int64_t exponent) {
    if (v.is_zero()) return BigFloat::zero();
    return BigFloat(v, exponent);
}

inline bool magnitude_less(const BigInt& a, const BigInt& b) { return a.magnitude() < b.magnitude(); }

inline BigInt bareiss_det(IntMatrix m) {
    const std::size_t n = m.rows;
    if (n == 0) return BigInt::one();
    BigInt prev = BigInt::one();
    bool negate = false;

    for (std::size_t k = 0; k < n; ++k) {
        if (m.at(k, k).is_zero()) {
            std::size_t p = k + 1;
            while (p < n && m.at(p, k).is_zero()) ++p;
            if (p == n) return BigInt::zero();
            for (std::size_t j = 0; j < n; ++j) std::swap(m.at(k, j), m.at(p, j));
            negate = !negate;
        }

        for (std::size_t i = k + 1; i < n; ++i) {
            for (std::size_t j = k + 1; j < n; ++j) m.at(i, j) = (m.at(i, j) * m.at(k, k) - m.at(i, k) * m.at(k, j)) / prev;
        }

        prev = m.at(k, k);
    }

    return negate ? -m.at(n - 1, n - 1) : m.at(n - 1, n - 1);
}

inline BigFloat naive_det(Vec m, std::size_t n, const BigFloatContext& c) {
    BigFloat det = BigFloat::one();

    for (std::size_t k = 0; k < n; ++k) {
        std::size_t p = k;
        while (p < n && m[p * n + k].is_zero()) ++p;
        if (p == n) return BigFloat::zero();

        if (p != k) {
            for (std::size_t j = 0; j < n; ++j) std::swap(m[k * n + j], m[p * n + j]);
            det = -det;
        }

        det = BigFloat::mul(det, m[k * n + k], c);

        for (std::size_t i = k + 1; i < n; ++i) {
            const BigFloat f = BigFloat::div(m[i * n + k], m[k * n + k], c);
            for (std::size_t j = k; j < n; ++j) m[i * n + j] = BigFloat::sub(m[i * n + j], BigFloat::mul(f, m[k * n + j], c), c);
        }
    }

    return det;
}

inline BigFloat det(const Vec& m, std::size_t n) {
    if (n == 0) return BigFloat::one();
    if (!all_finite(m)) return naive_det(m, n, context(64));
    const IntMatrix im = to_integers(m, n, n);
    return from_integer(bareiss_det(im), im.exponent * static_cast<std::int64_t>(n));
}

inline Vec minor(const Vec& m, std::size_t n, std::size_t dr, std::size_t dc) {
    Vec out;
    out.reserve((n - 1) * (n - 1));

    for (std::size_t r = 0; r < n; ++r) {
        if (r == dr) continue;
        for (std::size_t c = 0; c < n; ++c) if (c != dc) out.push_back(m[r * n + c]);
    }

    return out;
}

inline Vec cofactors(const Vec& m, std::size_t n) {
    Vec out(n * n);

    if (n == 1) {
        out[0] = BigFloat::one();
        return out;
    }

    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t c = 0; c < n; ++c) {
            const BigFloat d = det(minor(m, n, r, c), n - 1);
            out[r * n + c] = ((r + c) % 2 == 0) ? d : -d;
        }
    }

    return out;
}

inline Vec adjugate(const Vec& m, std::size_t n) {
    const Vec cof = cofactors(m, n);
    Vec out(n * n);
    for (std::size_t r = 0; r < n; ++r) for (std::size_t c = 0; c < n; ++c) out[c * n + r] = cof[r * n + c];
    return out;
}

inline std::vector<std::size_t> pivot_columns(const Vec& m, std::size_t rows, std::size_t cols) {
    std::vector<std::size_t> pivots;
    if (!all_finite(m)) return pivots;
    IntMatrix a = to_integers(m, rows, cols);
    BigInt prev = BigInt::one();
    std::size_t r = 0;

    for (std::size_t col = 0; col < cols && r < rows; ++col) {
        std::size_t p = r;
        while (p < rows && a.at(p, col).is_zero()) ++p;
        if (p == rows) continue;
        if (p != r) for (std::size_t j = 0; j < cols; ++j) std::swap(a.at(r, j), a.at(p, j));

        for (std::size_t i = r + 1; i < rows; ++i) {
            for (std::size_t j = col + 1; j < cols; ++j) a.at(i, j) = (a.at(i, j) * a.at(r, col) - a.at(i, col) * a.at(r, j)) / prev;
            a.at(i, col) = BigInt::zero();
        }

        prev = a.at(r, col);
        pivots.push_back(col);
        ++r;
    }

    return pivots;
}

inline std::size_t rank(const Vec& m, std::size_t rows, std::size_t cols) {
    if (!all_finite(m)) return 0;
    return pivot_columns(m, rows, cols).size();
}

inline Vec gauss_jordan_inverse(Vec a, std::size_t n, const BigFloatContext& c) {
    Vec inv(n * n, BigFloat::zero());
    for (std::size_t i = 0; i < n; ++i) inv[i * n + i] = BigFloat::one();

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t p = col;
        for (std::size_t r = col + 1; r < n; ++r) if (a[r * n + col].abs() > a[p * n + col].abs()) p = r;

        if (p != col) {
            for (std::size_t j = 0; j < n; ++j) {
                std::swap(a[col * n + j], a[p * n + j]);
                std::swap(inv[col * n + j], inv[p * n + j]);
            }
        }

        const BigFloat piv = a[col * n + col];

        for (std::size_t j = 0; j < n; ++j) {
            a[col * n + j] = BigFloat::div(a[col * n + j], piv, c);
            inv[col * n + j] = BigFloat::div(inv[col * n + j], piv, c);
        }

        for (std::size_t r = 0; r < n; ++r) {
            if (r == col || a[r * n + col].is_zero()) continue;
            const BigFloat f = a[r * n + col];

            for (std::size_t j = 0; j < n; ++j) {
                a[r * n + j] = BigFloat::sub(a[r * n + j], BigFloat::mul(f, a[col * n + j], c), c);
                inv[r * n + j] = BigFloat::sub(inv[r * n + j], BigFloat::mul(f, inv[col * n + j], c), c);
            }
        }
    }

    return inv;
}

inline Vec rect_mul(const Vec& a, const Vec& b, std::size_t n, std::size_t k, std::size_t m, const BigFloatContext& c) {
    Vec out(n * m);
    Vec row(k), col(k);

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t t = 0; t < k; ++t) row[t] = a[i * k + t];

        for (std::size_t j = 0; j < m; ++j) {
            for (std::size_t t = 0; t < k; ++t) col[t] = b[t * m + j];
            out[i * m + j] = scalar::dot(row.data(), col.data(), k, c);
        }
    }

    return out;
}

inline Vec rect_transpose(const Vec& a, std::size_t n, std::size_t m) {
    Vec out(n * m);
    for (std::size_t i = 0; i < n; ++i) for (std::size_t j = 0; j < m; ++j) out[j * n + i] = a[i * m + j];
    return out;
}

inline Vec pinverse(const Vec& m, std::size_t n, const std::vector<std::size_t>& pivots, const BigFloatContext& c) {
    const std::size_t r = pivots.size();
    if (r == 0) return Vec(n * n, BigFloat::zero());
    const BigFloatContext w = widen(c, 32);
    Vec C(n * r);
    for (std::size_t i = 0; i < n; ++i) for (std::size_t k = 0; k < r; ++k) C[i * r + k] = m[i * n + pivots[k]];
    const Vec Ct = rect_transpose(C, n, r);
    const Vec Ginv = gauss_jordan_inverse(rect_mul(Ct, C, r, n, r, w), r, w);
    const Vec R = rect_mul(Ginv, rect_mul(Ct, m, r, n, n, w), r, r, n, w);
    const Vec Rt = rect_transpose(R, r, n);
    const Vec Hinv = gauss_jordan_inverse(rect_mul(R, Rt, r, n, r, w), r, w);
    const Vec left = rect_mul(Rt, rect_mul(Hinv, Ginv, r, r, r, w), n, r, r, w);
    return rect_mul(left, Ct, n, r, n, w);
}

struct ExactLU {
    bool              ok = false;
    std::vector<BigFloat> l_num;
    std::vector<BigFloat> l_den;
    std::vector<BigFloat> u_num;
    std::vector<BigFloat> u_den;
    std::vector<std::size_t> perm;
    int               sign = 1;
};

inline ExactLU exact_lu(const Vec& m, std::size_t n) {
    ExactLU out;
    if (!all_finite(m)) return out;
    IntMatrix a = to_integers(m, n, n);
    std::vector<BigInt> pivots(n, BigInt::one());
    std::vector<std::size_t> perm(n);
    for (std::size_t i = 0; i < n; ++i) perm[i] = i;
    BigInt prev = BigInt::one();
    std::vector<BigInt> prevs(n, BigInt::one());
    int sign = 1;

    for (std::size_t k = 0; k < n; ++k) {
        std::size_t p = k;
        for (std::size_t i = k + 1; i < n; ++i) if (magnitude_less(a.at(p, k), a.at(i, k))) p = i;
        if (a.at(p, k).is_zero()) return out;

        if (p != k) {
            for (std::size_t j = 0; j < n; ++j) std::swap(a.at(k, j), a.at(p, j));
            std::swap(perm[k], perm[p]);
            sign = -sign;
        }

        prevs[k] = prev;
        pivots[k] = a.at(k, k);

        for (std::size_t i = k + 1; i < n; ++i) {
            for (std::size_t j = k + 1; j < n; ++j) a.at(i, j) = (a.at(i, j) * a.at(k, k) - a.at(i, k) * a.at(k, j)) / prev;
        }

        prev = a.at(k, k);
    }

    out.ok = true;
    out.sign = sign;
    out.perm = perm;
    out.l_num.assign(n * n, BigFloat::zero());
    out.l_den.assign(n * n, BigFloat::one());
    out.u_num.assign(n * n, BigFloat::zero());
    out.u_den.assign(n * n, BigFloat::one());

    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t j = 0; j < n; ++j) {
            if (i == j) {
                out.l_num[i * n + j] = BigFloat::one();
            } else if (i > j) {
                out.l_num[i * n + j] = from_integer(a.at(i, j), 0);
                out.l_den[i * n + j] = from_integer(pivots[j], 0);
            }

            if (j >= i) {
                out.u_num[i * n + j] = from_integer(a.at(i, j), a.exponent);
                out.u_den[i * n + j] = from_integer(prevs[i], 0);
            }
        }
    }

    return out;
}

inline Vec lu_working(const Vec& m, std::size_t n, const BigFloatContext& c) {
    Vec L(n * n, BigFloat::zero());
    Vec U = m;
    Vec P(n * n, BigFloat::zero());
    for (std::size_t i = 0; i < n; ++i) { L[i * n + i] = BigFloat::one(); P[i * n + i] = BigFloat::one(); }
    BigFloat sign = BigFloat::one();

    for (std::size_t col = 0; col < n; ++col) {
        std::size_t pivot = col;
        BigFloat best = U[col * n + col].abs();

        for (std::size_t r = col + 1; r < n; ++r) {
            const BigFloat v = U[r * n + col].abs();
            if (v > best) { best = v; pivot = r; }
        }

        if (pivot != col) {
            for (std::size_t j = 0; j < n; ++j) { std::swap(U[col * n + j], U[pivot * n + j]); std::swap(P[col * n + j], P[pivot * n + j]); }
            for (std::size_t k = 0; k < col; ++k) std::swap(L[col * n + k], L[pivot * n + k]);
            sign = -sign;
        }

        if (U[col * n + col].is_zero()) continue;

        for (std::size_t r = col + 1; r < n; ++r) {
            const BigFloat factor = BigFloat::div(U[r * n + col], U[col * n + col], c);
            L[r * n + col] = factor;
            U[r * n + col] = BigFloat::zero();
            for (std::size_t j = col + 1; j < n; ++j) U[r * n + j] = BigFloat::sub(U[r * n + j], BigFloat::mul(factor, U[col * n + j], c), c);
        }
    }

    Vec out;
    out.reserve(3 * n * n + 1);
    out.insert(out.end(), L.begin(), L.end());
    out.insert(out.end(), U.begin(), U.end());
    out.insert(out.end(), P.begin(), P.end());
    out.push_back(sign);
    return out;
}

inline Vec qr(const Vec& m, std::size_t n, const BigFloatContext& c) {
    Vec Q(n * n, BigFloat::zero());
    Vec R(n * n, BigFloat::zero());
    std::vector<Vec> basis(n, Vec(n));

    for (std::size_t j = 0; j < n; ++j) {
        for (std::size_t i = 0; i < n; ++i) basis[j][i] = m[i * n + j];

        for (std::size_t k = 0; k < j; ++k) {
            const BigFloat d = scalar::dot(basis[k].data(), basis[j].data(), n, c);
            R[k * n + j] = d;
            if (d.is_zero()) continue;
            for (std::size_t i = 0; i < n; ++i) basis[j][i] = BigFloat::sub(basis[j][i], BigFloat::mul(d, basis[k][i], c), c);
        }

        const BigFloat nrm = scalar::norm(basis[j].data(), n, c);
        R[j * n + j] = nrm;
        if (!nrm.is_zero()) for (std::size_t i = 0; i < n; ++i) basis[j][i] = BigFloat::div(basis[j][i], nrm, c);
        for (std::size_t i = 0; i < n; ++i) Q[i * n + j] = basis[j][i];
    }

    Vec out;
    out.reserve(2 * n * n);
    out.insert(out.end(), Q.begin(), Q.end());
    out.insert(out.end(), R.begin(), R.end());
    return out;
}

inline Vec matmul(const Vec& a, const Vec& b, std::size_t n, const BigFloatContext& c) {
    Vec out(n * n);
    Vec row(n), col(n);

    for (std::size_t r = 0; r < n; ++r) {
        for (std::size_t k = 0; k < n; ++k) row[k] = a[r * n + k];

        for (std::size_t cc = 0; cc < n; ++cc) {
            for (std::size_t k = 0; k < n; ++k) col[k] = b[k * n + cc];
            out[r * n + cc] = scalar::dot(row.data(), col.data(), n, c);
        }
    }

    return out;
}

inline Vec identity(std::size_t n) {
    Vec out(n * n, BigFloat::zero());
    for (std::size_t i = 0; i < n; ++i) out[i * n + i] = BigFloat::one();
    return out;
}

inline Vec inverse_working(const Vec& m, std::size_t n, const BigFloatContext& c) {
    const BigFloat d = det(m, n);
    const Vec adj = adjugate(m, n);
    Vec out(n * n);
    for (std::size_t i = 0; i < n * n; ++i) out[i] = BigFloat::div(adj[i], d, c);
    return out;
}

inline Vec matrix_pow(const Vec& m, std::size_t n, long long e, const BigFloatContext& c) {
    if (e == 0) return identity(n);
    Vec base = e < 0 ? inverse_working(m, n, widen(c, 16)) : m;
    unsigned long long k = e < 0 ? static_cast<unsigned long long>(-(e + 1)) + 1ULL : static_cast<unsigned long long>(e);
    std::size_t bits = 1;
    for (unsigned long long t = k >> 1; t != 0ULL; t >>= 1) ++bits;
    const BigFloatContext w = widen(c, 2 * bits + 16);
    Vec result = identity(n);
    bool first = true;

    while (k > 0) {
        if (k & 1ULL) {
            result = first ? base : matmul(result, base, n, w);
            first = false;
        }

        k >>= 1;
        if (k) base = matmul(base, base, n, w);
    }

    return result;
}

inline Vec axis_rotation(const BigFloat& ax, const BigFloat& ay, const BigFloat& az, const BigFloat& angle, bool degrees, const BigFloatContext& c) {
    const BigFloat l2 = exact_add(exact_add(exact_mul(ax, ax), exact_mul(ay, ay)), exact_mul(az, az));
    const BigFloatContext w = widen(c, 16);
    const CosSin cs = cos_sin(angle, degrees, w);
    const BigFloat ic = BigFloat::sub(BigFloat::one(), cs.cos, w);
    const BigFloat len = math::sqrt(l2, w);
    const BigFloat a[3] = { ax, ay, az };
    Vec out(9);

    for (std::size_t r = 0; r < 3; ++r) {
        for (std::size_t k = 0; k < 3; ++k) {
            const BigFloat nn = BigFloat::div(exact_mul(a[r], a[k]), l2, w);
            BigFloat v = BigFloat::mul(nn, ic, w);
            if (r == k) v = BigFloat::add(v, cs.cos, w);
            out[r * 3 + k] = v;
        }
    }

    const BigFloat sx = BigFloat::div(BigFloat::mul(ax, cs.sin, w), len, w);
    const BigFloat sy = BigFloat::div(BigFloat::mul(ay, cs.sin, w), len, w);
    const BigFloat sz = BigFloat::div(BigFloat::mul(az, cs.sin, w), len, w);
    out[1] = BigFloat::sub(out[1], sz, c);
    out[2] = BigFloat::add(out[2], sy, c);
    out[3] = BigFloat::add(out[3], sz, c);
    out[5] = BigFloat::sub(out[5], sx, c);
    out[6] = BigFloat::sub(out[6], sy, c);
    out[7] = BigFloat::add(out[7], sx, c);
    out[0] = out[0].rounded(c);
    out[4] = out[4].rounded(c);
    out[8] = out[8].rounded(c);
    return out;
}

inline Vec look_at(const Vec& eye, const Vec& center, const Vec& up, const BigFloatContext& c) {
    const BigFloatContext w = widen(c, 16);
    Vec f(3), r(3), v(3);
    for (std::size_t i = 0; i < 3; ++i) f[i] = exact_sub(center[i], eye[i]);
    r[0] = exact_sub(exact_mul(f[1], up[2]), exact_mul(f[2], up[1]));
    r[1] = exact_sub(exact_mul(f[2], up[0]), exact_mul(f[0], up[2]));
    r[2] = exact_sub(exact_mul(f[0], up[1]), exact_mul(f[1], up[0]));
    v[0] = exact_sub(exact_mul(r[1], f[2]), exact_mul(r[2], f[1]));
    v[1] = exact_sub(exact_mul(r[2], f[0]), exact_mul(r[0], f[2]));
    v[2] = exact_sub(exact_mul(r[0], f[1]), exact_mul(r[1], f[0]));
    const Vec fu = unit(f, w);
    const Vec ru = unit(r, w);
    const Vec vu = unit(v, w);
    Vec m = identity(4);

    for (std::size_t i = 0; i < 3; ++i) {
        m[0 * 4 + i] = ru[i].rounded(c);
        m[1 * 4 + i] = vu[i].rounded(c);
        m[2 * 4 + i] = (-fu[i]).rounded(c);
    }

    m[0 * 4 + 3] = (-scalar::dot(ru.data(), eye.data(), 3, w)).rounded(c);
    m[1 * 4 + 3] = (-scalar::dot(vu.data(), eye.data(), 3, w)).rounded(c);
    m[2 * 4 + 3] = scalar::dot(fu.data(), eye.data(), 3, w).rounded(c);
    return m;
}

} // namespace kernel
} // namespace scalar
} // namespace fizmo

#endif // FIZMO_SCALAR_LINEAR_KERNELS_HPP
