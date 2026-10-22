#include "sparse.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace fizmo {
namespace numerics {

namespace {

double norm2(const std::vector<double>& v) {
    double s = 0.0;
    for (double x : v) s += x * x;
    return std::sqrt(s);
}

double dot(const std::vector<double>& a, const std::vector<double>& b) {
    double s = 0.0;
    for (std::size_t i = 0; i < a.size(); ++i) s += a[i] * b[i];
    return s;
}

} // namespace

CsrMatrix CsrMatrix::from_triplets(std::size_t size, const std::vector<std::size_t>& rows, const std::vector<int>& cols, const std::vector<double>& values, bool keep_diagonal) {
    CsrMatrix m;
    m.n = size;
    std::vector<std::size_t> count(size + 1, 0);
    for (std::size_t r : rows) ++count[r + 1];
    for (std::size_t i = 0; i < size; ++i) count[i + 1] += count[i];
    std::vector<int> c(rows.size());
    std::vector<double> v(rows.size());
    std::vector<std::size_t> fill(count.begin(), count.end() - 1);
    for (std::size_t k = 0; k < rows.size(); ++k) {
        const std::size_t at = fill[rows[k]]++;
        c[at] = cols[k];
        v[at] = values[k];
    }
    m.ptr.assign(size + 1, 0);
    m.col.reserve(rows.size() + (keep_diagonal ? size : 0));
    m.val.reserve(rows.size() + (keep_diagonal ? size : 0));
    std::vector<std::pair<int, double>> row;
    for (std::size_t i = 0; i < size; ++i) {
        row.clear();
        bool has_diag = false;
        for (std::size_t p = count[i]; p < count[i + 1]; ++p) {
            row.push_back({ c[p], v[p] });
            if (static_cast<std::size_t>(c[p]) == i) has_diag = true;
        }
        if (keep_diagonal && !has_diag) row.push_back({ static_cast<int>(i), 0.0 });
        std::sort(row.begin(), row.end(), [](const std::pair<int, double>& a, const std::pair<int, double>& b) { return a.first < b.first; });
        for (const auto& e : row) {
            if (m.col.size() > m.ptr[i] && m.col.back() == e.first) m.val.back() += e.second;
            else {
                m.col.push_back(e.first);
                m.val.push_back(e.second);
            }
        }
        m.ptr[i + 1] = m.col.size();
    }
    return m;
}

CsrMatrix CsrMatrix::from_triplets(std::size_t size, const std::vector<Triplet>& entries, bool keep_diagonal) {
    std::vector<std::size_t> rows;
    std::vector<int> cols;
    std::vector<double> values;
    rows.reserve(entries.size());
    cols.reserve(entries.size());
    values.reserve(entries.size());
    for (const Triplet& t : entries) {
        rows.push_back(t.row);
        cols.push_back(static_cast<int>(t.col));
        values.push_back(t.value);
    }
    return from_triplets(size, rows, cols, values, keep_diagonal);
}

CsrMatrix CsrMatrix::identity(std::size_t size) {
    CsrMatrix m;
    m.n = size;
    m.ptr.resize(size + 1);
    m.col.resize(size);
    m.val.assign(size, 1.0);
    for (std::size_t i = 0; i <= size; ++i) m.ptr[i] = i;
    for (std::size_t i = 0; i < size; ++i) m.col[i] = static_cast<int>(i);
    return m;
}

double CsrMatrix::at(std::size_t row, std::size_t column) const {
    for (std::size_t p = ptr[row]; p < ptr[row + 1]; ++p) {
        if (static_cast<std::size_t>(col[p]) == column) return val[p];
    }
    return 0.0;
}

void CsrMatrix::multiply(const std::vector<double>& x, std::vector<double>& y) const {
    y.assign(n, 0.0);
    for (std::size_t i = 0; i < n; ++i) {
        double s = 0.0;
        for (std::size_t p = ptr[i]; p < ptr[i + 1]; ++p) s += val[p] * x[static_cast<std::size_t>(col[p])];
        y[i] = s;
    }
}

std::vector<double> CsrMatrix::operator*(const std::vector<double>& x) const {
    std::vector<double> y;
    multiply(x, y);
    return y;
}

void CsrMatrix::bandwidth(std::size_t& lower, std::size_t& upper) const {
    lower = 0;
    upper = 0;
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t p = ptr[i]; p < ptr[i + 1]; ++p) {
            const long d = static_cast<long>(col[p]) - static_cast<long>(i);
            if (d < 0) lower = std::max(lower, static_cast<std::size_t>(-d));
            else upper = std::max(upper, static_cast<std::size_t>(d));
        }
    }
}

std::vector<double> CsrMatrix::scale_rows() {
    std::vector<double> scale(n, 1.0);
    for (std::size_t i = 0; i < n; ++i) {
        double big = 0.0;
        for (std::size_t p = ptr[i]; p < ptr[i + 1]; ++p) big = std::max(big, std::fabs(val[p]));
        if (!(big > 0.0) || !std::isfinite(big)) big = 1.0;
        const double inv = 1.0 / big;
        for (std::size_t p = ptr[i]; p < ptr[i + 1]; ++p) val[p] *= inv;
        scale[i] = inv;
    }
    return scale;
}

bool BandedLU::factor(const CsrMatrix& a) {
    m_ready = false;
    m_n = a.n;
    a.bandwidth(m_kl, m_ku);
    m_w = 2 * m_kl + m_ku + 1;
    m_band.assign(m_n * m_w, 0.0);
    m_piv.assign(m_n, 0);
    if (m_n == 0) {
        m_ready = true;
        return true;
    }
    for (std::size_t i = 0; i < m_n; ++i) {
        for (std::size_t p = a.ptr[i]; p < a.ptr[i + 1]; ++p) at(i, static_cast<std::size_t>(a.col[p])) += a.val[p];
    }
    for (std::size_t k = 0; k < m_n; ++k) {
        const std::size_t last = std::min(m_n - 1, k + m_kl);
        std::size_t p = k;
        double best = std::fabs(at(k, k));
        for (std::size_t i = k + 1; i <= last; ++i) {
            const double v = std::fabs(at(i, k));
            if (v > best) {
                best = v;
                p = i;
            }
        }
        if (!(best > 0.0) || !std::isfinite(best)) return false;
        m_piv[k] = p;
        const std::size_t right = std::min(m_n - 1, k + m_ku + m_kl);
        if (p != k) {
            for (std::size_t j = k; j <= right; ++j) std::swap(at(k, j), at(p, j));
        }
        const double d = at(k, k);
        for (std::size_t i = k + 1; i <= last; ++i) {
            double& m = at(i, k);
            if (m == 0.0) continue;
            m /= d;
            const double mm = m;
            if (k + 1 > right) continue;
            double* ri = &at(i, k + 1);
            const double* rk = &at(k, k + 1);
            for (std::size_t j = k + 1; j <= right; ++j) *ri++ -= mm * *rk++;
        }
    }
    m_ready = true;
    return true;
}

void BandedLU::solve(std::vector<double>& b) const {
    if (m_n == 0) return;
    for (std::size_t k = 0; k < m_n; ++k) {
        if (m_piv[k] != k) std::swap(b[k], b[m_piv[k]]);
        const std::size_t last = std::min(m_n - 1, k + m_kl);
        const double bk = b[k];
        if (bk == 0.0) continue;
        for (std::size_t i = k + 1; i <= last; ++i) b[i] -= at(i, k) * bk;
    }
    for (std::size_t k = m_n; k-- > 0;) {
        const std::size_t right = std::min(m_n - 1, k + m_ku + m_kl);
        double s = b[k];
        for (std::size_t j = k + 1; j <= right; ++j) s -= at(k, j) * b[j];
        b[k] = s / at(k, k);
    }
}

double BandedLU::cost(const CsrMatrix& a) {
    std::size_t l = 0, u = 0;
    a.bandwidth(l, u);
    return static_cast<double>(a.n) * static_cast<double>(l + 1) * static_cast<double>(2 * l + u + 1);
}

void Ilu0::factor(const CsrMatrix& a) {
    m_lu = a;
    const std::size_t n = m_lu.n;
    const std::size_t none = static_cast<std::size_t>(-1);
    m_diag.assign(n, none);
    std::vector<long> iw(n, -1);
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t p = m_lu.ptr[i]; p < m_lu.ptr[i + 1]; ++p) {
            if (static_cast<std::size_t>(m_lu.col[p]) == i) m_diag[i] = p;
        }
    }
    for (std::size_t i = 0; i < n; ++i) {
        for (std::size_t p = m_lu.ptr[i]; p < m_lu.ptr[i + 1]; ++p) iw[static_cast<std::size_t>(m_lu.col[p])] = static_cast<long>(p);
        for (std::size_t p = m_lu.ptr[i]; p < m_lu.ptr[i + 1]; ++p) {
            const std::size_t k = static_cast<std::size_t>(m_lu.col[p]);
            if (k >= i) break;
            if (m_diag[k] == none) continue;
            m_lu.val[p] /= m_lu.val[m_diag[k]];
            const double f = m_lu.val[p];
            for (std::size_t q = m_diag[k] + 1; q < m_lu.ptr[k + 1]; ++q) {
                const long at = iw[static_cast<std::size_t>(m_lu.col[q])];
                if (at >= 0) m_lu.val[static_cast<std::size_t>(at)] -= f * m_lu.val[q];
            }
        }
        for (std::size_t p = m_lu.ptr[i]; p < m_lu.ptr[i + 1]; ++p) iw[static_cast<std::size_t>(m_lu.col[p])] = -1;
        if (m_diag[i] != none) {
            double& d = m_lu.val[m_diag[i]];
            if (std::fabs(d) < 1e-14 || !std::isfinite(d)) d = d < 0.0 ? -1e-8 : 1e-8;
        }
    }
    m_ready = true;
}

void Ilu0::apply(const std::vector<double>& r, std::vector<double>& z) const {
    const std::size_t n = m_lu.n;
    const std::size_t none = static_cast<std::size_t>(-1);
    z = r;
    for (std::size_t i = 0; i < n; ++i) {
        double s = z[i];
        for (std::size_t p = m_lu.ptr[i]; p < m_lu.ptr[i + 1]; ++p) {
            const std::size_t k = static_cast<std::size_t>(m_lu.col[p]);
            if (k >= i) break;
            s -= m_lu.val[p] * z[k];
        }
        z[i] = s;
    }
    for (std::size_t i = n; i-- > 0;) {
        if (m_diag[i] == none) continue;
        double s = z[i];
        for (std::size_t p = m_diag[i] + 1; p < m_lu.ptr[i + 1]; ++p) s -= m_lu.val[p] * z[static_cast<std::size_t>(m_lu.col[p])];
        z[i] = s / m_lu.val[m_diag[i]];
    }
}

IterativeResult gmres(const CsrMatrix& a, const Ilu0* pre, const std::vector<double>& b, std::vector<double>& x, double tol, int max_iter, int restart) {
    IterativeResult out;
    const std::size_t n = a.n;
    const int m = std::max(2, restart);
    x.assign(n, 0.0);
    const double bnorm = norm2(b);
    if (bnorm == 0.0) {
        out.converged = true;
        return out;
    }
    std::vector<double> r = b;
    double beta = bnorm;
    int total = 0;
    thread_local std::vector<std::vector<double>> V;
    thread_local std::vector<std::vector<double>> Z;
    V.resize(static_cast<std::size_t>(m) + 1);
    Z.resize(static_cast<std::size_t>(m));
    for (auto& v : V) v.resize(n);
    for (auto& z : Z) z.resize(n);
    std::vector<std::vector<double>> H(static_cast<std::size_t>(m) + 1, std::vector<double>(static_cast<std::size_t>(m), 0.0));
    std::vector<double> cs(static_cast<std::size_t>(m)), sn(static_cast<std::size_t>(m)), g(static_cast<std::size_t>(m) + 1);
    std::vector<double> w;
    while (total < max_iter) {
        for (std::size_t i = 0; i < n; ++i) V[0][i] = r[i] / beta;
        std::fill(g.begin(), g.end(), 0.0);
        g[0] = beta;
        int j = 0;
        bool done = false;
        for (; j < m && total < max_iter; ++j, ++total) {
            const std::size_t js = static_cast<std::size_t>(j);
            if (pre) pre->apply(V[js], Z[js]);
            else Z[js] = V[js];
            a.multiply(Z[js], w);
            for (int i = 0; i <= j; ++i) {
                const std::size_t is = static_cast<std::size_t>(i);
                const double h = dot(w, V[is]);
                H[is][js] = h;
                const std::vector<double>& vi = V[is];
                for (std::size_t k = 0; k < n; ++k) w[k] -= h * vi[k];
            }
            const double hn = norm2(w);
            H[js + 1][js] = hn;
            if (hn > 0.0) {
                for (std::size_t k = 0; k < n; ++k) V[js + 1][k] = w[k] / hn;
            }
            for (int i = 0; i < j; ++i) {
                const std::size_t is = static_cast<std::size_t>(i);
                const double h0 = H[is][js];
                const double h1 = H[is + 1][js];
                H[is][js] = cs[is] * h0 + sn[is] * h1;
                H[is + 1][js] = -sn[is] * h0 + cs[is] * h1;
            }
            const double h0 = H[js][js];
            const double h1 = H[js + 1][js];
            const double den = std::hypot(h0, h1);
            cs[js] = den > 0.0 ? h0 / den : 1.0;
            sn[js] = den > 0.0 ? h1 / den : 0.0;
            H[js][js] = den;
            H[js + 1][js] = 0.0;
            g[js + 1] = -sn[js] * g[js];
            g[js] = cs[js] * g[js];
            if (std::fabs(g[js + 1]) <= tol * bnorm || hn == 0.0) {
                ++j;
                ++total;
                done = true;
                break;
            }
        }
        std::vector<double> y(static_cast<std::size_t>(j), 0.0);
        for (int i = j - 1; i >= 0; --i) {
            const std::size_t is = static_cast<std::size_t>(i);
            double s = g[is];
            for (int k = i + 1; k < j; ++k) s -= H[is][static_cast<std::size_t>(k)] * y[static_cast<std::size_t>(k)];
            const double d = H[is][is];
            y[is] = d != 0.0 ? s / d : 0.0;
        }
        for (int i = 0; i < j; ++i) {
            const std::vector<double>& zi = Z[static_cast<std::size_t>(i)];
            const double yi = y[static_cast<std::size_t>(i)];
            for (std::size_t k = 0; k < n; ++k) x[k] += yi * zi[k];
        }
        a.multiply(x, w);
        for (std::size_t k = 0; k < n; ++k) r[k] = b[k] - w[k];
        beta = norm2(r);
        out.iterations = total;
        out.residual = beta / bnorm;
        if (beta <= tol * bnorm * 10.0 || (done && beta <= tol * bnorm * 1000.0)) {
            out.converged = true;
            return out;
        }
        if (!std::isfinite(beta)) break;
    }
    out.iterations = total;
    out.residual = beta / bnorm;
    out.converged = beta <= 1e-6 * bnorm;
    return out;
}

IterativeResult conjugate_gradient(const CsrMatrix& a, const std::vector<double>& b, std::vector<double>& x, double tol, int max_iter) {
    IterativeResult out;
    const std::size_t n = a.n;
    if (x.size() != n) x.assign(n, 0.0);
    std::vector<double> inv(n, 1.0);
    for (std::size_t i = 0; i < n; ++i) {
        const double d = a.at(i, i);
        if (d != 0.0 && std::isfinite(d)) inv[i] = 1.0 / d;
    }
    std::vector<double> r, z(n), p(n), q;
    a.multiply(x, r);
    for (std::size_t i = 0; i < n; ++i) r[i] = b[i] - r[i];
    const double bnorm = std::max(norm2(b), 1e-300);
    for (std::size_t i = 0; i < n; ++i) z[i] = inv[i] * r[i];
    p = z;
    double rz = dot(r, z);
    for (int it = 0; it < max_iter; ++it) {
        const double rn = norm2(r);
        out.iterations = it;
        out.residual = rn / bnorm;
        if (rn <= tol * bnorm) {
            out.converged = true;
            return out;
        }
        a.multiply(p, q);
        const double pq = dot(p, q);
        if (!(std::fabs(pq) > 0.0)) break;
        const double alpha = rz / pq;
        for (std::size_t i = 0; i < n; ++i) {
            x[i] += alpha * p[i];
            r[i] -= alpha * q[i];
        }
        for (std::size_t i = 0; i < n; ++i) z[i] = inv[i] * r[i];
        const double rz2 = dot(r, z);
        const double beta = rz2 / rz;
        rz = rz2;
        for (std::size_t i = 0; i < n; ++i) p[i] = z[i] + beta * p[i];
    }
    out.residual = norm2(r) / bnorm;
    out.converged = out.residual <= tol * 10.0;
    return out;
}

IterativeResult bicgstab(const CsrMatrix& a, const Ilu0* pre, const std::vector<double>& b, std::vector<double>& x, double tol, int max_iter) {
    IterativeResult out;
    const std::size_t n = a.n;
    if (x.size() != n) x.assign(n, 0.0);
    std::vector<double> r, rhat, p(n, 0.0), v(n, 0.0), s(n), t, ph(n), sh(n);
    a.multiply(x, r);
    for (std::size_t i = 0; i < n; ++i) r[i] = b[i] - r[i];
    rhat = r;
    const double bnorm = std::max(norm2(b), 1e-300);
    double rho = 1.0, alpha = 1.0, omega = 1.0;
    for (int it = 0; it < max_iter; ++it) {
        const double rn = norm2(r);
        out.iterations = it;
        out.residual = rn / bnorm;
        if (rn <= tol * bnorm) {
            out.converged = true;
            return out;
        }
        const double rho2 = dot(rhat, r);
        if (rho2 == 0.0) break;
        const double beta = (rho2 / rho) * (alpha / omega);
        for (std::size_t i = 0; i < n; ++i) p[i] = r[i] + beta * (p[i] - omega * v[i]);
        if (pre) pre->apply(p, ph);
        else ph = p;
        a.multiply(ph, v);
        const double rv = dot(rhat, v);
        if (rv == 0.0) break;
        alpha = rho2 / rv;
        for (std::size_t i = 0; i < n; ++i) s[i] = r[i] - alpha * v[i];
        if (norm2(s) <= tol * bnorm) {
            for (std::size_t i = 0; i < n; ++i) x[i] += alpha * ph[i];
            out.converged = true;
            out.iterations = it + 1;
            out.residual = norm2(s) / bnorm;
            return out;
        }
        if (pre) pre->apply(s, sh);
        else sh = s;
        a.multiply(sh, t);
        const double tt = dot(t, t);
        omega = tt > 0.0 ? dot(t, s) / tt : 0.0;
        for (std::size_t i = 0; i < n; ++i) {
            x[i] += alpha * ph[i] + omega * sh[i];
            r[i] = s[i] - omega * t[i];
        }
        rho = rho2;
        if (omega == 0.0) break;
    }
    a.multiply(x, t);
    double rr = 0.0;
    for (std::size_t i = 0; i < n; ++i) rr += (b[i] - t[i]) * (b[i] - t[i]);
    out.residual = std::sqrt(rr) / bnorm;
    out.converged = out.residual <= tol * 10.0;
    return out;
}

bool SparseSolver::factor(const CsrMatrix& a) {
    m_a = a;
    m_ready = false;
    m_chosen = method;
    if (m_chosen == SparseMethod::Automatic) m_chosen = BandedLU::cost(a) <= band_budget ? SparseMethod::BandedLU : SparseMethod::Gmres;
    if (m_chosen == SparseMethod::BandedLU) {
        if (!m_lu.factor(a)) {
            if (method == SparseMethod::BandedLU) return false;
            m_chosen = SparseMethod::Gmres;
        }
    }
    if (m_chosen == SparseMethod::Gmres || m_chosen == SparseMethod::BiCgStab) m_ilu.factor(a);
    m_ready = true;
    return true;
}

bool SparseSolver::solve(std::vector<double>& b) {
    if (!m_ready) return false;
    if (m_chosen == SparseMethod::BandedLU) {
        m_lu.solve(b);
        for (double v : b) {
            if (!std::isfinite(v)) return false;
        }
        return true;
    }
    std::vector<double> x;
    IterativeResult r;
    if (m_chosen == SparseMethod::Gmres) r = gmres(m_a, &m_ilu, b, x, tolerance, max_iterations);
    else if (m_chosen == SparseMethod::BiCgStab) r = bicgstab(m_a, &m_ilu, b, x, tolerance, max_iterations);
    else r = conjugate_gradient(m_a, b, x, tolerance, max_iterations);
    m_iterations += r.iterations;
    b = x;
    for (double v : b) {
        if (!std::isfinite(v)) return false;
    }
    return r.converged;
}

bool SparseSolver::solve(const std::vector<double>& b, std::vector<double>& x) {
    x = b;
    return solve(x);
}

std::string SparseSolver::name() const {
    switch (m_chosen) {
    case SparseMethod::BandedLU: return "banded LU";
    case SparseMethod::Gmres: return "GMRES with ILU(0)";
    case SparseMethod::BiCgStab: return "BiCGSTAB with ILU(0)";
    case SparseMethod::ConjugateGradient: return "conjugate gradient";
    default: return "automatic";
    }
}

} // namespace numerics
} // namespace fizmo
